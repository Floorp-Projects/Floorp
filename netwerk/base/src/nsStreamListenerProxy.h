/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsStreamListenerProxy_h__
#define nsStreamListenerProxy_h__

#include "nsRequestObserverProxy.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRequest.h"
#include "nsCOMPtr.h"

class nsStreamListenerProxy : public nsIStreamListenerProxy
                            , public nsIInputStreamObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMLISTENERPROXY
    NS_DECL_NSIINPUTSTREAMOBSERVER

    nsStreamListenerProxy();
    virtual ~nsStreamListenerProxy();

    nsresult GetListener(nsIStreamListener **);

    void     SetListenerStatus(nsresult status) { mListenerStatus = status; }
    nsresult GetListenerStatus() { return mListenerStatus; }

    PRUint32 GetPendingCount();

protected:
    nsRequestObserverProxy      *mObserverProxy;

    nsCOMPtr<nsIInputStream>     mPipeIn;
    nsCOMPtr<nsIOutputStream>    mPipeOut;

    nsCOMPtr<nsIRequest>         mRequestToResume;
    PRLock                      *mLock;
    PRUint32                     mPendingCount;
    PRBool                       mPipeEmptied;
    nsresult                     mListenerStatus;
};

#endif // nsStreamListenerProxy_h__
