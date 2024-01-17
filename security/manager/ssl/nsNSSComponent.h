/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "nsINSSComponent.h"

#include "EnterpriseRoots.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsNSSCallbacks.h"
#include "nsServiceManagerUtils.h"
#include "prerror.h"
#include "sslt.h"

#ifdef XP_WIN
#  include <windows.h>  // this needs to be before the following includes
#  include <wincrypt.h>
#endif  // XP_WIN

class nsIDOMWindow;
class nsIPrompt;
class nsISerialEventTarget;
class nsITimer;

namespace mozilla {
namespace psm {

[[nodiscard]] ::already_AddRefed<mozilla::psm::SharedCertVerifier>
GetDefaultCertVerifier();
UniqueCERTCertList FindClientCertificatesWithPrivateKeys();

}  // namespace psm
}  // namespace mozilla

#define NS_NSSCOMPONENT_CID                          \
  {                                                  \
    0x4cb64dfd, 0xca98, 0x4e24, {                    \
      0xbe, 0xfd, 0x0d, 0x92, 0x85, 0xa3, 0x3b, 0xcb \
    }                                                \
  }

bool EnsureNSSInitializedChromeOrContent();
bool HandleTLSPrefChange(const nsCString& aPref);
void SetValidationOptionsCommon();
void PrepareForShutdownInSocketProcess();

// Implementation of the PSM component interface.
class nsNSSComponent final : public nsINSSComponent, public nsIObserver {
 public:
  // LoadLoadableCertsTask updates mLoadableCertsLoaded and
  // mLoadableCertsLoadedResult and then signals mLoadableCertsLoadedMonitor.
  friend class LoadLoadableCertsTask;
  // BackgroundImportEnterpriseCertsTask calls ImportEnterpriseRoots and
  // UpdateCertVerifierWithEnterpriseRoots.
  friend class BackgroundImportEnterpriseCertsTask;

  nsNSSComponent();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINSSCOMPONENT
  NS_DECL_NSIOBSERVER

  nsresult Init();

  static nsresult GetNewPrompter(nsIPrompt** result);

  static void FillTLSVersionRange(SSLVersionRange& rangeOut,
                                  uint32_t minFromPrefs, uint32_t maxFromPrefs,
                                  SSLVersionRange defaults);

  static nsresult SetEnabledTLSVersions();

  // This function does the actual work of clearing the session cache. It is to
  // be used by the socket process (where there is no nsINSSComponent) and
  // internally by nsNSSComponent.
  // NB: NSS must have already been initialized before this is called.
  static void DoClearSSLExternalAndInternalSessionCache();

 protected:
  virtual ~nsNSSComponent();

 private:
  nsresult InitializeNSS();
  void PrepareForShutdown();

  void setValidationOptions(bool isInitialSetting,
                            const mozilla::MutexAutoLock& proofOfLock);
  void GetRevocationBehaviorFromPrefs(
      /*out*/ mozilla::psm::CertVerifier::OcspDownloadConfig* odc,
      /*out*/ mozilla::psm::CertVerifier::OcspStrictConfig* osc,
      /*out*/ uint32_t* certShortLifetimeInDays,
      /*out*/ TimeDuration& softTimeout,
      /*out*/ TimeDuration& hardTimeout);
  void UpdateCertVerifierWithEnterpriseRoots();
  nsresult RegisterObservers();

  void MaybeImportEnterpriseRoots();
  void ImportEnterpriseRoots();
  void UnloadEnterpriseRoots();
  nsresult CommonGetEnterpriseCerts(
      nsTArray<nsTArray<uint8_t>>& enterpriseCerts, bool getRoots);

  nsresult MaybeEnableIntermediatePreloadingHealer();

  // mLoadableCertsLoadedMonitor protects mLoadableCertsLoaded.
  mozilla::Monitor mLoadableCertsLoadedMonitor;
  bool mLoadableCertsLoaded MOZ_GUARDED_BY(mLoadableCertsLoadedMonitor);
  nsresult mLoadableCertsLoadedResult
      MOZ_GUARDED_BY(mLoadableCertsLoadedMonitor);

  // mMutex protects all members that are accessed from more than one thread.
  mozilla::Mutex mMutex;

  // The following members are accessed from more than one thread:

#ifdef DEBUG
  nsCString mTestBuiltInRootHash MOZ_GUARDED_BY(mMutex);
#endif
  RefPtr<mozilla::psm::SharedCertVerifier> mDefaultCertVerifier
      MOZ_GUARDED_BY(mMutex);
  nsString mMitmCanaryIssuer MOZ_GUARDED_BY(mMutex);
  bool mMitmDetecionEnabled MOZ_GUARDED_BY(mMutex);
  nsTArray<EnterpriseCert> mEnterpriseCerts MOZ_GUARDED_BY(mMutex);

  // The following members are accessed only on the main thread:
  static int mInstanceCount;
  // If the intermediate preloading healer is enabled, the following timer
  // periodically dispatches events to the background task queue. Each of these
  // events scans the NSS certdb for preloaded intermediates that are in
  // cert_storage and thus can be removed. By default, the interval is 5
  // minutes.
  nsCOMPtr<nsISerialEventTarget> mIntermediatePreloadingHealerTaskQueue;
  nsCOMPtr<nsITimer> mIntermediatePreloadingHealerTimer;
};

inline nsresult BlockUntilLoadableCertsLoaded() {
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return NS_ERROR_FAILURE;
  }
  return component->BlockUntilLoadableCertsLoaded();
}

inline nsresult CheckForSmartCardChanges() {
#ifndef MOZ_NO_SMART_CARDS
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return NS_ERROR_FAILURE;
  }
  return component->CheckForSmartCardChanges();
#else
  return NS_OK;
#endif
}

#endif  // _nsNSSComponent_h_
