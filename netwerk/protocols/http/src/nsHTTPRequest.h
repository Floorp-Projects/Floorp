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

class nsIURI;

/* 
    The nsHTTPRequest class is the request object created for each HTTP 
    request before the connection. A request object may be cloned and 
    saved for later reuse. 

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 03/29/99
*/
class nsHTTPRequest : public nsIHTTPCommonHeaders
{

public:
    typedef enum _HTTPMethod {
        DELETE, // Assure corresponding array set ok.
        GET,
        HEAD,
        INDEX,
        LINK,
        OPTIONS,
        POST,
        PUT,
        PATCH,
        TRACE,
        UNLINK
    } HTTPMethod;

    // Constructor and destructor
    nsHTTPRequest(nsIURL* i_URL=0, HTTPMethod i_Method=GET);
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
    NS_METHOD        SetHeader(const char* i_Header, const char* i_Value);
    NS_METHOD        GetHeader(const char* i_Header, const char* *o_Value) const;
    //This will be a no-op initially
    NS_METHOD        GetHeaderMultiple(
                        const char* i_Header, 
                        const char** *o_ValueArray,
                        int o_Count) const;


    // Methods from nsIHTTPCommonHeaders
    NS_METHOD          SetAllow(const char* i_Value);
    NS_METHOD          GetAllow(const char* *o_Value) const;

    NS_METHOD          SetContentBase(const char* i_Value);
    NS_METHOD          GetContentBase(const char* *o_Value) const;

    NS_METHOD          SetContentEncoding(const char* i_Value);
    NS_METHOD          GetContentEncoding(const char* *o_Value) const;

    NS_METHOD          SetContentLanguage(const char* i_Value);
    NS_METHOD          GetContentLanguage(const char* *o_Value) const;

    NS_METHOD          SetContentLength(const char* i_Value);
    NS_METHOD          GetContentLength(const char* *o_Value) const;

    NS_METHOD          SetContentLocation(const char* i_Value);
    NS_METHOD          GetContentLocation(const char* *o_Value) const;

    NS_METHOD          SetContentMD5(const char* i_Value);
    NS_METHOD          GetContentMD5(const char* *o_Value) const;

    NS_METHOD          SetContentRange(const char* i_Value);
    NS_METHOD          GetContentRange(const char* *o_Value) const;

    NS_METHOD          SetContentTransferEncoding(const char* i_Value);
    NS_METHOD          GetContentTransferEncoding(const char* *o_Value) const;

    NS_METHOD          SetContentType(const char* i_Value);
    NS_METHOD          GetContentType(const char* *o_Value) const;

    NS_METHOD          SetDerivedFrom(const char* i_Value);
    NS_METHOD          GetDerivedFrom(const char* *o_Value) const;

    NS_METHOD          SetETag(const char* i_Value);
    NS_METHOD          GetETag(const char* *o_Value) const;

    NS_METHOD          SetExpires(const char* i_Value);
    NS_METHOD          GetExpires(const char* *o_Value) const;

    NS_METHOD          SetLastModified(const char* i_Value);
    NS_METHOD          GetLastModified(const char* *o_Value) const;

    /*
        To set multiple link headers, call set link again.
    */
    NS_METHOD          SetLink(const char* i_Value);
    NS_METHOD          GetLink(const char* *o_Value) const;
    NS_METHOD          GetLinkMultiple(
                            const char** *o_ValueArray, 
                            int count) const;

    NS_METHOD          SetTitle(const char* i_Value);
    NS_METHOD          GetTitle(const char* *o_Value) const;

    NS_METHOD          SetURI(const char* i_Value);
    NS_METHOD          GetURI(const char* *o_Value) const;

    NS_METHOD          SetVersion(const char* i_Value);
    NS_METHOD          GetVersion(const char* *o_Value) const;

    // Common Transaction headers
    NS_METHOD          SetCC(const char* i_Value);
    NS_METHOD          GetCC(const char* *o_Value) const;

    NS_METHOD          SetConnection(const char* i_Value);
    NS_METHOD          GetConnection(const char* *o_Value) const;

    NS_METHOD          SetDate(const char* i_Value);
    NS_METHOD          GetDate(const char* *o_Value) const;

    NS_METHOD          SetPragma(const char* i_Value);
    NS_METHOD          GetPragma(const char* *o_Value) const;

    NS_METHOD          SetForwarded(const char* i_Value);
    NS_METHOD          GetForwarded(const char* *o_Value) const;

