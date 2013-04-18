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
#include "nsIURIContentListener.h"
#include "nsIStreamListener.h"
#include "nsIEntropyCollector.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsIPrefBranch.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsIScriptSecurityManager.h"
#ifndef MOZ_DISABLE_CRYPTOLEGACY
#include "nsIDOMEventTarget.h"
#include "nsSmartCardMonitor.h"
#endif
#include "nsINSSErrorsService.h"
#include "nsITimer.h"
#include "nsNetUtil.h"
#include "nsHashtable.h"
#include "nsICryptoHash.h"
#include "nsICryptoHMAC.h"
#include "hasht.h"
#include "nsNSSCallbacks.h"
#include "nsNSSShutDown.h"

#include "nsNSSHelper.h"
#include "nsClientAuthRemember.h"

#ifndef NSS_NO_LIBPKIX
#include "nsCERTValInParamWrapper.h"
#endif

#define NS_NSSCOMPONENT_CID \
{0xa277189c, 0x1dd1, 0x11b2, {0xa8, 0xc9, 0xe4, 0xe8, 0xbf, 0xb1, 0x33, 0x8e}}

#define PSM_COMPONENT_CONTRACTID "@mozilla.org/psm;1"

//Define an interface that we can use to look up from the
//callbacks passed to NSS.

#define NS_INSSCOMPONENT_IID_STR "6ffbb526-205b-49c5-ae3f-5959c084075e"
#define NS_INSSCOMPONENT_IID \
  { 0x6ffbb526, 0x205b, 0x49c5, \
    { 0xae, 0x3f, 0x59, 0x59, 0xc0, 0x84, 0x7, 0x5e } }

#define NS_PSMCONTENTLISTEN_CID {0xc94f4a30, 0x64d7, 0x11d4, {0x99, 0x60, 0x00, 0xb0, 0xd0, 0x23, 0x54, 0xa0}}
#define NS_PSMCONTENTLISTEN_CONTRACTID "@mozilla.org/security/psmdownload;1"
#define NS_CRYPTO_HASH_CID {0x36a1d3b3, 0xd886, 0x4317, {0x96, 0xff, 0x87, 0xb0, 0x00, 0x5c, 0xfe, 0xf7}}
#define NS_CRYPTO_HMAC_CID {0xa496d0a2, 0xdff7, 0x4e23, {0xbd, 0x65, 0x1c, 0xa7, 0x42, 0xfa, 0x17, 0x8a}}

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

//--------------------------------------------
// Now we need a content listener to register 
//--------------------------------------------
class PSMContentDownloader : public nsIStreamListener
{
public:
  PSMContentDownloader() {NS_ASSERTION(false, "don't use this constructor."); }
  PSMContentDownloader(uint32_t type);
  virtual ~PSMContentDownloader();
  void setSilentDownload(bool flag);
  void setCrlAutodownloadKey(nsAutoString key);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  enum {UNKNOWN_TYPE = 0};
  enum {X509_CA_CERT  = 1};
  enum {X509_USER_CERT  = 2};
  enum {X509_EMAIL_CERT  = 3};
  enum {X509_SERVER_CERT  = 4};
  enum {PKCS7_CRL = 5};

protected:
  char* mByteData;
  int32_t mBufferOffset;
  int32_t mBufferSize;
  uint32_t mType;
  bool mDoSilentDownload;
  nsString mCrlAutoDownloadKey;
  nsCOMPtr<nsIURI> mURI;
  nsresult handleContentDownloadError(nsresult errCode);
};

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

  NS_IMETHOD RememberCert(CERTCertificate *cert) = 0;

  NS_IMETHOD RemoveCrlFromList(nsAutoString) = 0;

  NS_IMETHOD DefineNextTimer() = 0;

  NS_IMETHOD DownloadCRLDirectly(nsAutoString, nsAutoString) = 0;
  
  NS_IMETHOD LogoutAuthenticatedPK11() = 0;

#ifndef MOZ_DISABLE_CRYPTOLEGACY
  NS_IMETHOD LaunchSmartCardThread(SECMODModule *module) = 0;

  NS_IMETHOD ShutdownSmartCardThread(SECMODModule *module) = 0;

  NS_IMETHOD PostEvent(const nsAString &eventType, const nsAString &token) = 0;

  NS_IMETHOD DispatchEvent(const nsAString &eventType, const nsAString &token) = 0;
#endif

  NS_IMETHOD IsNSSInitialized(bool *initialized) = 0;

#ifndef NSS_NO_LIBPKIX
  NS_IMETHOD EnsureIdentityInfoLoaded() = 0;
  NS_IMETHOD GetDefaultCERTValInParam(
                  mozilla::RefPtr<nsCERTValInParamWrapper> &out) = 0;
  NS_IMETHOD GetDefaultCERTValInParamLocalOnly(
                  mozilla::RefPtr<nsCERTValInParamWrapper> &out) = 0;
#endif

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINSSComponent, NS_INSSCOMPONENT_IID)

class nsCryptoHash : public nsICryptoHash, public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRYPTOHASH

  nsCryptoHash();

private:
  ~nsCryptoHash();

  HASHContext* mHashContext;
  bool mInitialized;

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class nsCryptoHMAC : public nsICryptoHMAC, public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRYPTOHMAC

  nsCryptoHMAC();

