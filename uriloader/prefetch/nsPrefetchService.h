/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrefetchService_h__
#define nsPrefetchService_h__

#include "nsCPrefetchService.h"
#include "nsIObserver.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIRedirectResultListener.h"
#include "nsIWebProgressListener.h"
#include "nsIStreamListener.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIDOMLoadStatus.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsPrefetchService;
class nsPrefetchListener;
class nsPrefetchNode;

//-----------------------------------------------------------------------------
// nsPrefetchService
//-----------------------------------------------------------------------------

class nsPrefetchService MOZ_FINAL : public nsIPrefetchService
                                  , public nsIWebProgressListener
                                  , public nsIObserver
                                  , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPREFETCHSERVICE
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIOBSERVER

    nsPrefetchService();

    nsresult Init();
    void     ProcessNextURI();

    nsPrefetchNode *GetCurrentNode() { return mCurrentNode.get(); }
    nsPrefetchNode *GetQueueHead() { return mQueueHead; }

    void NotifyLoadRequested(nsPrefetchNode *node);
    void NotifyLoadCompleted(nsPrefetchNode *node);

private:
    ~nsPrefetchService();

    nsresult Prefetch(nsIURI *aURI,
                      nsIURI *aReferrerURI,
                      nsIDOMNode *aSource,
                      bool aExplicit);

    void     AddProgressListener();
    void     RemoveProgressListener();
    nsresult EnqueueURI(nsIURI *aURI, nsIURI *aReferrerURI,
                        nsIDOMNode *aSource, nsPrefetchNode **node);
    nsresult EnqueueNode(nsPrefetchNode *node);
    nsresult DequeueNode(nsPrefetchNode **node);
    void     EmptyQueue();

    void     StartPrefetching();
    void     StopPrefetching();

    nsPrefetchNode                   *mQueueHead;
    nsPrefetchNode                   *mQueueTail;
    nsRefPtr<nsPrefetchNode>          mCurrentNode;
    PRInt32                           mStopCount;
    // true if pending document loads have ever reached zero.
    PRInt32                           mHaveProcessed;
    bool                              mDisabled;
};

//-----------------------------------------------------------------------------
// nsPrefetchNode
//-----------------------------------------------------------------------------

class nsPrefetchNode MOZ_FINAL : public nsIDOMLoadStatus
                               , public nsIStreamListener
                               , public nsIInterfaceRequestor
                               , public nsIChannelEventSink
                               , public nsIRedirectResultListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMLOADSTATUS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIREDIRECTRESULTLISTENER

    nsPrefetchNode(nsPrefetchService *aPrefetchService,
                   nsIURI *aURI,
                   nsIURI *aReferrerURI,
                   nsIDOMNode *aSource);

    ~nsPrefetchNode() {}

    nsresult OpenChannel();
    nsresult CancelChannel(nsresult error);

    nsPrefetchNode             *mNext;
    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsIURI>            mReferrerURI;
    nsCOMPtr<nsIWeakReference>  mSource;

private:
    nsRefPtr<nsPrefetchService> mService;
    nsCOMPtr<nsIChannel>        mChannel;
    nsCOMPtr<nsIChannel>        mRedirectChannel;
    PRUint16                    mState;
    PRInt32                     mBytesRead;
};

#endif // !nsPrefetchService_h__
