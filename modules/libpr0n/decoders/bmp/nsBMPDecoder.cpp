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
 * The Original Code is the Mozilla BMP Decoder.
 * 
 * The Initial Developer of the Original Code is Christian Biesinger
 * <cbiesinger@web.de>.  Portions created by Christian Biesinger are
 * Copyright (C) 2001 Christian Biesinger.  All
 * Rights Reserved.
 * 
 * Contributor(s):
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
/* I got the format description from http://www.daubnet.com/formats/BMP.html */

/* This does not yet work in this version:
 * o) Compressed Bitmaps
 * This decoder was tested on Windows, Linux and Mac. */

/* This is a Cross-Platform BMP Decoder, which should work everywhere, including
 * Big-Endian machines like the PowerPC. */

#include "nsBMPDecoder.h"

#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsIImage.h"
#include "nsMemory.h"
#include "imgIContainerObserver.h"
#include "nsRect.h"

#include "imgILoad.h"

#include "ImageLogging.h"

PRLogModuleInfo *gBMPLog = PR_NewLogModule("BMPDecoder");

NS_IMPL_ISUPPORTS1(nsBMPDecoder, imgIDecoder)

nsBMPDecoder::nsBMPDecoder()
{
    NS_INIT_ISUPPORTS();
    mColors = nsnull;
    mRow = nsnull;
    mPos = mNumColors = mRowBytes = mCurLine = 0;
}

nsBMPDecoder::~nsBMPDecoder()
{
}

NS_IMETHODIMP nsBMPDecoder::Init(imgILoad *aLoad)
{
    PR_LOG(gBMPLog, PR_LOG_DEBUG, ("nsBMPDecoder::Init(%p)\n", aLoad));
    mObserver = do_QueryInterface(aLoad);

    mImage = do_CreateInstance("@mozilla.org/image/container;1");
    if (!mImage)
        return NS_ERROR_OUT_OF_MEMORY;

    mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
    if (!mFrame)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = aLoad->SetImage(mImage);
    NS_ENSURE_SUCCESS(rv, rv);

    mLOH = 54;

    return NS_OK;
}

