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
 * The Original Code is the Mozilla BMP Decoder.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Rashbrook <neil@parkwaycc.co.uk>
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
/* I got the format description from http://www.daubnet.com/formats/BMP.html */

/* This is a Cross-Platform BMP Decoder, which should work everywhere, including
 * Big-Endian machines like the PowerPC. */

#include <stdlib.h>

#include "nsBMPDecoder.h"

#include "nsIInputStream.h"
#include "RasterImage.h"
#include "imgIContainerObserver.h"

#include "prlog.h"

namespace mozilla {
namespace imagelib {

#ifdef PR_LOGGING
PRLogModuleInfo *gBMPLog = PR_NewLogModule("BMPDecoder");
#endif

// Convert from row (1..height) to absolute line (0..height-1)
#define LINE(row) ((mBIH.height < 0) ? (-mBIH.height - (row)) : ((row) - 1))
#define PIXEL_OFFSET(row, col) (LINE(row) * mBIH.width + col)

nsBMPDecoder::nsBMPDecoder()
{
    mColors = nsnull;
    mRow = nsnull;
    mCurPos = mPos = mNumColors = mRowBytes = 0;
    mOldLine = mCurLine = 1; // Otherwise decoder will never start
    mState = eRLEStateInitial;
    mStateData = 0;
    mLOH = WIN_HEADER_LENGTH;
}

nsBMPDecoder::~nsBMPDecoder()
{
  delete[] mColors;
  if (mRow)
      free(mRow);
}

nsresult
nsBMPDecoder::InitInternal()
{
    PR_LOG(gBMPLog, PR_LOG_DEBUG, ("nsBMPDecoder::Init(%p)\n", mImage.get()));

    // Fire OnStartDecode at init time to support bug 512435
    if (!IsSizeDecode() && mObserver)
        mObserver->OnStartDecode(nsnull);

    return NS_OK;
}

nsresult
nsBMPDecoder::FinishInternal()
{
    // We should never make multiple frames
    NS_ABORT_IF_FALSE(GetFrameCount() <= 1, "Multiple BMP frames?");

    // Send notifications if appropriate
    if (!IsSizeDecode() && !IsError() && (GetFrameCount() == 1)) {
        PostFrameStop();
        mImage->DecodingComplete();
        if (mObserver) {
            mObserver->OnStopContainer(nsnull, mImage);
            mObserver->OnStopDecode(nsnull, NS_OK, nsnull);
        }
    }
    return NS_OK;
}

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

static void calcBitmask(PRUint32 aMask, PRUint8& aBegin, PRUint8& aLength)
{
    // find the rightmost 1
    PRUint8 pos;
    PRBool started = PR_FALSE;
    aBegin = aLength = 0;
    for (pos = 0; pos <= 31; pos++) {
        if (!started && (aMask & (1 << pos))) {
            aBegin = pos;
            started = PR_TRUE;
        }
        else if (started && !(aMask & (1 << pos))) {
            aLength = pos - aBegin;
            break;
        }
    }
}

NS_METHOD nsBMPDecoder::CalcBitShift()
{
    PRUint8 begin, length;
    // red
    calcBitmask(mBitFields.red, begin, length);
    mBitFields.redRightShift = begin;
    mBitFields.redLeftShift = 8 - length;
    // green
    calcBitmask(mBitFields.green, begin, length);
    mBitFields.greenRightShift = begin;
    mBitFields.greenLeftShift = 8 - length;
    // blue
    calcBitmask(mBitFields.blue, begin, length);
    mBitFields.blueRightShift = begin;
    mBitFields.blueLeftShift = 8 - length;
    return NS_OK;
}

nsresult
nsBMPDecoder::WriteInternal(const char* aBuffer, PRUint32 aCount)
{
    // No forgiveness
    if (IsError())
      return NS_ERROR_FAILURE;

    // aCount=0 means EOF, mCurLine=0 means we're past end of image
    if (!aCount || !mCurLine)
        return NS_OK;

    nsresult rv;
    if (mPos < BFH_LENGTH) { /* In BITMAPFILEHEADER */
        PRUint32 toCopy = BFH_LENGTH - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + mPos, aBuffer, toCopy);
        mPos += toCopy;
        aCount -= toCopy;
        aBuffer += toCopy;
    }
    if (mPos == BFH_LENGTH) {
        ProcessFileHeader();
        if (mBFH.signature[0] != 'B' || mBFH.signature[1] != 'M') {
            PostDataError();
            return NS_ERROR_FAILURE;
        }
        if (mBFH.bihsize == OS2_BIH_LENGTH)
            mLOH = OS2_HEADER_LENGTH;
    }
    if (mPos >= BFH_LENGTH && mPos < mLOH) { /* In BITMAPINFOHEADER */
        PRUint32 toCopy = mLOH - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + (mPos - BFH_LENGTH), aBuffer, toCopy);
        mPos += toCopy;
        aCount -= toCopy;
        aBuffer += toCopy;
    }
    if (mPos == mLOH) {
        ProcessInfoHeader();
        PR_LOG(gBMPLog, PR_LOG_DEBUG, ("BMP image is %lix%lix%lu. compression=%lu\n",
            mBIH.width, mBIH.height, mBIH.bpp, mBIH.compression));
        // Verify we support this bit depth
        if (mBIH.bpp != 1 && mBIH.bpp != 4 && mBIH.bpp != 8 &&
            mBIH.bpp != 16 && mBIH.bpp != 24 && mBIH.bpp != 32) {
          PostDataError();
          return NS_ERROR_UNEXPECTED;
        }

        // BMPs with negative width are invalid
        // Reject extremely wide images to keep the math sane
        const PRInt32 k64KWidth = 0x0000FFFF;
        if (mBIH.width < 0 || mBIH.width > k64KWidth) {
            PostDataError();
            return NS_ERROR_FAILURE;
        }

        PRUint32 real_height = (mBIH.height > 0) ? mBIH.height : -mBIH.height;

        // Post our size to the superclass
        PostSize(mBIH.width, real_height);

        // We have the size. If we're doing a size decode, we got what
        // we came for.
        if (IsSizeDecode())
            return NS_OK;

        // We're doing a real decode.
        mOldLine = mCurLine = real_height;

        if (mBIH.bpp <= 8) {
            mNumColors = 1 << mBIH.bpp;
            if (mBIH.colors && mBIH.colors < mNumColors)
                mNumColors = mBIH.colors;

            // Always allocate 256 even though mNumColors might be smaller
            mColors = new colorTable[256];
            if (!mColors) {
                PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
                return NS_ERROR_OUT_OF_MEMORY;
            }

            memset(mColors, 0, 256 * sizeof(colorTable));
        }
        else if (mBIH.compression != BI_BITFIELDS && mBIH.bpp == 16) {
            // Use default 5-5-5 format
            mBitFields.red   = 0x7C00;
            mBitFields.green = 0x03E0;
            mBitFields.blue  = 0x001F;
            CalcBitShift();
        }

        PRUint32 imageLength;
        if ((mBIH.compression == BI_RLE8) || (mBIH.compression == BI_RLE4)) {
            rv = mImage->AppendFrame(0, 0, mBIH.width, real_height, gfxASurface::ImageFormatARGB32,
                                     (PRUint8**)&mImageData, &imageLength);
        } else {
            // mRow is not used for RLE encoded images
            mRow = (PRUint8*)malloc((mBIH.width * mBIH.bpp)/8 + 4);
            // +4 because the line is padded to a 4 bit boundary, but I don't want
            // to make exact calculations here, that's unnecessary.
            // Also, it compensates rounding error.
            if (!mRow) {
                PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
                return NS_ERROR_OUT_OF_MEMORY;
            }
            rv = mImage->AppendFrame(0, 0, mBIH.width, real_height, gfxASurface::ImageFormatRGB24,
                                     (PRUint8**)&mImageData, &imageLength);
        }
        NS_ENSURE_SUCCESS(rv, rv);
        if (!mImageData) {
            PostDecoderError(NS_ERROR_FAILURE);
            return NS_ERROR_FAILURE;
        }

        // Prepare for transparancy
        if ((mBIH.compression == BI_RLE8) || (mBIH.compression == BI_RLE4)) {
            if (((mBIH.compression == BI_RLE8) && (mBIH.bpp != 8)) 
             || ((mBIH.compression == BI_RLE4) && (mBIH.bpp != 4) && (mBIH.bpp != 1))) {
                PR_LOG(gBMPLog, PR_LOG_DEBUG, ("BMP RLE8/RLE4 compression only supports 8/4 bits per pixel\n"));
                PostDataError();
                return NS_ERROR_FAILURE;
            }
            // Clear the image, as the RLE may jump over areas
            memset(mImageData, 0, imageLength);
        }

        // Tell the superclass we're starting a frame
        PostFrameStart();
    }
    PRUint8 bpc; // bytes per color
    bpc = (mBFH.bihsize == OS2_BIH_LENGTH) ? 3 : 4; // OS/2 Bitmaps have no padding byte
    if (mColors && (mPos >= mLOH && (mPos < (mLOH + mNumColors * bpc)))) {
        // We will receive (mNumColors * bpc) bytes of color data
        PRUint32 colorBytes = mPos - mLOH; // Number of bytes already received
        PRUint8 colorNum = colorBytes / bpc; // Color which is currently received
        PRUint8 at = colorBytes % bpc;
        while (aCount && (mPos < (mLOH + mNumColors * bpc))) {
            switch (at) {
                case 0:
                    mColors[colorNum].blue = *aBuffer;
                    break;
                case 1:
                    mColors[colorNum].green = *aBuffer;
                    break;
                case 2:
                    mColors[colorNum].red = *aBuffer;
                    colorNum++;
                    break;
                case 3:
                    // This is a padding byte
                    break;
            }
            mPos++; aBuffer++; aCount--;
            at = (at + 1) % bpc;
        }
    }
    else if (aCount && mBIH.compression == BI_BITFIELDS && mPos < (WIN_HEADER_LENGTH + BITFIELD_LENGTH)) {
        // If compression is used, this is a windows bitmap, hence we can
        // use WIN_HEADER_LENGTH instead of mLOH
        PRUint32 toCopy = (WIN_HEADER_LENGTH + BITFIELD_LENGTH) - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + (mPos - WIN_HEADER_LENGTH), aBuffer, toCopy);
        mPos += toCopy;
        aBuffer += toCopy;
        aCount -= toCopy;
    }
    if (mBIH.compression == BI_BITFIELDS && mPos == WIN_HEADER_LENGTH + BITFIELD_LENGTH) {
        mBitFields.red = LITTLE_TO_NATIVE32(*(PRUint32*)mRawBuf);
        mBitFields.green = LITTLE_TO_NATIVE32(*(PRUint32*)(mRawBuf + 4));
        mBitFields.blue = LITTLE_TO_NATIVE32(*(PRUint32*)(mRawBuf + 8));
        CalcBitShift();
    }
    while (aCount && (mPos < mBFH.dataoffset)) { // Skip whatever is between header and data
        mPos++; aBuffer++; aCount--;
    }
    if (aCount && ++mPos >= mBFH.dataoffset) {
        // Need to increment mPos, else we might get to mPos==mLOH again
        // From now on, mPos is irrelevant
        if (!mBIH.compression || mBIH.compression == BI_BITFIELDS) {
            PRUint32 rowSize = (mBIH.bpp * mBIH.width + 7) / 8; // +7 to round up
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
                    // Collected a whole row into mRow, process it
                    PRUint8* p = mRow;
                    PRUint32* d = mImageData + PIXEL_OFFSET(mCurLine, 0);
                    PRUint32 lpos = mBIH.width;
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
                          PRUint16 val = LITTLE_TO_NATIVE16(*(PRUint16*)p);
                          SetPixel(d,
                                  (val & mBitFields.red) >> mBitFields.redRightShift << mBitFields.redLeftShift,
                                  (val & mBitFields.green) >> mBitFields.greenRightShift << mBitFields.greenLeftShift,
                                  (val & mBitFields.blue) >> mBitFields.blueRightShift << mBitFields.blueLeftShift);
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
                        NS_NOTREACHED("Unsupported color depth, but earlier check didn't catch it");
                    }
                    mCurLine --;
                    if (mCurLine == 0) { // Finished last line
                        break;
                    }
                    mRowBytes = 0;

                }
            } while (aCount > 0);
        } 
        else if ((mBIH.compression == BI_RLE8) || (mBIH.compression == BI_RLE4)) {
            if (((mBIH.compression == BI_RLE8) && (mBIH.bpp != 8)) 
             || ((mBIH.compression == BI_RLE4) && (mBIH.bpp != 4) && (mBIH.bpp != 1))) {
                PR_LOG(gBMPLog, PR_LOG_DEBUG, ("BMP RLE8/RLE4 compression only supports 8/4 bits per pixel\n"));
                PostDataError();
                return NS_ERROR_FAILURE;
            }

            while (aCount > 0) {
                PRUint8 byte;

                switch(mState) {
                    case eRLEStateInitial:
                        mStateData = (PRUint8)*aBuffer++;
                        aCount--;

                        mState = eRLEStateNeedSecondEscapeByte;
                        continue;

                    case eRLEStateNeedSecondEscapeByte:
                        byte = *aBuffer++;
                        aCount--;
                        if (mStateData != RLE_ESCAPE) { // encoded mode
                            // Encoded mode consists of two bytes: 
                            // the first byte (mStateData) specifies the
                            // number of consecutive pixels to be drawn 
                            // using the color index contained in
                            // the second byte
                            // Work around bitmaps that specify too many pixels
                            mState = eRLEStateInitial;
                            PRUint32 pixelsNeeded = PR_MIN((PRUint32)(mBIH.width - mCurPos), mStateData);
                            if (pixelsNeeded) {
                                PRUint32* d = mImageData + PIXEL_OFFSET(mCurLine, mCurPos);
                                mCurPos += pixelsNeeded;
                                if (mBIH.compression == BI_RLE8) {
                                    do {
                                        SetPixel(d, byte, mColors);
                                        pixelsNeeded --;
                                    } while (pixelsNeeded);
                                } else {
                                    do {
                                        Set4BitPixel(d, byte, pixelsNeeded, mColors);
                                    } while (pixelsNeeded);
                                }
                            }
                            continue;
                        }

                        switch(byte) {
                            case RLE_ESCAPE_EOL:
                                // End of Line: Go to next row
                                mCurLine --;
                                mCurPos = 0;
                                mState = eRLEStateInitial;
                                break;

                            case RLE_ESCAPE_EOF: // EndOfFile
                                mCurPos = mCurLine = 0;
                                break;

                            case RLE_ESCAPE_DELTA:
                                mState = eRLEStateNeedXDelta;
                                continue;

                            default : // absolute mode
                                // Save the number of pixels to read
                                mStateData = byte;
                                if (mCurPos + mStateData > (PRUint32)mBIH.width) {
                                    // We can work around bitmaps that specify one
                                    // pixel too many, but only if their width is odd.
                                    mStateData -= mBIH.width & 1;
                                    if (mCurPos + mStateData > (PRUint32)mBIH.width) {
                                        PostDataError();
                                        return NS_ERROR_FAILURE;
                                    }
                                }

                                // See if we will need to skip a byte
                                // to word align the pixel data
                                // mStateData is a number of pixels
                                // so allow for the RLE compression type
                                // Pixels RLE8=1 RLE4=2
                                //    1    Pad    Pad
                                //    2    No     Pad
                                //    3    Pad    No
                                //    4    No     No
                                if (((mStateData - 1) & mBIH.compression) != 0)
                                    mState = eRLEStateAbsoluteMode;
                                else
                                    mState = eRLEStateAbsoluteModePadded;
                                continue;
                        }
                        break;

                    case eRLEStateNeedXDelta:
                        // Handle the XDelta and proceed to get Y Delta
                        byte = *aBuffer++;
                        aCount--;
                        mCurPos += byte;
                        if (mCurPos > mBIH.width)
                            mCurPos = mBIH.width;

                        mState = eRLEStateNeedYDelta;
                        continue;

                    case eRLEStateNeedYDelta:
                        // Get the Y Delta and then "handle" the move
                        byte = *aBuffer++;
                        aCount--;
                        mState = eRLEStateInitial;
                        mCurLine -= PR_MIN(byte, mCurLine);
                        break;

                    case eRLEStateAbsoluteMode: // Absolute Mode
                    case eRLEStateAbsoluteModePadded:
                        if (mStateData) {
                            // In absolute mode, the second byte (mStateData)
                            // represents the number of pixels 
                            // that follow, each of which contains 
                            // the color index of a single pixel.
                            PRUint32* d = mImageData + PIXEL_OFFSET(mCurLine, mCurPos);
                            PRUint32* oldPos = d;
                            if (mBIH.compression == BI_RLE8) {
                                while (aCount > 0 && mStateData > 0) {
                                    byte = *aBuffer++;
                                    aCount--;
                                    SetPixel(d, byte, mColors);
                                    mStateData--;
                                }
                            } else {
                                while (aCount > 0 && mStateData > 0) {
                                    byte = *aBuffer++;
                                    aCount--;
                                    Set4BitPixel(d, byte, mStateData, mColors);
                                }
                            }
                            mCurPos += d - oldPos;
                        }

                        if (mStateData == 0) {
                            // In absolute mode, each run must 
                            // be aligned on a word boundary

                            if (mState == eRLEStateAbsoluteMode) { // Word Aligned
                                mState = eRLEStateInitial;
                            } else if (aCount > 0) {               // Not word Aligned
                                // "next" byte is just a padding byte
                                // so "move" past it and we can continue
                                aBuffer++;
                                aCount--;
                                mState = eRLEStateInitial;
                            }
                        }
                        // else state is still eRLEStateAbsoluteMode
                        continue;

                    default :
                        NS_ABORT_IF_FALSE(0, "BMP RLE decompression: unknown state!");
                        PostDecoderError(NS_ERROR_UNEXPECTED);
                        return NS_ERROR_FAILURE;
                }
                // Because of the use of the continue statement
                // we only get here for eol, eof or y delta
                if (mCurLine == 0) { // Finished last line
                    break;
                }
            }
        }
    }
    
    const PRUint32 rows = mOldLine - mCurLine;
    if (rows) {

        // Invalidate
        nsIntRect r(0, mBIH.height < 0 ? -mBIH.height - mOldLine : mCurLine,
                    mBIH.width, rows);
        PostInvalidation(r);

        mOldLine = mCurLine;
    }

    return NS_OK;
}

