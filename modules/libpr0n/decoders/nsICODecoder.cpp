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
 *   Brian R. Bondy <netzen@gmail.com>
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

#include "Endian.h"
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

PRUint32
nsICODecoder::CalcAlphaRowSize() 
{
  // Calculate rowsize in DWORD's and then return in # of bytes
  PRUint32 rowSize = (mDirEntry.mWidth + 31) / 32; // +31 to round up
  return rowSize * 4; // Return rowSize in bytes
}

// Obtains the number of colors from the bits per pixel
PRUint16
nsICODecoder::GetNumColors() 
{
  PRUint16 numColors = 0;
  if (mBPP <= 8) {
    switch (mBPP) {
    case 1:
      numColors = 2;
      break;
    case 4:
      numColors = 16;
      break;
    case 8:
      numColors = 256;
      break;
    default:
      numColors = (PRUint16)-1;
    }
  }
  return numColors;
}


nsICODecoder::nsICODecoder()
{
  mPos = mImageOffset = mCurrIcon = mNumIcons = mBPP = mRowBytes = 0;
  mIsPNG = PR_FALSE;
  mRow = nsnull;
  mOldLine = mCurLine = 1; // Otherwise decoder will never start
}

nsICODecoder::~nsICODecoder()
{
  if (mRow) {
    moz_free(mRow);
  }
}

void
nsICODecoder::FinishInternal()
{
  // We shouldn't be called in error cases
  NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call FinishInternal after error!");

  // Finish the internally used decoder as well
  if (mContainedDecoder) {
    mContainedDecoder->FinishSharedDecoder();
    mDecodeDone = mContainedDecoder->GetDecodeDone();
  }
}

// Returns a buffer filled with the bitmap file header in little endian:
// Signature 2 bytes 'BM'
// FileSize	 4 bytes File size in bytes
// reserved	 4 bytes unused (=0)
// DataOffset	 4 bytes File offset to Raster Data
// Returns PR_TRUE if successful
PRBool nsICODecoder::FillBitmapFileHeaderBuffer(PRInt8 *bfh) 
{
  memset(bfh, 0, 14);
  bfh[0] = 'B';
  bfh[1] = 'M';
  PRInt32 dataOffset = 0;
  PRInt32 fileSize = 0;
  dataOffset = BFH_LENGTH + BITMAPINFOSIZE;

  // The color table is present only if BPP is <= 8
  if (mDirEntry.mBitCount <= 8) {
    PRUint16 numColors = GetNumColors();
    if (numColors == (PRUint16)-1) {
      return PR_FALSE;
    }
    dataOffset += 4 * numColors;
    fileSize = dataOffset + mDirEntry.mWidth * mDirEntry.mHeight;
  } else {
    fileSize = dataOffset + (mDirEntry.mBitCount * mDirEntry.mWidth * 
                             mDirEntry.mHeight) / 8;
  }

  fileSize = NATIVE32_TO_LITTLE(fileSize);
  memcpy(bfh + 2, &fileSize, sizeof(fileSize));
  dataOffset = NATIVE32_TO_LITTLE(dataOffset);
  memcpy(bfh + 10, &dataOffset, sizeof(dataOffset));
  return PR_TRUE;
}

// A BMP inside of an ICO has *2 height because of the AND mask
// that follows the actual bitmap.  The BMP shouldn't know about
// this difference though.
void 
nsICODecoder::FillBitmapInformationBufferHeight(PRInt8 *bih) 
{
  PRInt32 height = mDirEntry.mHeight;
  height = NATIVE32_TO_LITTLE(height);
  memcpy(bih + 8, &height, sizeof(height));
}

// The BMP information header's bits per pixel should be trusted
// more than what we have.  Usually the ICO's BPP is set to 0
PRInt32 
nsICODecoder::ExtractBPPFromBitmap(PRInt8 *bih)
{
  PRInt32 bitsPerPixel;
  memcpy(&bitsPerPixel, bih + 14, sizeof(bitsPerPixel));
  bitsPerPixel = LITTLE_TO_NATIVE32(bitsPerPixel);
  return bitsPerPixel;
}

PRInt32 
nsICODecoder::ExtractBIHSizeFromBitmap(PRInt8 *bih)
{
  PRInt32 headerSize;
  memcpy(&headerSize, bih, sizeof(headerSize));
  headerSize = LITTLE_TO_NATIVE32(headerSize);
  return headerSize;
}

