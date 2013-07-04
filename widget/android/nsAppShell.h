/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsBaseAppShell.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsInterfaceHashtable.h"
#include "nsIAndroidBridge.h"

namespace mozilla {
class AndroidGeckoEvent;
bool ProcessNextEvent();
void NotifyEvent();
}

class nsWindow;

class nsAppShell :
    public nsBaseAppShell
{
    typedef mozilla::CondVar CondVar;
    typedef mozilla::Mutex Mutex;

public:
    static nsAppShell *gAppShell;
    static mozilla::AndroidGeckoEvent *gEarlyEvent;

    nsAppShell();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOBSERVER

    nsresult Init();

    void NotifyNativeEvent();

    virtual bool ProcessNextNativeEvent(bool mayWait);

    void PostEvent(mozilla::AndroidGeckoEvent *event);
    void OnResume();

    nsresult AddObserver(const nsAString &aObserverKey, nsIObserver *aObserver);
    void ResendLastResizeEvent(nsWindow* aDest);

    void SetBrowserApp(nsIAndroidBrowserApp* aBrowserApp) {
        mBrowserApp = aBrowserApp;
    }

    void GetBrowserApp(nsIAndroidBrowserApp* *aBrowserApp) {
        *aBrowserApp = mBrowserApp;
    }

protected:
    virtual void ScheduleNativeEventCallback();
    virtual ~nsAppShell();

    Mutex mQueueLock;
    Mutex mCondLock;
    CondVar mQueueCond;
    mozilla::AndroidGeckoEvent *mQueuedDrawEvent;
    mozilla::AndroidGeckoEvent *mQueuedViewportEvent;
    bool mAllowCoalescingNextDraw;
    bool mAllowCoalescingTouches;
    nsTArray<mozilla::AndroidGeckoEvent *> mEventQueue;
    nsInterfaceHashtable<nsStringHashKey, nsIObserver> mObserversHash;

    mozilla::AndroidGeckoEvent *PopNextEvent();
    mozilla::AndroidGeckoEvent *PeekNextEvent();

    nsCOMPtr<nsIAndroidBrowserApp> mBrowserApp;
};

#endif // nsAppShell_h__

