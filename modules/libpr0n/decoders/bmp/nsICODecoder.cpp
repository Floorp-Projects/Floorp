/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla ICO Decoder.
 *
 * The Initial Developer of the Original Code is
 * Netscape.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
 *   Christian Biesinger <cbiesinger@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This is a Cross-Platform ICO Decoder, which should work everywhere, including
 * Big-Endian machines like the PowerPC. */

#include <stdlib.h>

#include "nsICODecoder.h"

#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "imgIContainerObserver.h"

#include "imgILoad.h"

#include "nsIProperties.h"
#include "nsISupportsPrimitives.h"

#include "nsAutoPtr.h"

NS_IMPL_ISUPPORTS1(nsICODecoder, imgIDecoder)

#define ICONCOUNTOFFSET 4
#define DIRENTRYOFFSET 6
#define BITMAPINFOSIZE 40
#define PREFICONSIZE 16

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

static PRUint32 premultiply(PRUint32 x)
{
    PRUint32 a = x >> 24;
    PRUint32 t = (x & 0xff00ff) * a + 0x800080;
    t = (t + ((t >> 8) & 0xff00ff)) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff) * a + 0x80;
    x = (x + ((x >> 8) & 0xff));
    x &= 0xff00;
    x |= t | (a << 24);
    return x;
}

nsresult nsICODecoder::SetImageData()
{
  if (mHaveAlphaData) {
    // We have premultiply the pixels when we have alpha transparency
    PRUint32* p = (PRUint32*)mDecodedBuffer;
    for (PRUint32 c = mDirEntry.mWidth * mDirEntry.mHeight; c > 0; --c) {
      *p = premultiply(*p);
      p++;
    }
  }
  // In Cairo we can set the whole image in one go
  PRUint32 dataLen = mDirEntry.mHeight * mDirEntry.mWidth * 4;
  mFrame->SetImageData(mDecodedBuffer, dataLen, 0);

  nsIntRect r(0, 0, 0, 0);
  mFrame->GetWidth(&r.width);
  mFrame->GetHeight(&r.height);
  mObserver->OnDataAvailable(nsnull, mFrame, &r);

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
  mPos = mNumColors = mRowBytes = mImageOffset = mCurrIcon = mNumIcons = 0;
  mCurLine = 1; // Otherwise decoder will never start
  mStatus = NS_OK;
  mColors = nsnull;
  mRow = nsnull;
  mHaveAlphaData = 0;
  mDecodingAndMask = PR_FALSE;
  mDecodedBuffer = nsnull;
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
    mObserver->OnStopContainer(nsnull, mImage);
    mObserver->OnStopDecode(nsnull, NS_OK, nsnull);
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

  free(mRow);
  mRow = nsnull;

  mDecodingAndMask = PR_FALSE;
  free(mDecodedBuffer);

  return NS_OK;
}

NS_IMETHODIMP nsICODecoder::Flush()
{
  // Set Data here because some ICOs don't have a complete AND Mask
  // see bug 115357
  if (mDecodingAndMask) {
    SetImageData();
    mObserver->OnStopFrame(nsnull, mFrame);
  }
  return NS_OK;
}


NS_METHOD nsICODecoder::ReadSegCb(nsIInputStream* aIn, void* aClosure,
                             const char* aFromRawSegment, PRUint32 aToOffset,
                             PRUint32 aCount, PRUint32 *aWriteCount) {
  nsICODecoder *decoder = NS_REINTERPRET_CAST(nsICODecoder*, aClosure);
  *aWriteCount = aCount;
  decoder->mStatus = decoder->ProcessData(aFromRawSegment, aCount);
  return decoder->mStatus;
}

NS_IMETHODIMP nsICODecoder::WriteFrom(nsIInputStream *aInStr, PRUint32 aCount, PRUint32 *aRetval)
{
  nsresult rv = aInStr->ReadSegments(ReadSegCb, this, aCount, aRetval);
  NS_ENSURE_SUCCESS(rv, rv);
  return mStatus;
}

