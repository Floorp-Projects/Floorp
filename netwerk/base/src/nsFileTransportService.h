/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsFileTransportService_h___
#define nsFileTransportService_h___

#include "nsIFileTransportService.h"
#include "nsIThreadPool.h"
#include "nsISupportsArray.h"

#define NS_FILE_TRANSPORT_WORKER_COUNT_MIN  1
#define NS_FILE_TRANSPORT_WORKER_COUNT_MAX  4//16

class nsFileTransportService : public nsIFileTransportService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFILETRANSPORTSERVICE

    // nsFileTransportService methods:
    nsFileTransportService();
    virtual ~nsFileTransportService();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();

    PRInt32   mConnectedTransports;
    PRInt32   mTotalTransports;
    PRInt32   mInUseTransports;
    
protected:
    nsCOMPtr<nsIThreadPool>     mPool;
    nsCOMPtr<nsISupportsArray>  mOpened;
    nsCOMPtr<nsISupportsArray>  mSuspended;
};

#endif /* nsFileTransportService_h___ */
