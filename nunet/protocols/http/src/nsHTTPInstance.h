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

#ifndef _nsHTTPInstance_h_
#define _nsHTTPInstance_h_

#include "nsIHTTPInstance.h"
class nsICoolURL;

/* 
    The nsHTTPInstance class is an example implementation of a
    protocol instnce that is active on a per-URL basis.

    Currently this is being built with the Netlib dll. But after
    the registration stuff that DP is working on gets completed 
    this will move to the HTTP lib.

    -Gagan Saksena 02/25/99
*/
class nsHTTPInstance : public nsIHTTPInstance 
{

public:

                        nsHTTPInstance(nsICoolURL* i_URL);
                        ~nsHTTPInstance();

    //Functions from nsISupports
    NS_DECL_ISUPPORTS;


    // Functions from nsIProtocolInstance
    NS_METHOD               GetInputStream( nsIInputStream* *o_Stream);
    NS_METHOD               Interrupt(void);
    NS_METHOD               Load(void);

    // Functions from nsIHTTPInstance
    NS_METHOD               SetAccept(const char* i_AcceptHeader);
    //NS_METHOD             SetAcceptType();
    NS_METHOD               SetCookie(const char* i_Cookie);
    NS_METHOD               SetUserAgent(const char* i_UserAgent);
    NS_METHOD               SetHTTPVersion(HTTPVersion i_Version = HTTP_ONE_ONE);

    NS_METHOD_(PRInt32)     GetContentLength(void) const;
    NS_METHOD               GetContentType(const char* *o_Type) const;
    //NS_METHOD_(PRTime)    GetDate(void) const;
    NS_METHOD_(PRInt32)     GetResponseStatus(void) const;
    NS_METHOD               GetResponseString(const char* *o_String) const;
    NS_METHOD               GetServer(const char* *o_String) const;

private:
    nsICoolURL*     m_pURL;
    PRBool          m_bConnected;
};

#endif /* _nsHTTPInstance_h_ */
