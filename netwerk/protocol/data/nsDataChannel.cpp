/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// data implementation

#include "nsIOService.h"
#include "nsDataChannel.h"
#include "nsDataHandler.h"
#include "nsNetUtil.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsReadableUtils.h"
#include "nsEscape.h"
#include "plbase64.h"
#include "plstr.h"
#include "prmem.h"

nsresult
nsDataChannel::OpenContentStream(bool async, nsIInputStream **result,
                                 nsIChannel** channel)
{
    NS_ENSURE_TRUE(URI(), NS_ERROR_NOT_INITIALIZED);

    nsresult rv;

    nsAutoCString spec;
    rv = URI()->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;

    nsCString contentType, contentCharset, dataBuffer, hashRef;
    bool lBase64;
    rv = nsDataHandler::ParseURI(spec, contentType, contentCharset,
                                 lBase64, dataBuffer, hashRef);

    NS_UnescapeURL(dataBuffer);

    if (lBase64) {
        // Don't allow spaces in base64-encoded content. This is only
        // relevant for escaped spaces; other spaces are stripped in
        // NewURI.
        dataBuffer.StripWhitespace();
    }
    
    nsCOMPtr<nsIInputStream> bufInStream;
    nsCOMPtr<nsIOutputStream> bufOutStream;
    
    // create an unbounded pipe.
    rv = NS_NewPipe(getter_AddRefs(bufInStream),
                    getter_AddRefs(bufOutStream),
                    nsIOService::gDefaultSegmentSize,
                    UINT32_MAX,
                    async, true);
    if (NS_FAILED(rv))
        return rv;

    uint32_t contentLen;
    if (lBase64) {
        const uint32_t dataLen = dataBuffer.Length();
        int32_t resultLen = 0;
        if (dataLen >= 1 && dataBuffer[dataLen-1] == '=') {
            if (dataLen >= 2 && dataBuffer[dataLen-2] == '=')
                resultLen = dataLen-2;
            else
                resultLen = dataLen-1;
        } else {
            resultLen = dataLen;
        }
        resultLen = ((resultLen * 3) / 4);

        // XXX PL_Base64Decode will return a null pointer for decoding
        // errors.  Since those are more likely than out-of-memory,
        // should we return NS_ERROR_MALFORMED_URI instead?
        char * decodedData = PL_Base64Decode(dataBuffer.get(), dataLen, nullptr);
        if (!decodedData) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        rv = bufOutStream->Write(decodedData, resultLen, &contentLen);

        PR_Free(decodedData);
    } else {
        rv = bufOutStream->Write(dataBuffer.get(), dataBuffer.Length(), &contentLen);
    }
    if (NS_FAILED(rv))
        return rv;

    SetContentType(contentType);
    SetContentCharset(contentCharset);
    mContentLength = contentLen;

    NS_ADDREF(*result = bufInStream);

    return NS_OK;
}
