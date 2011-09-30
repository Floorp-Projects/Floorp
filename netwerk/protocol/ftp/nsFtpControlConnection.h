/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et ts=4 sts=4 sw=4 cin: */
/* ***** BEGIN LICENSE BLOCK *****
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

#ifndef nsFtpControlConnection_h___
#define nsFtpControlConnection_h___

#include "nsCOMPtr.h"

#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsIRequest.h"
#include "nsISocketTransport.h"
#include "nsIOutputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class nsIProxyInfo;
class nsITransportEventSink;

class nsFtpControlConnectionListener : public nsISupports {
public:
    /**
     * Called when a chunk of data arrives on the control connection.
     * @param data
     *        The new data or null if an error occurred.
     * @param dataLen
     *        The data length in bytes.
     */
    virtual void OnControlDataAvailable(const char *data, PRUint32 dataLen) = 0;

    /**
     * Called when an error occurs on the control connection.
     * @param status
     *        A failure code providing more info about the error.
     */
    virtual void OnControlError(nsresult status) = 0;
};

class nsFtpControlConnection : public nsIInputStreamCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAMCALLBACK

    nsFtpControlConnection(const nsCSubstring& host, PRUint32 port);
    ~nsFtpControlConnection();

    nsresult Connect(nsIProxyInfo* proxyInfo, nsITransportEventSink* eventSink);
    nsresult Disconnect(nsresult status);
    nsresult Write(const nsCSubstring& command);

    bool IsAlive();

    nsITransport *Transport()   { return mSocket; }

    /**
     * Call this function to be notified asynchronously when there is data
     * available for the socket.  The listener passed to this method replaces
     * any existing listener, and the listener can be null to disconnect the
     * previous listener.
     */
    nsresult WaitData(nsFtpControlConnectionListener *listener);

    PRUint32         mServerType;           // what kind of server is it.
    nsString         mPassword;
    PRInt32          mSuspendedWrite;
    nsCString        mPwd;
    PRUint32         mSessionId;

private:
    nsCString mHost;
    PRUint32  mPort;

    nsCOMPtr<nsISocketTransport>     mSocket;
    nsCOMPtr<nsIOutputStream>        mSocketOutput;
    nsCOMPtr<nsIAsyncInputStream>    mSocketInput;

    nsRefPtr<nsFtpControlConnectionListener> mListener;
};

#endif
