/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsNSSCallbacks.h"
#include "prerror.h"
#include "sslt.h"

#ifdef XP_WIN
#include "windows.h" // this needs to be before the following includes
#include "wincrypt.h"
#endif // XP_WIN

class nsIDOMWindow;
class nsIPrompt;
class nsIX509CertList;
class SmartCardThreadList;

namespace mozilla { namespace psm {

MOZ_MUST_USE
  ::already_AddRefed<mozilla::psm::SharedCertVerifier>
  GetDefaultCertVerifier();

} } // namespace mozilla::psm


#define NS_NSSCOMPONENT_CID \
{0x4cb64dfd, 0xca98, 0x4e24, {0xbe, 0xfd, 0x0d, 0x92, 0x85, 0xa3, 0x3b, 0xcb}}

#define PSM_COMPONENT_CONTRACTID "@mozilla.org/psm;1"

#define NS_INSSCOMPONENT_IID \
  { 0xa0a8f52b, 0xea18, 0x4abc, \
    { 0xa3, 0xca, 0xec, 0xcf, 0x70, 0x4f, 0xfe, 0x63 } }

extern bool EnsureNSSInitializedChromeOrContent();

class NS_NO_VTABLE nsINSSComponent : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INSSCOMPONENT_IID)

  NS_IMETHOD LogoutAuthenticatedPK11() = 0;

#ifdef DEBUG
  NS_IMETHOD IsCertTestBuiltInRoot(CERTCertificate* cert, bool& result) = 0;
#endif

  NS_IMETHOD IsCertContentSigningRoot(CERTCertificate* cert, bool& result) = 0;

#ifdef XP_WIN
  NS_IMETHOD GetEnterpriseRoots(nsIX509CertList** enterpriseRoots) = 0;
  NS_IMETHOD TrustLoaded3rdPartyRoots() = 0;
#endif

  NS_IMETHOD BlockUntilLoadableRootsLoaded() = 0;
  NS_IMETHOD CheckForSmartCardChanges() = 0;
  // IssuerMatchesMitmCanary succeeds if aCertIssuer matches the canary and
  // the feature is enabled. It returns an error if the strings don't match,
  // the canary is not set, or the feature is disabled.
  NS_IMETHOD IssuerMatchesMitmCanary(const char* aCertIssuer) = 0;

  // Main thread only
  NS_IMETHOD HasActiveSmartCards(bool& result) = 0;
  NS_IMETHOD HasUserCertsInstalled(bool& result) = 0;

  virtual ::already_AddRefed<mozilla::psm::SharedCertVerifier>
    GetDefaultCertVerifier() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINSSComponent, NS_INSSCOMPONENT_IID)

// Implementation of the PSM component interface.
class nsNSSComponent final : public nsINSSComponent
                           , public nsIObserver
{
public:
  // LoadLoadableRootsTask updates mLoadableRootsLoaded and
  // mLoadableRootsLoadedResult and then signals mLoadableRootsLoadedMonitor.
  friend class LoadLoadableRootsTask;

  NS_DEFINE_STATIC_CID_ACCESSOR( NS_NSSCOMPONENT_CID )

  nsNSSComponent();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsresult Init();

  static nsresult GetNewPrompter(nsIPrompt** result);

  NS_IMETHOD LogoutAuthenticatedPK11() override;

#ifdef DEBUG
  NS_IMETHOD IsCertTestBuiltInRoot(CERTCertificate* cert, bool& result) override;
#endif

  NS_IMETHOD IsCertContentSigningRoot(CERTCertificate* cert, bool& result) override;

#ifdef XP_WIN
  NS_IMETHOD GetEnterpriseRoots(nsIX509CertList** enterpriseRoots) override;
  NS_IMETHOD TrustLoaded3rdPartyRoots() override;
#endif

  NS_IMETHOD BlockUntilLoadableRootsLoaded() override;
  NS_IMETHOD CheckForSmartCardChanges() override;
  NS_IMETHOD IssuerMatchesMitmCanary(const char* aCertIssuer) override;

  // Main thread only
  NS_IMETHOD HasActiveSmartCards(bool& result) override;
  NS_IMETHOD HasUserCertsInstalled(bool& result) override;

  ::already_AddRefed<mozilla::psm::SharedCertVerifier>
    GetDefaultCertVerifier() override;

  // The following two methods are thread-safe.
  static bool AreAnyWeakCiphersEnabled();
  static void UseWeakCiphersOnSocket(PRFileDesc* fd);

  static void FillTLSVersionRange(SSLVersionRange& rangeOut,
                                  uint32_t minFromPrefs,
                                  uint32_t maxFromPrefs,
                                  SSLVersionRange defaults);

protected:
  virtual ~nsNSSComponent();

private:
  nsresult InitializeNSS();
  void ShutdownNSS();

  void setValidationOptions(bool isInitialSetting,
                            const mozilla::MutexAutoLock& proofOfLock);
  nsresult setEnabledTLSVersions();
  nsresult ConfigureInternalPKCS11Token();
  nsresult RegisterObservers();

  void MaybeEnableFamilySafetyCompatibility();
  void MaybeImportEnterpriseRoots();
#ifdef XP_WIN
  void ImportEnterpriseRootsForLocation(
    DWORD locationFlag, const mozilla::MutexAutoLock& proofOfLock);
  nsresult MaybeImportFamilySafetyRoot(PCCERT_CONTEXT certificate,
                                       bool& wasFamilySafetyRoot);
  nsresult LoadFamilySafetyRoot();
  void UnloadFamilySafetyRoot();

  void UnloadEnterpriseRoots();
#endif // XP_WIN

  // mLoadableRootsLoadedMonitor protects mLoadableRootsLoaded.
  mozilla::Monitor mLoadableRootsLoadedMonitor;
  bool mLoadableRootsLoaded;
  nsresult mLoadableRootsLoadedResult;

  // mMutex protects all members that are accessed from more than one thread.
  mozilla::Mutex mMutex;

  // The following members are accessed from more than one thread:
  bool mNSSInitialized;
#ifdef DEBUG
  nsString mTestBuiltInRootHash;
#endif
  nsString mContentSigningRootHash;
  RefPtr<mozilla::psm::SharedCertVerifier> mDefaultCertVerifier;
  nsString mMitmCanaryIssuer;
  bool mMitmDetecionEnabled;
#ifdef XP_WIN
  mozilla::UniqueCERTCertificate mFamilySafetyRoot;
  mozilla::UniqueCERTCertList mEnterpriseRoots;
#endif // XP_WIN

  // The following members are accessed only on the main thread:
  static int mInstanceCount;
};

inline nsresult
BlockUntilLoadableRootsLoaded()
{
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return NS_ERROR_FAILURE;
  }
  return component->BlockUntilLoadableRootsLoaded();
}

inline nsresult
CheckForSmartCardChanges()
{
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

#endif // _nsNSSComponent_h_
