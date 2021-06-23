/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TRRServiceBase.h"

#include "mozilla/Preferences.h"
#include "nsHostResolver.h"
#include "nsNetUtil.h"
#include "nsIOService.h"
#include "nsIDNSService.h"
// Put DNSLogging.h at the end to avoid LOG being overwritten by other headers.
#include "DNSLogging.h"
#include "mozilla/StaticPrefs_network.h"

namespace mozilla {
namespace net {

TRRServiceBase::TRRServiceBase()
    : mMode(nsIDNSService::MODE_NATIVEONLY), mURISetByDetection(false) {}

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
  MaybeSetPrivateURI(mDefaultURIPref);
}

// static
nsIDNSService::ResolverMode ModeFromPrefs() {
  // 0 - off, 1 - reserved, 2 - TRR first, 3 - TRR only, 4 - reserved,
  // 5 - explicit off

  auto processPrefValue = [](uint32_t value) -> nsIDNSService::ResolverMode {
    if (value == nsIDNSService::MODE_RESERVED1 ||
        value == nsIDNSService::MODE_RESERVED4 ||
        value > nsIDNSService::MODE_TRROFF) {
      return nsIDNSService::MODE_TRROFF;
    }
    return static_cast<nsIDNSService::ResolverMode>(value);
  };

  uint32_t tmp;
  if (NS_FAILED(Preferences::GetUint("network.trr.mode", &tmp))) {
    tmp = 0;
  }
  nsIDNSService::ResolverMode modeFromPref = processPrefValue(tmp);

  if (modeFromPref != nsIDNSService::MODE_NATIVEONLY) {
    return modeFromPref;
  }

  if (NS_FAILED(Preferences::GetUint(kRolloutModePref, &tmp))) {
    tmp = 0;
  }
  modeFromPref = processPrefValue(tmp);

  return modeFromPref;
}

void TRRServiceBase::OnTRRModeChange() {
  uint32_t oldMode = mMode;
  mMode = ModeFromPrefs();
  if (mMode != oldMode) {
    LOG(("TRR Mode changed from %d to %d", oldMode, int(mMode)));
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, NS_NETWORK_TRR_MODE_CHANGED_TOPIC, nullptr);
    }
  }

  static bool readHosts = false;
  if ((mMode == nsIDNSService::MODE_TRRFIRST ||
       mMode == nsIDNSService::MODE_TRRONLY) &&
      !readHosts) {
    readHosts = true;
    ReadEtcHostsFile();
  }
}

void TRRServiceBase::OnTRRURIChange() {
  mURIPrefHasUserValue = Preferences::HasUserValue("network.trr.uri");
  Preferences::GetCString("network.trr.uri", mURIPref);
  Preferences::GetCString(kRolloutURIPref, mRolloutURIPref);
  Preferences::GetCString("network.trr.default_provider_uri", mDefaultURIPref);

  CheckURIPrefs();
}

}  // namespace net
}  // namespace mozilla
