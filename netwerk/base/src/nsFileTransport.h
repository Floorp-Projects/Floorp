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

#ifndef nsFileTransport_h___
#define nsFileTransport_h___

#include "nsITransport.h"
#include "nsIThread.h"
#include "plevent.h"

class nsFileTransportService;
class nsIProtocolConnection;
class nsIInputStream;
class nsIString;

class nsFileTransport : public nsITransport, public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS

    // nsICancelable methods:
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);

    // nsITransport methods:

    // nsIRunnable methods:
    NS_IMETHOD Run(void);

    // nsFileTransport methods:
    nsFileTransport();
    virtual ~nsFileTransport();

    nsresult Init(const char* path,
                  nsIStreamListener* listener,
                  PLEventQueue* appEventQueue,
                  nsIProtocolConnection* connection);

protected:
    nsIProtocolConnection*      mConnection;
    char*                       mPath;
    nsIStreamListener*          mListener;
    PRBool                      mCanceled;
    
};

#define NS_FILE_TRANSPORT_BUFFER_SIZE   (4*1024)

#endif /* nsFileTransport_h___ */
