/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TRRServiceBase.h"

#include "mozilla/Preferences.h"
#include "nsIOService.h"

namespace mozilla {
namespace net {

#undef LOG
extern mozilla::LazyLogModule gHostResolverLog;
#define LOG(args) MOZ_LOG(gHostResolverLog, mozilla::LogLevel::Debug, args)

TRRServiceBase::TRRServiceBase() : mMode(0) {}

void TRRServiceBase::ProcessURITemplate(nsACString& aURI) {
  // URI Template, RFC 6570.
  if (aURI.IsEmpty()) {
    return;
  }
  nsAutoCString scheme;
  nsCOMPtr<nsIIOService> ios(do_GetIOService());
  if (ios) {
    ios->ExtractScheme(aURI, scheme);
  }
  if (!scheme.Equals("https")) {
    LOG(("TRRService TRR URI %s is not https. Not used.\n",
         PromiseFlatCString(aURI).get()));
    aURI.Truncate();
    return;
  }

  // cut off everything from "{" to "}" sequences (potentially multiple),
  // as a crude conversion from template into URI.
  nsAutoCString uri(aURI);

  do {
    nsCCharSeparatedTokenizer openBrace(uri, '{');
    if (openBrace.hasMoreTokens()) {
      // the 'nextToken' is the left side of the open brace (or full uri)
      nsAutoCString prefix(openBrace.nextToken());

      // if there is an open brace, there's another token
      const nsACString& endBrace = openBrace.nextToken();
      nsCCharSeparatedTokenizer closeBrace(endBrace, '}');
      if (closeBrace.hasMoreTokens()) {
        // there is a close brace as well, make a URI out of the prefix
        // and the suffix
        closeBrace.nextToken();
        nsAutoCString suffix(closeBrace.nextToken());
        uri = prefix + suffix;
      } else {
        // no (more) close brace
        break;
      }
    } else {
      // no (more) open brace
      break;
    }
  } while (true);

  aURI = uri;
}

void TRRServiceBase::CheckURIPrefs() {
  mURISetByDetection = false;

  // The user has set a custom URI so it takes precedence.
  if (mURIPrefHasUserValue) {
    MaybeSetPrivateURI(mURIPref);
    return;
  }

  // Check if the rollout addon has set a pref.
  if (!mRolloutURIPref.IsEmpty()) {
    MaybeSetPrivateURI(mRolloutURIPref);
    return;
  }

  // Otherwise just use the default value.
  MaybeSetPrivateURI(mURIPref);
}

// static
uint32_t ModeFromPrefs() {
  // 0 - off, 1 - reserved, 2 - TRR first, 3 - TRR only, 4 - reserved,
  // 5 - explicit off

  auto processPrefValue = [](uint32_t value) -> uint32_t {
    if (value == MODE_RESERVED1 || value == MODE_RESERVED4 ||
        value > MODE_TRROFF) {
      return MODE_TRROFF;
    }
    return value;
  };

  uint32_t tmp = MODE_NATIVEONLY;
  if (NS_SUCCEEDED(Preferences::GetUint("network.trr.mode", &tmp))) {
    tmp = processPrefValue(tmp);
  }

  if (tmp != MODE_NATIVEONLY) {
    return tmp;
  }

  if (NS_SUCCEEDED(Preferences::GetUint(kRolloutModePref, &tmp))) {
    return processPrefValue(tmp);
  }

  return MODE_NATIVEONLY;
}

void TRRServiceBase::OnTRRModeChange() {
  uint32_t oldMode = mMode;
  mMode = ModeFromPrefs();
  if (mMode != oldMode) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, NS_NETWORK_TRR_MODE_CHANGED_TOPIC, nullptr);
    }
  }
}

void TRRServiceBase::OnTRRURIChange() {
  mURIPrefHasUserValue = Preferences::HasUserValue("network.trr.uri");
  Preferences::GetCString("network.trr.uri", mURIPref);
  Preferences::GetCString(kRolloutURIPref, mRolloutURIPref);

  CheckURIPrefs();
}

}  // namespace net
}  // namespace mozilla