NS_IMETHODIMP nsBMPDecoder::Close()
{
    PR_LOG(gBMPLog, PR_LOG_DEBUG, ("nsBMPDecoder::Close()\n"));
    mPos = 0;
    delete[] mColors;
    mColors = nsnull;
    mNumColors = 0;
    delete[] mRow;
    mRow = nsnull;
    mRowBytes = 0;
    mCurLine = 0;

    if (mObserver) {
        mObserver->OnStopContainer(nsnull, nsnull, mImage);
        mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
        mObserver = nsnull;
    }
    mImage = nsnull;
    mFrame = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsBMPDecoder::Flush()
{
    mFrame->SetMutable(PR_FALSE);
    return NS_OK;
}

NS_METHOD nsBMPDecoder::ReadSegCb(nsIInputStream* aIn, void* aClosure,
                             const char* aFromRawSegment, PRUint32 aToOffset,
                             PRUint32 aCount, PRUint32 *aWriteCount) {
    nsBMPDecoder *decoder = NS_REINTERPRET_CAST(nsBMPDecoder*, aClosure);
    *aWriteCount = aCount;
    return decoder->ProcessData(aFromRawSegment, aCount);
}

NS_IMETHODIMP nsBMPDecoder::WriteFrom(nsIInputStream *aInStr, PRUint32 aCount, PRUint32 *aRetval)
{
    PR_LOG(gBMPLog, PR_LOG_DEBUG, ("nsBMPDecoder::WriteFrom(%p, %lu, %p)\n", aInStr, aCount, aRetval));

    return aInStr->ReadSegments(ReadSegCb, this, aCount, aRetval);
}

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

nsresult nsBMPDecoder::SetData(PRUint8* aData)
{
    PRUint32 bpr;
    nsresult rv;

    rv = mFrame->GetImageBytesPerRow(&bpr);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 line = (mBIH.height < 0) ? (-mBIH.height - mCurLine - 1) : mCurLine;
    rv = mFrame->SetImageData(aData, bpr, line * bpr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRect r(0, line, mBIH.width, 1);
    rv = mObserver->OnDataAvailable(nsnull, nsnull, mFrame, &r);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

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

NS_METHOD nsBMPDecoder::ProcessData(const char* aBuffer, PRUint32 aCount)
{
    if (!aCount || (mCurLine < 0)) // aCount=0 means EOF, mCurLine < 0 means we're past end of image
        return NS_OK;

    nsresult rv;
    if (mPos < 18) { /* In BITMAPFILEHEADER */
        PRUint32 toCopy = 18 - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + mPos, aBuffer, toCopy);
        mPos += toCopy;
        aCount -= toCopy;
        aBuffer += toCopy;
    }
    if (mPos == 18) {
        rv = mObserver->OnStartDecode(nsnull, nsnull);
        NS_ENSURE_SUCCESS(rv, rv);
        ProcessFileHeader();
        if (mBFH.signature[0] != 'B' || mBFH.signature[1] != 'M')
            return NS_ERROR_FAILURE;
        if (mBFH.bihsize == 12)
            mLOH = 26;
    }
    if (mPos >= 18 && mPos < mLOH) { /* In BITMAPINFOHEADER */
        PRUint32 toCopy = mLOH - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + (mPos - 18), aBuffer, toCopy);
        mPos += toCopy;
        aCount -= toCopy;
        aBuffer += toCopy;
    }
    if (mPos == mLOH) {
        ProcessInfoHeader();
        PR_LOG(gBMPLog, PR_LOG_DEBUG, ("BMP image is %lix%lix%lu. compression=%lu\n",
            mBIH.width, mBIH.height, mBIH.bpp, mBIH.compression));
        if (mBIH.compression && mBIH.compression != BI_BITFIELDS) {
            PR_LOG(gBMPLog, PR_LOG_DEBUG, ("Don't yet support compressed BMPs\n"));
            return NS_ERROR_FAILURE;
        }

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
            if (mBIH.colors)
                mNumColors = mBIH.colors;

            mColors = new colorTable[mNumColors];
            if (!mColors)
                return NS_ERROR_OUT_OF_MEMORY;
        }
        else if (mBIH.compression != BI_BITFIELDS && mBIH.bpp == 16) {
            // Use default 5-5-5 format
            mBitFields.red   = 0x7C00;
            mBitFields.green = 0x03E0;
            mBitFields.blue  = 0x001F;
            CalcBitShift();
        }
        // BMPs with negative width are invalid
        if (mBIH.width < 0)
            return NS_ERROR_FAILURE;

        PRUint32 real_height = (mBIH.height > 0) ? mBIH.height : -mBIH.height;
        rv = mImage->Init(mBIH.width, real_height, mObserver);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = mObserver->OnStartContainer(nsnull, nsnull, mImage);
        NS_ENSURE_SUCCESS(rv, rv);
        mCurLine = real_height - 1;

        mRow = new PRUint8[(mBIH.width * mBIH.bpp)/8 + 4];
        // +4 because the line is padded to a 4 bit boundary, but I don't want
        // to make exact calculations here, that's unnecessary.
        // Also, it compensates rounding error.
        if (!mRow) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        rv = mFrame->Init(0, 0, mBIH.width, real_height, BMP_GFXFORMAT, 24);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = mImage->AppendFrame(mFrame);
        NS_ENSURE_SUCCESS(rv, rv);
        mObserver->OnStartFrame(nsnull, nsnull, mFrame);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    PRUint8 bpc; // bytes per color
    bpc = (mBFH.bihsize == 12) ? 3 : 4; // OS/2 Bitmaps have no padding byte
    if (mColors && (mPos >= mLOH && (mPos < (mLOH + mNumColors * bpc)))) {
        // We will receive (mNumColors * 4) bytes of color data
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
    else if (mBIH.compression == BI_BITFIELDS && mPos < (mLOH + 12)) {
        PRUint32 toCopy = (mLOH + 12) - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + (mPos - mLOH), aBuffer, toCopy);
        mPos += toCopy;
        aBuffer += toCopy;
        aCount -= toCopy;
    }
    if (mBIH.compression == BI_BITFIELDS && mPos == mLOH+12) {
        mBitFields.red = LITTLE_TO_NATIVE32(*(PRUint32*)mRawBuf);
        mBitFields.green = LITTLE_TO_NATIVE32(*(PRUint32*)(mRawBuf + 4));
        mBitFields.blue = LITTLE_TO_NATIVE32(*(PRUint32*)(mRawBuf + 8));
        CalcBitShift();
    }
    while (aCount && (mPos < mBFH.dataoffset)) { // Skip whatever is between header and data
        mPos++; aBuffer++; aCount--;
    }
    if (++mPos >= mBFH.dataoffset) {
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
                if ((rowSize - mRowBytes) == 0) {
                    PRUint32 bpr;
                    rv = mFrame->GetImageBytesPerRow(&bpr);
                    NS_ENSURE_SUCCESS(rv, rv);
                    PRUint8* decoded = new PRUint8[bpr];
                    if (!decoded)
                        return NS_ERROR_OUT_OF_MEMORY;

                    PRUint8* p = mRow;
                    PRUint8* d = decoded;
                    PRUint32 lpos = 0;
                    switch (mBIH.bpp) {
                      case 1:
                        while (lpos < mBIH.width) {
                          PRInt8 bit;
                          PRUint8 idx;
                          for (bit = 7; bit >= 0; bit--) {
                              if (lpos >= mBIH.width)
                                  break;
                              idx = (*p >> bit) & 1;
                              SetPixel(d, idx, mColors);
                              ++lpos;
                          }
                          ++p;
                        }
                        break;
                      case 4:
                        while (lpos < mBIH.width) {
                          Set4BitPixel(d, *p, lpos, mBIH.width, mColors);
                          ++p;
                        }
                        break;
                      case 8:
                        while (lpos < mBIH.width) {
                          SetPixel(d, *p, mColors);
                          ++lpos;
                          ++p;
                        }
                        break;
                      case 16:
                        while (lpos < mBIH.width) {
                          PRUint16 val = LITTLE_TO_NATIVE16(*(PRUint16*)p);
                          SetPixel(d,
                                  (val & mBitFields.red) >> mBitFields.redRightShift << mBitFields.redLeftShift,
                                  (val & mBitFields.green) >> mBitFields.greenRightShift << mBitFields.greenLeftShift,
                                  (val & mBitFields.blue) >> mBitFields.blueRightShift << mBitFields.blueLeftShift);
                          ++lpos;
                          p+=2;
                        }
                        break;
                      case 32:
                      case 24:
                        while (lpos < mBIH.width) {
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
                      
                    nsresult rv = SetData(decoded);
                    delete[] decoded;
                    NS_ENSURE_SUCCESS(rv, rv);

                    if (mCurLine == 0) { // Finished last line
                        return mObserver->OnStopFrame(nsnull, nsnull, mFrame);
                    }
                    mCurLine--; mRowBytes = 0;

                }
            } while (aCount > 0);
        }
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
