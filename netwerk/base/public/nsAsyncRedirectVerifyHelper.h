/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAsyncRedirectVerifyHelper_h
#define nsAsyncRedirectVerifyHelper_h

#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

class nsIChannel;

/**
 * This class simplifies call of OnChannelRedirect of IOService and
 * the sink bound with the channel being redirected while the result of
 * redirect decision is returned through the callback.
 */
class nsAsyncRedirectVerifyHelper MOZ_FINAL : public nsIRunnable,
                                              public nsIAsyncVerifyRedirectCallback
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

public:
    nsAsyncRedirectVerifyHelper();

    /*
     * Calls AsyncOnChannelRedirect() on the given sink with the given
     * channels and flags. Keeps track of number of async callbacks to expect.
     */
    nsresult DelegateOnChannelRedirect(nsIChannelEventSink *sink,
                                       nsIChannel *oldChannel, 
                                       nsIChannel *newChannel,
                                       uint32_t flags);
 
    /**
     * Initialize and run the chain of AsyncOnChannelRedirect calls. OldChannel
     * is QI'ed for nsIAsyncVerifyRedirectCallback. The result of the redirect
     * decision is passed through this interface back to the oldChannel.
     *
     * @param oldChan
     *    channel being redirected, MUST implement
     *    nsIAsyncVerifyRedirectCallback
     * @param newChan
     *    target of the redirect channel
     * @param flags
     *    redirect flags
     * @param synchronize
     *    set to TRUE if you want the Init method wait synchronously for
     *    all redirect callbacks
     */
    nsresult Init(nsIChannel* oldChan,
                  nsIChannel* newChan,
                  uint32_t flags,
                  bool synchronize = false);

protected:
    nsCOMPtr<nsIChannel> mOldChan;
    nsCOMPtr<nsIChannel> mNewChan;
    uint32_t mFlags;
    bool mWaitingForRedirectCallback;
    nsCOMPtr<nsIThread>      mCallbackThread;
    bool                     mCallbackInitiated;
    int32_t                  mExpectedCallbacks;
    nsresult                 mResult; // value passed to callback

    void InitCallback();
    
    /**
     * Calls back to |oldChan| as described in Init()
     */
    void ExplicitCallback(nsresult result);

private:
    ~nsAsyncRedirectVerifyHelper();
    
    bool IsOldChannelCanceled();
};

/*
 * Helper to make the call-stack handle some control-flow for us
 */
class nsAsyncRedirectAutoCallback
{
public:
    nsAsyncRedirectAutoCallback(nsIAsyncVerifyRedirectCallback* aCallback)
        : mCallback(aCallback)
    {
        mResult = NS_OK;
    }
    ~nsAsyncRedirectAutoCallback()
    {
        if (mCallback)
            mCallback->OnRedirectVerifyCallback(mResult);
    }
    /*
     * Call this is you want it to call back with a different result-code
     */
    void SetResult(nsresult aRes)
    {
        mResult = aRes;
    }
    /*
     * Call this is you want to avoid the callback
     */
    void DontCallback()
    {
        mCallback = nullptr;
    }
private:
    nsIAsyncVerifyRedirectCallback* mCallback;
    nsresult mResult;
};

#endif
