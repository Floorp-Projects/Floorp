/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/LGPL 2.1/GPL 2.0
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla ICO Decoder.
 * 
 * The Initial Developer of the Original Code is Netscape.
 * Portions created by Netscape are
 * Copyright (C) 2001 Netscape.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ----- END LICENSE BLOCK ----- */

/* This is a Cross-Platform ICO Decoder, which should work everywhere, including
 * Big-Endian machines like the PowerPC. */

#include "nsICODecoder.h"

#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsIImage.h"
#include "nsMemory.h"
#include "imgIContainerObserver.h"
#include "nsRect.h"
#include "nsCRT.h"

#include "imgILoad.h"

#include "prcpucfg.h" // To get IS_LITTLE_ENDIAN / IS_BIG_ENDIAN

NS_IMPL_ISUPPORTS1(nsICODecoder, imgIDecoder)

#define ICONCOUNTOFFSET 4
#define DIRENTRYOFFSET 6
#define BITMAPINFOSIZE 40
#define PREFICONSIZE 16

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

inline nsresult nsICODecoder::SetPixel(PRUint8*& aDecoded, PRUint8 idx) {
  PRUint8 red, green, blue;
  red = mColors[idx].red;
  green = mColors[idx].green;
  blue = mColors[idx].blue;
  return SetPixel(aDecoded, red, green, blue);
}

inline nsresult nsICODecoder::SetPixel(PRUint8*& aDecoded, PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue) {
#ifdef XP_MAC
  *aDecoded++ = 0; // Mac needs this padding byte
#endif
#ifdef USE_RGBA1
 *aDecoded++ = aRed;
 *aDecoded++ = aGreen;
 *aDecoded++ = aBlue;
#else
 *aDecoded++ = aBlue;
 *aDecoded++ = aGreen;
 *aDecoded++ = aRed;
#endif
  return NS_OK;
}

inline nsresult nsICODecoder::Set4BitPixel(PRUint8*& aDecoded, PRUint8 aData, PRUint32& aPos)
{
  PRUint8 idx = aData >> 4;
  nsresult rv = SetPixel(aDecoded, idx);
  if ((++aPos >= mDirEntry.mWidth) || NS_FAILED(rv))
      return rv;

  idx = aData & 0xF;
  rv = SetPixel(aDecoded, idx);
  ++aPos;
  return rv;
}

inline nsresult nsICODecoder::SetImageData()
{
  PRUint32 bpr;
  mFrame->GetImageBytesPerRow(&bpr);
  for (PRUint32 offset = 0, i = 0; i < mDirEntry.mHeight; ++i, offset += bpr)
    mFrame->SetImageData(mDecodedBuffer+offset, bpr, offset);
  return NS_OK;
}

inline nsresult nsICODecoder::SetAlphaData()
{
  PRUint32 bpr;
  mFrame->GetAlphaBytesPerRow(&bpr);
  for (PRUint32 offset = 0, i = 0; i < mDirEntry.mHeight; ++i, offset += bpr)
    mFrame->SetAlphaData(mAlphaBuffer+offset, bpr, offset);
  return NS_OK;
}

nsICODecoder::nsICODecoder()
{
  NS_INIT_ISUPPORTS();
  mPos = mNumColors = mCurLine = mRowBytes = mImageOffset = mCurrIcon = 0;
  mColors = nsnull;
  mRow = nsnull;
  mDecodingAndMask = PR_FALSE;
  mDecodedBuffer = nsnull;
  mAlphaBuffer = nsnull;
}

nsICODecoder::~nsICODecoder()
{
}

NS_IMETHODIMP nsICODecoder::Init(imgILoad *aLoad)
{ 
  mObserver = do_QueryInterface(aLoad);
    
  mImage = do_CreateInstance("@mozilla.org/image/container;1");
  if (!mImage)
    return NS_ERROR_OUT_OF_MEMORY;

  mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
  if (!mFrame)
    return NS_ERROR_OUT_OF_MEMORY;

  return aLoad->SetImage(mImage);
}

NS_IMETHODIMP nsICODecoder::Close()
{
  if (mObserver) {
    mObserver->OnStopContainer(nsnull, nsnull, mImage);
    mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
    mObserver = nsnull;
  }

  mImage = nsnull;
  mFrame = nsnull;
 
  mPos = 0;

  delete[] mColors;
  mColors = nsnull;

  mCurLine = 0;
  mRowBytes = 0;
  mImageOffset = 0;
  mCurrIcon = 0;
  mNumIcons = 0;

  delete[] mRow;
  mRow = nsnull;

  mDecodingAndMask = PR_FALSE;
  delete[] mDecodedBuffer;
  delete[] mAlphaBuffer;

  return NS_OK;
}

