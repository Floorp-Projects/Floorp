/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsHTTPChannel_h_
#define _nsHTTPChannel_h_

#include "nsIHTTPChannel.h"
#include "nsIChannel.h"
#include "nsHTTPEnums.h"
#include "nsIURI.h"
#include "nsHTTPHandler.h"
#include "nsIEventQueue.h"
#include "nsIHttpEventSink.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsHTTPRequest;
class nsHTTPResponse;
/* 
    The nsHTTPChannel class is an example implementation of a
    protocol instnce that is active on a per-URL basis.

    Currently this is being built with the Netlib dll. But after
    the registration stuff that DP is working on gets completed 
    this will move to the HTTP lib.

    -Gagan Saksena 02/25/99
*/
class nsHTTPChannel : public nsIHTTPChannel
{

public:

    // Constructors and Destructor
    nsHTTPChannel(nsIURI* i_URL, 
                  nsIHTTPEventSink* i_HTTPEventSink,
                  nsHTTPHandler* i_Handler);

    virtual ~nsHTTPChannel();

    // Functions from nsISupports
    NS_DECL_ISUPPORTS

    // nsIRequest methods:
    NS_IMETHOD IsPending(PRBool *result);
    NS_IMETHOD Cancel();
    NS_IMETHOD Suspend();
    NS_IMETHOD Resume();

    // nsIChannel methods:
    NS_IMETHOD GetURI(nsIURI * *aURL);
    NS_IMETHOD OpenInputStream(PRUint32 startPosition, PRInt32 readCount, nsIInputStream **_retval);
    NS_IMETHOD OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval);
    NS_IMETHOD AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIStreamListener *listener);
    NS_IMETHOD AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition,
                          PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIStreamObserver *observer);
    NS_IMETHOD GetLoadAttributes(PRUint32 *aLoadAttributes);
    NS_IMETHOD SetLoadAttributes(PRUint32 aLoadAttributes);
    NS_IMETHOD GetContentType(char * *aContentType);
    NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup);
    NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup);

    // nsIHTTPChannel methods:
    NS_IMETHOD GetRequestHeader(nsIAtom *headerName, char **_retval);
    NS_IMETHOD SetRequestHeader(nsIAtom *headerName, const char *value);
    NS_IMETHOD GetRequestHeaderEnumerator(nsISimpleEnumerator** aResult);
    NS_IMETHOD SetRequestMethod(PRUint32 method);

    NS_IMETHOD GetResponseHeader(nsIAtom *headerName, char **_retval);
    NS_IMETHOD GetResponseHeaderEnumerator(nsISimpleEnumerator** aResult);

    NS_IMETHOD SetPostDataStream(nsIInputStream *i_postStream);
    NS_IMETHOD GetPostDataStream(nsIInputStream **o_postStream);

    NS_IMETHOD GetResponseStatus(PRUint32 *aResponseStatus);
    NS_IMETHOD GetResponseString(char * *aResponseString);
    NS_IMETHOD GetEventSink(nsIHTTPEventSink* *eventSink);
    NS_IMETHOD GetResponseDataListener(nsIStreamListener* *aListener);

    // nsHTTPChannel methods:
    nsresult            Init();
    nsresult            Open();
    nsresult            ResponseCompleted(nsIChannel* aTransport);
    nsresult            SetResponse(nsHTTPResponse* i_pResp);
    nsresult            GetResponseContext(nsISupports** aContext);
    nsresult            SetContentType(const char* aContentType);

protected:
    nsCOMPtr<nsIURI>            m_URI;
    PRBool                      m_bConnected; 
    nsCOMPtr<nsHTTPHandler>     m_pHandler;
    HTTPState                   m_State;
    nsCOMPtr<nsIHTTPEventSink>  m_pEventSink;
    nsHTTPRequest*              m_pRequest;
    nsHTTPResponse*             m_pResponse;
    nsIStreamListener*          m_pResponseDataListener;
    PRUint32                    mLoadAttributes;

    nsCOMPtr<nsISupports>       mResponseContext;
    nsILoadGroup*               mLoadGroup;

    nsCString                   mContentType;
    nsIInputStream*             mPostStream;
};

#endif /* _nsHTTPChannel_h_ */
