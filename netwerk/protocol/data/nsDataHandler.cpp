/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "nsDataChannel.h"
#include "nsDataHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsNetCID.h"
#include "nsError.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsDataHandler::nsDataHandler() {
}

nsDataHandler::~nsDataHandler() {
}

NS_IMPL_ISUPPORTS1(nsDataHandler, nsIProtocolHandler)

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
    nsRefPtr<nsIURI> uri;

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
        nsAutoCString contentType, contentCharset, dataBuffer, hashRef;
        bool base64;
        rv = ParseURI(spec, contentType, contentCharset, base64, dataBuffer, hashRef);
        if (NS_FAILED(rv))
            return rv;

        // Strip whitespace unless this is text, where whitespace is important
        // Don't strip escaped whitespace though (bug 391951)
        if (base64 || (strncmp(contentType.get(),"text/",5) != 0 &&
                       contentType.Find("xml") == kNotFound)) {
            // it's ascii encoded binary, don't let any spaces in
            spec.StripWhitespace();
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
nsDataHandler::NewChannel(nsIURI* uri, nsIChannel* *result) {
    NS_ENSURE_ARG_POINTER(uri);
    nsDataChannel* channel = new nsDataChannel(uri);
    if (!channel)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(channel);

    nsresult rv = channel->Init();
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
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
                        nsCString& contentCharset,
                        bool&    isBase64,
                        nsCString& dataBuffer,
                        nsCString& hashRef) {
    isBase64 = false;

    // move past "data:"
    char *buffer = (char *) PL_strcasestr(spec.BeginWriting(), "data:");
    if (!buffer) {
        // malformed uri
        return NS_ERROR_MALFORMED_URI;
    }
    buffer += 5;

    // First, find the start of the data
    char *comma = strchr(buffer, ',');
    if (!comma)
        return NS_ERROR_MALFORMED_URI;

    *comma = '\0';

    // determine if the data is base64 encoded.
    char *base64 = PL_strcasestr(buffer, BASE64_EXTENSION);
    if (base64) {
        char *beyond = base64 + strlen(BASE64_EXTENSION);
        // per the RFC 2397 grammar, "base64" MUST be followed by a comma
        // previously substituted by '\0', but we also allow it in between
        // parameters so a subsequent ";" is ok as well (this deals with
        // *broken* data URIs, see bug 781693 for an example)
        if (*beyond == '\0' || *beyond == ';') {
            isBase64 = true;
            *base64 = '\0';
        }
    }

    if (comma == buffer) {
        // nothing but data
        contentType.AssignLiteral("text/plain");
        contentCharset.AssignLiteral("US-ASCII");
    } else {
        // everything else is content type
        char *semiColon = (char *) strchr(buffer, ';');
        if (semiColon)
            *semiColon = '\0';
        
        if (semiColon == buffer || base64 == buffer) {
            // there is no content type, but there are other parameters
            contentType.AssignLiteral("text/plain");
        } else {
            contentType = buffer;
            ToLowerCase(contentType);
        }

        if (semiColon) {
            char *charset = PL_strcasestr(semiColon + 1, "charset=");
            if (charset)
                contentCharset = charset + sizeof("charset=") - 1;

            *semiColon = ';';
        }
    }

    *comma = ',';
    if (isBase64)
        *base64 = ';';

    contentType.StripWhitespace();
    contentCharset.StripWhitespace();

    // Split encoded data from terminal "#ref" (if present)
    char *data = comma + 1;
    char *hash = strchr(data, '#');
    if (!hash) {
        dataBuffer.Assign(data);
        hashRef.Truncate();
    } else {
        dataBuffer.Assign(data, hash - data);
        hashRef.Assign(hash);
    }

    return NS_OK;
}
