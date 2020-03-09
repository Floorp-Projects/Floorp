/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLDecorationStripper.h"

#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/Preferences.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsEffectiveTLDService.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"

namespace {
static const char* kPrefName =
    "privacy.restrict3rdpartystorage.url_decorations";

inline bool IgnoreWhitespace(char16_t c) { return false; }
}  // namespace

namespace mozilla {

nsresult URLDecorationStripper::StripTrackingIdentifiers(nsIURI* aURI,
                                                         nsACString& aOutSpec) {
  nsAutoString tokenList;
  nsresult rv = Preferences::GetString(kPrefName, tokenList);
  ToLowerCase(tokenList);

  nsAutoCString path;
  rv = aURI->GetPathQueryRef(path);
  NS_ENSURE_SUCCESS(rv, rv);
  ToLowerCase(path);

  int32_t queryBegins = path.FindChar('?');
  // Only positive values are valid since the path must begin with a '/'.
  if (queryBegins > 0) {
    typedef nsCharSeparatedTokenizerTemplate<IgnoreWhitespace> Tokenizer;

    Tokenizer tokenizer(tokenList, ' ');
    nsAutoString token;
    while (tokenizer.hasMoreTokens()) {
      token = tokenizer.nextToken();
      if (token.IsEmpty()) {
        continue;
      }

      nsAutoString value;
      if (dom::URLParams::Extract(Substring(path, queryBegins + 1), token,
                                  value) &&
          !value.IsVoid()) {
        // Tracking identifier found in the URL!
        return StripToRegistrableDomain(aURI, aOutSpec);
      }
    }
  }

  return aURI->GetSpec(aOutSpec);
}

nsresult URLDecorationStripper::StripToRegistrableDomain(nsIURI* aURI,
                                                         nsACString& aOutSpec) {
  NS_MutateURI mutator(aURI);
  mutator.SetPathQueryRef(EmptyCString()).SetUserPass(EmptyCString());

  RefPtr<nsEffectiveTLDService> etldService =
      nsEffectiveTLDService::GetInstance();
  NS_ENSURE_TRUE(etldService, NS_ERROR_FAILURE);
  nsAutoCString baseDomain;
  nsresult rv = etldService->GetBaseDomain(aURI, 0, baseDomain);
  if (NS_SUCCEEDED(rv)) {
    mutator.SetHost(baseDomain);
  } else {
    // If this is an IP address or something like "localhost", ignore the error.
    if (rv != NS_ERROR_HOST_IS_IP_ADDRESS &&
        rv != NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
      return rv;
    }
  }

  nsCOMPtr<nsIURI> uri;
  rv = mutator.Finalize(uri);
  NS_ENSURE_SUCCESS(rv, rv);
  return uri->GetSpec(aOutSpec);
}

}  // namespace mozilla
