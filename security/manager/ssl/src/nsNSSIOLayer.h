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
 *  Brian Ryner <bryner@netscape.com>
 */

#ifndef _NSNSSIOLAYER_H
#define _NSNSSIOLAYER_H

#include "prtypes.h"
#include "prio.h"
#include "certt.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITransportSecurityInfo.h"
#include "nsISSLSocketControl.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsXPIDLString.h"
#include "nsNSSShutDown.h"

class nsIChannel;

class nsNSSSocketInfo : public nsITransportSecurityInfo,
                        public nsISSLSocketControl,
                        public nsIInterfaceRequestor,
                        public nsISSLStatusProvider,
                        public nsNSSShutDownObject,
                        public nsOnPK11LogoutCancelObject
{
public:
  nsNSSSocketInfo();
  virtual ~nsNSSSocketInfo();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO
  NS_DECL_NSISSLSOCKETCONTROL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISSLSTATUSPROVIDER

  nsresult SetSecurityState(PRUint32 aState);
  nsresult SetShortSecurityDescription(const PRUnichar *aText);

  nsresult SetForSTARTTLS(PRBool aForSTARTTLS);
  nsresult GetForSTARTTLS(PRBool *aForSTARTTLS);

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  nsresult GetHandshakePending(PRBool *aHandshakePending);
  nsresult SetHandshakePending(PRBool aHandshakePending);

  nsresult GetHostName(char **aHostName);
  nsresult SetHostName(const char *aHostName);

  nsresult GetPort(PRInt32 *aPort);
  nsresult SetPort(PRInt32 aPort);

  void SetCanceled(PRBool aCanceled);
  PRBool GetCanceled();
  
  void SetHasCleartextPhase(PRBool aHasCleartextPhase);
  PRBool GetHasCleartextPhase();
  
  void SetHandshakeInProgress(PRBool aIsIn) { mHandshakeInProgress = aIsIn; }
  PRBool GetHandshakeInProgress() { return mHandshakeInProgress; }

  nsresult RememberCAChain(CERTCertList *aCertList);

  /* Set SSL Status values */
  nsresult SetSSLStatus(nsISSLStatus *aSSLStatus);  

protected:
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  PRFileDesc* mFd;
  PRUint32 mSecurityState;
  nsString mShortDesc;
  PRPackedBool mForSTARTTLS;
  PRPackedBool mHandshakePending;
  PRPackedBool mCanceled;
  PRPackedBool mHasCleartextPhase;
  PRPackedBool mHandshakeInProgress;
  PRInt32 mPort;
  nsXPIDLCString mHostName;
  CERTCertList *mCAChain;

  /* SSL Status */
  nsCOMPtr<nsISSLStatus> mSSLStatus;

  nsresult ActivateSSL();
private:
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

nsresult nsSSLIOLayerNewSocket(const char *host,
                               PRInt32 port,
                               const char *proxyHost,
                               PRInt32 proxyPort,
                               PRFileDesc **fd,
                               nsISupports **securityInfo,
                               PRBool forSTARTTLS);

nsresult nsSSLIOLayerAddToSocket(const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc *fd,
                                 nsISupports **securityInfo,
                                 PRBool forSTARTTLS);

nsresult nsSSLIOLayerFreeTLSIntolerantSites();
nsresult displayAlert(nsXPIDLString formattedString, nsNSSSocketInfo *infoObject);
nsresult displayUnknownCertErrorAlert(nsNSSSocketInfo *infoObject, int error);
 
#endif /* _NSNSSIOLAYER_H */