NS_IMETHODIMP nsICODecoder::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP nsICODecoder::WriteFrom(nsIInputStream *aInStr, PRUint32 aCount, PRUint32 *aRetval)
{
  char* buffer = new char[aCount];
  if (!buffer)
    return NS_ERROR_OUT_OF_MEMORY;
  
  unsigned int realCount = aCount;
  nsresult rv = aInStr->Read(buffer, aCount, &realCount);
  NS_ENSURE_SUCCESS(rv, rv);
  
  *aRetval = realCount;

  rv = ProcessData(buffer, realCount);
  delete []buffer;
  return rv;
}

nsresult nsICODecoder::ProcessData(const char* aBuffer, PRUint32 aCount) {
  if (!aCount) // aCount=0 means EOF
    return NS_OK;

  while (aCount && (mPos < ICONCOUNTOFFSET)) { // Skip to the # of icons.
    mPos++; aBuffer++; aCount--;
  }

  if (mPos == ICONCOUNTOFFSET && aCount >= 2) {
    mNumIcons = LITTLE_TO_NATIVE16(((PRUint16*)aBuffer)[0]);
    aBuffer += 2;
    mPos += 2;
    aCount -= 2;
  }

  if (mNumIcons == 0)
    return NS_OK; // Nothing to do.

  while (mCurrIcon < mNumIcons) {
    if (mPos >= DIRENTRYOFFSET + (mCurrIcon*sizeof(mDirEntryArray)) && 
        mPos < DIRENTRYOFFSET + ((mCurrIcon+1)*sizeof(mDirEntryArray))) {
      PRUint32 toCopy = sizeof(mDirEntryArray) - (mPos - DIRENTRYOFFSET - mCurrIcon*sizeof(mDirEntryArray));
      if (toCopy > aCount)
        toCopy = aCount;
      nsCRT::memcpy(mDirEntryArray + sizeof(mDirEntryArray) - toCopy, aBuffer, toCopy);
      mPos += toCopy;
      aCount -= toCopy;
      aBuffer += toCopy;
    }

    if (mPos == 22+mCurrIcon*sizeof(mDirEntryArray)) {
      mCurrIcon++;
      if (mImageOffset == 0) {
        ProcessDirEntry();
        if (mDirEntry.mReserved != 0)
          return NS_ERROR_FAILURE; // This ICO is garbage.
        if ((mDirEntry.mWidth == PREFICONSIZE && mDirEntry.mHeight == PREFICONSIZE) || mCurrIcon == mNumIcons)
          mImageOffset = mDirEntry.mImageOffset;
      }
    }
  }

  while (aCount && mPos < mImageOffset) { // Skip to our offset
    mPos++; aBuffer++; aCount--;
  }

  if (mCurrIcon == mNumIcons && mPos >= mImageOffset && mPos < mImageOffset + BITMAPINFOSIZE) {
    // We've found the icon.
    PRUint32 toCopy = sizeof(mBIHraw) - (mPos - mImageOffset);
    if (toCopy > aCount)
      toCopy = aCount;

    nsCRT::memcpy(mBIHraw + (mPos - mImageOffset), aBuffer, toCopy);
    mPos += toCopy;
    aCount -= toCopy;
    aBuffer += toCopy;
  }

  if (mPos == mImageOffset + BITMAPINFOSIZE) {
    ProcessInfoHeader();
    if (mBIH.bpp <= 8) {
      switch (mBIH.bpp) {
        case 1:
          mNumColors = 2;
          break;
        case 4:
          mNumColors = 16;
          break;
        case 8:
          mNumColors = 256;
          break;
        default:
          return NS_ERROR_FAILURE;
      }

      mColors = new colorTable[mNumColors];
      if (!mColors)
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = mImage->Init(mDirEntry.mWidth, mDirEntry.mHeight, mObserver);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mObserver->OnStartContainer(nsnull, nsnull, mImage);
    NS_ENSURE_SUCCESS(rv, rv);

    mCurLine = mDirEntry.mHeight;
    mRow = new PRUint8[(mDirEntry.mWidth * mBIH.bpp)/8 + 4];
    // +4 because the line is padded to a 4 bit boundary, but I don't want
    // to make exact calculations here, that's unnecessary.
    // Also, it compensates rounding error.
    if (!mRow)
      return NS_ERROR_OUT_OF_MEMORY;
    
    rv = mFrame->Init(0, 0, mDirEntry.mWidth, mDirEntry.mWidth, GFXFORMATALPHA);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mImage->AppendFrame(mFrame);
    NS_ENSURE_SUCCESS(rv, rv);
    mObserver->OnStartFrame(nsnull, nsnull, mFrame);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mColors && (mPos >= mImageOffset + BITMAPINFOSIZE) && 
                 (mPos < (mImageOffset + BITMAPINFOSIZE + mNumColors * 4))) {
    // We will receive (mNumColors * 4) bytes of color data
    PRUint32 colorBytes = mPos - (mImageOffset + 40); // Number of bytes already received
    PRUint8 colorNum = colorBytes / 4; // Color which is currently received
    PRUint8 at = colorBytes % 4;
    while (aCount && (mPos < (mImageOffset + BITMAPINFOSIZE + mNumColors * 4))) {
      switch (at) {
        case 0:
          mColors[colorNum].blue = *aBuffer;
          break;
        case 1:
          mColors[colorNum].green = *aBuffer;
          break;
        case 2:
          mColors[colorNum].red = *aBuffer;
          break;
        case 3:
          colorNum++; // This is a padding byte
          break;
      }
      mPos++; aBuffer++; aCount--;
      at = (at + 1) % 4;
    }
  }

  if (!mDecodingAndMask && (mPos >= (mImageOffset + BITMAPINFOSIZE + mNumColors*4))) {
    // Increment mPos to avoid reprocessing the info header.
    if (mPos == (mImageOffset + BITMAPINFOSIZE + mNumColors*4)) {
      mPos++;
#ifdef XP_MAC
      mDecodedBuffer = new PRUint8[mDirEntry.mHeight*mDirEntry.mWidth*4];
#else
      mDecodedBuffer = new PRUint8[mDirEntry.mHeight*mDirEntry.mWidth*3];
#endif
      if (!mDecodedBuffer)
        return NS_ERROR_OUT_OF_MEMORY;
    }

    PRUint32 rowSize = (mBIH.bpp * mDirEntry.mWidth + 7) / 8; // +7 to round up
    if (rowSize % 4)
      rowSize += (4 - (rowSize % 4)); // Pad to DWORD Boundary
    PRUint32 toCopy;
    do {
        toCopy = rowSize - mRowBytes;
        if (toCopy) {
            if (toCopy > aCount)
                toCopy = aCount;
            nsCRT::memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
        }
        if ((rowSize - mRowBytes) == 0) {
#ifdef XP_MAC
            PRUint8* decoded = mDecodedBuffer+((mCurLine-1)*mDirEntry.mWidth*4);
#else
            PRUint8* decoded = mDecodedBuffer+((mCurLine-1)*mDirEntry.mWidth*3);
#endif
            PRUint8* p = mRow;
            PRUint8* d = decoded;
            PRUint32 lpos = 0;
            switch (mBIH.bpp) {
              case 1:
                while (lpos < mDirEntry.mWidth) {
                  PRInt8 bit;
                  PRUint8 idx;
                  for (bit = 7; bit >= 0; bit--) {
                      if (lpos >= mDirEntry.mWidth)
                          break;
                      idx = (*p >> bit) & 1;
                      SetPixel(d, idx);
                      ++lpos;
                  }
                  ++p;
                }
                break;
              case 4:
                while (lpos < mDirEntry.mWidth) {
                  Set4BitPixel(d, *p, lpos);
                  ++p;
                }
                break;
              case 8:
                while (lpos < mDirEntry.mWidth) {
                  SetPixel(d, *p);
                  ++lpos;
                  ++p;
                }
                break;
              case 16:
                while (lpos < mDirEntry.mWidth) {
                  SetPixel(d,
                          (p[1] & 124) << 1,
                          ((p[1] & 3) << 6) | ((p[0] & 224) >> 2),
                          (p[0] & 31) << 3);

                  ++lpos;
                  p+=2;
                }
                break;
              case 32:
              case 24:
                while (lpos < mDirEntry.mWidth) {
                  SetPixel(d, p[2], p[1], p[0]);
                  p += 2;
                  ++lpos;
                  if (mBIH.bpp == 32)
                    p++; // Padding byte
                  ++p;
                }
                break;
              default:
                // This is probably the wrong place to check this...
                return NS_ERROR_FAILURE;
            }

            if (mCurLine == 1)
              mDecodingAndMask = PR_TRUE;
              
            mCurLine--; mRowBytes = 0;
        }
    } while (!mDecodingAndMask && aCount > 0);
  }

  if (mDecodingAndMask) {
    PRUint32 rowSize = (mDirEntry.mWidth + 7) / 8; // +7 to round up
    if (rowSize % 4)
      rowSize += (4 - (rowSize % 4)); // Pad to DWORD Boundary
    
    if (mPos == (1 + mImageOffset + BITMAPINFOSIZE + mNumColors*4)) {
      mPos++;
      mRowBytes = 0;
      mCurLine = mDirEntry.mHeight;
      delete []mRow;
      mRow = new PRUint8[rowSize];
      mAlphaBuffer = new PRUint8[mDirEntry.mHeight*rowSize];
    }

    PRUint32 toCopy;
    do {
        toCopy = rowSize - mRowBytes;
        if (toCopy) {
            if (toCopy > aCount)
                toCopy = aCount;
            nsCRT::memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
        }
        if ((rowSize - mRowBytes) == 0) {
            PRUint8* decoded = mAlphaBuffer+((mCurLine-1)*rowSize);
            PRUint8* p = mRow;
            PRUint32 lpos = 0;
            while (lpos < rowSize) {
              PRUint8 idx = *p;
              idx ^= 255;  // We complement the value, since our method of storing transparency is opposite
                           // what Win32 uses in its masks.
              decoded[lpos] = idx;
              lpos++;
              p++;
            }
            
            if (mCurLine == 1) {
              SetAlphaData();
              SetImageData();
              mObserver->OnStopFrame(nsnull, nsnull, mFrame);
              return NS_OK;
            }
              
            mCurLine--; mRowBytes = 0;
        }
    } while (aCount > 0);
  }

  return NS_OK;
}

void
nsICODecoder::ProcessDirEntry()
{
  nsCRT::memset(&mDirEntry, 0, sizeof(mDirEntry));
  DOCOPY(&mDirEntry.mWidth, mDirEntryArray);
  DOCOPY(&mDirEntry.mHeight, mDirEntryArray+1);
  DOCOPY(&mDirEntry.mColorCount, mDirEntryArray+2);
  DOCOPY(&mDirEntry.mReserved, mDirEntryArray+3);
  
  DOCOPY(&mDirEntry.mPlanes, mDirEntryArray+4);
  mDirEntry.mPlanes = LITTLE_TO_NATIVE16(mDirEntry.mPlanes);

  DOCOPY(&mDirEntry.mBitCount, mDirEntryArray+6);
  mDirEntry.mBitCount = LITTLE_TO_NATIVE16(mDirEntry.mBitCount);

  DOCOPY(&mDirEntry.mBytesInRes, mDirEntryArray+8);
  mDirEntry.mBytesInRes = LITTLE_TO_NATIVE32(mDirEntry.mBytesInRes);

  DOCOPY(&mDirEntry.mImageOffset, mDirEntryArray+12);
  mDirEntry.mImageOffset = LITTLE_TO_NATIVE32(mDirEntry.mImageOffset);
}

void nsICODecoder::ProcessInfoHeader() {
  nsCRT::memset(&mBIH, 0, sizeof(mBIH));
  // Ignoring the size; it should always be 40 for icons, anyway

  DOCOPY(&mBIH.width, mBIHraw + 4);
  DOCOPY(&mBIH.height, mBIHraw + 8);
  DOCOPY(&mBIH.planes, mBIHraw + 12);
  DOCOPY(&mBIH.bpp, mBIHraw + 14);
  DOCOPY(&mBIH.compression, mBIHraw + 16);
  DOCOPY(&mBIH.image_size, mBIHraw + 20);
  DOCOPY(&mBIH.xppm, mBIHraw + 24);
  DOCOPY(&mBIH.yppm, mBIHraw + 28);
  DOCOPY(&mBIH.colors, mBIHraw + 32);
  DOCOPY(&mBIH.important_colors, mBIHraw + 36);

  // Convert endianness
  mBIH.width = LITTLE_TO_NATIVE32(mBIH.width);
  mBIH.height = LITTLE_TO_NATIVE32(mBIH.height);
  mBIH.planes = LITTLE_TO_NATIVE16(mBIH.planes);
  mBIH.bpp = LITTLE_TO_NATIVE16(mBIH.bpp);

  mBIH.compression = LITTLE_TO_NATIVE32(mBIH.compression);
  mBIH.image_size = LITTLE_TO_NATIVE32(mBIH.image_size);
  mBIH.xppm = LITTLE_TO_NATIVE32(mBIH.xppm);
  mBIH.yppm = LITTLE_TO_NATIVE32(mBIH.yppm);
  mBIH.colors = LITTLE_TO_NATIVE32(mBIH.colors);
  mBIH.important_colors = LITTLE_TO_NATIVE32(mBIH.important_colors);
}

