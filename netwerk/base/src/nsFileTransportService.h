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

class nsISupportsArray;
class nsIRunnable;
class nsFileChannel;
class nsIThreadPool;

#define NS_FILE_TRANSPORT_WORKER_COUNT  1//4

// {25029490-F132-11d2-9588-00805F369F95}
#define NS_FileTransportService_CID                   \
{ /* fbc81170-1f69-11d3-9344-00104ba0fd40 */         \
    0xfbc81170,                                      \
    0x1f69,                                          \
    0x11d3,                                          \
    {0x93, 0x44, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

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

protected:
    nsIThreadPool*      mPool;
    nsISupportsArray*   mSuspended;
};

#endif /* nsFileTransportService_h___ */
