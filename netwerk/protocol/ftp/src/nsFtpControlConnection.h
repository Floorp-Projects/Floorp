/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 */

#ifndef nsFtpControlConnection_h___
#define nsFtpControlConnection_h___

#include "nsCOMPtr.h"

#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsIRequest.h"
#include "nsITransport.h"
#include "nsString.h"
#include "nsIOutputStream.h"
#include "nsAutoLock.h"

class nsIProxyInfo;

class nsFtpControlConnection  : public nsIStreamListener
{
public:
	NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

	nsFtpControlConnection(const char* host, PRUint32 port);
	virtual ~nsFtpControlConnection();
    
    nsresult Connect(nsIProxyInfo* proxyInfo);
    nsresult Disconnect(nsresult status);
    nsresult Write(nsCString& command, PRBool suspend);
    
    void     GetReadRequest(nsIRequest** request) { NS_IF_ADDREF(*request=mReadRequest); }
    void     GetWriteRequest(nsIRequest** request) { NS_IF_ADDREF(*request=mWriteRequest); }

    PRBool   IsAlive();
    
    nsresult GetTransport(nsITransport** controlTransport);
    nsresult SetStreamListener(nsIStreamListener *aListener);

    PRUint32         mServerType;           // what kind of server is it.
    nsString         mPassword;
    PRInt32          mSuspendedWrite;

private:
	PRLock* mLock;  // protects mListener.

    
    nsXPIDLCString   mHost;
    PRUint32         mPort;

    nsCOMPtr<nsIRequest>        mReadRequest;
    nsCOMPtr<nsIRequest>        mWriteRequest;
    nsCOMPtr<nsITransport>      mCPipe;
    nsCOMPtr<nsIOutputStream>   mOutStream;
    nsCOMPtr<nsIStreamListener> mListener;
    PRPackedBool                mWriteSuspened;
};


#endif
