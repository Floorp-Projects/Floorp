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
#include "nsIHTTPCommonHeaders.h"
#include "nsIHTTPRequest.h"
#include "nsIStreamObserver.h"
#include "nsIURL.h"

class nsIInputStream;
class nsVoidArray;
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
class nsHTTPRequest : public nsIHTTPRequest , public nsIStreamObserver
{

public:

    // Constructor and destructor
    nsHTTPRequest(nsIURI* i_URL=0, HTTPMethod i_Method=HM_GET, nsIChannel* i_pTranport = nsnull);
    virtual ~nsHTTPRequest();

    // Methods from nsISupports
    NS_DECL_ISUPPORTS

    // Methods from nsIHeader
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
    NS_IMETHOD          SetHeader(const char* i_Header, const char* i_Value);
    NS_IMETHOD          GetHeader(const char* i_Header, char* *o_Value);
    //This will be a no-op initially
    NS_IMETHOD          GetHeaderMultiple(const char* i_Header, 
                                          char** *o_ValueArray,
                                          int *o_Count);

    // Methods from nsIHTTPCommonHeaders
    NS_IMETHOD          SetAllow(const char* i_Value);
    NS_IMETHOD          GetAllow(char* *o_Value);

    NS_IMETHOD          SetContentBase(const char* i_Value);
    NS_IMETHOD          GetContentBase(char* *o_Value);

    NS_IMETHOD          SetContentEncoding(const char* i_Value);
    NS_IMETHOD          GetContentEncoding(char* *o_Value);

    NS_IMETHOD          SetContentLanguage(const char* i_Value);
    NS_IMETHOD          GetContentLanguage(char* *o_Value);

    NS_IMETHOD          SetContentLength(const char* i_Value);
    NS_IMETHOD          GetContentLength(char* *o_Value);

    NS_IMETHOD          SetContentLocation(const char* i_Value);
    NS_IMETHOD          GetContentLocation(char* *o_Value);

    NS_IMETHOD          SetContentMD5(const char* i_Value);
    NS_IMETHOD          GetContentMD5(char* *o_Value);

    NS_IMETHOD          SetContentRange(const char* i_Value);
    NS_IMETHOD          GetContentRange(char* *o_Value);

    NS_IMETHOD          SetContentTransferEncoding(const char* i_Value);
    NS_IMETHOD          GetContentTransferEncoding(char* *o_Value);

    NS_IMETHOD          SetContentType(const char* i_Value);
    NS_IMETHOD          GetContentType(char* *o_Value);

    NS_IMETHOD          SetDerivedFrom(const char* i_Value);
    NS_IMETHOD          GetDerivedFrom(char* *o_Value);

    NS_IMETHOD          SetETag(const char* i_Value);
    NS_IMETHOD          GetETag(char* *o_Value);

    NS_IMETHOD          SetExpires(const char* i_Value);
    NS_IMETHOD          GetExpires(char* *o_Value);

    NS_IMETHOD          SetLastModified(const char* i_Value);
    NS_IMETHOD          GetLastModified(char* *o_Value);

    /*
        To set multiple link headers, call set link again.
    */
    NS_IMETHOD          SetLink(const char* i_Value);
    NS_IMETHOD          GetLink(char* *o_Value);
    NS_IMETHOD          GetLinkMultiple(
                            const char** *o_ValueArray, 
                            int count) const;

    NS_IMETHOD          SetTitle(const char* i_Value);
    NS_IMETHOD          GetTitle(char* *o_Value);

    NS_IMETHOD          SetURI(const char* i_Value);
    NS_IMETHOD          GetURI(char* *o_Value);

    NS_IMETHOD          SetVersion(const char* i_Value);
    NS_IMETHOD          GetVersion(char* *o_Value);

    // Common Transaction headers
    NS_IMETHOD          SetConnection(const char* i_Value);
    NS_IMETHOD          GetConnection(char* *o_Value);

    NS_IMETHOD          SetDate(const char* i_Value);
    NS_IMETHOD          GetDate(char* *o_Value);

    NS_IMETHOD          SetPragma(const char* i_Value);
    NS_IMETHOD          GetPragma(char* *o_Value);

    NS_IMETHOD          SetForwarded(const char* i_Value);
    NS_IMETHOD          GetForwarded(char* *o_Value);

    NS_IMETHOD          SetMessageID(const char* i_Value);
    NS_IMETHOD          GetMessageID(char* *o_Value);

    NS_IMETHOD          SetMIME(const char* i_Value);
    NS_IMETHOD          GetMIME(char* *o_Value);

    NS_IMETHOD          SetTrailer(const char* i_Value);
    NS_IMETHOD          GetTrailer(char* *o_Value);

