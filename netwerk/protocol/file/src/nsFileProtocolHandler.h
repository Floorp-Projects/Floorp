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

#ifndef nsFileProtocolHandler_h___
#define nsFileProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsWeakReference.h"

class nsISupportsArray;
class nsIRunnable;
class nsFileChannel;
class nsIThreadPool;

#define NS_FILE_TRANSPORT_WORKER_COUNT  1//4

// {25029490-F132-11d2-9588-00805F369F95}
#define NS_FILEPROTOCOLHANDLER_CID                   \
{ /* fbc81170-1f69-11d3-9344-00104ba0fd40 */         \
    0xfbc81170,                                      \
    0x1f69,                                          \
    0x11d3,                                          \
    {0x93, 0x44, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsFileProtocolHandler : public nsIProtocolHandler, public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER

    nsFileProtocolHandler();
    virtual ~nsFileProtocolHandler();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();
};

#endif /* nsFileProtocolHandler_h___ */
