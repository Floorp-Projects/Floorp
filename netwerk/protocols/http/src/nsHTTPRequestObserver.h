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

#ifndef _nsHTTPRequestObserver_h_
#define _nsHTTPRequestObserver_h_

#include "nsIStreamObserver.h"
class nsIChannel;
class nsIString;

/* 
    The nsHTTPRequestObserver class is the request writer observer that 
    receives notifications of OnStartBinding and OnStopBinding as the
    request is being written out to the server. Each instance of this 
    class is tied to the corresponding transport that it writes the 
    request to. 
    
    The essential purpose of this class is to create the response listener 
    once it is done writing a request and also notify the handler when 
    this is done writing a request out. The latter could be used (later) 
    to do pipelining.

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 04/29/99
*/
class nsHTTPRequestObserver : public nsIStreamObserver
{

public:

    nsHTTPRequestObserver(nsIChannel* i_pTransport);
    virtual ~nsHTTPRequestObserver();

    // nsISupports functions
    NS_DECL_ISUPPORTS;

    // nsIStreamObserver functions
    NS_IMETHOD OnStartBinding(nsISupports* context);

    NS_IMETHOD OnStopBinding(nsISupports* context,
               nsresult aStatus,
               nsIString* aMsg);

protected:

    nsIChannel* m_pTransport;

};

#endif /* _nsHTTPRequestObserver_h_ */
