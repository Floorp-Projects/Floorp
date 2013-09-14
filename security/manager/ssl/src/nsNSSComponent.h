/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISignatureVerifier.h"
#include "nsIEntropyCollector.h"
#include "nsIStringBundle.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#ifndef MOZ_DISABLE_CRYPTOLEGACY
#endif
#include "nsINSSErrorsService.h"
#include "nsNSSCallbacks.h"
#include "ScopedNSSTypes.h"
#include "nsNSSHelper.h"
#include "nsClientAuthRemember.h"
#include "prerror.h"

class nsIDOMWindow;
class nsIPrompt;
class SmartCardThreadList;

namespace mozilla { namespace psm {

class CertVerifier;

} } // namespace mozilla::psm


#define NS_NSSCOMPONENT_CID \
{0xa277189c, 0x1dd1, 0x11b2, {0xa8, 0xc9, 0xe4, 0xe8, 0xbf, 0xb1, 0x33, 0x8e}}

#define PSM_COMPONENT_CONTRACTID "@mozilla.org/psm;1"

//Define an interface that we can use to look up from the
//callbacks passed to NSS.

#define NS_INSSCOMPONENT_IID_STR "6ffbb526-205b-49c5-ae3f-5959c084075e"
#define NS_INSSCOMPONENT_IID \
  { 0x6ffbb526, 0x205b, 0x49c5, \
    { 0xae, 0x3f, 0x59, 0x59, 0xc0, 0x84, 0x7, 0x5e } }

enum EnsureNSSOperator
{
  nssLoadingComponent = 0,
  nssInitSucceeded = 1,
  nssInitFailed = 2,
  nssShutdown = 3,
  nssEnsure = 100,
  nssEnsureOnChromeOnly = 101
};

extern bool EnsureNSSInitialized(EnsureNSSOperator op);

class nsNSSComponent;

class NS_NO_VTABLE nsINSSComponent : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INSSCOMPONENT_IID)

  NS_IMETHOD ShowAlertFromStringBundle(const char * messageID) = 0;

  NS_IMETHOD GetPIPNSSBundleString(const char *name,
                                   nsAString &outString) = 0;
  NS_IMETHOD PIPBundleFormatStringFromName(const char *name,
                                           const PRUnichar **params,
                                           uint32_t numParams,
                                           nsAString &outString) = 0;

  NS_IMETHOD GetNSSBundleString(const char *name,
                                nsAString &outString) = 0;
  NS_IMETHOD NSSBundleFormatStringFromName(const char *name,
                                           const PRUnichar **params,
                                           uint32_t numParams,
                                           nsAString &outString) = 0;

  // This method will just disable OCSP in NSS, it will not
  // alter the respective pref values.
  NS_IMETHOD SkipOcsp() = 0;

  // This method will set the OCSP value according to the 
  // values in the preferences.
  NS_IMETHOD SkipOcspOff() = 0;

  NS_IMETHOD LogoutAuthenticatedPK11() = 0;

#ifndef MOZ_DISABLE_CRYPTOLEGACY
  NS_IMETHOD LaunchSmartCardThread(SECMODModule *module) = 0;

  NS_IMETHOD ShutdownSmartCardThread(SECMODModule *module) = 0;

  NS_IMETHOD PostEvent(const nsAString &eventType, const nsAString &token) = 0;

  NS_IMETHOD DispatchEvent(const nsAString &eventType, const nsAString &token) = 0;
#endif

#ifndef NSS_NO_LIBPKIX  
  NS_IMETHOD EnsureIdentityInfoLoaded() = 0;
#endif

  NS_IMETHOD IsNSSInitialized(bool *initialized) = 0;

  NS_IMETHOD GetDefaultCertVerifier(
                  mozilla::RefPtr<mozilla::psm::CertVerifier> &out) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINSSComponent, NS_INSSCOMPONENT_IID)

class nsNSSShutDownList;
class nsCertVerificationThread;

