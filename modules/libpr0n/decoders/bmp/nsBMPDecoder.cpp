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
 * o) Compressed Bitmaps, including ones using bitfields
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

#include "prcpucfg.h" // To get IS_LITTLE_ENDIAN / IS_BIG_ENDIAN

#include "ImageLogging.h"

PRLogModuleInfo *gBMPLog = PR_NewLogModule("BMPDecoder");

NS_IMPL_ISUPPORTS1(nsBMPDecoder, imgIDecoder)

nsBMPDecoder::nsBMPDecoder()
{
    NS_INIT_ISUPPORTS();
    mColors = nsnull;
    mRow = nsnull;
    mPos = mNumColors = mRowBytes = mCurLine = 0;
    mLOH = 54;
}

nsBMPDecoder::~nsBMPDecoder()
{
}

NS_IMETHODIMP nsBMPDecoder::Init(imgILoad *aLoad)
{
    PR_LOG(gBMPLog, PR_LOG_DEBUG, ("nsBMPDecoder::Init(%p)\n", aLoad));
    NS_ENSURE_ARG_POINTER(aLoad);
    mObserver = do_QueryInterface(aLoad);

    mImage = do_CreateInstance("@mozilla.org/image/container;1");
    if (!mImage)
        return NS_ERROR_OUT_OF_MEMORY;

    mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
    if (!mFrame)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = aLoad->SetImage(mImage);
    NS_ENSURE_SUCCESS(rv, rv);

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
    mLOH = 54;
    return NS_OK;
}

NS_IMETHODIMP nsBMPDecoder::Flush()
{
    mFrame->SetMutable(PR_FALSE);
    return NS_OK;
}

