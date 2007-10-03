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
 * The Original Code is the Mozilla XBM Decoder.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Kaluszka <ask@swva.net>
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

/* KNOWN BUGS:
 * o first #define line is assumed to be width, second height */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "nsXBMDecoder.h"

#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsIImage.h"
#include "nsIInterfaceRequestorUtils.h"

#include "imgILoad.h"

#include "nsIProperties.h"
#include "nsISupportsPrimitives.h"

#include "gfxColor.h"
#include "nsIImage.h"
#include "nsIInterfaceRequestorUtils.h"

// Static colormap
static const PRUint32 kColors[2] = {
    GFX_PACKED_PIXEL(0, 0, 0, 0),     // Transparent 
    GFX_PACKED_PIXEL(255, 0, 0, 0)    // Black
};

NS_IMPL_ISUPPORTS1(nsXBMDecoder, imgIDecoder)

nsXBMDecoder::nsXBMDecoder() : mBuf(nsnull), mPos(nsnull), mImageData(nsnull)
{
}

nsXBMDecoder::~nsXBMDecoder()
{
    if (mBuf)
        free(mBuf);
}

NS_IMETHODIMP nsXBMDecoder::Init(imgILoad *aLoad)
{
    nsresult rv;
    mObserver = do_QueryInterface(aLoad);

    mImage = do_CreateInstance("@mozilla.org/image/container;1", &rv);
    if (NS_FAILED(rv))
        return rv;

    mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2", &rv);
    if (NS_FAILED(rv))
        return rv;

    aLoad->SetImage(mImage);

    mCurRow = mBufSize = mWidth = mHeight = 0;
    mState = RECV_HEADER;

    return NS_OK;
}

NS_IMETHODIMP nsXBMDecoder::Close()
{
    mObserver->OnStopContainer(nsnull, mImage);
    mObserver->OnStopDecode(nsnull, NS_OK, nsnull);
    mObserver = nsnull;
    mImage = nsnull;
    mFrame = nsnull;
    mImageData = nsnull;

    return NS_OK;
}

NS_IMETHODIMP nsXBMDecoder::Flush()
{
    mFrame->SetMutable(PR_FALSE);
    return NS_OK;
}

NS_METHOD nsXBMDecoder::ReadSegCb(nsIInputStream* aIn, void* aClosure,
                             const char* aFromRawSegment, PRUint32 aToOffset,
                             PRUint32 aCount, PRUint32 *aWriteCount) {
    nsXBMDecoder *decoder = reinterpret_cast<nsXBMDecoder*>(aClosure);
    *aWriteCount = aCount;
    return decoder->ProcessData(aFromRawSegment, aCount);
}

NS_IMETHODIMP nsXBMDecoder::WriteFrom(nsIInputStream *aInStr, PRUint32 aCount, PRUint32 *aRetval)
{
    return aInStr->ReadSegments(ReadSegCb, this, aCount, aRetval);
}

