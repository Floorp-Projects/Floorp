/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsHTTPResponseListener_h_
#define _nsHTTPResponseListener_h_


#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsHTTPChunkConv.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsXPIDLString.h" 
#include "nsHTTPHandler.h"
#include "nsHTTPRequest.h"

#include "nsISupportsPrimitives.h"

class nsIBufferInputStream;
class nsHTTPResponse;
class nsHTTPChannel;

/* 
    The nsHTTPResponseListener class is the response reader listener that 
    receives notifications of OnStartRequest, OnDataAvailable and 
    OnStopRequest as the data is received from the server. Each instance 
    of this class is tied to the corresponding transport that it reads the
    response data stream from. 

  
    The essential purpose of this class is to create the actual response 
    based on the data that is coming off the net. 

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 04/29/99
*/
class nsHTTPResponseListener : public nsIStreamListener
{
public:
    nsHTTPResponseListener (nsHTTPChannel* aConnection, nsHTTPHandler *handler);
    virtual ~nsHTTPResponseListener();

    // nsISupport methods...
    NS_DECL_ISUPPORTS


    // abstract methods implemented by the various ResponseListener
    // subclasses...
    virtual nsresult FireSingleOnData(nsIStreamListener *aListener, 
                                      nsISupports *aContext) = 0;
    virtual nsresult Abort() = 0;

    void SetListener(nsIStreamListener *aListener);

protected:
    nsCOMPtr<nsIStreamListener> mResponseDataListener;
    nsHTTPChannel*              mChannel;
    nsHTTPHandler*              mHandler;
};


/*
 * This class pocesses responses from HTTP servers...
 */
class nsHTTPServerListener : public nsHTTPResponseListener
{

public:

    nsHTTPServerListener (nsHTTPChannel* aConnection, nsHTTPHandler *handler, nsHTTPPipelinedRequest * request, PRBool aDoingProxySSLConnect);
    virtual ~nsHTTPServerListener();

    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

    virtual nsresult FireSingleOnData(nsIStreamListener *aListener, 
                                      nsISupports *aContext);
    virtual nsresult Abort();

    nsresult Discard304Response(void);
    nsresult FinishedResponseHeaders ();

protected:
    // nsHTTPResponseListener methods...
    nsresult FireOnHeadersAvailable();

    nsresult ParseStatusLine(nsIInputStream* in, PRUint32 aLength,
                             PRUint32 *aBytesRead);

    nsresult ParseHTTPHeader(nsIInputStream* in, PRUint32 aLength, 
                             PRUint32* aBytesRead);

protected:
    nsCString                   mHeaderBuffer;
    nsHTTPResponse*             mResponse;
    PRBool                      mFirstLineParsed;
    PRBool                      mHeadersDone;
    PRBool                      mSimpleResponse;

    nsCOMPtr<nsIInputStream>    mDataStream;
    PRUint32                    mBytesReceived; 
    PRInt32                     mBodyBytesReceived;

    PRBool                      mCompressHeaderChecked;
    PRBool                      mChunkHeaderChecked;
    PRBool                      mDataReceived;
    nsCOMPtr<nsISupportsVoid>   mChunkHeaderEOF;
    nsHTTPPipelinedRequest*     mPipelinedRequest;

    nsHTTPChunkConvContext      mChunkHeaderCtx;
    PRBool                      mDoingProxySSLConnect;
};


/*
 * This class processes responses from the cache...
 */
class nsHTTPCacheListener : public nsHTTPResponseListener
{
public:
  nsHTTPCacheListener(nsHTTPChannel* aChannel, nsHTTPHandler *handler);
  virtual ~nsHTTPCacheListener();

  // nsIStreamObserver methods...
  NS_DECL_NSISTREAMOBSERVER

  // nsIStreamListener methods...
  NS_DECL_NSISTREAMLISTENER

  virtual nsresult FireSingleOnData(nsIStreamListener *aListener, 
                                    nsISupports *aContext);
  virtual nsresult Abort();
protected:
  PRInt32                     mBodyBytesReceived;
  PRInt32                     mContentLength;
};

/*
 * This is a final listener which enforces OnStart/OnStop/etc. policies
 */
class nsHTTPFinalListener : public nsIStreamListener
{
public:
    nsHTTPFinalListener (nsHTTPChannel* aChannel, 
                nsIStreamListener *aListener, nsISupports* aContext);

    virtual ~nsHTTPFinalListener();

    // nsISupport methods...
    NS_DECL_ISUPPORTS

    // nsIStreamObserver methods...
    NS_DECL_NSISTREAMOBSERVER

    // nsIStreamListener methods...
    NS_DECL_NSISTREAMLISTENER

    void FireNotifications ();
    void Shutdown ();

    nsIStreamListener* GetListener ()
    {
        return mListener;
    }

private:
    nsHTTPChannel*              mChannel;
    nsCOMPtr<nsISupports>       mContext;
    nsCOMPtr<nsIStreamListener> mListener;

    PRBool  mOnStartFired;
    PRBool  mOnStopFired;
    PRBool  mShutdown;
    PRBool  mBusy;
    PRBool  mOnStopPending;
};


#endif /* _nsHTTPResponseListener_h_ */
