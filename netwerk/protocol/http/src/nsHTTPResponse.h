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

#ifndef _nsHTTPResponse_h_
#define _nsHTTPResponse_h_

#include "nsIHTTPCommonHeaders.h"
#include "nsIHTTPResponse.h"
#include "nsCOMPtr.h"
#include "nsIHTTPConnection.h"

class nsIUrl;
class nsVoidArray;

/* 
    The nsHTTPResponse class is the response object created by the response
    listener as it reads in data from the input stream.

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 03/29/99
*/
class nsHTTPResponse : public nsIHTTPResponse
{

public:
    // Constructor and destructor
    nsHTTPResponse(nsIHTTPConnection* i_pConnection);
    virtual ~nsHTTPResponse();

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

    // Stuff from nsIHTTPResponse
    NS_IMETHOD          GetContentLength(PRInt32* o_Value) const;
    NS_IMETHOD          GetStatus(PRInt32* o_Value) const;
    NS_IMETHOD          GetStatusString(const char* *o_String) const;
    NS_IMETHOD          GetServer(const char* *o_String) const;

    // Finally our own methods...

    NS_IMETHOD          SetServerVersion(const char* i_ServerVersion);

protected:

    NS_IMETHOD          SetHeaderInternal(const char* i_Header, const char* i_Value);
    NS_IMETHOD          SetStatus(PRInt32 i_Value) { m_Status = i_Value; return NS_OK;};
    NS_IMETHOD          SetStatusString(const char* i_Value);

    char*                       m_Buffer; /* Used for holding header data */
    nsVoidArray*                m_pArray;
    nsCOMPtr<nsIHTTPConnection> m_pConn;
    HTTPVersion                 m_ServerVersion;
    char*                       m_pStatusString;
    PRUint32                    m_Status;

    friend class nsHTTPResponseListener;
};

#endif /* _nsHTTPResponse_h_ */