nsresult nsXBMDecoder::ProcessData(const char* aData, PRUint32 aCount) {
    char *endPtr;
    // calculate the offset since the absolute position might no longer
    // be valid after realloc
    const PRPtrdiff posOffset = mPos ? (mPos - mBuf) : 0;

    // expand the buffer to hold the new data
    char* oldbuf = mBuf;
    PRUint32 newbufsize = mBufSize + aCount + 1;
    if (newbufsize < mBufSize)
        mBuf = nsnull;  // size wrapped around, give up
    else
        mBuf = (char*)realloc(mBuf, newbufsize);

    if (!mBuf) {
        mState = RECV_DONE;
        if (oldbuf)
            free(oldbuf);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    memcpy(mBuf + mBufSize, aData, aCount);
    mBufSize += aCount;
    mBuf[mBufSize] = 0;
    mPos = mBuf + posOffset;

    // process latest data according to current state
    if (mState == RECV_HEADER) {
        mPos = strstr(mBuf, "#define");
        if (!mPos)
            // #define not found. return for now, waiting for more data.
            return NS_OK;

        // Convert width and height to numbers.  Convert hotspot for cursor functionality, if present
        if (sscanf(mPos, "#define %*s %u #define %*s %u #define %*s %u #define %*s %u unsigned", &mWidth, &mHeight, &mXHotspot, &mYHotspot) == 4)
            mIsCursor = PR_TRUE;
        else if (sscanf(mPos, "#define %*s %u #define %*s %u unsigned", &mWidth, &mHeight) == 2)
            mIsCursor = PR_FALSE;
        else
             // No identifiers found.  Return for now, waiting for more data.
            return NS_OK;

        // Check for X11 flavor
        if (strstr(mPos, " char "))
            mIsX10 = PR_FALSE;
        // Check for X10 flavor
        else if (strstr(mPos, " short "))
            mIsX10 = PR_TRUE;
        else
            // Neither identifier found.  Return for now, waiting for more data.
            return NS_OK;

        mImage->Init(mWidth, mHeight, mObserver);
        mObserver->OnStartContainer(nsnull, mImage);

        nsresult rv = mFrame->Init(0, 0, mWidth, mHeight, gfxIFormats::RGB_A1, 24);
        if (NS_FAILED(rv))
            return rv;

        if (mIsCursor) {
            nsCOMPtr<nsIProperties> props(do_QueryInterface(mImage));
            if (props) {
                nsCOMPtr<nsISupportsPRUint32> intwrapx = do_CreateInstance("@mozilla.org/supports-PRUint32;1");
                nsCOMPtr<nsISupportsPRUint32> intwrapy = do_CreateInstance("@mozilla.org/supports-PRUint32;1");

                if (intwrapx && intwrapy) {
                    intwrapx->SetData(mXHotspot);
                    intwrapy->SetData(mYHotspot);

                    props->Set("hotspotX", intwrapx);
                    props->Set("hotspotY", intwrapy);
                }
            }
        }

        mImage->AppendFrame(mFrame);
        mObserver->OnStartFrame(nsnull, mFrame);

        PRUint32 imageLen;
        mFrame->GetImageData((PRUint8**)&mImageData, &imageLen);

        mState = RECV_SEEK;

        mCurRow = 0;
        mCurCol = 0;

    }
    if (mState == RECV_SEEK) {
        if ((endPtr = strchr(mPos, '{')) != NULL) {
            mPos = endPtr+1;
            mState = RECV_DATA;
        } else {
            mPos = mBuf + mBufSize;
            return NS_OK;
        }
    }
    if (mState == RECV_DATA) {
        nsCOMPtr<nsIImage> img = do_GetInterface(mFrame);
        PRUint32 *ar = mImageData + mCurRow * mWidth + mCurCol;

        do {
            PRUint32 pixel = strtoul(mPos, &endPtr, 0);
            if (endPtr == mPos)
                return NS_OK;   // no number to be found - need more data
            if (!*endPtr)
                return NS_OK;   // number at the end - might be missing a digit
            if (pixel == 0 && *endPtr == 'x')
                return NS_OK;   // 0x at the end, actual number is missing
            while (*endPtr && isspace(*endPtr))
                endPtr++;       // skip whitespace looking for comma

            if (!*endPtr) {
                // Need more data
                return NS_OK;
            }
            if (*endPtr != ',') {
                *endPtr = '\0';
                mState = RECV_DONE;  // strange character (or ending '}')
            } else {
                // Skip the comma
                endPtr++;
            }
            mPos = endPtr;
            PRUint32 numPixels = 8;
            if (mIsX10) { // X10 use 16bits values, but bytes are swapped
                pixel = (pixel >> 8) | ((pixel&0xFF) << 8);
                numPixels = 16;
            }
            numPixels = PR_MIN(numPixels, mWidth - mCurCol);
            for (PRUint32 i = numPixels; i > 0; --i) {
                *ar++ = kColors[pixel & 1];
                pixel >>= 1;
            }
            mCurCol += numPixels;
            if (mCurCol == mWidth || mState == RECV_DONE) {
                nsIntRect r(0, mCurRow, mWidth, 1);
                img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);
                mObserver->OnDataAvailable(nsnull, mFrame, &r);

                mCurRow++;
                if (mCurRow == mHeight) {
                    mState = RECV_DONE;
                    return mObserver->OnStopFrame(nsnull, mFrame);
                }
                mCurCol = 0;
            }
        } while ((mState == RECV_DATA) && *mPos);
    }

    return NS_OK;
}


