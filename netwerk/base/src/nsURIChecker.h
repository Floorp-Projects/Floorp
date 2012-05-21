/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsURIChecker_h__
#define nsURIChecker_h__

#include "nsIURIChecker.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"

//-----------------------------------------------------------------------------

class nsURIChecker : public nsIURIChecker,
                     public nsIStreamListener,
                     public nsIChannelEventSink,
                     public nsIInterfaceRequestor
{
public:
    nsURIChecker();
    virtual ~nsURIChecker() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIURICHECKER
    NS_DECL_NSIREQUEST
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR

protected:
    nsCOMPtr<nsIChannel>         mChannel;
    nsCOMPtr<nsIRequestObserver> mObserver;
    nsCOMPtr<nsISupports>        mObserverContext;
    nsresult                     mStatus;
    bool                         mIsPending;
    bool                         mAllowHead;

    void     SetStatusAndCallBack(nsresult aStatus);
    nsresult CheckStatus();
};

#endif // nsURIChecker_h__