NS_IMETHODIMP nsBMPDecoder::WriteFrom(nsIInputStream *aInStr, PRUint32 aCount, PRUint32 *aRetval)
{
    PR_LOG(gBMPLog, PR_LOG_DEBUG, ("nsBMPDecoder::WriteFrom(%p, %lu, %p)\n", aInStr, aCount, aRetval));
    NS_ENSURE_ARG_POINTER(aInStr);
    NS_ENSURE_ARG_POINTER(aRetval);

    char* buffer = new char[aCount];
    if (!buffer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    unsigned int realCount = aCount;
    nsresult rv = aInStr->Read(buffer, aCount, &realCount);
    NS_ENSURE_SUCCESS(rv, rv);
    *aRetval = realCount;

    rv = ProcessData(buffer, realCount);
    delete []buffer;
    return rv;
}

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

inline nsresult nsBMPDecoder::SetPixel(PRUint8*& aDecoded, PRUint8 idx)
{
    PRUint8 red, green, blue;
    red = mColors[idx].red;
    green = mColors[idx].green;
    blue = mColors[idx].blue;
    return SetPixel(aDecoded, red, green, blue);
}

inline nsresult nsBMPDecoder::SetPixel(PRUint8*& aDecoded, PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    *aDecoded++ = 0; // Mac needs this padding byte
#endif
#ifdef USE_RGB
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

inline nsresult nsBMPDecoder::Set4BitPixel(PRUint8*& aDecoded, PRUint8 aData, PRUint32& aPos)
{
    PRUint8 idx = aData >> 4;
    nsresult rv = SetPixel(aDecoded, idx);
    if ((++aPos >= mBIH.width) || NS_FAILED(rv))
        return rv;

    idx = aData & 0xF;
    rv = SetPixel(aDecoded, idx);
    ++aPos;
    return rv;
}

inline nsresult nsBMPDecoder::SetData(PRUint8* aData)
{
    NS_ENSURE_ARG_POINTER(aData);
    PRUint32 bpr;
    nsresult rv;

    rv = mFrame->GetImageBytesPerRow(&bpr);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 offset = (mCurLine - 1) * bpr;
    rv = mFrame->SetImageData(aData, bpr, offset);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRect r(0, mCurLine, mBIH.width, 1);
    rv = mObserver->OnDataAvailable(nsnull, nsnull, mFrame, &r);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_METHOD nsBMPDecoder::ProcessData(const char* aBuffer, PRUint32 aCount)
{
    if (!aCount) // aCount=0 means EOF
        return NS_OK;

    nsresult rv;
    if (mPos < 18) { /* In BITMAPFILEHEADER */
        PRUint32 toCopy = 18 - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        nsCRT::memcpy(mRawBuf + mPos, aBuffer, toCopy);
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
        nsCRT::memcpy(mRawBuf + (mPos - 18), aBuffer, toCopy);
        mPos += toCopy;
        aCount -= toCopy;
        aBuffer += toCopy;
    }
    if (mPos == mLOH) {
        ProcessInfoHeader();
        PR_LOG(gBMPLog, PR_LOG_DEBUG, ("BMP image is %lux%lux%lu. compression=%lu\n",
            mBIH.width, mBIH.height, mBIH.bpp, mBIH.compression));
        if (mBIH.compression) {
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

        rv = mImage->Init(mBIH.width, mBIH.height, mObserver);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = mObserver->OnStartContainer(nsnull, nsnull, mImage);
        NS_ENSURE_SUCCESS(rv, rv);
        mCurLine = mBIH.height;

        mRow = new PRUint8[(mBIH.width * mBIH.bpp)/8 + 4];
        // +4 because the line is padded to a 4 bit boundary, but I don't want
        // to make exact calculations here, that's unnecessary.
        // Also, it compensates rounding error.
        if (!mRow) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        rv = mFrame->Init(0, 0, mBIH.width, mBIH.height, GFXFORMAT);
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
    while (aCount && (mPos < mBFH.dataoffset)) { // Skip whatever is between header and data
        mPos++; aBuffer++; aCount--;
    }
    if (++mPos >= mBFH.dataoffset) {
        if (!mBIH.compression) {
            // Need to increment mPos, else we might get to mPos==mLOH again
            // From now on, mPos is irrelevant
            PRUint32 rowSize = (mBIH.bpp * mBIH.width + 7) / 8; // +7 to round up
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
                              SetPixel(d, idx);
                              ++lpos;
                          }
                          ++p;
                        }
                        break;
                      case 4:
                        while (lpos < mBIH.width) {
                          Set4BitPixel(d, *p, lpos);
                          ++p;
                        }
                        break;
                      case 8:
                        while (lpos < mBIH.width) {
                          SetPixel(d, *p);
                          ++lpos;
                          ++p;
                        }
                        break;
                      case 16:
                        while (lpos < mBIH.width) {
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

                    if (mCurLine == 1) { // Finished last line
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
    nsCRT::memset(&mBFH, 0, sizeof(mBFH));
    DOCOPY(&mBFH.signature, mRawBuf);
    DOCOPY(&mBFH.filesize, mRawBuf + 2);
    DOCOPY(&mBFH.reserved, mRawBuf + 6);
    DOCOPY(&mBFH.dataoffset, mRawBuf + 10);
    DOCOPY(&mBFH.bihsize, mRawBuf + 14);

    // Now correct the endianness of the header
    mBFH.filesize = LITTLE_TO_NATIVE32(mBFH.filesize);
    mBFH.dataoffset = LITTLE_TO_NATIVE32(mBFH.dataoffset);
    mBFH.bihsize = LITTLE_TO_NATIVE32(mBFH.bihsize);
}

void nsBMPDecoder::ProcessInfoHeader()
{
    nsCRT::memset(&mBIH, 0, sizeof(mBIH));
    if (mBFH.bihsize == 12) { // OS/2 Bitmap
        nsCRT::memcpy(&mBIH.width, mRawBuf, 2);
        nsCRT::memcpy(&mBIH.height, mRawBuf + 2, 2);
        DOCOPY(&mBIH.planes, mRawBuf + 4);
        DOCOPY(&mBIH.bpp, mRawBuf + 6);
    }
    else {
        DOCOPY(&mBIH.width, mRawBuf);
        DOCOPY(&mBIH.height, mRawBuf + 4);
        DOCOPY(&mBIH.planes, mRawBuf + 8);
        DOCOPY(&mBIH.bpp, mRawBuf + 10);
        DOCOPY(&mBIH.compression, mRawBuf + 12);
        DOCOPY(&mBIH.image_size, mRawBuf + 16);
        DOCOPY(&mBIH.xppm, mRawBuf + 20);
        DOCOPY(&mBIH.yppm, mRawBuf + 24);
        DOCOPY(&mBIH.colors, mRawBuf + 28);
        DOCOPY(&mBIH.important_colors, mRawBuf + 32);
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
