/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsHTTPRequest_h_
#define _nsHTTPRequest_h_

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIStreamObserver.h"
#include "nsIURL.h"
#include "nsIRequest.h"
#include "nsHTTPHeaderArray.h"
#include "nsHTTPEnums.h"

class nsIInputStream;
class nsIBufferInputStream;
class nsIInputStream;
class nsIChannel;
class nsHTTPChannel;

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
class nsHTTPRequest : public nsIStreamObserver,
                      public nsIRequest
{

public:

    // Constructor and destructor
    nsHTTPRequest(nsIURI* i_URL=0, HTTPMethod i_Method=HM_GET, nsIChannel* i_pTranport = nsnull);
    virtual ~nsHTTPRequest();

    // Methods from nsISupports
    NS_DECL_ISUPPORTS

    // nsIStreamObserver functions
    NS_DECL_NSISTREAMOBSERVER

    // nsIRequest methods:
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
    NS_IMETHOD          SetHeader(nsIAtom* i_Header, const char* i_Value);
    NS_IMETHOD          GetHeader(nsIAtom* i_Header, char* *o_Value);

    /*
        Clone the current request for later use. Release it
        after you are done.
    */
    NS_IMETHOD          Clone(const nsHTTPRequest* *o_Copy) const;
                        
    NS_IMETHOD          SetHTTPVersion(HTTPVersion i_Version);
    NS_IMETHOD          GetHTTPVersion(HTTPVersion* o_Version);

    NS_IMETHOD          SetMethod(HTTPMethod i_Method);
    HTTPMethod          GetMethod(void) const;
                        
    NS_IMETHOD          SetPriority(); // TODO 
    NS_IMETHOD          GetPriority(); //TODO

    nsresult            GetHeaderEnumerator(nsISimpleEnumerator** aResult);
        
    /* 
        Returns the stream set up to hold the request data
        Calls build if not already built.
    */
    NS_IMETHOD          GetInputStream(nsIInputStream* *o_Stream);

    NS_IMETHOD          SetTransport(nsIChannel* i_pTransport);

    NS_IMETHOD          SetConnection(nsHTTPChannel* i_pConnection);

protected:

    // Build the actual request string based on the settings. 
    NS_METHOD           Build(void);

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

    HTTPMethod                  mMethod;
    nsCOMPtr<nsIURL>            mURI;
    HTTPVersion                 mVersion;
    // The actual request stream! 
    nsIBufferInputStream*       mRequest; 
    nsIChannel*                 mTransport;
    nsHTTPChannel*              mConnection;

    nsHTTPHeaderArray           mHeaders;
};

#define NS_HTTP_REQUEST_SEGMENT_SIZE     (4*1024)
#define NS_HTTP_REQUEST_BUFFER_SIZE      (16*1024)

#endif /* _nsHTTPRequest_h_ */
