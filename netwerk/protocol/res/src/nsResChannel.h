/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsResChannel_h__
#define nsResChannel_h__

#include "nsIResChannel.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsIResProtocolHandler.h"
#include "nsIURI.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsAutoLock.h"
#ifdef DEBUG
#include "prthread.h"
#endif

class nsResChannel : public nsIResChannel,
                     public nsIFileChannel,
                     public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIFILECHANNEL
    NS_DECL_NSIRESCHANNEL
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsResChannel();
    virtual ~nsResChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsIResProtocolHandler* handler, nsIURI* uri);

protected:
    class Substitutions {
    public:
        Substitutions() : mCurrentIndex(0) {}
        ~Substitutions() {}

        nsresult Init();
        nsresult Next(char* *result);
    protected:
        nsCOMPtr<nsIURI>                mResourceURI;
        nsCOMPtr<nsISupportsArray>      mSubstitutions;
        PRUint32                        mCurrentIndex;
    };
    friend class Substitutions;

#define GET_SUBSTITUTIONS_CHANNEL(_this) \
    ((nsResChannel*)((char*)(_this) - offsetof(nsResChannel, mSubstitutions)))

    enum State {
        QUIESCENT,
        ASYNC_READ,
        ASYNC_WRITE
    };

    nsIStreamListener* GetUserListener() {
        // this method doesn't addref the listener
        NS_ASSERTION(mState == ASYNC_READ, "wrong state");
        // this cast is safe because we set mUserObserver in AsyncRead
        nsIStreamObserver* obs = mUserObserver;
        nsIStreamListener* listener = NS_STATIC_CAST(nsIStreamListener*, obs);
        return listener;
    }

    nsIStreamProvider* GetUserProvider() {
        // this method doesn't addref the provider
        NS_ASSERTION(mState == ASYNC_WRITE, "wrong state");
        // this cast is safe because we set mUserObserver in AsyncWrite
        nsIStreamObserver* obs = mUserObserver;
        nsIStreamProvider* provider = NS_STATIC_CAST(nsIStreamProvider*, obs);
        return provider;
    }

    nsresult EnsureNextResolvedChannel();
    nsresult EndRequest(nsresult aStatus, const PRUnichar* aStatusArg);

protected:
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mResourceURI;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    PRUint32                            mLoadAttributes;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsISupports>               mOwner;

    nsCOMPtr<nsIResProtocolHandler>     mHandler;
    nsCOMPtr<nsIChannel>                mResolvedChannel;
    State                               mState;
    Substitutions                       mSubstitutions;
    nsCOMPtr<nsIStreamObserver>         mUserObserver;
    nsCOMPtr<nsISupports>               mUserContext;
    nsresult                            mStatus;
#ifdef DEBUG
    PRThread*                           mInitiator;
#endif
};

#endif // nsResChannel_h__
