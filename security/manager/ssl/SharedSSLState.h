/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedSSLState_h
#define SharedSSLState_h

#include "mozilla/RefPtr.h"
#include "nsNSSIOLayer.h"

class nsClientAuthRememberService;
class nsIObserver;

namespace mozilla {
namespace psm {

class SharedSSLState {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedSSLState)
  explicit SharedSSLState(uint32_t aTlsFlags = 0);

  static void GlobalInit();
  static void GlobalCleanup();

  nsClientAuthRememberService* GetClientAuthRememberService() {
    return mClientAuthRemember;
  }

  nsSSLIOLayerHelpers& IOLayerHelpers() {
    return mIOLayerHelpers;
  }

  // Main-thread only
  void ResetStoredData();
  void NotePrivateBrowsingStatus();
  void SetOCSPStaplingEnabled(bool staplingEnabled)
  {
    mOCSPStaplingEnabled = staplingEnabled;
  }
  void SetOCSPMustStapleEnabled(bool mustStapleEnabled)
  {
    mOCSPMustStapleEnabled = mustStapleEnabled;
  }
  void SetSignedCertTimestampsEnabled(bool signedCertTimestampsEnabled)
  {
    mSignedCertTimestampsEnabled = signedCertTimestampsEnabled;
  }

  // The following methods may be called from any thread
  bool SocketCreated();
  void NoteSocketCreated();
  static void NoteCertOverrideServiceInstantiated();
  bool IsOCSPStaplingEnabled() const { return mOCSPStaplingEnabled; }
  bool IsOCSPMustStapleEnabled() const { return mOCSPMustStapleEnabled; }
  bool IsSignedCertTimestampsEnabled() const
  {
    return mSignedCertTimestampsEnabled;
  }

private:
  ~SharedSSLState();

  void Cleanup();

  nsCOMPtr<nsIObserver> mObserver;
  RefPtr<nsClientAuthRememberService> mClientAuthRemember;
  nsSSLIOLayerHelpers mIOLayerHelpers;

  // True if any sockets have been created that use this shared data.
  // Requires synchronization between the socket and main threads for
  // reading/writing.
  Mutex mMutex;
  bool mSocketCreated;
  bool mOCSPStaplingEnabled;
  bool mOCSPMustStapleEnabled;
  bool mSignedCertTimestampsEnabled;
};

SharedSSLState* PublicSSLState();
SharedSSLState* PrivateSSLState();

} // namespace psm
} // namespace mozilla

#endif
