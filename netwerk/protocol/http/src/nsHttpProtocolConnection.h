/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsHttpProtocolConnection_h___
#define nsHttpProtocolConnection_h___

#include "nsIHttpProtocolConnection.h"

class nsIConnectionGroup;
class nsIHttpEventSink;

class nsHttpProtocolConnection : public nsIHttpProtocolConnection
{
public:
    NS_DECL_ISUPPORTS

    // nsICancelable methods:
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);

    // nsIProtocolConnection methods:
    NS_IMETHOD Open(nsIUrl* url, nsISupports* eventSink);
    NS_IMETHOD GetContentType(char* *contentType);
    NS_IMETHOD GetInputStream(nsIInputStream* *result);
    NS_IMETHOD GetOutputStream(nsIOutputStream* *result);
    NS_IMETHOD AsyncWrite(nsIInputStream* data, PRUint32 count,
                          nsresult (*callback)(void* closure, PRUint32 count),
                          void* closure);

    // nsIHttpProtocolConnection methods:
    NS_IMETHOD Get(void);
    NS_IMETHOD GetByteRange(PRUint32 from, PRUint32 to);
    NS_IMETHOD Put(void);
    NS_IMETHOD Post(void);

    // nsHttpProtocolConnection methods:
    nsHttpProtocolConnection();
    virtual ~nsHttpProtocolConnection();

    nsresult Init(nsIUrl* url, nsISupports* eventSink);

protected:
    nsIUrl*             mUrl;
    nsIHttpEventSink*   mEventSink;
};

#endif /* nsHttpProtocolConnection_h___ */
