/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsHttpChunkedDecoder.h"
#include "nsHttp.h"

//-----------------------------------------------------------------------------
// nsHttpChunkedDecoder <public>
//-----------------------------------------------------------------------------

nsresult
nsHttpChunkedDecoder::HandleChunkedContent(char *buf,
                                           PRUint32 count,
                                           PRUint32 *countRead)
{
    LOG(("nsHttpChunkedDecoder::HandleChunkedContent [count=%u]\n", count));

    *countRead = 0;

    while (count) {
        if (mChunkRemaining) {
            PRUint32 amt = PR_MIN(mChunkRemaining, count);

            count -= amt;
            mChunkRemaining -= amt;

            *countRead += amt;
            buf += amt;
        }

        if (count) {
            PRUint32 bytesConsumed = 0;

            nsresult rv = ParseChunkRemaining(buf, count, &bytesConsumed);
            if (NS_FAILED(rv)) return rv;

            count -= bytesConsumed;

            if (count) {
                // shift buf by bytesConsumed
                memmove(buf, buf + bytesConsumed, count);
            }
        }
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChunkedDecoder <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpChunkedDecoder::ParseChunkRemaining(char *buf,
                                          PRUint32 count,
                                          PRUint32 *bytesConsumed)
{
    NS_PRECONDITION(mChunkRemaining == 0, "chunk remaining should be zero");
    NS_PRECONDITION(count, "unexpected");

    *bytesConsumed = 0;
    
    char *p = PL_strnchr(buf, '\n', count);
    if (p) {
        *p = 0;
        *bytesConsumed = p - buf + 1;
        if (mLineBuf.IsEmpty())
            p = buf;
        else {
            // append to the line buf and use that as the buf to parse.
            mLineBuf.Append(buf);
            p = (char *) mLineBuf.get();
        }
    }
    else {
        mLineBuf.Append(buf, count);
        *bytesConsumed = count;
        return NS_OK;
    }

    if (*p && *p != '\r') {
        buf = p;

        // ignore any chunk-extensions
        if ((p = PL_strchr(buf, ';')) != nsnull)
            *p = 0;

        if (!sscanf(buf, "%x", &mChunkRemaining)) {
            LOG(("sscanf failed parsing hex on string [%s]\n", buf));
            return NS_ERROR_UNEXPECTED;
        }

        // XXX need to add code to consume "trailer" headers
        if ((mChunkRemaining == 0) && (*buf != 0))
            mReachedEOF = PR_TRUE;
    }

    // clear the line buffer if it is not already empty
    if (!mLineBuf.IsEmpty())
        mLineBuf.SetLength(0);

    return NS_OK;
}
