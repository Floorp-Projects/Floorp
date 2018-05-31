/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// data implementation

#include "nsDataChannel.h"

#include "mozilla/Base64.h"
#include "nsIOService.h"
#include "nsDataHandler.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsEscape.h"

using namespace mozilla;

/**
 * Helper for performing a fallible unescape.
 *
 * @param aStr The string to unescape.
 * @param aBuffer Buffer to unescape into if necessary.
 * @param rv Out: nsresult indicating success or failure of unescaping.
 * @return Reference to the string containing the unescaped data.
 */
const nsACString& Unescape(const nsACString& aStr, nsACString& aBuffer,
                           nsresult* rv)
{
    MOZ_ASSERT(rv);

    bool appended = false;
    *rv = NS_UnescapeURL(aStr.Data(), aStr.Length(), /* aFlags = */ 0,
                         aBuffer, appended, mozilla::fallible);
    if (NS_FAILED(*rv) || !appended) {
        return aStr;
    }

    return aBuffer;
}

nsresult
nsDataChannel::OpenContentStream(bool async, nsIInputStream **result,
                                 nsIChannel** channel)
{
    NS_ENSURE_TRUE(URI(), NS_ERROR_NOT_INITIALIZED);

    nsresult rv;

    // In order to avoid potentially building up a new path including the
    // ref portion of the URI, which we don't care about, we clone a version
    // of the URI that does not have a ref and in most cases should share
    // string buffers with the original URI.
    nsCOMPtr<nsIURI> uri;
    rv = URI()->CloneIgnoringRef(getter_AddRefs(uri));
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString path;
    rv = uri->GetPathQueryRef(path);
    if (NS_FAILED(rv))
        return rv;

    nsCString contentType, contentCharset;
    nsDependentCSubstring dataRange;
    bool lBase64;
    rv = nsDataHandler::ParsePathWithoutRef(path, contentType, &contentCharset,
                                            lBase64, &dataRange);
    if (NS_FAILED(rv))
        return rv;

    // This will avoid a copy if nothing needs to be unescaped.
    nsAutoCString unescapedBuffer;
    const nsACString& data = Unescape(dataRange, unescapedBuffer, &rv);
    if (NS_FAILED(rv)) {
        return rv;
    }

    if (lBase64 && &data == &unescapedBuffer) {
        // Don't allow spaces in base64-encoded content. This is only
        // relevant for escaped spaces; other spaces are stripped in
        // NewURI. We know there were no escaped spaces if the data buffer
        // wasn't used in |Unescape|.
        unescapedBuffer.StripWhitespace();
    }

    nsCOMPtr<nsIInputStream> bufInStream;
    nsCOMPtr<nsIOutputStream> bufOutStream;

    // create an unbounded pipe.
    rv = NS_NewPipe(getter_AddRefs(bufInStream),
                    getter_AddRefs(bufOutStream),
                    net::nsIOService::gDefaultSegmentSize,
                    UINT32_MAX,
                    async, true);
    if (NS_FAILED(rv))
        return rv;

    uint32_t contentLen;
    if (lBase64) {
        nsAutoCString decodedData;
        rv = Base64Decode(data, decodedData);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = bufOutStream->Write(decodedData.get(), decodedData.Length(), &contentLen);
    } else {
        rv = bufOutStream->Write(data.Data(), data.Length(), &contentLen);
    }

    if (NS_FAILED(rv))
        return rv;

    SetContentType(contentType);
    SetContentCharset(contentCharset);
    mContentLength = contentLen;

    bufInStream.forget(result);

    return NS_OK;
}
