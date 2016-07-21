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
#include "mozilla/StaticPtr.h"
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
        typedef mozilla::HangMonitor::ActivityType Type;

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

        virtual Type ActivityType() const
        {
            return Type::kGeneralActivity;
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

    class ProxyEvent : public Event
    {
    protected:
        mozilla::UniquePtr<Event> baseEvent;

    public:
        ProxyEvent(mozilla::UniquePtr<Event>&& event)
            : baseEvent(mozilla::Move(event))
        {}

        void PostTo(mozilla::LinkedList<Event>& queue) override
        {
            baseEvent->PostTo(queue);
        }

        void Run() override
        {
            baseEvent->Run();
        }
    };

    static nsAppShell* Get()
    {
        MOZ_ASSERT(NS_IsMainThread());
        return sAppShell;
    }

    nsAppShell();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOBSERVER

    nsresult Init();

    void NotifyNativeEvent();
    bool ProcessNextNativeEvent(bool mayWait) override;

    // Post a subclass of Event.
    // e.g. PostEvent(mozilla::MakeUnique<MyEvent>());
    template<typename T, typename D>
    static void PostEvent(mozilla::UniquePtr<T, D>&& event)
    {
        mozilla::MutexAutoLock lock(*sAppShellLock);
        if (!sAppShell) {
            return;
        }
        sAppShell->mEventQueue.Post(mozilla::Move(event));
    }

    // Post a event that will call a lambda
    // e.g. PostEvent([=] { /* do something */ });
    template<typename T>
    static void PostEvent(T&& lambda)
    {
        mozilla::MutexAutoLock lock(*sAppShellLock);
        if (!sAppShell) {
            return;
        }
        sAppShell->mEventQueue.Post(mozilla::MakeUnique<LambdaEvent<T>>(
                mozilla::Move(lambda)));
    }

    static void PostEvent(mozilla::AndroidGeckoEvent* event);

    // Post a event and wait for it to finish running on the Gecko thread.
    static void SyncRunEvent(Event&& event,
                             mozilla::UniquePtr<Event>(*eventFactory)(
                                    mozilla::UniquePtr<Event>&&) = nullptr);

    static already_AddRefed<nsIURI> ResolveURI(const nsCString& aUriStr);

    void SetBrowserApp(nsIAndroidBrowserApp* aBrowserApp) {
        mBrowserApp = aBrowserApp;
    }

    void GetBrowserApp(nsIAndroidBrowserApp* *aBrowserApp) {
        *aBrowserApp = mBrowserApp;
    }

protected:
    static nsAppShell* sAppShell;
    static mozilla::StaticAutoPtr<mozilla::Mutex> sAppShellLock;

    virtual ~nsAppShell();

    nsresult AddObserver(const nsAString &aObserverKey, nsIObserver *aObserver);

    class NativeCallbackEvent : public Event
    {
        // Capturing the nsAppShell instance is safe because if the app
        // shell is detroyed, this lambda will not be called either.
        nsAppShell* const appShell;

    public:
        NativeCallbackEvent(nsAppShell* as) : appShell(as) {}
        void Run() override { appShell->NativeEventCallback(); }
    };

    void ScheduleNativeEventCallback() override
    {
        mEventQueue.Post(mozilla::MakeUnique<NativeCallbackEvent>(this));
    }

    class Queue
    {
    private:
        mozilla::Monitor mMonitor;
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
                mozilla::Unused << event.release();
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

    mozilla::CondVar mSyncRunFinished;
    bool mSyncRunQuit;

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
        nsAppShell::PostEvent(mozilla::Move(call));
    }
};

#endif // nsAppShell_h__

