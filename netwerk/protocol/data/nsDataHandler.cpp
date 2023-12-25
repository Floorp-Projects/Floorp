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
#include "nsNetUtil.h"
#include "nsSimpleURI.h"
#include "nsUnicharUtils.h"
#include "mozilla/dom/MimeType.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Try.h"
#include "DefaultURI.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsDataHandler, nsIProtocolHandler, nsISupportsWeakReference)

nsresult nsDataHandler::Create(const nsIID& aIID, void** aResult) {
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

/* static */ nsresult nsDataHandler::CreateNewURI(const nsACString& aSpec,
                                                  const char* aCharset,
                                                  nsIURI* aBaseURI,
                                                  nsIURI** result) {
  nsCOMPtr<nsIURI> uri;
  nsAutoCString contentType;
  bool base64;
  MOZ_TRY(ParseURI(aSpec, contentType, /* contentCharset = */ nullptr, base64,
                   /* dataBuffer = */ nullptr));

  // Strip whitespace unless this is text, where whitespace is important
  // Don't strip escaped whitespace though (bug 391951)
  nsresult rv;
  if (base64 || (StaticPrefs::network_url_strip_data_url_whitespace() &&
                 strncmp(contentType.get(), "text/", 5) != 0 &&
                 contentType.Find("xml") == kNotFound)) {
    // it's ascii encoded binary, don't let any spaces in
    rv = NS_MutateURI(new mozilla::net::nsSimpleURI::Mutator())
             .Apply(&nsISimpleURIMutator::SetSpecAndFilterWhitespace, aSpec,
                    nullptr)
             .Finalize(uri);
  } else {
    rv = NS_MutateURI(new mozilla::net::nsSimpleURI::Mutator())
             .SetSpec(aSpec)
             .Finalize(uri);
  }

  if (NS_FAILED(rv)) return rv;

  // use DefaultURI to check for validity when we have possible hostnames
  // since nsSimpleURI doesn't know about hostnames
  auto pos = aSpec.Find("data:");
  if (pos != kNotFound) {
    nsDependentCSubstring rest(aSpec, pos + sizeof("data:") - 1, -1);
    if (StringBeginsWith(rest, "//"_ns)) {
      nsCOMPtr<nsIURI> uriWithHost;
      rv = NS_MutateURI(new mozilla::net::DefaultURI::Mutator())
               .SetSpec(aSpec)
               .Finalize(uriWithHost);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

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

  // set the loadInfo on the new channel
  nsresult rv = channel->SetLoadInfo(aLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  channel.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::AllowPort(int32_t port, const char* scheme, bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

namespace {

bool TrimSpacesAndBase64(nsACString& aMimeType) {
  const char* beg = aMimeType.BeginReading();
  const char* end = aMimeType.EndReading();

  // trim leading and trailing spaces
  while (beg < end && NS_IsHTTPWhitespace(*beg)) {
    ++beg;
  }
  if (beg == end) {
    aMimeType.Truncate();
    return false;
  }
  while (end > beg && NS_IsHTTPWhitespace(*(end - 1))) {
    --end;
  }
  if (beg == end) {
    aMimeType.Truncate();
    return false;
  }

  // trim trailing `; base64` (if any) and remember it
  const char* pos = end - 1;
  bool foundBase64 = false;
  if (pos > beg && *pos == '4' && --pos > beg && *pos == '6' && --pos > beg &&
      ToLowerCaseASCII(*pos) == 'e' && --pos > beg &&
      ToLowerCaseASCII(*pos) == 's' && --pos > beg &&
      ToLowerCaseASCII(*pos) == 'a' && --pos > beg &&
      ToLowerCaseASCII(*pos) == 'b') {
    while (--pos > beg && NS_IsHTTPWhitespace(*pos)) {
    }
    if (pos >= beg && *pos == ';') {
      end = pos;
      foundBase64 = true;
    }
  }

  // actually trim off the spaces and trailing base64, returning if we found it.
  const char* s = aMimeType.BeginReading();
  aMimeType.Assign(Substring(aMimeType, beg - s, end - s));
  return foundBase64;
}

}  // namespace

nsresult nsDataHandler::ParsePathWithoutRef(const nsACString& aPath,
                                            nsCString& aContentType,
                                            nsCString* aContentCharset,
                                            bool& aIsBase64,
                                            nsDependentCSubstring* aDataBuffer,
                                            nsCString* aMimeType) {
  static constexpr auto kCharset = "charset"_ns;

  // This implements https://fetch.spec.whatwg.org/#data-url-processor
  // It also returns the full mimeType in aMimeType so fetch/XHR may access it
  // for content-length headers. The contentType and charset parameters retain
  // our legacy behavior, as much Gecko code generally expects GetContentType
  // to yield only the MimeType's essence, not its full value with parameters.

  aIsBase64 = false;

  int32_t commaIdx = aPath.FindChar(',');

  // This is a hack! When creating a URL using the DOM API we want to ignore
  // if a comma is missing. But if we're actually loading a data: URI, in which
  // case aContentCharset is not null, then we want to return an error if a
  // comma is missing.
  if (aContentCharset && commaIdx == kNotFound) {
    return NS_ERROR_MALFORMED_URI;
  }

  // "Let mimeType be the result of collecting a sequence of code points that
  // are not equal to U+002C (,), given position."
  nsCString mimeType(Substring(aPath, 0, commaIdx));

  // "Strip leading and trailing ASCII whitespace from mimeType."
  // "If mimeType ends with U+003B (;), followed by zero or more U+0020 SPACE,
  // followed by an ASCII case-insensitive match for "base64", then ..."
  aIsBase64 = TrimSpacesAndBase64(mimeType);

  // "If mimeType starts with ";", then prepend "text/plain" to mimeType."
  if (mimeType.Length() > 0 && mimeType.CharAt(0) == ';') {
    mimeType = "text/plain"_ns + mimeType;
  }

  // "Let mimeTypeRecord be the result of parsing mimeType."
  // This also checks for instances of ;base64 in the middle of the MimeType.
  // This is against the current spec, but we're doing it because we have
  // historically seen webcompat issues relying on this (see bug 781693).
  if (mozilla::UniquePtr<CMimeType> parsed = CMimeType::Parse(mimeType)) {
    parsed->GetEssence(aContentType);
    if (aContentCharset) {
      parsed->GetParameterValue(kCharset, *aContentCharset);
    }
    if (aMimeType) {
      parsed->Serialize(*aMimeType);
    }
    if (parsed->IsBase64() &&
        !StaticPrefs::network_url_strict_data_url_base64_placement()) {
      aIsBase64 = true;
    }
  } else {
    // "If mimeTypeRecord is failure, then set mimeTypeRecord to
    // text/plain;charset=US-ASCII."
    aContentType.AssignLiteral("text/plain");
    if (aContentCharset) {
      aContentCharset->AssignLiteral("US-ASCII");
    }
    if (aMimeType) {
      aMimeType->AssignLiteral("text/plain;charset=US-ASCII");
    }
  }

  if (aDataBuffer) {
    aDataBuffer->Rebind(aPath, commaIdx + 1);
  }

  return NS_OK;
}

static inline char ToLower(const char c) {
  if (c >= 'A' && c <= 'Z') {
    return char(c + ('a' - 'A'));
  }
  return c;
}

nsresult nsDataHandler::ParseURI(const nsACString& spec, nsCString& contentType,
                                 nsCString* contentCharset, bool& isBase64,
                                 nsCString* dataBuffer) {
  static constexpr auto kDataScheme = "data:"_ns;

  // move past "data:"
  const char* pos = std::search(
      spec.BeginReading(), spec.EndReading(), kDataScheme.BeginReading(),
      kDataScheme.EndReading(),
      [](const char a, const char b) { return ToLower(a) == ToLower(b); });
  if (pos == spec.EndReading()) {
    return NS_ERROR_MALFORMED_URI;
  }

  uint32_t scheme = pos - spec.BeginReading();
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
