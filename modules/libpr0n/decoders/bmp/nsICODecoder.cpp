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
#include "imgIContainerObserver.h"

#include "imgILoad.h"

NS_IMPL_ISUPPORTS1(nsICODecoder, imgIDecoder)

#define ICONCOUNTOFFSET 4
#define DIRENTRYOFFSET 6
#define BITMAPINFOSIZE 40
#define PREFICONSIZE 16

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

nsresult nsICODecoder::SetImageData()
{
  PRUint32 bpr;
  mFrame->GetImageBytesPerRow(&bpr);
 
  // Since the ICO is decoded into an exact sized array, the frame may use
  // more bytes per row of pixels than the decoding array.
#if defined(XP_MAC) || defined(XP_MACOSX)
  PRUint32 decodedLineLen = mDirEntry.mWidth * 4;
#else
  PRUint32 decodedLineLen = mDirEntry.mWidth * 3;
#endif

  PRUint8* decodeBufferPos = mDecodedBuffer;
  PRUint32 frameOffset = 0;

  for (PRUint32 i = 0;
       i < mDirEntry.mHeight;
       ++i, frameOffset += bpr, decodeBufferPos += decodedLineLen) {
    mFrame->SetImageData(decodeBufferPos, decodedLineLen, frameOffset);
  }
  return NS_OK;
}

nsresult nsICODecoder::SetAlphaData()
{
  PRUint32 bpr;
  mFrame->GetAlphaBytesPerRow(&bpr);

  PRUint32 decoderRowSize = CalcAlphaRowSize();

  // In case the decoder and frame have different sized alpha buffers, we
  // take the smaller of the two row length values as the row length to copy.
  PRUint32 rowCopyLen = PR_MIN(bpr, decoderRowSize);

  PRUint8* alphaBufferPos = mAlphaBuffer;
  PRUint32 frameOffset = 0;

  for (PRUint32 i = 0;
       i < mDirEntry.mHeight;
       ++i, frameOffset += bpr, alphaBufferPos += decoderRowSize) {
    mFrame->SetAlphaData(alphaBufferPos, rowCopyLen, frameOffset);
  }
  return NS_OK;
}

PRUint32 nsICODecoder::CalcAlphaRowSize()
{
  PRUint32 rowSize = (mDirEntry.mWidth + 7) / 8; // +7 to round up
  if (rowSize % 4)
    rowSize += (4 - (rowSize % 4)); // Pad to DWORD Boundary
  return rowSize;
}

nsICODecoder::nsICODecoder()
{
  NS_INIT_ISUPPORTS();
  mPos = mNumColors = mRowBytes = mImageOffset = mCurrIcon = mNumIcons = 0;
  mCurLine = 1; // Otherwise decoder will never start
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
  mObserver->OnStopContainer(nsnull, nsnull, mImage);
  mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
  mObserver = nsnull;

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
  // Set Data here because some ICOs don't have a complete AND Mask
  // see bug 115357
  if (mDecodingAndMask) {
    SetAlphaData();
    SetImageData();
    mObserver->OnStopFrame(nsnull, nsnull, mFrame);
  }
  return NS_OK;
}


NS_METHOD nsICODecoder::ReadSegCb(nsIInputStream* aIn, void* aClosure,
                             const char* aFromRawSegment, PRUint32 aToOffset,
                             PRUint32 aCount, PRUint32 *aWriteCount) {
    nsICODecoder *decoder = NS_REINTERPRET_CAST(nsICODecoder*, aClosure);
    *aWriteCount = aCount;
    return decoder->ProcessData(aFromRawSegment, aCount);
}