void nsBMPDecoder::ProcessFileHeader()
{
    memset(&mBFH, 0, sizeof(mBFH));
    memcpy(&mBFH.signature, mRawBuf, sizeof(mBFH.signature));
    memcpy(&mBFH.filesize, mRawBuf + 2, sizeof(mBFH.filesize));
    memcpy(&mBFH.reserved, mRawBuf + 6, sizeof(mBFH.reserved));
    memcpy(&mBFH.dataoffset, mRawBuf + 10, sizeof(mBFH.dataoffset));
    memcpy(&mBFH.bihsize, mRawBuf + 14, sizeof(mBFH.bihsize));

    // Now correct the endianness of the header
    mBFH.filesize = LITTLE_TO_NATIVE32(mBFH.filesize);
    mBFH.dataoffset = LITTLE_TO_NATIVE32(mBFH.dataoffset);
    mBFH.bihsize = LITTLE_TO_NATIVE32(mBFH.bihsize);
}

void nsBMPDecoder::ProcessInfoHeader()
{
    memset(&mBIH, 0, sizeof(mBIH));
    if (mBFH.bihsize == 12) { // OS/2 Bitmap
        memcpy(&mBIH.width, mRawBuf, 2);
        memcpy(&mBIH.height, mRawBuf + 2, 2);
        memcpy(&mBIH.planes, mRawBuf + 4, sizeof(mBIH.planes));
        memcpy(&mBIH.bpp, mRawBuf + 6, sizeof(mBIH.bpp));
    }
    else {
        memcpy(&mBIH.width, mRawBuf, sizeof(mBIH.width));
        memcpy(&mBIH.height, mRawBuf + 4, sizeof(mBIH.height));
        memcpy(&mBIH.planes, mRawBuf + 8, sizeof(mBIH.planes));
        memcpy(&mBIH.bpp, mRawBuf + 10, sizeof(mBIH.bpp));
        memcpy(&mBIH.compression, mRawBuf + 12, sizeof(mBIH.compression));
        memcpy(&mBIH.image_size, mRawBuf + 16, sizeof(mBIH.image_size));
        memcpy(&mBIH.xppm, mRawBuf + 20, sizeof(mBIH.xppm));
        memcpy(&mBIH.yppm, mRawBuf + 24, sizeof(mBIH.yppm));
        memcpy(&mBIH.colors, mRawBuf + 28, sizeof(mBIH.colors));
        memcpy(&mBIH.important_colors, mRawBuf + 32, sizeof(mBIH.important_colors));
    }

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
