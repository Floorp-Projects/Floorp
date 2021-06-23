/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRRServiceBase_h_
#define TRRServiceBase_h_

#include "mozilla/Atomics.h"
#include "nsString.h"
#include "nsIDNSService.h"

namespace mozilla {
namespace net {

static const char kRolloutURIPref[] = "doh-rollout.uri";
static const char kRolloutModePref[] = "doh-rollout.mode";

class TRRServiceBase {
 public:
  TRRServiceBase();
  nsIDNSService::ResolverMode Mode() { return mMode; }

 protected:
  ~TRRServiceBase() = default;
  virtual bool MaybeSetPrivateURI(const nsACString& aURI) = 0;
  void ProcessURITemplate(nsACString& aURI);
  // Checks the network.trr.uri or the doh-rollout.uri prefs and sets the URI
  // in order of preference:
  // 1. The value of network.trr.uri if it is not the default one, meaning
  //    is was set by an explicit user action
  // 2. The value of doh-rollout.uri if it exists
  //    this is set by the rollout addon
  // 3. The default value of network.trr.uri
  void CheckURIPrefs();

  void OnTRRModeChange();
  void OnTRRURIChange();

  virtual void ReadEtcHostsFile() {}

  nsCString mPrivateURI;
  // Pref caches should only be used on the main thread.
  bool mURIPrefHasUserValue = false;
  nsCString mURIPref;
  nsCString mRolloutURIPref;
  nsCString mDefaultURIPref;

  Atomic<nsIDNSService::ResolverMode, Relaxed> mMode;
  Atomic<bool, Relaxed> mURISetByDetection;
};

}  // namespace net
}  // namespace mozilla

#endif  // TRRServiceBase_h_
