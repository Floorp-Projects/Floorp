/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "nsCOMPtr.h"
#include "nsISecurityManagerComponent.h"
#include "nsISignatureVerifier.h"
#include "nsIURIContentListener.h"
#include "nsIEntropyCollector.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsIScriptSecurityManager.h"

#include "nsNSSHelper.h"

#define SECURITY_STRING_BUNDLE_URL "chrome://communicator/locale/security.properties"

#define NS_NSSCOMPONENT_CID \
{0xa277189c, 0x1dd1, 0x11b2, {0xa8, 0xc9, 0xe4, 0xe8, 0xbf, 0xb1, 0x33, 0x8e}}

//Define an interface that we can use to look up from the
//callbacks passed to NSS.

#define NS_INSSCOMPONENT_IID_STR "d4b49dd6-1dd1-11b2-b6fe-b14cfaf69cbd"
#define NS_INSSCOMPONENT_IID \
  {0xd4b49dd6, 0x1dd1, 0x11b2, \
    { 0xb6, 0xfe, 0xb1, 0x4c, 0xfa, 0xf6, 0x9c, 0xbd }}

#define NS_PSMCONTENTLISTEN_CID {0xc94f4a30, 0x64d7, 0x11d4, {0x99, 0x60, 0x00, 0xb0, 0xd0, 0x23, 0x54, 0xa0}}
#define NS_PSMCONTENTLISTEN_CONTRACTID "@mozilla.org/security/psmdownload;1"


class NS_NO_VTABLE nsINSSComponent : public nsISupports {
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INSSCOMPONENT_IID)

  NS_IMETHOD GetPIPNSSBundleString(const PRUnichar *name,
                                   nsString &outString) = 0;
  NS_IMETHOD PIPBundleFormatStringFromName(const PRUnichar *name,
                                           const PRUnichar **params,
                                           PRUint32 numParams,
                                           PRUnichar **outString) = 0;

  // This method will just disable OCSP in NSS, it will not
  // alter the respective pref values.
  NS_IMETHOD DisableOCSP() = 0;

  // This method will set the OCSP value according to the 
  // values in the preferences.
  NS_IMETHOD EnableOCSP() = 0;

  NS_IMETHOD RememberCert(CERTCertificate *cert) = 0;
};



// Implementation of the PSM component interface.
class nsNSSComponent : public nsISecurityManagerComponent,
                       public nsISignatureVerifier,
                       public nsIEntropyCollector,
                       public nsINSSComponent,
                       public nsIObserver,
                       public nsSupportsWeakReference
{
public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_NSSCOMPONENT_CID );

  nsNSSComponent();
  virtual ~nsNSSComponent();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISECURITYMANAGERCOMPONENT
  NS_DECL_NSISIGNATUREVERIFIER
  NS_DECL_NSIENTROPYCOLLECTOR
  NS_DECL_NSIOBSERVER

  NS_METHOD Init();

  NS_IMETHOD GetPIPNSSBundleString(const PRUnichar *name,
                                   nsString &outString);
  NS_IMETHOD PIPBundleFormatStringFromName(const PRUnichar *name,
                                           const PRUnichar **params,
                                           PRUint32 numParams,
                                           PRUnichar **outString);
  NS_IMETHOD DisableOCSP();
  NS_IMETHOD EnableOCSP();
  nsresult InitializeNSS();
  NS_IMETHOD RememberCert(CERTCertificate *cert);

private:

  void InstallLoadableRoots();
  nsresult InitializePIPNSSBundle();
  nsresult ConfigureInternalPKCS11Token();
  char * GetPK11String(const PRUnichar *name, PRUint32 len);
  nsresult RegisterPSMContentListener();
  nsresult RegisterProfileChangeObserver();
  static int PR_CALLBACK PrefChangedCallback(const char* aPrefName, void* data);
  void PrefChanged(const char* aPrefName);

  nsCOMPtr<nsIScriptSecurityManager> mScriptSecurityManager;
  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  nsCOMPtr<nsIURIContentListener> mPSMContentListener;
  nsCOMPtr<nsIPref> mPref;
  static PRBool mNSSInitialized;
  PLHashTable *hashTableCerts;
};

//--------------------------------------------
// Now we need a content listener to register 
//--------------------------------------------

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

#endif // _nsNSSComponent_h_

