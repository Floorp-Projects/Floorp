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

#include "nsIHTTPCommonHeaders.h"
#include "nsIHTTPRequest.h"

class nsIUrl;
class nsVoidArray;
/* 
    The nsHTTPRequest class is the request object created for each HTTP 
    request before the connection. A request object may be cloned and 
    saved for later reuse. 

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 03/29/99
*/
class nsHTTPRequest : public nsIHTTPRequest
{

public:

    // Constructor and destructor
    nsHTTPRequest(nsIUrl* i_URL=0, HTTPMethod i_Method=HM_GET);
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
    NS_IMETHOD          GetHeader(const char* i_Header, const char* *o_Value) const;
    //This will be a no-op initially
    NS_IMETHOD          GetHeaderMultiple(
                            const char* i_Header, 
                            const char** *o_ValueArray,
                            int o_Count) const;

    // Methods from nsIHTTPCommonHeaders
    NS_IMETHOD          SetAllow(const char* i_Value);
    NS_IMETHOD          GetAllow(const char* *o_Value) const;

    NS_IMETHOD          SetContentBase(const char* i_Value);
    NS_IMETHOD          GetContentBase(const char* *o_Value) const;

    NS_IMETHOD          SetContentEncoding(const char* i_Value);
    NS_IMETHOD          GetContentEncoding(const char* *o_Value) const;

    NS_IMETHOD          SetContentLanguage(const char* i_Value);
    NS_IMETHOD          GetContentLanguage(const char* *o_Value) const;

    NS_IMETHOD          SetContentLength(const char* i_Value);
    NS_IMETHOD          GetContentLength(const char* *o_Value) const;

    NS_IMETHOD          SetContentLocation(const char* i_Value);
    NS_IMETHOD          GetContentLocation(const char* *o_Value) const;

    NS_IMETHOD          SetContentMD5(const char* i_Value);
    NS_IMETHOD          GetContentMD5(const char* *o_Value) const;

    NS_IMETHOD          SetContentRange(const char* i_Value);
    NS_IMETHOD          GetContentRange(const char* *o_Value) const;

    NS_IMETHOD          SetContentTransferEncoding(const char* i_Value);
    NS_IMETHOD          GetContentTransferEncoding(const char* *o_Value) const;

    NS_IMETHOD          SetContentType(const char* i_Value);
    NS_IMETHOD          GetContentType(const char* *o_Value) const;

    NS_IMETHOD          SetDerivedFrom(const char* i_Value);
    NS_IMETHOD          GetDerivedFrom(const char* *o_Value) const;

    NS_IMETHOD          SetETag(const char* i_Value);
    NS_IMETHOD          GetETag(const char* *o_Value) const;

    NS_IMETHOD          SetExpires(const char* i_Value);
    NS_IMETHOD          GetExpires(const char* *o_Value) const;

    NS_IMETHOD          SetLastModified(const char* i_Value);
    NS_IMETHOD          GetLastModified(const char* *o_Value) const;

    /*
        To set multiple link headers, call set link again.
    */
    NS_IMETHOD          SetLink(const char* i_Value);
    NS_IMETHOD          GetLink(const char* *o_Value) const;
    NS_IMETHOD          GetLinkMultiple(
                            const char** *o_ValueArray, 
                            int count) const;

    NS_IMETHOD          SetTitle(const char* i_Value);
    NS_IMETHOD          GetTitle(const char* *o_Value) const;

    NS_IMETHOD          SetURI(const char* i_Value);
    NS_IMETHOD          GetURI(const char* *o_Value) const;

    NS_IMETHOD          SetVersion(const char* i_Value);
    NS_IMETHOD          GetVersion(const char* *o_Value) const;

    // Common Transaction headers
    NS_IMETHOD          SetConnection(const char* i_Value);
    NS_IMETHOD          GetConnection(const char* *o_Value) const;

    NS_IMETHOD          SetDate(const char* i_Value);
    NS_IMETHOD          GetDate(const char* *o_Value) const;

    NS_IMETHOD          SetPragma(const char* i_Value);
    NS_IMETHOD          GetPragma(const char* *o_Value) const;

    NS_IMETHOD          SetForwarded(const char* i_Value);
    NS_IMETHOD          GetForwarded(const char* *o_Value) const;

    NS_IMETHOD          SetMessageID(const char* i_Value);
    NS_IMETHOD          GetMessageID(const char* *o_Value) const;

    NS_IMETHOD          SetMIME(const char* i_Value);
    NS_IMETHOD          GetMIME(const char* *o_Value) const;