private:
  ~nsCryptoHMAC();
  PK11Context* mHMACContext;

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class nsNSSShutDownList;
class nsCertVerificationThread;

// Implementation of the PSM component interface.
class nsNSSComponent : public nsISignatureVerifier,
                       public nsIEntropyCollector,
                       public nsINSSComponent,
                       public nsIObserver,
                       public nsSupportsWeakReference,
                       public nsITimerCallback
{
  typedef mozilla::Mutex Mutex;

public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_NSSCOMPONENT_CID )

  nsNSSComponent();
  virtual ~nsNSSComponent();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIGNATUREVERIFIER
  NS_DECL_NSIENTROPYCOLLECTOR
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

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
  nsresult InitializeCRLUpdateTimer();
  nsresult StopCRLUpdateTimer();
  NS_IMETHOD RemoveCrlFromList(nsAutoString);
  NS_IMETHOD DefineNextTimer();
  NS_IMETHOD LogoutAuthenticatedPK11();
  NS_IMETHOD DownloadCRLDirectly(nsAutoString, nsAutoString);
  NS_IMETHOD RememberCert(CERTCertificate *cert);

#ifndef MOZ_DISABLE_CRYPTOLEGACY
  NS_IMETHOD LaunchSmartCardThread(SECMODModule *module);
  NS_IMETHOD ShutdownSmartCardThread(SECMODModule *module);
  NS_IMETHOD PostEvent(const nsAString &eventType, const nsAString &token);
  NS_IMETHOD DispatchEvent(const nsAString &eventType, const nsAString &token);
  void LaunchSmartCardThreads();
  void ShutdownSmartCardThreads();
  nsresult DispatchEventToWindow(nsIDOMWindow *domWin, const nsAString &eventType, const nsAString &token);
#endif

  NS_IMETHOD IsNSSInitialized(bool *initialized);

private:

  nsresult InitializeNSS(bool showWarningBox);
  void ShutdownNSS();

#ifdef XP_MACOSX
  void TryCFM2MachOMigration(nsIFile *cfmPath, nsIFile *machoPath);
#endif
  
  void InstallLoadableRoots();
  void UnloadLoadableRoots();
  void setValidationOptions(nsIPrefBranch * pref);
  nsresult setEnabledTLSVersions(nsIPrefBranch * pref);
  nsresult InitializePIPNSSBundle();
  nsresult ConfigureInternalPKCS11Token();
  nsresult RegisterPSMContentListener();
  nsresult RegisterObservers();
  nsresult DeregisterObservers();
  nsresult DownloadCrlSilently();
  nsresult PostCRLImportEvent(const nsCSubstring &urlString, nsIStreamListener *psmDownloader);
  nsresult getParamsForNextCrlToDownload(nsAutoString *url, PRTime *time, nsAutoString *key);

  // Methods that we use to handle the profile change notifications (and to
  // synthesize a full profile change when we're just doing a profile startup):
  void DoProfileChangeNetTeardown();
  void DoProfileChangeTeardown(nsISupports* aSubject);
  void DoProfileBeforeChange(nsISupports* aSubject);
  void DoProfileChangeNetRestore();

#ifndef NSS_NO_LIBPKIX
  NS_IMETHOD EnsureIdentityInfoLoaded();
  static PRStatus IdentityInfoInit(void);
  void CleanupIdentityInfo();
  NS_IMETHOD GetDefaultCERTValInParam(
                  mozilla::RefPtr<nsCERTValInParamWrapper> &out);
  NS_IMETHOD GetDefaultCERTValInParamLocalOnly(
                  mozilla::RefPtr<nsCERTValInParamWrapper> &out);
#endif
  
  Mutex mutex;
  
  nsCOMPtr<nsIScriptSecurityManager> mScriptSecurityManager;
  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  nsCOMPtr<nsIStringBundle> mNSSErrorsBundle;
  nsCOMPtr<nsIURIContentListener> mPSMContentListener;
  nsCOMPtr<nsIPrefBranch> mPrefBranch;
  nsCOMPtr<nsITimer> mTimer;
  bool mNSSInitialized;
  bool mObserversRegistered;
  PLHashTable *hashTableCerts;
  nsAutoString mDownloadURL;
  nsAutoString mCrlUpdateKey;
  Mutex mCrlTimerLock;
  nsHashtable *crlsScheduledForDownload;
  bool crlDownloadTimerOn;
  bool mUpdateTimerInitialized;
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

#ifndef NSS_NO_LIBPKIX
private:
  mozilla::RefPtr<nsCERTValInParamWrapper> mDefaultCERTValInParam;
  mozilla::RefPtr<nsCERTValInParamWrapper> mDefaultCERTValInParamLocalOnly;
  PRCallOnceType mIdentityInfoCallOnce;
public:
  static bool globalConstFlagUsePKIXVerification;
#endif

};

class PSMContentListener : public nsIURIContentListener,
                            public nsSupportsWeakReference {
public:
  PSMContentListener();
  virtual ~PSMContentListener();
  nsresult init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURICONTENTLISTENER
private:
  nsCOMPtr<nsISupports> mLoadCookie;
  nsCOMPtr<nsIURIContentListener> mParentContentListener;
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

