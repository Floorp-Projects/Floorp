/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et ts=4 sts=4 sw=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/Attributes.h"

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

class nsFtpControlConnection MOZ_FINAL : public nsIInputStreamCallback
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
