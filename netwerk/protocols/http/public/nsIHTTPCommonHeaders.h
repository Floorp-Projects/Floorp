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

#ifndef _nsIHTTPCommonHeaders_h_
#define _nsIHTTPCommonHeaders_h_

#include "nsIHeader.h"
/* 
    nsIHTTPCommonHeaders. This class provides all the common headers
    between a request and a response HTTP object. These headers are
    classified in two categories. For the moment I did not split 
    them in two classes but maybe eventually that might be the right
    thing to do. The reason to split this should be evident if you 
    look from a client's perspective. A consumer of the data from 
    Nunet should not have to worry about transaction information
    associated with a connection. 
    
    The first category is the Entity headers that deal with a specific
    entity that is the key to the transaction (A transaction is either
    an HTTP request or a response) A common example of an Entity header
    is "Content-Length"

    The second category is the Transaction headers that are common to 
    both HTTP Request and HTTP Response. These specify information about
    the transaction (the connection, etc.) and not about the entity 
    being moved. A common example is "Connection: Keep-Alive" 

    - Gagan Saksena 03/27/99
*/

class nsIHTTPCommonHeaders : public nsIHeader
{
public:

    // Entity headers.
    NS_IMETHOD          SetAllow(const char* i_Value) = 0;
    NS_IMETHOD          GetAllow(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentBase(const char* i_Value) = 0;
    NS_IMETHOD          GetContentBase(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentEncoding(const char* i_Value) = 0;
    NS_IMETHOD          GetContentEncoding(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentLanguage(const char* i_Value) = 0;
    NS_IMETHOD          GetContentLanguage(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentLength(const char* i_Value) = 0;
    NS_IMETHOD          GetContentLength(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentLocation(const char* i_Value) = 0;
    NS_IMETHOD          GetContentLocation(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentMD5(const char* i_Value) = 0;
    NS_IMETHOD          GetContentMD5(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentRange(const char* i_Value) = 0;
    NS_IMETHOD          GetContentRange(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentTransferEncoding(const char* i_Value) = 0;
    NS_IMETHOD          GetContentTransferEncoding(const char* *o_Value) const = 0;

    NS_IMETHOD          SetContentType(const char* i_Value) = 0;
    NS_IMETHOD          GetContentType(const char* *o_Value) const = 0;

    NS_IMETHOD          SetDerivedFrom(const char* i_Value) = 0;
    NS_IMETHOD          GetDerivedFrom(const char* *o_Value) const = 0;

    NS_IMETHOD          SetETag(const char* i_Value) = 0;
    NS_IMETHOD          GetETag(const char* *o_Value) const = 0;

    NS_IMETHOD          SetExpires(const char* i_Value) = 0;
    NS_IMETHOD          GetExpires(const char* *o_Value) const = 0;

    NS_IMETHOD          SetLastModified(const char* i_Value) = 0;
    NS_IMETHOD          GetLastModified(const char* *o_Value) const = 0;

    /*
        To set multiple link headers, call set link again.
    */
    NS_IMETHOD          SetLink(const char* i_Value) = 0;
    NS_IMETHOD          GetLink(const char* *o_Value) const = 0;
    NS_IMETHOD          GetLinkMultiple(
                            const char** *o_ValueArray, 
                            int count) const = 0;

    NS_IMETHOD          SetTitle(const char* i_Value) = 0;
    NS_IMETHOD          GetTitle(const char* *o_Value) const = 0;

    NS_IMETHOD          SetURI(const char* i_Value) = 0;
    NS_IMETHOD          GetURI(const char* *o_Value) const = 0;

    NS_IMETHOD          SetVersion(const char* i_Value) = 0;
    NS_IMETHOD          GetVersion(const char* *o_Value) const = 0;

    // Common Transaction headers
    NS_IMETHOD          SetCC(const char* i_Value) = 0;
    NS_IMETHOD          GetCC(const char* *o_Value) const = 0;

    NS_IMETHOD          SetConnection(const char* i_Value) = 0;
    NS_IMETHOD          GetConnection(const char* *o_Value) const = 0;

    NS_IMETHOD          SetDate(const char* i_Value) = 0;
    NS_IMETHOD          GetDate(const char* *o_Value) const = 0;

    NS_IMETHOD          SetPragma(const char* i_Value) = 0;
    NS_IMETHOD          GetPragma(const char* *o_Value) const = 0;

    NS_IMETHOD          SetForwarded(const char* i_Value) = 0;
    NS_IMETHOD          GetForwarded(const char* *o_Value) const = 0;

    NS_IMETHOD          SetMessageID(const char* i_Value) = 0;
    NS_IMETHOD          GetMessageID(const char* *o_Value) const = 0;

    NS_IMETHOD          SetMIME(const char* i_Value) = 0;
    NS_IMETHOD          GetMIME(const char* *o_Value) const = 0;

    NS_IMETHOD          SetTrailer(const char* i_Value) = 0;
    NS_IMETHOD          GetTrailer(const char* *o_Value) const = 0;

    NS_IMETHOD          SetTransfer(const char* i_Value) = 0;
    NS_IMETHOD          GetTransfer(const char* *o_Value) const = 0;

    static const nsIID& GetIID() { 
        // {C81A4600-EBC0-11d2-B018-006097BFC036}
        static const nsIID NS_IHTTP_COMMON_HEADERS_IID = 
            { 0xc81a4600, 0xebc0, 0x11d2, { 0xb0, 0x18, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };
        return NS_IHTTP_COMMON_HEADERS_IID ;
    }
}
#endif // _nsIHTTPCommonHeaders_h_