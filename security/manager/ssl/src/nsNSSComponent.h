/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Brian Ryner <bryner@brianryner.com>
 *   Kai Engert <kaie@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "nsCOMPtr.h"
#include "nsISignatureVerifier.h"
#include "nsIURIContentListener.h"
#include "nsIStreamListener.h"
#include "nsIEntropyCollector.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsIDOMEventTarget.h"
#include "nsIPrefBranch.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsIScriptSecurityManager.h"
#include "nsSmartCardMonitor.h"
#include "nsINSSErrorsService.h"
#include "nsITimer.h"
#include "nsNetUtil.h"
#include "nsHashtable.h"
#include "prlock.h"
#include "nsICryptoHash.h"
#include "hasht.h"
#include "nsNSSCallbacks.h"

#include "nsNSSHelper.h"

#define NS_NSSCOMPONENT_CID \
{0xa277189c, 0x1dd1, 0x11b2, {0xa8, 0xc9, 0xe4, 0xe8, 0xbf, 0xb1, 0x33, 0x8e}}

#define PSM_COMPONENT_CONTRACTID "@mozilla.org/psm;1"
#define PSM_COMPONENT_CLASSNAME "Mozilla PSM Component"

//Define an interface that we can use to look up from the
//callbacks passed to NSS.

#define NS_INSSCOMPONENT_IID_STR "d4b49dd6-1dd1-11b2-b6fe-b14cfaf69cbd"
#define NS_INSSCOMPONENT_IID \
  {0xd4b49dd6, 0x1dd1, 0x11b2, \
    { 0xb6, 0xfe, 0xb1, 0x4c, 0xfa, 0xf6, 0x9c, 0xbd }}

#define NS_PSMCONTENTLISTEN_CID {0xc94f4a30, 0x64d7, 0x11d4, {0x99, 0x60, 0x00, 0xb0, 0xd0, 0x23, 0x54, 0xa0}}
#define NS_PSMCONTENTLISTEN_CONTRACTID "@mozilla.org/security/psmdownload;1"

#define NS_CRYPTO_HASH_CLASSNAME "Mozilla Cryto Hash Function Component"
#define NS_CRYPTO_HASH_CID {0x36a1d3b3, 0xd886, 0x4317, {0x96, 0xff, 0x87, 0xb0, 0x00, 0x5c, 0xfe, 0xf7}}

//--------------------------------------------
// Now we need a content listener to register 
//--------------------------------------------
class PSMContentDownloader : public nsIStreamListener
{
public:
  PSMContentDownloader() {NS_ASSERTION(PR_FALSE, "don't use this constructor."); }
  PSMContentDownloader(PRUint32 type);
  virtual ~PSMContentDownloader();
  void setSilentDownload(PRBool flag);
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
  PRInt32 mBufferOffset;
  PRInt32 mBufferSize;
  PRUint32 mType;
  PRBool mDoSilentDownload;
  nsAutoString mCrlAutoDownloadKey;
  nsCOMPtr<nsIURI> mURI;
  nsresult handleContentDownloadError(nsresult errCode);
};

class NS_NO_VTABLE nsINSSComponent : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INSSCOMPONENT_IID)

  NS_IMETHOD GetPIPNSSBundleString(const char *name,
                                   nsAString &outString) = 0;
  NS_IMETHOD PIPBundleFormatStringFromName(const char *name,
                                           const PRUnichar **params,
                                           PRUint32 numParams,
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

  NS_IMETHOD LaunchSmartCardThread(SECMODModule *module) = 0;

  NS_IMETHOD ShutdownSmartCardThread(SECMODModule *module) = 0;

  NS_IMETHOD PostEvent(const nsAString &eventType, const nsAString &token) = 0;

  NS_IMETHOD DispatchEvent(const nsAString &eventType, const nsAString &token) = 0;
  
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINSSComponent, NS_INSSCOMPONENT_IID)

class nsCryptoHash : public nsICryptoHash
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRYPTOHASH

  nsCryptoHash();

private:
  ~nsCryptoHash();
  HASHContext* mHashContext;
};