nsresult nsICODecoder::ProcessData(const char* aBuffer, PRUint32 aCount) {
  if (!aCount) // aCount=0 means EOF
    return NS_OK;

  while (aCount && (mPos < ICONCOUNTOFFSET)) { // Skip to the # of icons.
    if (mPos == 2) { // if the third byte is 1: This is an icon, 2: a cursor
      if ((*aBuffer != 1) && (*aBuffer != 2)) {
        return NS_ERROR_FAILURE;
      }
      mIsCursor = (*aBuffer == 2);
    }
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
      if ((e.mWidth == PREFICONSIZE && e.mHeight == PREFICONSIZE && e.mBitCount >= colorDepth)
           || (mCurrIcon == mNumIcons && mImageOffset == 0)) {
        mImageOffset = e.mImageOffset;

        // ensure mImageOffset is >= the size of the direntry headers (bug #245631)
        PRUint32 minImageOffset = DIRENTRYOFFSET + mNumIcons*sizeof(mDirEntryArray);
        if (mImageOffset < minImageOffset)
          return NS_ERROR_FAILURE;

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

  nsresult rv;

  if (mPos == mImageOffset + BITMAPINFOSIZE) {
    rv = mObserver->OnStartDecode(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

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

    rv = mImage->Init(mDirEntry.mWidth, mDirEntry.mHeight, mObserver);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mIsCursor) {
      nsCOMPtr<nsIProperties> props(do_QueryInterface(mImage));
      if (props) {
        nsCOMPtr<nsISupportsPRUint32> intwrapx = do_CreateInstance("@mozilla.org/supports-PRUint32;1");
        nsCOMPtr<nsISupportsPRUint32> intwrapy = do_CreateInstance("@mozilla.org/supports-PRUint32;1");

        if (intwrapx && intwrapy) {
          intwrapx->SetData(mDirEntry.mXHotspot);
          intwrapy->SetData(mDirEntry.mYHotspot);

          props->Set("hotspotX", intwrapx);
          props->Set("hotspotY", intwrapy);
        }
      }
    }

    rv = mObserver->OnStartContainer(nsnull, mImage);
    NS_ENSURE_SUCCESS(rv, rv);

    mCurLine = mDirEntry.mHeight;
    mRow = (PRUint8*)malloc((mDirEntry.mWidth * mBIH.bpp)/8 + 4);
    // +4 because the line is padded to a 4 bit boundary, but I don't want
    // to make exact calculations here, that's unnecessary.
    // Also, it compensates rounding error.
    if (!mRow)
      return NS_ERROR_OUT_OF_MEMORY;
    
    rv = mFrame->Init(0, 0, mDirEntry.mWidth, mDirEntry.mHeight, GFXFORMATALPHA8, 24);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mImage->AppendFrame(mFrame);
    NS_ENSURE_SUCCESS(rv, rv);
    mObserver->OnStartFrame(nsnull, mFrame);
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
    if (mPos == (mImageOffset + BITMAPINFOSIZE + mNumColors*4)) {
      // Increment mPos to avoid reprocessing the info header.
      mPos++;
      mDecodedBuffer = (PRUint8*)malloc(mDirEntry.mHeight*mDirEntry.mWidth*4);
      if (!mDecodedBuffer)
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Ensure memory has been allocated before decoding. If we get this far 
    // without allocated memory, the file is most likely invalid.
    NS_ASSERTION(mRow, "mRow is null");
    NS_ASSERTION(mDecodedBuffer, "mDecodedBuffer is null");
    if (!mRow || !mDecodedBuffer)
      return NS_ERROR_FAILURE;

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
            PRUint8* d = mDecodedBuffer + (mCurLine * mDirEntry.mWidth * GFXBYTESPERPIXEL);
            PRUint8* p = mRow;
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
              case 24:
                while (lpos > 0) {
                  SetPixel(d, p[2], p[1], p[0]);
                  p += 3;
                  --lpos;
                }
                break;
              case 32:
                while (lpos > 0) {
                  SetPixel(d, p[2], p[1], p[0], p[3]);
                  mHaveAlphaData |= p[3]; // Alpha value
                  p += 4;
                  --lpos;
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

  if (mDecodingAndMask && !mHaveAlphaData) {
    PRUint32 rowSize = CalcAlphaRowSize();

    if (mPos == (1 + mImageOffset + BITMAPINFOSIZE + mNumColors*4)) {
      mPos++;
      mRowBytes = 0;
      mCurLine = mDirEntry.mHeight;
      free(mRow);
      mRow = (PRUint8*)malloc(rowSize);
      if (!mRow)
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Ensure memory has been allocated before decoding.
    NS_ASSERTION(mRow, "mRow is null");
    NS_ASSERTION(mDecodedBuffer, "mDecodedBuffer is null");
    if (!mRow || !mDecodedBuffer)
      return NS_ERROR_FAILURE;

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

            PRUint8* decoded =
              mDecodedBuffer + (mCurLine * mDirEntry.mWidth * GFXBYTESPERPIXEL);
#ifdef IS_LITTLE_ENDIAN
            decoded += 3;
#endif
            PRUint8* decoded_end =
              decoded + mDirEntry.mWidth * GFXBYTESPERPIXEL;
            for (PRUint8* p = mRow, *p_end = mRow + rowSize; p < p_end; ++p) {
              PRUint8 idx = *p;
              for (PRUint8 bit = 0x80; bit && decoded<decoded_end; bit >>= 1) {
                // We complement the value, since our method of storing
                // transparency is opposite what Win32 uses in its masks.
                *decoded = (idx & bit) ? 0x00 : 0xff;
                decoded += GFXBYTESPERPIXEL;
              }
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
