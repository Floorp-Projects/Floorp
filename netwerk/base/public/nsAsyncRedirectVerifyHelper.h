/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org networking code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Honza Bambas <honzab@firemni.cz>
 *    Bjarne Geir Herland <bjarne@runitsoft.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

class nsIChannel;

/**
 * This class simplifies call of OnChannelRedirect of IOService and
 * the sink bound with the channel being redirected while the result of
 * redirect decision is returned through the callback.
 */
class nsAsyncRedirectVerifyHelper : public nsIRunnable,
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
                                       PRUint32 flags);
 
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
                  PRUint32 flags,
                  bool synchronize = false);

protected:
    nsCOMPtr<nsIChannel> mOldChan;
    nsCOMPtr<nsIChannel> mNewChan;
    PRUint32 mFlags;
    bool mWaitingForRedirectCallback;
    nsCOMPtr<nsIThread>      mCallbackThread;
    bool                     mCallbackInitiated;
    PRInt32                  mExpectedCallbacks;
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
        mCallback = nsnull;
    }
private:
    nsIAsyncVerifyRedirectCallback* mCallback;
    nsresult mResult;
};

#endif
