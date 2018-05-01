/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include <errno.h>
#include "nsHttpChunkedDecoder.h"
#include <algorithm>
#include "plstr.h"

#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// nsHttpChunkedDecoder <public>
//-----------------------------------------------------------------------------

nsresult
nsHttpChunkedDecoder::HandleChunkedContent(char *buf,
                                           uint32_t count,
                                           uint32_t *contentRead,
                                           uint32_t *contentRemaining)
{
    LOG(("nsHttpChunkedDecoder::HandleChunkedContent [count=%u]\n", count));

    *contentRead = 0;

    // from RFC2616 section 3.6.1, the chunked transfer coding is defined as:
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
            uint32_t amt = std::min(mChunkRemaining, count);

            count -= amt;
            mChunkRemaining -= amt;

            *contentRead += amt;
            buf += amt;
        }
        else if (mReachedEOF)
            break; // done
        else {
            uint32_t bytesConsumed = 0;

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
                                          uint32_t count,
                                          uint32_t *bytesConsumed)
{
    NS_PRECONDITION(mChunkRemaining == 0, "chunk remaining should be zero");
    NS_PRECONDITION(count, "unexpected");

    *bytesConsumed = 0;

    char *p = static_cast<char *>(memchr(buf, '\n', count));
    if (p) {
        *p = 0;
        count = p - buf; // new length
        *bytesConsumed = count + 1; // length + newline
        if ((p > buf) && (*(p-1) == '\r')) { // eliminate a preceding CR
            *(p-1) = 0;
            count--;
        }

        // make buf point to the full line buffer to parse
        if (!mLineBuf.IsEmpty()) {
            mLineBuf.Append(buf, count);
            buf = (char *) mLineBuf.get();
            count = mLineBuf.Length();
        }

        if (mWaitEOF) {
            if (*buf) {
                LOG(("got trailer: %s\n", buf));
                // allocate a header array for the trailers on demand
                if (!mTrailers) {
                    mTrailers = new nsHttpHeaderArray();
                }

                nsHttpAtom hdr = {nullptr};
                nsAutoCString headerNameOriginal;
                nsAutoCString val;
                if (NS_SUCCEEDED(mTrailers->ParseHeaderLine(nsDependentCSubstring(buf, count),
                                                            &hdr, &headerNameOriginal, &val))) {
                    if (hdr == nsHttp::Server_Timing) {
                        Unused << mTrailers->SetHeaderFromNet(hdr, headerNameOriginal,
                                                              val, true);
                    }
                }
            }
            else {
                mWaitEOF = false;
                mReachedEOF = true;
                LOG(("reached end of chunked-body\n"));
            }
        }
        else if (*buf) {
            char *endptr;
            unsigned long parsedval; // could be 64 bit, could be 32

            // ignore any chunk-extensions
            if ((p = PL_strchr(buf, ';')) != nullptr)
                *p = 0;

            // mChunkRemaining is an uint32_t!
            parsedval = strtoul(buf, &endptr, 16);
            mChunkRemaining = (uint32_t) parsedval;

            if ((endptr == buf) ||
                ((errno == ERANGE) && (parsedval == ULONG_MAX))  ||
                (parsedval != mChunkRemaining) ) {
                LOG(("failed parsing hex on string [%s]\n", buf));
                return NS_ERROR_UNEXPECTED;
            }

            // we've discovered the last chunk
            if (mChunkRemaining == 0)
                mWaitEOF = true;
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

} // namespace net
} // namespace mozilla
