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

#ifndef nsSyncStreamListener_h__
#define nsSyncStreamListener_h__

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

class nsSyncStreamListener : public nsISyncStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISYNCSTREAMLISTENER

    // nsSyncStreamListener methods:
    nsSyncStreamListener()
        : mOutputStream(nsnull) {
        NS_INIT_REFCNT();
    }
    virtual ~nsSyncStreamListener();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsIOutputStream* GetOutputStream() { return mOutputStream; }

protected:
    nsCOMPtr<nsIOutputStream> mOutputStream;
};

#define NS_SYNC_STREAM_LISTENER_SEGMENT_SIZE    (4 * 1024)
#define NS_SYNC_STREAM_LISTENER_BUFFER_SIZE     (32 * 1024)

#endif // nsSyncStreamListener_h__
