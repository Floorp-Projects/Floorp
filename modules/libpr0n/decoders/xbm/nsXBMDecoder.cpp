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

#include "imgILoad.h"

#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
#define GFXFORMAT gfxIFormats::BGR_A1
#else
#define USE_RGB
#define GFXFORMAT gfxIFormats::RGB_A1
#endif

NS_IMPL_ISUPPORTS1(nsXBMDecoder, imgIDecoder)

nsXBMDecoder::nsXBMDecoder() : mBuf(nsnull), mPos(nsnull), mRow(nsnull), mAlphaRow(nsnull)
{
}

nsXBMDecoder::~nsXBMDecoder()
{
    if (mBuf)
        free(mBuf);

    if (mRow)
        delete[] mRow;

    if (mAlphaRow)
        delete[] mAlphaRow;
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

    if (mRow) {
        delete[] mRow;
        mRow = nsnull;
    }
    if (mAlphaRow) {
        delete[] mAlphaRow;
        mAlphaRow = nsnull;
    }

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
    nsXBMDecoder *decoder = NS_REINTERPRET_CAST(nsXBMDecoder*, aClosure);
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
    mBuf = (char*)realloc(mBuf, mBufSize + aCount + 1);
    if (!mBuf) {
        mState = RECV_DONE;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    memcpy(mBuf + mBufSize, aData, aCount);
    mBufSize += aCount;
    mBuf[mBufSize] = 0;
    mPos = mBuf + posOffset;
    if (mState == RECV_HEADER) {
        mPos = strstr(mBuf, "#define");
        if (!mPos)
            // #define not found. return for now, waiting for more data.
            return NS_OK;

        // Convert width and height to numbers
        if (sscanf(mPos, "#define %*s %d #define %*s %d", &mWidth, &mHeight) != 2)
            return NS_OK;

        // Check for X11 flavor
        if (strstr(mBuf, " char "))
            mIsX10 = PR_FALSE;
        // Check for X10 flavor
        else if (strstr(mBuf, " short "))
            mIsX10 = PR_TRUE;
        else
            // Neither identifier found.  Return for now, waiting for more data.
            return NS_OK;

        mImage->Init(mWidth, mHeight, mObserver);
        mObserver->OnStartContainer(nsnull, mImage);

        nsresult rv = mFrame->Init(0, 0, mWidth, mHeight, GFXFORMAT, 24);
        if (NS_FAILED(rv))
          return rv;

        mImage->AppendFrame(mFrame);
        mObserver->OnStartFrame(nsnull, mFrame);

        PRUint32 bpr;
        mFrame->GetImageBytesPerRow(&bpr);
        PRUint32 abpr;
        mFrame->GetAlphaBytesPerRow(&abpr);

        mRow = new PRUint8[bpr];
        memset(mRow, 0, bpr);
        mAlphaRow = new PRUint8[abpr];

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
        PRUint32 bpr;
        mFrame->GetImageBytesPerRow(&bpr);
        PRUint32 abpr;
        mFrame->GetAlphaBytesPerRow(&abpr);
        PRBool hiByte = PR_TRUE;

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
            if (*endPtr && (*endPtr != ',')) {
                *endPtr = '\0';
                mState = RECV_DONE;  // strange character (or ending '}')
            }
            if (!mIsX10 || !hiByte)
                mPos = endPtr; // go to next value only when done with this one
            if (mIsX10) {
                // handle X10 flavor short values
                if (hiByte) 
                    pixel >>= 8;
                hiByte = !hiByte;
            }

            mAlphaRow[mCurCol/8] = 0;
            for (int i = 0; i < 8; i++) {
                PRUint8 val = (pixel & (1 << i)) >> i;
                mAlphaRow[mCurCol/8] |= val << (7 - i);
            }

            mCurCol = PR_MIN(mCurCol + 8, mWidth);
            if (mCurCol == mWidth || mState == RECV_DONE) {
                    // Row finished. Set Data.
                    mFrame->SetAlphaData(mAlphaRow, abpr, mCurRow * abpr);
                    mFrame->SetImageData(mRow, bpr, mCurRow * bpr);
                    nsRect r(0, mCurRow, mWidth, 1);
                    mObserver->OnDataAvailable(nsnull, mFrame, &r);

                    if ((mCurRow + 1) == mHeight) {
                        mState = RECV_DONE;
                        return mObserver->OnStopFrame(nsnull, mFrame);
                    }
                    mCurRow++;
                    mCurCol = 0;
            }

            mPos++;
        } while (*mPos && (mState == RECV_DATA));
    }
    else
        return NS_ERROR_FAILURE;
    
    return NS_OK;
}


