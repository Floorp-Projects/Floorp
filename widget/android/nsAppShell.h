/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShell_h__
#define nsAppShell_h__

#include <time.h>

#include <type_traits>
#include <utility>

#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"  // for mozilla::TimeDuration
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/jni/Natives.h"
#include "nsBaseAppShell.h"
#include "nsCOMPtr.h"
#include "nsIAndroidBridge.h"
#include "nsInterfaceHashtable.h"
#include "nsTArray.h"

namespace mozilla {
bool ProcessNextEvent();
void NotifyEvent();
}  // namespace mozilla

class nsWindow;

class nsAppShell : public nsBaseAppShell {
 public:
  struct Event : mozilla::LinkedListElement<Event> {
    static uint64_t GetTime() {
      timespec time;
      if (clock_gettime(CLOCK_MONOTONIC, &time)) {
        return 0ull;
      }
      return uint64_t(time.tv_sec) * 1000000000ull + time.tv_nsec;
    }

    uint64_t mPostTime{0};

    bool HasSameTypeAs(const Event* other) const {
      // Compare vtable addresses to determine same type.
      return *reinterpret_cast<const uintptr_t*>(this) ==
             *reinterpret_cast<const uintptr_t*>(other);
    }

    virtual ~Event() {}
    virtual void Run() = 0;

    virtual void PostTo(mozilla::LinkedList<Event>& queue) {
      queue.insertBack(this);
    }

    virtual bool IsUIEvent() const { return false; }
  };

  template <typename T>
  class LambdaEvent : public Event {
   protected:
    T lambda;

   public:
    explicit LambdaEvent(T&& l) : lambda(std::move(l)) {}
    void Run() override { lambda(); }
  };

  class ProxyEvent : public Event {
   protected:
    mozilla::UniquePtr<Event> baseEvent;

   public:
    explicit ProxyEvent(mozilla::UniquePtr<Event>&& event)
        : baseEvent(std::move(event)) {}

    void PostTo(mozilla::LinkedList<Event>& queue) override {
      baseEvent->PostTo(queue);
    }

    void Run() override { baseEvent->Run(); }
  };

  static nsAppShell* Get() {
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
  template <typename T, typename D>
  static void PostEvent(mozilla::UniquePtr<T, D>&& event) {
    mozilla::MutexAutoLock lock(*sAppShellLock);
    if (!sAppShell) {
      return;
    }
    sAppShell->mEventQueue.Post(std::move(event));
  }

  // Post a event that will call a lambda
  // e.g. PostEvent([=] { /* do something */ });
  template <typename T>
  static void PostEvent(T&& lambda) {
    mozilla::MutexAutoLock lock(*sAppShellLock);
    if (!sAppShell) {
      return;
    }
    sAppShell->mEventQueue.Post(
        mozilla::MakeUnique<LambdaEvent<T>>(std::move(lambda)));
  }

  // Post a event and wait for it to finish running on the Gecko thread.
  static bool SyncRunEvent(
      Event&& event,
      mozilla::UniquePtr<Event> (*eventFactory)(mozilla::UniquePtr<Event>&&) =
          nullptr,
      const mozilla::TimeDuration timeout = mozilla::TimeDuration::Forever());

  template <typename T>
  static std::enable_if_t<!std::is_base_of<Event, T>::value, void> SyncRunEvent(
      T&& lambda) {
    SyncRunEvent(LambdaEvent<T>(std::forward<T>(lambda)));
  }

  static already_AddRefed<nsIURI> ResolveURI(const nsCString& aUriStr);

 protected:
  static nsAppShell* sAppShell;
  static mozilla::StaticAutoPtr<mozilla::Mutex> sAppShellLock;

  virtual ~nsAppShell();

  NS_IMETHOD Exit() override;
  nsresult AddObserver(const nsAString& aObserverKey, nsIObserver* aObserver);

  class NativeCallbackEvent : public Event {
    // Capturing the nsAppShell instance is safe because if the app
    // shell is destroyed, this lambda will not be called either.
    nsAppShell* const appShell;

   public:
    explicit NativeCallbackEvent(nsAppShell* as) : appShell(as) {}
    void Run() override { appShell->NativeEventCallback(); }
  };

  void ScheduleNativeEventCallback() override {
    mEventQueue.Post(mozilla::MakeUnique<NativeCallbackEvent>(this));
  }

  class Queue {
   private:
    mozilla::Monitor mMonitor MOZ_UNANNOTATED;
    mozilla::LinkedList<Event> mQueue;

   public:
    enum { LATENCY_UI, LATENCY_OTHER, LATENCY_COUNT };
    Queue() : mMonitor("nsAppShell.Queue") {}

    void Signal() {
      mozilla::MonitorAutoLock lock(mMonitor);
      lock.NotifyAll();
    }

    void Post(mozilla::UniquePtr<Event>&& event) {
      MOZ_ASSERT(event && !event->isInList());

      mozilla::MonitorAutoLock lock(mMonitor);
      event->PostTo(mQueue);
      if (event->isInList()) {
        event->mPostTime = Event::GetTime();
        // Ownership of event object transfers to the queue.
        mozilla::Unused << event.release();
      }
      lock.NotifyAll();
    }

    mozilla::UniquePtr<Event> Pop(bool mayWait) {
      mozilla::MonitorAutoLock lock(mMonitor);

      if (mayWait && mQueue.isEmpty()) {
        lock.Wait();
      }

      // Ownership of event object transfers to the return value.
      mozilla::UniquePtr<Event> event(mQueue.popFirst());
      if (!event || !event->mPostTime) {
        return event;
      }

      return event;
    }

  } mEventQueue;

 private:
  mozilla::CondVar mSyncRunFinished;
  bool mSyncRunQuit;

  nsInterfaceHashtable<nsStringHashKey, nsIObserver> mObserversHash;
};

#endif  // nsAppShell_h__
