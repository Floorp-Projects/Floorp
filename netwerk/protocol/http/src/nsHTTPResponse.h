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
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"

class nsIUrl;
class nsVoidArray;
class nsIInputStream;

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
    nsHTTPResponse(nsIInputStream* i_InputStream);
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

    // Stuff from nsIHTTPResponse
    NS_IMETHOD          GetContentLength(PRInt32* o_Value) const;
    NS_IMETHOD          GetStatus(PRUint32* o_Value) const;
    NS_IMETHOD          GetStatusString(char* *o_String) const;
    NS_IMETHOD          GetServer(char* *o_String) const;

    // Finally our own methods...

    NS_IMETHOD          SetServerVersion(const char* i_ServerVersion);

    NS_IMETHOD          GetInputStream(nsIInputStream* *o_Stream);
protected:

    NS_IMETHOD          SetHeaderInternal(const char* i_Header, const char* i_Value);
    NS_IMETHOD          SetStatus(PRInt32 i_Value) { m_Status = i_Value; return NS_OK;};
    NS_IMETHOD          SetStatusString(const char* i_Value);

    nsVoidArray*                m_pArray;
    HTTPVersion                 m_ServerVersion;
    char*                       m_pStatusString;
    PRUint32                    m_Status;
    nsIInputStream*             m_pInputStream;

    friend class nsHTTPResponseListener;
};

#endif /* _nsHTTPResponse_h_ */