    NS_METHOD          SetMessageID(const char* i_Value);
    NS_METHOD          GetMessageID(const char* *o_Value) const;

    NS_METHOD          SetMIME(const char* i_Value);
    NS_METHOD          GetMIME(const char* *o_Value) const;

    NS_METHOD          SetTrailer(const char* i_Value);
    NS_METHOD          GetTrailer(const char* *o_Value) const;

    NS_METHOD          SetTransfer(const char* i_Value);
    NS_METHOD          GetTransfer(const char* *o_Value) const;

    // Methods from nsIHTTPRequest
    NS_METHOD          SetAccept(const char* i_Types);
    NS_METHOD          GetAccept(const char* *o_Types) const;
                    
    NS_METHOD          SetAcceptChar(const char* i_Chartype);
    NS_METHOD          GetAcceptChar(const char* *o_Chartype) const;
                    
    NS_METHOD          SetAcceptEncoding(const char* i_Encoding);
    NS_METHOD          GetAcceptEncoding(const char* *o_Encoding) const;
                    
    NS_METHOD          SetAcceptTransferEncoding(const char* i_Encoding);
    NS_METHOD          GetAcceptTransferEncoding(const char* *o_Encoding) const;
                    
    NS_METHOD          SetAcceptLanguage(const char* i_Lang);
    NS_METHOD          GetAcceptLanguage(const char* *o_Lang) const;
                    
    NS_METHOD          SetAuthentication(const char* i_Foo);
    NS_METHOD          GetAuthentication(const char* *o_Foo) const;
                    
    NS_METHOD          SetExpect(const char* i_Expect);
    NS_METHOD          GetExpect(const char** o_Expect) const;
                    
    NS_METHOD          SetFrom(const char* i_From);
    NS_METHOD          GetFrom(const char** o_From) const;

    /*
        This is the actual Host for connection. Not necessarily the
        host in the url (as in the cases of proxy connection)
    */
    NS_METHOD           SetHost(const char* i_Host);
    NS_METHOD           GetHost(const char** o_Host) const;

    NS_METHOD           SetIfModifiedSince(const char* i_Value);
    NS_METHOD           GetIfModifiedSince(const char* *o_Value) const;

    NS_METHOD           SetIfMatch(const char* i_Value);
    NS_METHOD           GetIfMatch(const char* *o_Value) const;

    NS_METHOD           SetIfMatchAny(const char* i_Value);
    NS_METHOD           GetIfMatchAny(const char* *o_Value) const;
                        
    NS_METHOD           SetIfNoneMatch(const char* i_Value);
    NS_METHOD           GetIfNoneMatch(const char* *o_Value) const;
                        
    NS_METHOD           SetIfNoneMatchAny(const char* i_Value);
    NS_METHOD           GetIfNoneMatchAny(const char* *o_Value) const;
                        
    NS_METHOD           SetIfRange(const char* i_Value);
    NS_METHOD           GetIfRange(const char* *o_Value) const;
                        
    NS_METHOD           SetIfUnmodifiedSince(const char* i_Value);
    NS_METHOD           GetIfUnmodifiedSince(const char* *o_Value) const;
                        
    NS_METHOD           SetMaxForwards(const char* i_Value);
    NS_METHOD           GetMaxForwards(const char* *o_Value) const;

    /* 
        Range information for byte-range requests 
    */
    NS_METHOD           SetRange(const char* i_Value);
    NS_METHOD           GetRange(const char* *o_Value) const;
                        
    NS_METHOD           SetReferer(const char* i_Value);
    NS_METHOD           GetReferer(const char* *o_Value) const;
                        
    NS_METHOD           SetUserAgent(const char* i_Value);
    NS_METHOD           GetUserAgent(const char* *o_Value) const;

    // Finally our own methods...
    NS_METHOD           Clone(const nsHTTPRequest* *o_Request) const;
                        
    NS_METHOD           SetMethod(HTTPMethod i_Method);
    HTTPMethod          GetMethod(void) const;
                        
    NS_METHOD           SetPriority(); // TODO 
    NS_METHOD           GetPriority(); //TODO


protected:

    // Build the actual request string based on the settings. 
    NS_METHOD           Build(void);

    // Use a method string corresponding to the method.
    const char*         MethodToString(HTTPMethod i_Method=GET)
    {
        static const char[][] methods = 
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

    nsIURI*             m_pURI;

    HTTPMethod          m_Method;
    // The actual request string! 
    char*               m_Request; 
};

#endif /* _nsHTTPRequest_h_ */
