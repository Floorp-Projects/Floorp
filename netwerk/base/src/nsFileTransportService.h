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

#ifndef nsFileTransportService_h___
#define nsFileTransportService_h___

#include "nsIFileTransportService.h"
#include "nsISupportsArray.h"
#include "nsIThread.h"
#include "nscore.h"
#include "plevent.h"

class nsFileTransport;

////////////////////////////////////////////////////////////////////////////////

#define NS_FILE_TRANSPORT_WORKER_COUNT  4

class nsFileTransportService : public nsIFileTransportService
{
public:
    NS_DECL_ISUPPORTS

    // nsIFileTransportService methods:
    NS_IMETHOD CreateTransport(const char* path, 
                               nsITransport* *result);
    NS_IMETHOD ProcessPendingRequests(void);

    // nsFileTransportService methods:
    nsFileTransportService();
    virtual ~nsFileTransportService();

    nsresult Init();
    nsresult DispatchRequest(nsIRunnable* runnable);
    nsresult Suspend(nsFileTransport* request);
    nsresult Resume(nsFileTransport* request);

protected:
    nsIThreadPool*              mPool;
    nsISupportsArray*           mSuspended;

};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsFileTransportService_h___ */