    NS_IMETHOD          SetTransfer(const char* i_Value);
    NS_IMETHOD          GetTransfer(char* *o_Value);

    // Methods from nsIHTTPRequest
    NS_IMETHOD          SetAccept(const char* i_Types);
    NS_IMETHOD          GetAccept(char* *o_Types);
                    
    NS_IMETHOD          SetAcceptChar(const char* i_Chartype);
    NS_IMETHOD          GetAcceptChar(char* *o_Chartype);
                    
    NS_IMETHOD          SetAcceptEncoding(const char* i_Encoding);
    NS_IMETHOD          GetAcceptEncoding(char* *o_Encoding);
                    
    NS_IMETHOD          SetAcceptLanguage(const char* i_Lang);
    NS_IMETHOD          GetAcceptLanguage(char* *o_Lang);
                    
    NS_IMETHOD          SetAuthentication(const char* i_Foo);
    NS_IMETHOD          GetAuthentication(char* *o_Foo);
                    
    NS_IMETHOD          SetExpect(const char* i_Expect);
    NS_IMETHOD          GetExpect(char** o_Expect);
                    
    NS_IMETHOD          SetFrom(const char* i_From);
    NS_IMETHOD          GetFrom(char** o_From);

    /*
        This is the actual Host for connection. Not necessarily the
        host in the url (as in the cases of proxy connection)
    */
    NS_IMETHOD          SetHost(const char* i_Host);
    NS_IMETHOD          GetHost(char** o_Host);

    NS_IMETHOD          SetHTTPVersion(HTTPVersion i_Version);
    NS_IMETHOD          GetHTTPVersion(HTTPVersion *o_Version);

    NS_IMETHOD          SetIfModifiedSince(const char* i_Value);
    NS_IMETHOD          GetIfModifiedSince(char* *o_Value);

    NS_IMETHOD          SetIfMatch(const char* i_Value);
    NS_IMETHOD          GetIfMatch(char* *o_Value);

    NS_IMETHOD          SetIfMatchAny(const char* i_Value);
    NS_IMETHOD          GetIfMatchAny(char* *o_Value);
                        
    NS_IMETHOD          SetIfNoneMatch(const char* i_Value);
    NS_IMETHOD          GetIfNoneMatch(char* *o_Value);
                        
    NS_IMETHOD          SetIfNoneMatchAny(const char* i_Value);
    NS_IMETHOD          GetIfNoneMatchAny(char* *o_Value);
                        
    NS_IMETHOD          SetIfRange(const char* i_Value);
    NS_IMETHOD          GetIfRange(char* *o_Value);
                        
    NS_IMETHOD          SetIfUnmodifiedSince(const char* i_Value);
    NS_IMETHOD          GetIfUnmodifiedSince(char* *o_Value);
                        
    NS_IMETHOD          SetMaxForwards(const char* i_Value);
    NS_IMETHOD          GetMaxForwards(char* *o_Value);

    /* 
        Range information for byte-range requests 
    */
    NS_IMETHOD          SetRange(const char* i_Value);
    NS_IMETHOD          GetRange(char* *o_Value);
                        
    NS_IMETHOD          SetReferer(const char* i_Value);
    NS_IMETHOD          GetReferer(char* *o_Value);
                        
    NS_IMETHOD          SetUserAgent(const char* i_Value);
    NS_IMETHOD          GetUserAgent(char* *o_Value);

    // nsIStreamObserver functions
    NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports* context);
    NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports* context,
                             nsresult aStatus,
                             const PRUnichar* aMsg);

    // Finally our own methods...
    /*
        Clone the current request for later use. Release it
        after you are done.
    */
    NS_IMETHOD          Clone(const nsHTTPRequest* *o_Copy) const;
                        
    NS_IMETHOD          SetMethod(HTTPMethod i_Method);
    HTTPMethod          GetMethod(void) const;
                        
    NS_IMETHOD          SetPriority(); // TODO 
    NS_IMETHOD          GetPriority(); //TODO
    
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

    nsCOMPtr<nsIURL>            m_pURL;
    HTTPVersion                 m_Version;
    HTTPMethod                  m_Method;
    // The actual request stream! 
    nsIBufferInputStream*       m_Request; 
    nsVoidArray*                m_pArray;
    nsIChannel*                 m_pTransport;
    nsHTTPChannel*              m_pConnection;
};

#define NS_HTTP_REQUEST_SEGMENT_SIZE     (4*1024)
#define NS_HTTP_REQUEST_BUFFER_SIZE      (16*1024)

#endif /* _nsHTTPRequest_h_ */
