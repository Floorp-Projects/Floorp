/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAsyncStreamCopier_h__
#define nsAsyncStreamCopier_h__

#include "nsIAsyncStreamCopier.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIRequestObserver.h"
#include "mozilla/Mutex.h"
#include "nsStreamUtils.h"
#include "nsCOMPtr.h"

//-----------------------------------------------------------------------------

class nsAsyncStreamCopier : public nsIAsyncStreamCopier
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSIASYNCSTREAMCOPIER

    nsAsyncStreamCopier();
    virtual ~nsAsyncStreamCopier();

    //-------------------------------------------------------------------------
    // these methods may be called on any thread

    bool IsComplete(nsresult *status = nullptr);
    void   Complete(nsresult status);

private:

    static void OnAsyncCopyComplete(void *, nsresult);

    nsCOMPtr<nsIInputStream>       mSource;
    nsCOMPtr<nsIOutputStream>      mSink;

    nsCOMPtr<nsIRequestObserver>   mObserver;

    nsCOMPtr<nsIEventTarget>       mTarget;

    nsCOMPtr<nsISupports>          mCopierCtx;

    mozilla::Mutex                 mLock;

    nsAsyncCopyMode                mMode;
    uint32_t                       mChunkSize;
    nsresult                       mStatus;
    bool                           mIsPending;
    bool                           mCloseSource;
    bool                           mCloseSink;
};

#endif // !nsAsyncStreamCopier_h__
