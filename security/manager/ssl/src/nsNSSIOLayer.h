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
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsITransportSecurityInfo.h"
#include "nsISSLSocketControl.h"
#include "nsISSLStatus.h"
#include "nsXPIDLString.h"

class nsIChannel;

class nsNSSSocketInfo : public nsITransportSecurityInfo,
                        public nsISSLSocketControl,
                        public nsIInterfaceRequestor,
                        public nsISSLStatusProvider
{
public:
  nsNSSSocketInfo();
  virtual ~nsNSSSocketInfo();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO
  NS_DECL_NSISSLSOCKETCONTROL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISSLSTATUSPROVIDER

  nsresult SetSecurityState(PRInt32 aState);
  nsresult SetShortSecurityDescription(const PRUnichar *aText);

  nsresult SetForTLSStepUp(PRBool useTLS);
  nsresult GetForTLSStepUp(PRBool *useTLS);

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  nsresult GetFirstWrite(PRBool *aFirstWrite);
  nsresult SetFirstWrite(PRBool aFirstWrite);

  nsresult GetHostName(char **aHostName);
  nsresult SetHostName(const char *aHostName);

  nsresult GetPort(PRInt32 *aPort);
  nsresult SetPort(PRInt32 aPort);

  nsresult GetProxyHost(char **aProxyHost);
  nsresult SetProxyHost(const char *aProxyHost);

  nsresult GetProxyPort(PRInt32 *aProxyPort);
  nsresult SetProxyPort(PRInt32 aProxyPort);

  nsresult GetNetAddr(PRNetAddr *aNetAddr);
  nsresult SetNetAddr(const PRNetAddr *aNetAddr);

  nsresult GetOldBlockVal(PRBool *aOldBlockVal);
  nsresult SetOldBlockVal(PRBool aOldBlockVal);

  /* Set SSL Status values */
  nsresult SetSSLStatus(nsISSLStatus *aSSLStatus);  

protected:
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  PRFileDesc* mFd;
  PRInt32 mSecurityState;
  nsString mShortDesc;
  PRBool mForceHandshake;
  PRBool mForTLSStepUp;
  PRBool mFirstWrite;
  nsXPIDLCString mHostName;
  nsXPIDLCString mProxyHostName;
  PRInt32 mPort, mProxyPort;
  PRNetAddr mNetAddr;
  PRBool mOldBlockVal;

  /* SSL Status */
  nsCOMPtr<nsISSLStatus> mSSLStatus;
};

nsresult nsSSLIOLayerNewSocket(const char *host,
                               PRInt32 port,
                               const char *proxyHost,
                               PRInt32 proxyPort,
                               PRFileDesc **fd,
                               nsISupports **securityInfo,
                               PRBool forTLSStepUp);

nsresult nsSSLIOLayerAddToSocket(const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc *fd,
                                 nsISupports **securityInfo,
                                 PRBool forTLSStepUp);
#endif /* _NSNSSIOLAYER_H */
