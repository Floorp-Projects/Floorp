/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#include "nsHttpChunkedDecoder.h"
#include "nsHttp.h"

//-----------------------------------------------------------------------------
// nsHttpChunkedDecoder <public>
//-----------------------------------------------------------------------------

nsresult
nsHttpChunkedDecoder::HandleChunkedContent(char *buf,
                                           PRUint32 count,
                                           PRUint32 *contentRead,
                                           PRUint32 *contentRemaining)
{
    LOG(("nsHttpChunkedDecoder::HandleChunkedContent [count=%u]\n", count));

    *contentRead = 0;
    
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

            *contentRead += amt;
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
    
    *contentRemaining = count;
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
