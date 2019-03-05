/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDataChannel.h"
#include "nsDataHandler.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "nsIOService.h"
#include "DataChannelChild.h"
#include "plstr.h"
#include "nsSimpleURI.h"
#include "mozilla/dom/MimeType.h"

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsDataHandler, nsIProtocolHandler, nsISupportsWeakReference)

nsresult nsDataHandler::Create(nsISupports* aOuter, const nsIID& aIID,
                               void** aResult) {
  RefPtr<nsDataHandler> ph = new nsDataHandler();
  return ph->QueryInterface(aIID, aResult);
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsDataHandler::GetScheme(nsACString& result) {
  result.AssignLiteral("data");
  return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::GetDefaultPort(int32_t* result) {
  // no ports for data protocol
  *result = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::GetProtocolFlags(uint32_t* result) {
  *result = URI_NORELATIVE | URI_NOAUTH | URI_INHERITS_SECURITY_CONTEXT |
            URI_LOADABLE_BY_ANYONE | URI_NON_PERSISTABLE |
            URI_IS_LOCAL_RESOURCE | URI_SYNC_LOAD_IS_OK;
  return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::NewURI(const nsACString& aSpec,
                      const char* aCharset,  // ignore charset info
                      nsIURI* aBaseURI, nsIURI** result) {
  return nsDataHandler::CreateNewURI(aSpec, aCharset, aBaseURI, result);
}

/* static */ nsresult nsDataHandler::CreateNewURI(const nsACString& aSpec,
                                                  const char* aCharset,
                                                  nsIURI* aBaseURI,
                                                  nsIURI** result) {
  nsresult rv;
  nsCOMPtr<nsIURI> uri;

  nsCString spec(aSpec);

  if (aBaseURI && !spec.IsEmpty() && spec[0] == '#') {
    // Looks like a reference instead of a fully-specified URI.
    // --> initialize |uri| as a clone of |aBaseURI|, with ref appended.
    rv = NS_MutateURI(aBaseURI).SetRef(spec).Finalize(uri);
  } else {
    // Otherwise, we'll assume |spec| is a fully-specified data URI
    nsAutoCString contentType;
    bool base64;
    rv = ParseURI(spec, contentType, /* contentCharset = */ nullptr, base64,
                  /* dataBuffer = */ nullptr);
    if (NS_FAILED(rv)) return rv;

    // Strip whitespace unless this is text, where whitespace is important
    // Don't strip escaped whitespace though (bug 391951)
    if (base64 || (strncmp(contentType.get(), "text/", 5) != 0 &&
                   contentType.Find("xml") == kNotFound)) {
      // it's ascii encoded binary, don't let any spaces in
      if (!spec.StripWhitespace(mozilla::fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    rv = NS_MutateURI(new mozilla::net::nsSimpleURI::Mutator())
             .SetSpec(spec)
             .Finalize(uri);
  }

  if (NS_FAILED(rv)) return rv;

  uri.forget(result);
  return rv;
}

NS_IMETHODIMP
nsDataHandler::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                          nsIChannel** result) {
  NS_ENSURE_ARG_POINTER(uri);
  RefPtr<nsDataChannel> channel;
  if (XRE_IsParentProcess()) {
    channel = new nsDataChannel(uri);
  } else {
    channel = new mozilla::net::DataChannelChild(uri);
  }

  nsresult rv = channel->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // set the loadInfo on the new channel
  rv = channel->SetLoadInfo(aLoadInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  channel.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::AllowPort(int32_t port, const char* scheme, bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

/**
 * Helper that performs a case insensitive match to find the offset of a given
 * pattern in a nsACString.
 * The search is performed starting from the end of the string; if the string
 * contains more than one match, the rightmost (last) match will be returned.
 */
static bool FindOffsetOf(const nsACString& aPattern, const nsACString& aSrc,
                         nsACString::size_type& aOffset) {
  static const nsCaseInsensitiveCStringComparator kComparator;

  nsACString::const_iterator begin, end;
  aSrc.BeginReading(begin);
  aSrc.EndReading(end);
  if (!RFindInReadable(aPattern, begin, end, kComparator)) {
    return false;
  }

  // FindInReadable updates |begin| and |end| to the match coordinates.
  aOffset = nsACString::size_type(begin.get() - aSrc.Data());
  return true;
}

nsresult nsDataHandler::ParsePathWithoutRef(
    const nsACString& aPath, nsCString& aContentType,
    nsCString* aContentCharset, bool& aIsBase64,
    nsDependentCSubstring* aDataBuffer) {
  static NS_NAMED_LITERAL_CSTRING(kBase64, "base64");
  static NS_NAMED_LITERAL_CSTRING(kCharset, "charset");

  aIsBase64 = false;

  // First, find the start of the data
  int32_t commaIdx = aPath.FindChar(',');
  if (commaIdx == kNotFound) {
    return NS_ERROR_MALFORMED_URI;
  }

  if (commaIdx == 0) {
    // Nothing but data.
    aContentType.AssignLiteral("text/plain");
    if (aContentCharset) {
      aContentCharset->AssignLiteral("US-ASCII");
    }
  } else {
    auto mediaType = Substring(aPath, 0, commaIdx);

    // Determine if the data is base64 encoded.
    nsACString::size_type base64;
    if (FindOffsetOf(kBase64, mediaType, base64) && base64 > 0) {
      nsACString::size_type offset = base64 + kBase64.Length();
      // Per the RFC 2397 grammar, "base64" MUST be at the end of the
      // non-data part.
      //
      // But we also allow it in between parameters so a subsequent ";"
      // is ok as well (this deals with *broken* data URIs, see bug
      // 781693 for an example). Anything after "base64" in the non-data
      // part will be discarded in this case, however.
      if (offset == mediaType.Length() || mediaType[offset] == ';' ||
          mediaType[offset] == ' ') {
        MOZ_DIAGNOSTIC_ASSERT(base64 > 0, "Did someone remove the check?");
        // Index is on the first character of matched "base64" so we
        // move to the preceding character
        base64--;
        // Skip any preceding spaces, searching for a semicolon
        while (base64 > 0 && mediaType[base64] == ' ') {
          base64--;
        }
        if (mediaType[base64] == ';') {
          aIsBase64 = true;
          // Trim the base64 part off.
          mediaType.Rebind(aPath, 0, base64);
        }
      }
    }

    // Skip any leading spaces
    nsACString::size_type startIndex = 0;
    while (startIndex < mediaType.Length() && mediaType[startIndex] == ' ') {
      startIndex++;
    }

    nsAutoCString mediaTypeBuf;
    // If the mimetype starts with ';' we assume text/plain
    if (startIndex < mediaType.Length() && mediaType[startIndex] == ';') {
      mediaTypeBuf.AssignLiteral("text/plain");
      mediaTypeBuf.Append(mediaType);
      mediaType.Rebind(mediaTypeBuf, 0, mediaTypeBuf.Length());
    }

    // Everything else is content type.
    UniquePtr<CMimeType> parsed = CMimeType::Parse(mediaType);
    if (parsed) {
      parsed->GetFullType(aContentType);
      if (aContentCharset) {
        parsed->GetParameterValue(kCharset, *aContentCharset);
      }
    } else {
      // Mime Type parsing failed
      aContentType.AssignLiteral("text/plain");
      if (aContentCharset) {
        aContentCharset->AssignLiteral("US-ASCII");
      }
    }
  }

  if (aDataBuffer) {
    aDataBuffer->Rebind(aPath, commaIdx + 1);
  }

  return NS_OK;
}

nsresult nsDataHandler::ParseURI(nsCString& spec, nsCString& contentType,
                                 nsCString* contentCharset, bool& isBase64,
                                 nsCString* dataBuffer) {
  static NS_NAMED_LITERAL_CSTRING(kDataScheme, "data:");

  // move past "data:"
  int32_t scheme = spec.Find(kDataScheme, /* aIgnoreCase = */ true);
  if (scheme == kNotFound) {
    // malformed uri
    return NS_ERROR_MALFORMED_URI;
  }

  scheme += kDataScheme.Length();

  // Find the start of the hash ref if present.
  int32_t hash = spec.FindChar('#', scheme);

  auto pathWithoutRef = Substring(spec, scheme, hash != kNotFound ? hash : -1);
  nsDependentCSubstring dataRange;
  nsresult rv = ParsePathWithoutRef(pathWithoutRef, contentType, contentCharset,
                                    isBase64, &dataRange);
  if (NS_SUCCEEDED(rv) && dataBuffer) {
    if (!dataBuffer->Assign(dataRange, mozilla::fallible)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return rv;
}
