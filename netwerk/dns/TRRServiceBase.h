/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRRServiceBase_h_
#define TRRServiceBase_h_

#include "mozilla/Atomics.h"
#include "mozilla/DataMutex.h"
#include "mozilla/net/rust_helper.h"
#include "nsString.h"
#include "nsIDNSService.h"
#include "nsIProtocolProxyService2.h"

class nsICancelable;
class nsIProxyInfo;

namespace mozilla {
namespace net {

class nsHttpConnectionInfo;

static const char kRolloutURIPref[] = "doh-rollout.uri";
static const char kRolloutModePref[] = "doh-rollout.mode";

class TRRServiceBase : public nsIProxyConfigChangedCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  TRRServiceBase();
  nsIDNSService::ResolverMode Mode() { return mMode; }
  virtual void GetURI(nsACString& result) = 0;
  already_AddRefed<nsHttpConnectionInfo> TRRConnectionInfo();
  // Called to initialize the connection info. Once the connection info is
  // created first time, mTRRConnectionInfoInited will be set to true.
  virtual void InitTRRConnectionInfo();
  bool TRRConnectionInfoInited() const { return mTRRConnectionInfoInited; }

 protected:
  virtual ~TRRServiceBase();

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

  void DoReadEtcHostsFile(ParsingCallback aCallback);
  virtual void ReadEtcHostsFile() = 0;
  // Called to create a connection info that will be used by TRRServiceChannel.
  // Note that when this function is called, mDefaultTRRConnectionInfo will be
  // set to null to invalidate the connection info.
  // When the connection info is created, SetDefaultTRRConnectionInfo() is
  // called to set the result to mDefaultTRRConnectionInfo.
  // Note that this method does nothing when mTRRConnectionInfoInited is false.
  // We want to starting updating the connection info after it's create first
  // time.
  void AsyncCreateTRRConnectionInfo(const nsACString& aURI);
  void AsyncCreateTRRConnectionInfoInternal(const nsACString& aURI);
  virtual void SetDefaultTRRConnectionInfo(nsHttpConnectionInfo* aConnInfo);
  void RegisterProxyChangeListener();
  void UnregisterProxyChangeListener();

  nsCString mPrivateURI;  // protected by mMutex
  // Pref caches should only be used on the main thread.
  nsCString mURIPref;
  nsCString mRolloutURIPref;
  nsCString mDefaultURIPref;
  nsCString mOHTTPURIPref;

  Atomic<nsIDNSService::ResolverMode, Relaxed> mMode{
      nsIDNSService::MODE_NATIVEONLY};
  Atomic<bool, Relaxed> mURISetByDetection{false};
  Atomic<bool, Relaxed> mTRRConnectionInfoInited{false};
  DataMutex<RefPtr<nsHttpConnectionInfo>> mDefaultTRRConnectionInfo;
};

}  // namespace net
}  // namespace mozilla

#endif  // TRRServiceBase_h_