void
nsICODecoder::SetHotSpotIfCursor() {
  if (!mIsCursor) {
    return;
  }

  nsCOMPtr<nsISupportsPRUint32> intwrapx = 
    do_CreateInstance("@mozilla.org/supports-PRUint32;1");
  nsCOMPtr<nsISupportsPRUint32> intwrapy = 
    do_CreateInstance("@mozilla.org/supports-PRUint32;1");

  if (intwrapx && intwrapy) {
    intwrapx->SetData(mDirEntry.mXHotspot);
    intwrapy->SetData(mDirEntry.mYHotspot);

    mImage->Set("hotspotX", intwrapx);
    mImage->Set("hotspotY", intwrapy);
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
  // Loop through each entry's dir entry
  while (mCurrIcon < mNumIcons) { 
    if (mPos >= DIRENTRYOFFSET + (mCurrIcon * sizeof(mDirEntryArray)) && 
        mPos < DIRENTRYOFFSET + ((mCurrIcon + 1) * sizeof(mDirEntryArray))) {
      PRUint32 toCopy = sizeof(mDirEntryArray) - 
                        (mPos - DIRENTRYOFFSET - mCurrIcon * sizeof(mDirEntryArray));
      if (toCopy > aCount) {
        toCopy = aCount;
      }
      memcpy(mDirEntryArray + sizeof(mDirEntryArray) - toCopy, aBuffer, toCopy);
      mPos += toCopy;
      aCount -= toCopy;
      aBuffer += toCopy;
    }
    if (aCount == 0)
      return; // Need more data

    IconDirEntry e;
    if (mPos == (DIRENTRYOFFSET + ICODIRENTRYSIZE) + 
                (mCurrIcon * sizeof(mDirEntryArray))) {
      mCurrIcon++;
      ProcessDirEntry(e);
      if ((e.mWidth == PREFICONSIZE && e.mHeight == PREFICONSIZE && 
           e.mBitCount >= colorDepth) || 
          (mCurrIcon == mNumIcons && mImageOffset == 0)) {
        mImageOffset = e.mImageOffset;

        // ensure mImageOffset is >= size of the direntry headers (bug #245631)
        PRUint32 minImageOffset = DIRENTRYOFFSET + 
                                  mNumIcons * sizeof(mDirEntryArray);
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

  // If we are within the first PNGSIGNATURESIZE bytes of the image data,
  // then we have either a BMP or a PNG.  We use the first PNGSIGNATURESIZE
  // bytes to determine which one we have.
  if (mCurrIcon == mNumIcons && mPos >= mImageOffset && 
      mPos < mImageOffset + PNGSIGNATURESIZE)
  {
    PRUint32 toCopy = PNGSIGNATURESIZE - (mPos - mImageOffset);
    if (toCopy > aCount) {
      toCopy = aCount;
    }

    memcpy(mSignature + (mPos - mImageOffset), aBuffer, toCopy);
    mPos += toCopy;
    aCount -= toCopy;
    aBuffer += toCopy;

    mIsPNG = !memcmp(mSignature, nsPNGDecoder::pngSignatureBytes, 
                     PNGSIGNATURESIZE);
    if (mIsPNG) {
      mContainedDecoder = new nsPNGDecoder();
      mContainedDecoder->InitSharedDecoder(mImage, mObserver);
      mContainedDecoder->Write(mSignature, PNGSIGNATURESIZE);
      mDataError = mContainedDecoder->HasDataError();
      if (mContainedDecoder->HasDataError()) {
        return;
      }
    }
  }

  // If we have a PNG, let the PNG decoder do all of the rest of the work
  if (mIsPNG && mContainedDecoder && mPos >= mImageOffset + PNGSIGNATURESIZE) {
    mContainedDecoder->Write(aBuffer, aCount);
    mDataError = mContainedDecoder->HasDataError();
    if (mContainedDecoder->HasDataError()) {
      return;
    }
    mPos += aCount;
    aBuffer += aCount;
    aCount = 0;

    // Raymond Chen says that 32bpp only are valid PNG ICOs
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/10/22/10079192.aspx
    if (static_cast<nsPNGDecoder*>(mContainedDecoder.get())->HasValidInfo() && 
        static_cast<nsPNGDecoder*>(mContainedDecoder.get())->GetPixelDepth() != 32) {
      PostDataError();
    }
    return;
  }

  // We've processed all of the icon dir entries and are within the 
  // bitmap info size
  if (!mIsPNG && mCurrIcon == mNumIcons && mPos >= mImageOffset && 
      mPos >= mImageOffset + PNGSIGNATURESIZE && 
      mPos < mImageOffset + BITMAPINFOSIZE) {

    // As we were decoding, we did not know if we had a PNG signature or the
    // start of a bitmap information header.  At this point we know we had
    // a bitmap information header and not a PNG signature, so fill the bitmap
    // information header with the data it should already have.
    memcpy(mBIHraw, mSignature, PNGSIGNATURESIZE);

    // We've found the icon.
    PRUint32 toCopy = sizeof(mBIHraw) - (mPos - mImageOffset);
    if (toCopy > aCount)
      toCopy = aCount;

    memcpy(mBIHraw + (mPos - mImageOffset), aBuffer, toCopy);
    mPos += toCopy;
    aCount -= toCopy;
    aBuffer += toCopy;
  }

  // If we have a BMP inside the ICO and we have read the BIH header
  if (!mIsPNG && mPos == mImageOffset + BITMAPINFOSIZE) {

    // Make sure we have a sane value for the bitmap information header
    PRInt32 bihSize = ExtractBIHSizeFromBitmap(reinterpret_cast<PRInt8*>(mBIHraw));
    if (bihSize != BITMAPINFOSIZE) {
      PostDataError();
      return;
    }
    // We are extracting the BPP from the BIH header as it should be trusted 
    // over the one we have from the icon header
    mBPP = ExtractBPPFromBitmap(reinterpret_cast<PRInt8*>(mBIHraw));
    
    // Init the bitmap decoder which will do most of the work for us
    // It will do everything except the AND mask which isn't present in bitmaps
    // bmpDecoder is for local scope ease, it will be freed by mContainedDecoder
    nsBMPDecoder *bmpDecoder = new nsBMPDecoder(); 
    mContainedDecoder = bmpDecoder;
    bmpDecoder->SetUseAlphaData(PR_TRUE);
    mContainedDecoder->SetSizeDecode(IsSizeDecode());
    mContainedDecoder->InitSharedDecoder(mImage, mObserver);

    // The ICO format when containing a BMP does not include the 14 byte
    // bitmap file header. To use the code of the BMP decoder we need to 
    // generate this header ourselves and feed it to the BMP decoder.
    PRInt8 bfhBuffer[BMPFILEHEADERSIZE];
    if (!FillBitmapFileHeaderBuffer(bfhBuffer)) {
      PostDataError();
      return;
    }
    mContainedDecoder->Write((const char*)bfhBuffer, sizeof(bfhBuffer));
    mDataError = mContainedDecoder->HasDataError();
    if (mContainedDecoder->HasDataError()) {
      return;
    }

    // Setup the cursor hot spot if one is present
    SetHotSpotIfCursor();

    // Fix the height on the BMP resource
    FillBitmapInformationBufferHeight((PRInt8*)mBIHraw);

    // Write out the BMP's bitmap info header
    mContainedDecoder->Write(mBIHraw, sizeof(mBIHraw));
    mDataError = mContainedDecoder->HasDataError();
    if (mContainedDecoder->HasDataError()) {
      return;
    }

    // We have the size. If we're doing a size decode, we got what
    // we came for.
    if (IsSizeDecode())
      return;

    // Sometimes the ICO BPP header field is not filled out
    // so we should trust the contained resource over our own
    // information.
    mBPP = bmpDecoder->GetBitsPerPixel();

    // Check to make sure we have valid color settings
    PRUint16 numColors = GetNumColors();
    if (numColors == (PRUint16)-1) {
      PostDataError();
      return;
    }
  }

  // If we have a BMP
  if (!mIsPNG && mContainedDecoder && mPos >= mImageOffset + BITMAPINFOSIZE) {
    PRUint16 numColors = GetNumColors();
    if (numColors == (PRUint16)-1) {
      PostDataError();
      return;
    }
    // Feed the actual image data (not including headers) into the BMP decoder
    PRInt32 bmpDataOffset = mDirEntry.mImageOffset + BITMAPINFOSIZE;
    PRInt32 bmpDataEnd = mDirEntry.mImageOffset + BITMAPINFOSIZE + 
                         static_cast<nsBMPDecoder*>(mContainedDecoder.get())->GetCompressedImageSize() +
                         4 * numColors;

    // If we are feeding in the core image data, but we have not yet
    // reached the ICO's 'AND buffer mask'
    if (mPos >= bmpDataOffset && mPos < bmpDataEnd) {

      // Figure out how much data the BMP decoder wants
      PRUint32 toFeed = bmpDataEnd - mPos;
      if (toFeed > aCount) {
        toFeed = aCount;
      }

      mContainedDecoder->Write(aBuffer, toFeed);
      mDataError = mContainedDecoder->HasDataError();
      if (mContainedDecoder->HasDataError()) {
        return;
      }

      mPos += toFeed;
      aCount -= toFeed;
      aBuffer += toFeed;
    }
  
    // If the bitmap is fully processed, treat any left over data as the ICO's
    // 'AND buffer mask' which appears after the bitmap resource.
    if (!mIsPNG && mPos >= bmpDataEnd) {
      // There may be an optional AND bit mask after the data.  This is
      // only used if the alpha data is not already set. The alpha data 
      // is used for 32bpp bitmaps as per the comment in ICODecoder.h
      // The alpha mask should be checked in all other cases.
      if (static_cast<nsBMPDecoder*>(mContainedDecoder.get())->GetBitsPerPixel() != 32) {
        PRUint32 rowSize = ((mDirEntry.mWidth + 31) / 32) * 4; // + 31 to round up
        if (mPos == bmpDataEnd) {
          mPos++;
          mRowBytes = 0;
          mCurLine = mDirEntry.mHeight;
          mRow = (PRUint8*)moz_realloc(mRow, rowSize);
          if (!mRow) {
            PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
            return;
          }
        }

        // Ensure memory has been allocated before decoding.
        NS_ABORT_IF_FALSE(mRow, "mRow is null");
        NS_ABORT_IF_FALSE(mImage, "mImage is null");
        if (!mRow || !mImage) {
          PostDataError();
          return;
        }

        while (mCurLine > 0 && aCount > 0) {
          PRUint32 toCopy = NS_MIN(rowSize - mRowBytes, aCount);
          if (toCopy) {
            memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
          }
          if (rowSize == mRowBytes) {
            mCurLine--;
            mRowBytes = 0;

            PRUint32* imageData = 
              static_cast<nsBMPDecoder*>(mContainedDecoder.get())->GetImageData();
            if (!imageData) {
              PostDataError();
              return;
            }
            PRUint32* decoded = imageData + mCurLine * mDirEntry.mWidth;
            PRUint32* decoded_end = decoded + mDirEntry.mWidth;
            PRUint8* p = mRow, *p_end = mRow + rowSize; 
            while (p < p_end) {
              PRUint8 idx = *p++;
              for (PRUint8 bit = 0x80; bit && decoded<decoded_end; bit >>= 1) {
                // Clear pixel completely for transparency.
                if (idx & bit) {
                  *decoded = 0;
                }
                decoded++;
              }
            }
          }
        }
      }
    }
  }
}

void
nsICODecoder::ProcessDirEntry(IconDirEntry& aTarget)
{
  memset(&aTarget, 0, sizeof(aTarget));
  memcpy(&aTarget.mWidth, mDirEntryArray, sizeof(aTarget.mWidth));
  memcpy(&aTarget.mHeight, mDirEntryArray + 1, sizeof(aTarget.mHeight));
  memcpy(&aTarget.mColorCount, mDirEntryArray + 2, sizeof(aTarget.mColorCount));
  memcpy(&aTarget.mReserved, mDirEntryArray + 3, sizeof(aTarget.mReserved));
  memcpy(&aTarget.mPlanes, mDirEntryArray + 4, sizeof(aTarget.mPlanes));
  aTarget.mPlanes = LITTLE_TO_NATIVE16(aTarget.mPlanes);
  memcpy(&aTarget.mBitCount, mDirEntryArray + 6, sizeof(aTarget.mBitCount));
  aTarget.mBitCount = LITTLE_TO_NATIVE16(aTarget.mBitCount);
  memcpy(&aTarget.mBytesInRes, mDirEntryArray + 8, sizeof(aTarget.mBytesInRes));
  aTarget.mBytesInRes = LITTLE_TO_NATIVE32(aTarget.mBytesInRes);
  memcpy(&aTarget.mImageOffset, mDirEntryArray + 12, 
         sizeof(aTarget.mImageOffset));
  aTarget.mImageOffset = LITTLE_TO_NATIVE32(aTarget.mImageOffset);
}

} // namespace imagelib
} // namespace mozilla
