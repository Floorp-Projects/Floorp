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
    
    // from RFC2617 section 3.6.1, the chunked transfer coding is defined as:
    //
    //   Chunked-Body    = *chunk
    //                     last-chunk
    //                     trailer
    //                     CRLF
    //   chunk           = chunk-size [ chunk-extension ] CRLF
    //                     chunk-data CRLF
    //   chunk-size      = 1*HEX
    //   last-chunk      = 1*("0") [ chunk-extension ] CRLF
    //       
    //   chunk-extension = *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
    //   chunk-ext-name  = token
    //   chunk-ext-val   = token | quoted-string
    //   chunk-data      = chunk-size(OCTET)
    //   trailer         = *(entity-header CRLF)
    //
    // the chunk-size field is a string of hex digits indicating the size of the
    // chunk.  the chunked encoding is ended by any chunk whose size is zero, 
    // followed by the trailer, which is terminated by an empty line.

    while (count) {
        if (mChunkRemaining) {
            PRUint32 amt = PR_MIN(mChunkRemaining, count);

            count -= amt;
            mChunkRemaining -= amt;

            *countRead += amt;
            buf += amt;
        }
        else if (mReachedEOF)
            break; // done
        else {
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
    
    char *p = NS_STATIC_CAST(char *, memchr(buf, '\n', count));
    if (p) {
        *p = 0;
        if ((p > buf) && (*(p-1) == '\r')) // eliminate a preceding CR
            *(p-1) = 0;
        *bytesConsumed = p - buf + 1;

        // make buf point to the full line buffer to parse
        if (!mLineBuf.IsEmpty()) {
            mLineBuf.Append(buf);
            buf = (char *) mLineBuf.get();
        }

        if (mWaitEOF) {
            if (*buf) {
                LOG(("got trailer: %s\n", buf));
                // allocate a header array for the trailers on demand
                if (!mTrailers) {
                    mTrailers = new nsHttpHeaderArray();
                    if (!mTrailers)
                        return NS_ERROR_OUT_OF_MEMORY;
                }
                mTrailers->ParseHeaderLine(buf);
            }
            else {
                mWaitEOF = PR_FALSE;
                mReachedEOF = PR_TRUE;
                LOG(("reached end of chunked-body\n"));
            }
        }
        else if (*buf) {
            // ignore any chunk-extensions
            if ((p = PL_strchr(buf, ';')) != nsnull)
                *p = 0;

            if (!sscanf(buf, "%x", &mChunkRemaining)) {
                LOG(("sscanf failed parsing hex on string [%s]\n", buf));
                return NS_ERROR_UNEXPECTED;
            }

            // we've discovered the last chunk
            if (mChunkRemaining == 0)
                mWaitEOF = PR_TRUE;
        }

        // ensure that the line buffer is clear
        mLineBuf.Truncate();
    }
    else {
        // save the partial line; wait for more data
        *bytesConsumed = count;
        // ignore a trailing CR
        if (buf[count-1] == '\r')
            count--;
        mLineBuf.Append(buf, count);
    }

    return NS_OK;
}