NS_IMETHODIMP nsICODecoder::WriteFrom(nsIInputStream *aInStr, PRUint32 aCount, PRUint32 *aRetval)
{
    return aInStr->ReadSegments(ReadSegCb, this, aCount, aRetval);
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

  PRUint16 colorDepth = 0;
  while (mCurrIcon < mNumIcons) {
    if (mPos >= DIRENTRYOFFSET + (mCurrIcon*sizeof(mDirEntryArray)) && 
        mPos < DIRENTRYOFFSET + ((mCurrIcon+1)*sizeof(mDirEntryArray))) {
      PRUint32 toCopy = sizeof(mDirEntryArray) - (mPos - DIRENTRYOFFSET - mCurrIcon*sizeof(mDirEntryArray));
      if (toCopy > aCount)
        toCopy = aCount;
      memcpy(mDirEntryArray + sizeof(mDirEntryArray) - toCopy, aBuffer, toCopy);
      mPos += toCopy;
      aCount -= toCopy;
      aBuffer += toCopy;
    }
    if (aCount == 0)
      return NS_OK; // Need more data

    IconDirEntry e;
    if (mPos == 22+mCurrIcon*sizeof(mDirEntryArray)) {
      mCurrIcon++;
      ProcessDirEntry(e);
      if (e.mReserved != 0)
        return NS_ERROR_FAILURE; // This ICO is garbage.
      if ((e.mWidth == PREFICONSIZE && e.mHeight == PREFICONSIZE && e.mBitCount >= colorDepth)
           || (mCurrIcon == mNumIcons && mImageOffset == 0)) {
        mImageOffset = e.mImageOffset;
        colorDepth = e.mBitCount;
        memcpy(&mDirEntry, &e, sizeof(IconDirEntry));
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

    memcpy(mBIHraw + (mPos - mImageOffset), aBuffer, toCopy);
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
    
    rv = mFrame->Init(0, 0, mDirEntry.mWidth, mDirEntry.mHeight, GFXFORMATALPHA, 24);
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
#if defined(XP_MAC) || defined(XP_MACOSX)
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
            memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
        }
        if (rowSize == mRowBytes) {
            mCurLine--;
            PRUint8* decoded = mDecodedBuffer + (mCurLine * mDirEntry.mWidth * GFXBYTESPERPIXEL);
            PRUint8* p = mRow;
            PRUint8* d = decoded;
            PRUint32 lpos = mDirEntry.mWidth;
            switch (mBIH.bpp) {
              case 1:
                while (lpos > 0) {
                  PRInt8 bit;
                  PRUint8 idx;
                  for (bit = 7; bit >= 0 && lpos > 0; bit--) {
                      idx = (*p >> bit) & 1;
                      SetPixel(d, idx, mColors);
                      --lpos;
                  }
                  ++p;
                }
                break;
              case 4:
                while (lpos > 0) {
                  Set4BitPixel(d, *p, lpos, mColors);
                  ++p;
                }
                break;
              case 8:
                while (lpos > 0) {
                  SetPixel(d, *p, mColors);
                  --lpos;
                  ++p;
                }
                break;
              case 16:
                while (lpos > 0) {
                  SetPixel(d,
                          (p[1] & 124) << 1,
                          ((p[1] & 3) << 6) | ((p[0] & 224) >> 2),
                          (p[0] & 31) << 3);

                  --lpos;
                  p+=2;
                }
                break;
              case 32:
              case 24:
                while (lpos > 0) {
                  SetPixel(d, p[2], p[1], p[0]);
                  p += 2;
                  --lpos;
                  if (mBIH.bpp == 32)
                    p++; // Padding byte
                  ++p;
                }
                break;
              default:
                // This is probably the wrong place to check this...
                return NS_ERROR_FAILURE;
            }

            if (mCurLine == 0)
              mDecodingAndMask = PR_TRUE;
              
            mRowBytes = 0;
        }
    } while (!mDecodingAndMask && aCount > 0);
  }

  if (mDecodingAndMask) {
    PRUint32 rowSize = CalcAlphaRowSize();

    if (mPos == (1 + mImageOffset + BITMAPINFOSIZE + mNumColors*4)) {
      mPos++;
      mRowBytes = 0;
      mCurLine = mDirEntry.mHeight;
      delete []mRow;
      mRow = new PRUint8[rowSize];
      mAlphaBuffer = new PRUint8[mDirEntry.mHeight*rowSize];
      memset(mAlphaBuffer, 0xff, mDirEntry.mHeight*rowSize);
    }

    PRUint32 toCopy;
    do {
        if (mCurLine == 0) {
          return NS_OK;
        }

        toCopy = rowSize - mRowBytes;
        if (toCopy) {
            if (toCopy > aCount)
                toCopy = aCount;
            memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
        }
        if ((rowSize - mRowBytes) == 0) {
            mCurLine--;
            PRUint8* decoded = mAlphaBuffer+(mCurLine*rowSize);
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
            
            mRowBytes = 0;
        }
    } while (aCount > 0);
  }

  return NS_OK;
}

void
nsICODecoder::ProcessDirEntry(IconDirEntry& aTarget)
{
  memset(&aTarget, 0, sizeof(aTarget));
  memcpy(&aTarget.mWidth, mDirEntryArray, sizeof(aTarget.mWidth));
  memcpy(&aTarget.mHeight, mDirEntryArray+1, sizeof(aTarget.mHeight));
  memcpy(&aTarget.mColorCount, mDirEntryArray+2, sizeof(aTarget.mColorCount));
  memcpy(&aTarget.mReserved, mDirEntryArray+3, sizeof(aTarget.mReserved));
  
  memcpy(&aTarget.mPlanes, mDirEntryArray+4, sizeof(aTarget.mPlanes));
  aTarget.mPlanes = LITTLE_TO_NATIVE16(aTarget.mPlanes);

  memcpy(&aTarget.mBitCount, mDirEntryArray+6, sizeof(aTarget.mBitCount));
  aTarget.mBitCount = LITTLE_TO_NATIVE16(aTarget.mBitCount);

  memcpy(&aTarget.mBytesInRes, mDirEntryArray+8, sizeof(aTarget.mBytesInRes));
  aTarget.mBytesInRes = LITTLE_TO_NATIVE32(aTarget.mBytesInRes);

  memcpy(&aTarget.mImageOffset, mDirEntryArray+12, sizeof(aTarget.mImageOffset));
  aTarget.mImageOffset = LITTLE_TO_NATIVE32(aTarget.mImageOffset);
}

void nsICODecoder::ProcessInfoHeader() {
  memset(&mBIH, 0, sizeof(mBIH));
  // Ignoring the size; it should always be 40 for icons, anyway

  memcpy(&mBIH.width, mBIHraw + 4, sizeof(mBIH.width));
  memcpy(&mBIH.height, mBIHraw + 8, sizeof(mBIH.height));
  memcpy(&mBIH.planes, mBIHraw + 12, sizeof(mBIH.planes));
  memcpy(&mBIH.bpp, mBIHraw + 14, sizeof(mBIH.bpp));
  memcpy(&mBIH.compression, mBIHraw + 16, sizeof(mBIH.compression));
  memcpy(&mBIH.image_size, mBIHraw + 20, sizeof(mBIH.image_size));
  memcpy(&mBIH.xppm, mBIHraw + 24, sizeof(mBIH.xppm));
  memcpy(&mBIH.yppm, mBIHraw + 28, sizeof(mBIH.yppm));
  memcpy(&mBIH.colors, mBIHraw + 32, sizeof(mBIH.colors));
  memcpy(&mBIH.important_colors, mBIHraw + 36, sizeof(mBIH.important_colors));

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