    NS_IMETHOD          SetTrailer(const char* i_Value);
    NS_IMETHOD          GetTrailer(const char* *o_Value) const;

    NS_IMETHOD          SetTransfer(const char* i_Value);
    NS_IMETHOD          GetTransfer(const char* *o_Value) const;

    // Methods from nsIHTTPRequest
    NS_IMETHOD          SetAccept(const char* i_Types);
    NS_IMETHOD          GetAccept(const char* *o_Types) const;
                    
    NS_IMETHOD          SetAcceptChar(const char* i_Chartype);
    NS_IMETHOD          GetAcceptChar(const char* *o_Chartype) const;
                    
    NS_IMETHOD          SetAcceptEncoding(const char* i_Encoding);
    NS_IMETHOD          GetAcceptEncoding(const char* *o_Encoding) const;
                    
    NS_IMETHOD          SetAcceptLanguage(const char* i_Lang);
    NS_IMETHOD          GetAcceptLanguage(const char* *o_Lang) const;
                    
    NS_IMETHOD          SetAuthentication(const char* i_Foo);
    NS_IMETHOD          GetAuthentication(const char* *o_Foo) const;
                    
    NS_IMETHOD          SetExpect(const char* i_Expect);
    NS_IMETHOD          GetExpect(const char** o_Expect) const;
                    
    NS_IMETHOD          SetFrom(const char* i_From);
    NS_IMETHOD          GetFrom(const char** o_From) const;

    /*
        This is the actual Host for connection. Not necessarily the
        host in the url (as in the cases of proxy connection)
    */
    NS_IMETHOD          SetHost(const char* i_Host);
    NS_IMETHOD          GetHost(const char** o_Host) const;

    NS_IMETHOD          SetHTTPVersion(HTTPVersion i_Version);
    NS_IMETHOD          GetHTTPVersion(HTTPVersion *o_Version) const;

    NS_IMETHOD          SetIfModifiedSince(const char* i_Value);
    NS_IMETHOD          GetIfModifiedSince(const char* *o_Value) const;

    NS_IMETHOD          SetIfMatch(const char* i_Value);
    NS_IMETHOD          GetIfMatch(const char* *o_Value) const;

    NS_IMETHOD          SetIfMatchAny(const char* i_Value);
    NS_IMETHOD          GetIfMatchAny(const char* *o_Value) const;
                        
    NS_IMETHOD          SetIfNoneMatch(const char* i_Value);
    NS_IMETHOD          GetIfNoneMatch(const char* *o_Value) const;
                        
    NS_IMETHOD          SetIfNoneMatchAny(const char* i_Value);
    NS_IMETHOD          GetIfNoneMatchAny(const char* *o_Value) const;
                        
    NS_IMETHOD          SetIfRange(const char* i_Value);
    NS_IMETHOD          GetIfRange(const char* *o_Value) const;
                        
    NS_IMETHOD          SetIfUnmodifiedSince(const char* i_Value);
    NS_IMETHOD          GetIfUnmodifiedSince(const char* *o_Value) const;
                        
    NS_IMETHOD          SetMaxForwards(const char* i_Value);
    NS_IMETHOD          GetMaxForwards(const char* *o_Value) const;

    /* 
        Range information for byte-range requests 
    */
    NS_IMETHOD          SetRange(const char* i_Value);
    NS_IMETHOD          GetRange(const char* *o_Value) const;
                        
    NS_IMETHOD          SetReferer(const char* i_Value);
    NS_IMETHOD          GetReferer(const char* *o_Value) const;
                        
    NS_IMETHOD          SetUserAgent(const char* i_Value);
    NS_IMETHOD          GetUserAgent(const char* *o_Value) const;

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


protected:

    // Build the actual request string based on the settings. 
    NS_METHOD           Build(void);

    // Use a method string corresponding to the method.
    const char*         MethodToString(HTTPMethod i_Method=HM_GET)
    {
        static const char methods[][TOTAL_NUMBER_OF_METHODS] = 
        {
            "DELETE",
            "GET",
            "HEAD",
            "INDEX",
            "LINK",
            "OPTIONS",
            "POST",
            "PUT",
            "PATCH",
            "TRACE",
            "UNLINK"
        };

        return methods[i_Method];
    }

    nsIUrl*                     m_pURI;
    HTTPVersion                 m_Version;
    HTTPMethod                  m_Method;
    // The actual request string! 
    char*                       m_Request; 
    nsVoidArray*                m_pArray;
};

#endif /* _nsHTTPRequest_h_ */
