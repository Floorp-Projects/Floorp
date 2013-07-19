/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPROTECTEDAUTHTHREAD_H_
#define NSPROTECTEDAUTHTHREAD_H_

#include <nsCOMPtr.h>
#include "keyhi.h"
#include "nspr.h"

#include "mozilla/Mutex.h"
#include "nsIProtectedAuthThread.h"

class nsIRunnable;

class nsProtectedAuthThread : public nsIProtectedAuthThread
{
private:
    mozilla::Mutex mMutex;

    nsCOMPtr<nsIRunnable> mNotifyObserver;

    bool        mIAmRunning;
    bool        mLoginReady;

    PRThread    *mThreadHandle;

    // Slot to do authentication on
    PK11SlotInfo*   mSlot;

    // Result of the authentication
    SECStatus       mLoginResult;

public:

    nsProtectedAuthThread();
    virtual ~nsProtectedAuthThread();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIPROTECTEDAUTHTHREAD

    // Sets parameters for the thread
    void SetParams(PK11SlotInfo *slot);

    // Gets result of the protected authentication operation
    SECStatus GetResult();

    void Join(void);

    void Run(void);
};

#endif // NSPROTECTEDAUTHTHREAD_H_
