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

#ifndef _nsHTTPRequest_h_
#define _nsHTTPRequest_h_

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIStreamObserver.h"
#include "nsIInputStream.h"
#include "nsIPipe.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsHTTPHeaderArray.h"
#include "nsHTTPEnums.h"
#include "nsHTTPHandler.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"

class nsIInputStream;
class nsHTTPChannel;
class nsHTTPResponseListener;

/* 
    The nsHTTPRequest class is the request object created for each HTTP 
    request before the connection. A request object may be cloned and 
    saved for later reuse. 

    This is also the observer class for writing to the transport. This
    receives notifications of OnStartRequest and OnStopRequest as the
    request is being written out to the server. Each instance of this 
    class is tied to the corresponding transport that it writes the 
    request to. 
    
    The essential purpose of the observer portion is to create the 
    response listener once it is done writing a request and also notify 
    the handler when this is done writing a request out. The latter could 
    be used (later) to do pipelining.

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 03/29/99
*/

class nsHTTPPipelinedRequest;

class nsHTTPRequest : public nsIRequest
{

public:

    // Constructor
    nsHTTPRequest(nsIURI* i_URL, nsHTTPHandler* i_Handler, PRUint32 bufferSegmentSize, PRUint32 bufferMaxSize);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST

    // Finally our own methods...
    /*
        Set or Get a header on the request. Note that for the first iteration
        of this design, a set call will not replace an existing singleton
        header (like User-Agent) So calling this will only append the 
        specified header to the request. Later on I would like to break 
        headers into singleton and multi-types... And then search and
        replace an exising singleton header. 

        Similarly getting will for now only get the first occurence. 
        TODO change to get the list.
    */
    nsresult            SetHeader(nsIAtom* i_Header, const char* i_Value);
    nsresult            GetHeader(nsIAtom* i_Header, char* *o_Value);

    /*
        Clone the current request for later use. Release it
        after you are done.
    */
    nsresult            Clone(const nsHTTPRequest* *o_Copy) const;
                        
    nsresult            SetHTTPVersion (PRUint32   i_Version);
    nsresult            GetHTTPVersion (PRUint32 * o_Version);

    nsresult            SetMethod(nsIAtom * i_Method);
    nsIAtom *           GetMethod(void) const;
                        
    nsresult            SetPriority(); // TODO 
    nsresult            GetPriority(); //TODO

    nsresult            GetHeaderEnumerator(nsISimpleEnumerator** aResult);
        
    nsresult            GetConnection(nsHTTPChannel** o_Connection);
    nsresult            SetConnection(nsHTTPChannel*  i_Connection);

    nsresult            SetTransport (nsIChannel * aTransport);
    nsresult            GetTransport (nsIChannel **aTransport);

    nsresult            GetUploadStream(nsIInputStream** o_UploadStream);
    nsresult            SetUploadStream(nsIInputStream* i_UploadStream);

    nsresult            SetOverrideRequestSpec(const char* i_Spec);
    nsresult            GetOverrideRequestSpec(char** o_Spec);

    PRUint32            mBufferSegmentSize;
    PRUint32            mBufferMaxSize;

    // for POST or PUT data...
    nsCOMPtr<nsIInputStream>    mInputStream;

    nsresult formHeaders (PRUint32 capabilities);
    nsresult formBuffer  (nsCString * reqBuffer, PRUint32 capabilities);

    nsHTTPPipelinedRequest*     mPipelinedRequest;
    nsHTTPChannel*              mConnection;
    nsCOMPtr<nsIURL>            mURI;
    PRBool                      mDoingProxySSLConnect;

protected:
    virtual ~nsHTTPRequest();

    // Use a method string corresponding to the method.
    const char*         MethodToString(HTTPMethod i_Method=HM_GET)
    {
        static const char methods[][TOTAL_NUMBER_OF_METHODS] = 
        {
            "DELETE ",
            "GET ",
            "HEAD ",
            "INDEX ",
            "LINK ",
            "OPTIONS ",
            "POST ",
            "PUT ",
            "PATCH ",
            "TRACE ",
            "UNLINK "
        };

        return methods[i_Method];
    }

    nsCOMPtr<nsIAtom>           mMethod;
    PRUint32                    mVersion;
    PRUint32                    mKeepAliveTimeout;

    nsHTTPHeaderArray           mHeaders;

    char*                       mRequestSpec; 

    nsHTTPHandler*              mHandler;
    nsresult                    mAbortStatus;
    PRBool                      mHeadersFormed;

    nsXPIDLCString              mHost;
    nsXPIDLCString              mSpec;
    PRInt32                     mPort;
    
    PRBool                      mProxySSLConnectAllowed;
};

#define REQUEST_RESTART_NORMAL  0
#define REQUEST_RESTART_SSL     1

class nsHTTPPipelinedRequest : public nsIStreamObserver
{

public:
    // Constructor
    nsHTTPPipelinedRequest (nsHTTPHandler* i_Handler, const char *host, PRInt32 port, PRUint32 capabilities);

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER

    nsresult    SetTransport (nsIChannel * aTransport);
    nsresult    GetTransport (nsIChannel **aTransport);

    // Build the actual request string based on the settings. 
    nsresult    WriteRequest(nsIInputStream* iRequestStream);

    nsresult    AddToPipeline(nsHTTPRequest *aRequest);
    nsresult    GetRequestCount (PRUint32 * aReqCount);

    nsresult    GetMustCommit (PRBool * aMustCommit);
    nsresult    SetMustCommit (PRBool   aMustCommit);
    nsresult    GetSameRequest(const char *host, PRInt32 port, PRBool * aSame);

    nsresult    GetCurrentRequest (nsHTTPRequest ** o_Req);
    nsresult    AdvanceToNextRequest ();
    nsresult    RestartRequest (PRUint32 flags);

    nsresult    GetName(PRUnichar* *result);
    nsresult    IsPending (PRBool *result);
    nsresult    Cancel  (nsresult status );
    nsresult    Suspend ();
    nsresult    Resume  ();

protected:
    virtual ~nsHTTPPipelinedRequest ();

    PRUint32                mCapabilities;
    PRUint32                mAttempts;
    nsCOMPtr<nsIChannel>    mTransport;

    PRUint32                mBufferSegmentSize;
    PRUint32                mBufferMaxSize;
    PRBool                  mMustCommit;   

    PRUint32                mTotalProcessed;
    PRUint32                mTotalWritten;

private:
    nsCOMPtr<nsISupportsArray> mRequests;

    nsHTTPHandler*          mHandler;
    nsCString               mRequestBuffer;

    nsCOMPtr<nsIInputStream>    mInputStream; 

    nsXPIDLCString  mHost;
    PRInt32         mPort;
    
    nsHTTPResponseListener* mListener;
    PRBool                      mOnStopDone;
};

#endif /* _nsHTTPRequest_h_ */
