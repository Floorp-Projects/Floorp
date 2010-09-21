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
 *   Bobby Holley <bobbyholley@gmail.com>
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
#include "RasterImage.h"
#include "imgIContainerObserver.h"

#include "nsIProperties.h"
#include "nsISupportsPrimitives.h"

namespace mozilla {
namespace imagelib {

#define ICONCOUNTOFFSET 4
#define DIRENTRYOFFSET 6
#define BITMAPINFOSIZE 40
#define PREFICONSIZE 16

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

PRUint32 nsICODecoder::CalcAlphaRowSize()
{
  // Calculate rowsize in DWORD's and then return in # of bytes
  PRUint32 rowSize = (mDirEntry.mWidth + 31) / 32; // +31 to round up
  return rowSize * 4;        // Return rowSize in bytes
}

nsICODecoder::nsICODecoder()
{
  mPos = mNumColors = mRowBytes = mImageOffset = mCurrIcon = mNumIcons = 0;
  mCurLine = 1; // Otherwise decoder will never start
  mColors = nsnull;
  mRow = nsnull;
  mHaveAlphaData = mDecodingAndMask = PR_FALSE;
}

nsICODecoder::~nsICODecoder()
{
  mPos = 0;

  delete[] mColors;

  mCurLine = 0;
  mRowBytes = 0;
  mImageOffset = 0;
  mCurrIcon = 0;
  mNumIcons = 0;

  if (mRow) {
    free(mRow);
    mRow = nsnull;
  }
  mDecodingAndMask = PR_FALSE;
}

void
nsICODecoder::FinishInternal()
{
  // We shouldn't be called in error cases
  NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call FinishInternal after error!");

  // We should never make multiple frames
  NS_ABORT_IF_FALSE(GetFrameCount() <= 1, "Multiple ICO frames?");

  // Send notifications if appropriate
  if (!IsSizeDecode() && (GetFrameCount() == 1)) {

    // Invalidate
    nsIntRect r(0, 0, mDirEntry.mWidth, mDirEntry.mHeight);
    PostInvalidation(r);

    PostFrameStop();
    PostDecodeDone();
  }
}

void
nsICODecoder::WriteInternal(const char* aBuffer, PRUint32 aCount)
{
  NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call WriteInternal after error!");

  if (!aCount) // aCount=0 means EOF
    return;

  while (aCount && (mPos < ICONCOUNTOFFSET)) { // Skip to the # of icons.
    if (mPos == 2) { // if the third byte is 1: This is an icon, 2: a cursor
      if ((*aBuffer != 1) && (*aBuffer != 2)) {
        PostDataError();
        return;
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
    return; // Nothing to do.

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
      return; // Need more data

    IconDirEntry e;
    if (mPos == 22+mCurrIcon*sizeof(mDirEntryArray)) {
      mCurrIcon++;
      ProcessDirEntry(e);
      if ((e.mWidth == PREFICONSIZE && e.mHeight == PREFICONSIZE && e.mBitCount >= colorDepth)
           || (mCurrIcon == mNumIcons && mImageOffset == 0)) {
        mImageOffset = e.mImageOffset;

        // ensure mImageOffset is >= the size of the direntry headers (bug #245631)
        PRUint32 minImageOffset = DIRENTRYOFFSET + mNumIcons*sizeof(mDirEntryArray);
        if (mImageOffset < minImageOffset) {
          PostDataError();
          return;
        }

        colorDepth = e.mBitCount;
        memcpy(&mDirEntry, &e, sizeof(IconDirEntry));
      }
    }
  }

  if (mPos < mImageOffset) {
    // Skip to (or at least towards) the desired image offset
    PRUint32 toSkip = mImageOffset - mPos;
    if (toSkip > aCount)
      toSkip = aCount;

    mPos    += toSkip;
    aBuffer += toSkip;
    aCount  -= toSkip;
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

    ProcessInfoHeader();
    PostSize(mDirEntry.mWidth, mDirEntry.mHeight);
    if (IsSizeDecode())
      return;

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
          PostDataError();
          return;
      }

      mColors = new colorTable[mNumColors];
    }

    if (mIsCursor) {
      nsCOMPtr<nsISupportsPRUint32> intwrapx = do_CreateInstance("@mozilla.org/supports-PRUint32;1");
      nsCOMPtr<nsISupportsPRUint32> intwrapy = do_CreateInstance("@mozilla.org/supports-PRUint32;1");

      if (intwrapx && intwrapy) {
        intwrapx->SetData(mDirEntry.mXHotspot);
        intwrapy->SetData(mDirEntry.mYHotspot);

        mImage->Set("hotspotX", intwrapx);
        mImage->Set("hotspotY", intwrapy);
      }
    }

    mCurLine = mDirEntry.mHeight;
    mRow = (PRUint8*)moz_malloc((mDirEntry.mWidth * mBIH.bpp)/8 + 4);
    // +4 because the line is padded to a 4 bit boundary, but I don't want
    // to make exact calculations here, that's unnecessary.
    // Also, it compensates rounding error.
    if (!mRow) {
      PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    PRUint32 imageLength;
    rv = mImage->AppendFrame(0, 0, mDirEntry.mWidth, mDirEntry.mHeight,
                             gfxASurface::ImageFormatARGB32, (PRUint8**)&mImageData, &imageLength);
    if (NS_FAILED(rv)) {
      PostDecoderError(rv);
      return;
    }

    // Tell the superclass we're starting a frame
    PostFrameStart();
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
    }

    // Ensure memory has been allocated before decoding. If we get this far 
    // without allocated memory, the file is most likely invalid.
    // XXXbholley - If null values can be triggered by bad input, why are we
    // asserting here?
    NS_ASSERTION(mRow, "mRow is null");
    NS_ASSERTION(mImageData, "mImageData is null");
    if (!mRow || !mImageData) {
      PostDataError();
      return;
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
            PRUint32* d = mImageData + (mCurLine * mDirEntry.mWidth);
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
                // We assume that 32bit doesn't have alpha data until we
                // find a non-zero alpha byte. If we find such a byte, 
                // it means that all previous pixels are really clear (alphabyte=0).
                // This working assumption prevents us having to premultiply afterwards.
                while (lpos > 0) {
                  if (!mHaveAlphaData && p[3]) {
                    // Non-zero alpha byte detected! Clear previous pixels from current row to end
                    memset(mImageData + mCurLine * mDirEntry.mWidth, 0, 
                           (mDirEntry.mHeight - mCurLine) * mDirEntry.mWidth * sizeof(PRUint32));
                    mHaveAlphaData = PR_TRUE;
                  }                        
                  SetPixel(d, p[2], p[1], p[0], mHaveAlphaData ? p[3] : 0xFF);
                  p += 4;
                  --lpos;
                }
                break;
              default:
                // This is probably the wrong place to check this...
                PostDataError();
                return;
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
      mRow = (PRUint8*)realloc(mRow, rowSize);
      if (!mRow) {
        PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }

    // Ensure memory has been allocated before decoding.
    NS_ASSERTION(mRow, "mRow is null");
    NS_ASSERTION(mImageData, "mImageData is null");
    if (!mRow || !mImageData) {
      PostDataError();
      return;
    }

    while (mCurLine > 0 && aCount > 0) {
      PRUint32 toCopy = PR_MIN(rowSize - mRowBytes, aCount);
      if (toCopy) {
        memcpy(mRow + mRowBytes, aBuffer, toCopy);
        aCount -= toCopy;
        aBuffer += toCopy;
        mRowBytes += toCopy;
      }
      if (rowSize == mRowBytes) {
        mCurLine--;
        mRowBytes = 0;

        PRUint32* decoded = mImageData + mCurLine * mDirEntry.mWidth;
        PRUint32* decoded_end = decoded + mDirEntry.mWidth;
        PRUint8* p = mRow, *p_end = mRow + rowSize; 
        while (p < p_end) {
          PRUint8 idx = *p++;
          for (PRUint8 bit = 0x80; bit && decoded<decoded_end; bit >>= 1) {
            // Clear pixel completely for transparency.
            if (idx & bit) *decoded = 0;
            decoded ++;
          }
        }
      }
    }
  }

  return;
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

} // namespace imagelib
} // namespace mozilla
