/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDataChannel.h"
#include "nsDataHandler.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "DataChannelChild.h"
#include "plstr.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsDataHandler::nsDataHandler() {
}

nsDataHandler::~nsDataHandler() {
}

NS_IMPL_ISUPPORTS(nsDataHandler, nsIProtocolHandler, nsISupportsWeakReference)

nsresult
nsDataHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {

    nsDataHandler* ph = new nsDataHandler();
    if (ph == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->QueryInterface(aIID, aResult);
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsDataHandler::GetScheme(nsACString &result) {
    result.AssignLiteral("data");
    return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::GetDefaultPort(int32_t *result) {
    // no ports for data protocol
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::GetProtocolFlags(uint32_t *result) {
    *result = URI_NORELATIVE | URI_NOAUTH | URI_INHERITS_SECURITY_CONTEXT |
              URI_LOADABLE_BY_ANYONE | URI_NON_PERSISTABLE | URI_IS_LOCAL_RESOURCE |
              URI_SYNC_LOAD_IS_OK;
    return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::NewURI(const nsACString &aSpec,
                      const char *aCharset, // ignore charset info
                      nsIURI *aBaseURI,
                      nsIURI **result) {
    nsresult rv;
    RefPtr<nsIURI> uri;

    nsCString spec(aSpec);

    if (aBaseURI && !spec.IsEmpty() && spec[0] == '#') {
        // Looks like a reference instead of a fully-specified URI.
        // --> initialize |uri| as a clone of |aBaseURI|, with ref appended.
        rv = aBaseURI->Clone(getter_AddRefs(uri));
        if (NS_FAILED(rv))
            return rv;
        rv = uri->SetRef(spec);
    } else {
        // Otherwise, we'll assume |spec| is a fully-specified data URI
        nsAutoCString contentType;
        bool base64;
        rv = ParseURI(spec, contentType, /* contentCharset = */ nullptr,
                      base64, /* dataBuffer = */ nullptr);
        if (NS_FAILED(rv))
            return rv;

        // Strip whitespace unless this is text, where whitespace is important
        // Don't strip escaped whitespace though (bug 391951)
        if (base64 || (strncmp(contentType.get(),"text/",5) != 0 &&
                       contentType.Find("xml") == kNotFound)) {
            // it's ascii encoded binary, don't let any spaces in
            if (!spec.StripWhitespace(mozilla::fallible)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        uri = do_CreateInstance(kSimpleURICID, &rv);
        if (NS_FAILED(rv))
            return rv;
        rv = uri->SetSpec(spec);
    }

    if (NS_FAILED(rv))
        return rv;

    uri.forget(result);
    return rv;
}

NS_IMETHODIMP
nsDataHandler::NewChannel2(nsIURI* uri,
                           nsILoadInfo* aLoadInfo,
                           nsIChannel** result)
{
    NS_ENSURE_ARG_POINTER(uri);
    nsDataChannel* channel;
    if (XRE_IsParentProcess()) {
        channel = new nsDataChannel(uri);
    } else {
        channel = new mozilla::net::DataChannelChild(uri);
    }
    NS_ADDREF(channel);

    nsresult rv = channel->Init();
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    // set the loadInfo on the new channel
    rv = channel->SetLoadInfo(aLoadInfo);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    return NewChannel2(uri, nullptr, result);
}

NS_IMETHODIMP
nsDataHandler::AllowPort(int32_t port, const char *scheme, bool *_retval) {
    // don't override anything.
    *_retval = false;
    return NS_OK;
}

#define BASE64_EXTENSION ";base64"

nsresult
nsDataHandler::ParseURI(nsCString& spec,
                        nsCString& contentType,
                        nsCString* contentCharset,
                        bool&    isBase64,
                        nsCString* dataBuffer)
{
    isBase64 = false;

    // move past "data:"
    const char* roBuffer = (const char*) PL_strcasestr(spec.get(), "data:");
    if (!roBuffer) {
        // malformed uri
        return NS_ERROR_MALFORMED_URI;
    }
    roBuffer += sizeof("data:") - 1;

    // First, find the start of the data
    const char* roComma = strchr(roBuffer, ',');
    const char* roHash = strchr(roBuffer, '#');
    if (!roComma || (roHash && roHash < roComma)) {
        return NS_ERROR_MALFORMED_URI;
    }

    if (roComma == roBuffer) {
        // nothing but data
        contentType.AssignLiteral("text/plain");
        if (contentCharset) {
            contentCharset->AssignLiteral("US-ASCII");
        }
    } else {
        // Make a copy of the non-data part so we can null out parts of it as
        // we go. This copy will be a small number of chars, in contrast to the
        // data which may be large.
        char* buffer = PL_strndup(roBuffer, roComma - roBuffer);

        // determine if the data is base64 encoded.
        char* base64 = PL_strcasestr(buffer, BASE64_EXTENSION);
        if (base64) {
            char *beyond = base64 + sizeof(BASE64_EXTENSION) - 1;
            // Per the RFC 2397 grammar, "base64" MUST be at the end of the
            // non-data part.
            //
            // But we also allow it in between parameters so a subsequent ";"
            // is ok as well (this deals with *broken* data URIs, see bug
            // 781693 for an example). Anything after "base64" in the non-data
            // part will be discarded in this case, however.
            if (*beyond == '\0' || *beyond == ';') {
                isBase64 = true;
                *base64 = '\0';
            }
        }

        // everything else is content type
        char *semiColon = (char *) strchr(buffer, ';');
        if (semiColon)
            *semiColon = '\0';

        if (semiColon == buffer || base64 == buffer) {
            // there is no content type, but there are other parameters
            contentType.AssignLiteral("text/plain");
        } else {
            contentType.Assign(buffer);
            ToLowerCase(contentType);
            if (!contentType.StripWhitespace(mozilla::fallible)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        if (semiColon && contentCharset) {
            char *charset = PL_strcasestr(semiColon + 1, "charset=");
            if (charset) {
                contentCharset->Assign(charset + sizeof("charset=") - 1);
                if (!contentCharset->StripWhitespace(mozilla::fallible)) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }

        free(buffer);
    }

    if (dataBuffer) {
        // Split encoded data from terminal "#ref" (if present)
        const char* roData = roComma + 1;
        bool ok = !roHash
                ? dataBuffer->Assign(roData, mozilla::fallible)
                : dataBuffer->Assign(roData, roHash - roData, mozilla::fallible);
        if (!ok) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_OK;
}