struct PRLock;
class nsNSSShutDownList;
class nsSSLThread;
class nsCertVerificationThread;

// Implementation of the PSM component interface.
class nsNSSComponent : public nsISignatureVerifier,
                       public nsIEntropyCollector,
                       public nsINSSComponent,
                       public nsIObserver,
                       public nsSupportsWeakReference,
                       public nsITimerCallback,
                       public nsINSSErrorsService
{
public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_NSSCOMPONENT_CID )

  nsNSSComponent();
  virtual ~nsNSSComponent();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIGNATUREVERIFIER
  NS_DECL_NSIENTROPYCOLLECTOR
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINSSERRORSSERVICE

  NS_METHOD Init();

  NS_IMETHOD GetPIPNSSBundleString(const char *name,
                                   nsAString &outString);
  NS_IMETHOD PIPBundleFormatStringFromName(const char *name,
                                           const PRUnichar **params,
                                           PRUint32 numParams,
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
  static nsresult GetNSSCipherIDFromPrefString(const nsACString &aPrefString, PRUint16 &aCipherId);

  NS_IMETHOD LaunchSmartCardThread(SECMODModule *module);
  NS_IMETHOD ShutdownSmartCardThread(SECMODModule *module);
  NS_IMETHOD PostEvent(const nsAString &eventType, const nsAString &token);
  NS_IMETHOD DispatchEvent(const nsAString &eventType, const nsAString &token);

private:

  nsresult InitializeNSS(PRBool showWarningBox);
  nsresult ShutdownNSS();

#ifdef XP_MACOSX
  void TryCFM2MachOMigration(nsIFile *cfmPath, nsIFile *machoPath);
#endif
  
  enum AlertIdentifier {
    ai_nss_init_problem, 
    ai_sockets_still_active, 
    ai_crypto_ui_active,
    ai_incomplete_logout
  };
  
  void ShowAlert(AlertIdentifier ai);
  void InstallLoadableRoots();
  void UnloadLoadableRoots();
  void LaunchSmartCardThreads();
  void ShutdownSmartCardThreads();
  nsresult InitializePIPNSSBundle();
  nsresult ConfigureInternalPKCS11Token();
  nsresult RegisterPSMContentListener();
  nsresult RegisterObservers();
  nsresult DownloadCrlSilently();
  nsresult PostCRLImportEvent(const nsCSubstring &urlString, nsIStreamListener *psmDownloader);
  nsresult getParamsForNextCrlToDownload(nsAutoString *url, PRTime *time, nsAutoString *key);
  nsresult DispatchEventToWindow(nsIDOMWindow *domWin, const nsAString &eventType, const nsAString &token);
  PRLock *mutex;
  
  nsCOMPtr<nsIScriptSecurityManager> mScriptSecurityManager;
  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  nsCOMPtr<nsIURIContentListener> mPSMContentListener;
  nsCOMPtr<nsIPrefBranch> mPrefBranch;
  nsCOMPtr<nsITimer> mTimer;
  PRBool mNSSInitialized;
  PRBool mObserversRegistered;
  PLHashTable *hashTableCerts;
  nsAutoString mDownloadURL;
  nsAutoString mCrlUpdateKey;
  PRLock *mCrlTimerLock;
  nsHashtable *crlsScheduledForDownload;
  PRBool crlDownloadTimerOn;
  PRBool mUpdateTimerInitialized;
  static int mInstanceCount;
  nsNSSShutDownList *mShutdownObjectList;
  SmartCardThreadList *mThreadList;
  PRBool mIsNetworkDown;
  nsSSLThread *mSSLThread;
  nsCertVerificationThread *mCertVerificationThread;
  nsNSSHttpInterface mHttpForNSS;
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
  static const char *getDefaultErrorStringName(PRInt32 err);
  static const char *getOverrideErrorStringName(PRInt32 aErrorCode);
  static nsresult getErrorMessageFromCode(PRInt32 err,
                                          nsINSSComponent *component,
                                          nsString &returnedMessage);
};

#endif // _nsNSSComponent_h_