// Implementation of the PSM component interface.
class nsNSSComponent : public nsISignatureVerifier,
                       public nsIEntropyCollector,
                       public nsINSSComponent,
                       public nsIObserver,
                       public nsSupportsWeakReference
{
  typedef mozilla::Mutex Mutex;

public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_NSSCOMPONENT_CID )

  nsNSSComponent();
  virtual ~nsNSSComponent();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISIGNATUREVERIFIER
  NS_DECL_NSIENTROPYCOLLECTOR
  NS_DECL_NSIOBSERVER

  NS_METHOD Init();

  static nsresult GetNewPrompter(nsIPrompt ** result);
  static nsresult ShowAlertWithConstructedString(const nsString & message);
  NS_IMETHOD ShowAlertFromStringBundle(const char * messageID);

  NS_IMETHOD GetPIPNSSBundleString(const char *name,
                                   nsAString &outString);
  NS_IMETHOD PIPBundleFormatStringFromName(const char *name,
                                           const PRUnichar **params,
                                           uint32_t numParams,
                                           nsAString &outString);
  NS_IMETHOD GetNSSBundleString(const char *name,
                               nsAString &outString);
  NS_IMETHOD NSSBundleFormatStringFromName(const char *name,
                                           const PRUnichar **params,
                                           uint32_t numParams,
                                           nsAString &outString);
  NS_IMETHOD SkipOcsp();
  NS_IMETHOD SkipOcspOff();
  NS_IMETHOD LogoutAuthenticatedPK11();

#ifndef MOZ_DISABLE_CRYPTOLEGACY
  NS_IMETHOD LaunchSmartCardThread(SECMODModule *module);
  NS_IMETHOD ShutdownSmartCardThread(SECMODModule *module);
  NS_IMETHOD PostEvent(const nsAString &eventType, const nsAString &token);
  NS_IMETHOD DispatchEvent(const nsAString &eventType, const nsAString &token);
  void LaunchSmartCardThreads();
  void ShutdownSmartCardThreads();
  nsresult DispatchEventToWindow(nsIDOMWindow *domWin, const nsAString &eventType, const nsAString &token);
#endif

#ifndef NSS_NO_LIBPKIX
  NS_IMETHOD EnsureIdentityInfoLoaded();
#endif
  NS_IMETHOD IsNSSInitialized(bool *initialized);

  NS_IMETHOD GetDefaultCertVerifier(
                  mozilla::RefPtr<mozilla::psm::CertVerifier> &out);
private:

  nsresult InitializeNSS(bool showWarningBox);
  void ShutdownNSS();

  void InstallLoadableRoots();
  void UnloadLoadableRoots();
  void CleanupIdentityInfo();
  void setValidationOptions();
  nsresult setEnabledTLSVersions();
  nsresult InitializePIPNSSBundle();
  nsresult ConfigureInternalPKCS11Token();
  nsresult RegisterObservers();
  nsresult DeregisterObservers();

  // Methods that we use to handle the profile change notifications (and to
  // synthesize a full profile change when we're just doing a profile startup):
  void DoProfileChangeNetTeardown();
  void DoProfileChangeTeardown(nsISupports* aSubject);
  void DoProfileBeforeChange(nsISupports* aSubject);
  void DoProfileChangeNetRestore();
  
  Mutex mutex;
  
  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  nsCOMPtr<nsIStringBundle> mNSSErrorsBundle;
  bool mNSSInitialized;
  bool mObserversRegistered;
  static int mInstanceCount;
  nsNSSShutDownList *mShutdownObjectList;
#ifndef MOZ_DISABLE_CRYPTOLEGACY
  SmartCardThreadList *mThreadList;
#endif
  bool mIsNetworkDown;

  void deleteBackgroundThreads();
  void createBackgroundThreads();
  nsCertVerificationThread *mCertVerificationThread;

  nsNSSHttpInterface mHttpForNSS;
  mozilla::RefPtr<mozilla::psm::CertVerifier> mDefaultCertVerifier;


  static PRStatus IdentityInfoInit(void);
  PRCallOnceType mIdentityInfoCallOnce;

public:
  static bool globalConstFlagUsePKIXVerification;
};

class nsNSSErrors
{
public:
  static const char *getDefaultErrorStringName(PRErrorCode err);
  static const char *getOverrideErrorStringName(PRErrorCode aErrorCode);
  static nsresult getErrorMessageFromCode(PRErrorCode err,
                                          nsINSSComponent *component,
                                          nsString &returnedMessage);
};

class nsPSMInitPanic
{
private:
  static bool isPanic;
public:
  static void SetPanic() {isPanic = true;}
  static bool GetPanic() {return isPanic;}
};

#endif // _nsNSSComponent_h_

