/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAsyncStreamCopier_h__
#define nsAsyncStreamCopier_h__

#include "nsIAsyncStreamCopier.h"
#include "nsIAsyncStreamCopier2.h"
#include "mozilla/Mutex.h"
#include "nsStreamUtils.h"
#include "nsCOMPtr.h"

class nsIRequestObserver;

//-----------------------------------------------------------------------------

class nsAsyncStreamCopier MOZ_FINAL : public nsIAsyncStreamCopier,
                                      nsIAsyncStreamCopier2
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSIASYNCSTREAMCOPIER

    // nsIAsyncStreamCopier2
    // We declare it by hand instead of NS_DECL_NSIASYNCSTREAMCOPIER2
    // as nsIAsyncStreamCopier2 duplicates methods of nsIAsyncStreamCopier
    NS_IMETHOD Init(nsIInputStream *aSource,
                    nsIOutputStream *aSink,
                    nsIEventTarget *aTarget,
                    uint32_t aChunkSize,
                    bool aCloseSource,
                    bool aCloseSink);

    nsAsyncStreamCopier();

    //-------------------------------------------------------------------------
    // these methods may be called on any thread

    bool IsComplete(nsresult *status = nullptr);
    void   Complete(nsresult status);

private:
    virtual ~nsAsyncStreamCopier();

    nsresult InitInternal(nsIInputStream *source,
                          nsIOutputStream *sink,
                          nsIEventTarget *target,
                          uint32_t chunkSize,
                          bool closeSource,
                          bool closeSink);

    static void OnAsyncCopyComplete(void *, nsresult);

    void AsyncCopyInternal();
    nsresult ApplyBufferingPolicy();
    nsIRequest* AsRequest();

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
    bool                           mShouldSniffBuffering;

    friend class ProceedWithAsyncCopy;
    friend class AsyncApplyBufferingPolicyEvent;
};

#endif // !nsAsyncStreamCopier_h__
