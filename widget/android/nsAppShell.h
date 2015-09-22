/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include "mozilla/HangMonitor.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Monitor.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"
#include "mozilla/jni/Natives.h"
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
public:
    struct Event : mozilla::LinkedListElement<Event>
    {
        bool HasSameTypeAs(const Event* other) const
        {
            // Compare vtable addresses to determine same type.
            return *reinterpret_cast<const uintptr_t*>(this)
                    == *reinterpret_cast<const uintptr_t*>(other);
        }

        virtual ~Event() {}
        virtual void Run() = 0;

        virtual void PostTo(mozilla::LinkedList<Event>& queue)
        {
            queue.insertBack(this);
        }

        virtual mozilla::HangMonitor::ActivityType ActivityType() const
        {
            return mozilla::HangMonitor::kGeneralActivity;
        }
    };

    class LegacyGeckoEvent;

    template<typename T>
    class LambdaEvent : public Event
    {
    protected:
        T lambda;

    public:
        LambdaEvent(T&& l) : lambda(mozilla::Move(l)) {}
        void Run() override { return lambda(); }
    };

    static nsAppShell *gAppShell;

    nsAppShell();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOBSERVER

    nsresult Init();

    void NotifyNativeEvent();
    bool ProcessNextNativeEvent(bool mayWait) override;

    // Post a subclass of Event.
    // e.g. PostEvent(mozilla::MakeUnique<MyEvent>());
    template<typename T, typename D>
    void PostEvent(mozilla::UniquePtr<T, D>&& event)
    {
        mEventQueue.Post(mozilla::Move(event));
    }

    // Post a event that will call a lambda
    // e.g. PostEvent([=] { /* do something */ });
    template<typename T>
    void PostEvent(T&& lambda)
    {
        mEventQueue.Post(mozilla::MakeUnique<LambdaEvent<T>>(
                mozilla::Move(lambda)));
    }

    void PostEvent(mozilla::AndroidGeckoEvent* event);

    void ResendLastResizeEvent(nsWindow* aDest);

    void OnResume() {}

    void SetBrowserApp(nsIAndroidBrowserApp* aBrowserApp) {
        mBrowserApp = aBrowserApp;
    }

    void GetBrowserApp(nsIAndroidBrowserApp* *aBrowserApp) {
        *aBrowserApp = mBrowserApp;
    }

protected:
    virtual ~nsAppShell();

    nsresult AddObserver(const nsAString &aObserverKey, nsIObserver *aObserver);

    void ScheduleNativeEventCallback() override
    {
        // Capturing the nsAppShell instance is safe because if the app
        // shell is detroyed, this lambda will not be called either.
        PostEvent([this] {
            NativeEventCallback();
        });
    }

    class Queue
    {
    public:
        // XXX need to be public for the mQueuedViewportEvent ugliness.
        mozilla::Monitor mMonitor;

    private:
        mozilla::LinkedList<Event> mQueue;

    public:
        Queue() : mMonitor("nsAppShell.Queue")
        {}

        void Signal()
        {
            mozilla::MonitorAutoLock lock(mMonitor);
            lock.NotifyAll();
        }

        void Post(mozilla::UniquePtr<Event>&& event)
        {
            MOZ_ASSERT(event && !event->isInList());

            mozilla::MonitorAutoLock lock(mMonitor);
            event->PostTo(mQueue);
            if (event->isInList()) {
                // Ownership of event object transfers to the queue.
                mozilla::unused << event.release();
            }
            lock.NotifyAll();
        }

        mozilla::UniquePtr<Event> Pop(bool mayWait)
        {
            mozilla::MonitorAutoLock lock(mMonitor);

            if (mayWait && mQueue.isEmpty()) {
                lock.Wait();
            }
            // Ownership of event object transfers to the return value.
            return mozilla::UniquePtr<Event>(mQueue.popFirst());
        }

    } mEventQueue;

    Event* mQueuedViewportEvent;
    bool mAllowCoalescingTouches;

    nsCOMPtr<nsIAndroidBrowserApp> mBrowserApp;
    nsInterfaceHashtable<nsStringHashKey, nsIObserver> mObserversHash;
};

// Class that implement native JNI methods can inherit from
// UsesGeckoThreadProxy to have the native call forwarded
// automatically to the Gecko thread.
class UsesGeckoThreadProxy : public mozilla::jni::UsesNativeCallProxy
{
public:
    template<class Functor>
    static void OnNativeCall(Functor&& call)
    {
        nsAppShell::gAppShell->PostEvent(mozilla::Move(call));
    }
};

#endif // nsAppShell_h__

