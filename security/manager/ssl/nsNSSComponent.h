/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIStringBundle.h"
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

enum EnsureNSSOperator
{
  nssLoadingComponent = 0,
  nssInitSucceeded = 1,
  nssInitFailed = 2,
  nssShutdown = 3,
  nssEnsure = 100,
  nssEnsureOnChromeOnly = 101,
  nssEnsureChromeOrContent = 102,
};

extern bool EnsureNSSInitializedChromeOrContent();

extern bool EnsureNSSInitialized(EnsureNSSOperator op);

class NS_NO_VTABLE nsINSSComponent : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INSSCOMPONENT_IID)

  NS_IMETHOD ShowAlertFromStringBundle(const char* messageID) = 0;

  NS_IMETHOD GetPIPNSSBundleString(const char* name,
                                   nsAString& outString) = 0;
  NS_IMETHOD PIPBundleFormatStringFromName(const char* name,
                                           const char16_t** params,
                                           uint32_t numParams,
                                           nsAString& outString) = 0;

  NS_IMETHOD GetNSSBundleString(const char* name,
                                nsAString& outString) = 0;

  NS_IMETHOD LogoutAuthenticatedPK11() = 0;

#ifndef MOZ_NO_SMART_CARDS
  NS_IMETHOD LaunchSmartCardThread(SECMODModule* module) = 0;

  NS_IMETHOD ShutdownSmartCardThread(SECMODModule* module) = 0;
#endif

  NS_IMETHOD IsNSSInitialized(bool* initialized) = 0;

#ifdef DEBUG
  NS_IMETHOD IsCertTestBuiltInRoot(CERTCertificate* cert, bool& result) = 0;
#endif

  NS_IMETHOD IsCertContentSigningRoot(CERTCertificate* cert, bool& result) = 0;

#ifdef XP_WIN
  NS_IMETHOD GetEnterpriseRoots(nsIX509CertList** enterpriseRoots) = 0;
#endif

  virtual ::already_AddRefed<mozilla::psm::SharedCertVerifier>
    GetDefaultCertVerifier() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINSSComponent, NS_INSSCOMPONENT_IID)

class nsNSSShutDownList;

// Implementation of the PSM component interface.
class nsNSSComponent final : public nsINSSComponent
                           , public nsIObserver
{
public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_NSSCOMPONENT_CID )

  nsNSSComponent();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsresult Init();

  static nsresult GetNewPrompter(nsIPrompt** result);
  static nsresult ShowAlertWithConstructedString(const nsString& message);
  NS_IMETHOD ShowAlertFromStringBundle(const char* messageID) override;

  NS_IMETHOD GetPIPNSSBundleString(const char* name,
                                   nsAString& outString) override;
  NS_IMETHOD PIPBundleFormatStringFromName(const char* name,
                                           const char16_t** params,
                                           uint32_t numParams,
                                           nsAString& outString) override;
  NS_IMETHOD GetNSSBundleString(const char* name, nsAString& outString) override;
  NS_IMETHOD LogoutAuthenticatedPK11() override;

#ifndef MOZ_NO_SMART_CARDS
  NS_IMETHOD LaunchSmartCardThread(SECMODModule* module) override;
  NS_IMETHOD ShutdownSmartCardThread(SECMODModule* module) override;
  void LaunchSmartCardThreads();
  void ShutdownSmartCardThreads();
  nsresult DispatchEventToWindow(nsIDOMWindow* domWin,
                                 const nsAString& eventType,
                                 const nsAString& token);
#endif

  NS_IMETHOD IsNSSInitialized(bool* initialized) override;

#ifdef DEBUG
  NS_IMETHOD IsCertTestBuiltInRoot(CERTCertificate* cert, bool& result) override;
#endif

  NS_IMETHOD IsCertContentSigningRoot(CERTCertificate* cert, bool& result) override;

#ifdef XP_WIN
  NS_IMETHOD GetEnterpriseRoots(nsIX509CertList** enterpriseRoots) override;
#endif

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

  void LoadLoadableRoots();
  void UnloadLoadableRoots();
  void setValidationOptions(bool isInitialSetting,
                            const mozilla::MutexAutoLock& lock);
  nsresult setEnabledTLSVersions();
  nsresult InitializePIPNSSBundle();
  nsresult ConfigureInternalPKCS11Token();
  nsresult RegisterObservers();

  void DoProfileBeforeChange();

  void MaybeEnableFamilySafetyCompatibility();
  void MaybeImportEnterpriseRoots();
#ifdef XP_WIN
  nsresult MaybeImportFamilySafetyRoot(PCCERT_CONTEXT certificate,
                                       bool& wasFamilySafetyRoot);
  nsresult LoadFamilySafetyRoot();
  void UnloadFamilySafetyRoot();

  void UnloadEnterpriseRoots();

  mozilla::UniqueCERTCertificate mFamilySafetyRoot;
  mozilla::UniqueCERTCertList mEnterpriseRoots;
#endif // XP_WIN

  mozilla::Mutex mutex;

  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  nsCOMPtr<nsIStringBundle> mNSSErrorsBundle;
  bool mNSSInitialized;
  static int mInstanceCount;
#ifndef MOZ_NO_SMART_CARDS
  SmartCardThreadList* mThreadList;
#endif

#ifdef DEBUG
  nsAutoString mTestBuiltInRootHash;
#endif
  nsString mContentSigningRootHash;

  nsNSSHttpInterface mHttpForNSS;
  RefPtr<mozilla::psm::SharedCertVerifier> mDefaultCertVerifier;

  static PRStatus IdentityInfoInit(void);
};

class nsNSSErrors
{
public:
  static const char* getDefaultErrorStringName(PRErrorCode err);
  static const char* getOverrideErrorStringName(PRErrorCode aErrorCode);
  static nsresult getErrorMessageFromCode(PRErrorCode err,
                                          nsINSSComponent* component,
                                          nsString& returnedMessage);
};

#endif // _nsNSSComponent_h_
