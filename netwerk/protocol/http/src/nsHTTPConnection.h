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

#ifndef _nsHTTPConnection_h_
#define _nsHTTPConnection_h_

#ifndef nsIURL
#define nsIURL nsIUrl
#endif

#include "nsIHTTPConnection.h"
#include "nsIProtocolConnection.h"
#include "nsHTTPEnums.h"
#include "nsIUrl.h"
#include "nsHTTPHandler.h"
#include "nsIEventQueue.h"
#include "nsIHTTPEventSink.h"

class nsHTTPRequest;
class nsHTTPResponse;
/* 
    The nsHTTPConnection class is an example implementation of a
    protocol instnce that is active on a per-URL basis.

    Currently this is being built with the Netlib dll. But after
    the registration stuff that DP is working on gets completed 
    this will move to the HTTP lib.

    -Gagan Saksena 02/25/99
*/
class nsHTTPConnection : public nsIHTTPConnection , public nsIProtocolConnection
{

public:

    // Constructors and Destructor
    nsHTTPConnection(
        nsIURL* i_URL, 
        nsIEventQueue* i_EQ, 
        nsIHTTPEventSink* i_HTTPEventSink,
        nsIHTTPHandler* i_Handler 
        );

    ~nsHTTPConnection();

    // Functions from nsISupports
    NS_DECL_ISUPPORTS;

    // Functions from nsICancelable
    NS_IMETHOD              Cancel(void);

    NS_IMETHOD              Suspend(void);

    NS_IMETHOD              Resume(void);

    // Functions from nsIProtocolConnection
    
    // blocking:
    NS_IMETHOD              GetInputStream(nsIInputStream* *o_Stream);
    
    // The actual connect call
    NS_IMETHOD              Open(void);

    // blocking:
    NS_IMETHOD              GetOutputStream(nsIOutputStream* *o_Stream);

    // Functions from nsIHTTPConnection
    NS_IMETHOD              GetRequestHeader(const char* i_Header, const char* *o_Value) const;
    NS_IMETHOD              SetRequestHeader(const char* i_Header, const char* i_Value);

    NS_IMETHOD              SetRequestMethod(HTTPMethod i_Method=HM_GET);
                            
    NS_IMETHOD              GetResponseHeader(const char* i_Header, const char* *o_Value);
                            
    NS_IMETHOD              GetResponseStatus(PRInt32* o_Status);
                            
    NS_IMETHOD              GetResponseString(const char* *o_String);

    NS_IMETHOD              GetURL(nsIURL* *o_URL) const;

    NS_IMETHOD              EventSink(nsIHTTPEventSink* *o_EventSink) const { if (o_EventSink) *o_EventSink = m_pEventSink; return NS_OK; };
    
    nsIEventQueue*          EventQueue(void) const { return m_pEventQ; };

private:
    nsCOMPtr<nsIURL>            m_pURL;
    PRBool                      m_bConnected; 
    nsCOMPtr<nsIHTTPHandler>    m_pHandler;
    HTTPState                   m_State;
    nsCOMPtr<nsIEventQueue>     m_pEventQ;
    nsCOMPtr<nsIHTTPEventSink>  m_pEventSink;
    nsHTTPRequest*              m_pRequest;
    nsHTTPResponse*             m_pResponse;
};

#endif /* _nsHTTPConnection_h_ */
