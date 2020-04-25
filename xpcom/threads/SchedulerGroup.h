/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SchedulerGroup_h
#define mozilla_SchedulerGroup_h

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Queue.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

class nsIEventTarget;
class nsIRunnable;
class nsISerialEventTarget;

namespace mozilla {
class AbstractThread;
namespace dom {
class DocGroup;
}  // namespace dom

#define NS_SCHEDULERGROUPRUNNABLE_IID                \
  {                                                  \
    0xd31b7420, 0x872b, 0x4cfb, {                    \
      0xa9, 0xc6, 0xae, 0x4c, 0x0f, 0x06, 0x36, 0x74 \
    }                                                \
  }

// The "main thread" in Gecko will soon be a set of cooperatively scheduled
// "fibers". Global state in Gecko will be partitioned into a series of "groups"
// (with roughly one group per tab). Runnables will be annotated with the set of
// groups that they touch. Two runnables may run concurrently on different
// fibers as long as they touch different groups.
//
// A SchedulerGroup is an abstract class to represent a "group". Essentially the
// only functionality offered by a SchedulerGroup is the ability to dispatch
// runnables to the group. DocGroup, and SystemGroup are the concrete
// implementations of SchedulerGroup.
class SchedulerGroup {
 public:
  SchedulerGroup();

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  class Runnable final : public mozilla::Runnable, public nsIRunnablePriority {
   public:
    Runnable(already_AddRefed<nsIRunnable>&& aRunnable,
             dom::DocGroup* aDocGroup);

    dom::DocGroup* DocGroup() const;

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    NS_IMETHOD GetName(nsACString& aName) override;
#endif

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIRUNNABLEPRIORITY

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_SCHEDULERGROUPRUNNABLE_IID);

   private:
    friend class SchedulerGroup;

    ~Runnable() = default;

    nsCOMPtr<nsIRunnable> mRunnable;
    RefPtr<dom::DocGroup> mDocGroup;
  };
  friend class Runnable;

  static nsresult Dispatch(TaskCategory aCategory,
                           already_AddRefed<nsIRunnable>&& aRunnable);

  static nsresult UnlabeledDispatch(TaskCategory aCategory,
                                    already_AddRefed<nsIRunnable>&& aRunnable);

  static void MarkVsyncReceived();

  static void MarkVsyncRan();

  static nsresult DispatchWithDocGroup(
      TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable,
      dom::DocGroup* aDocGroup);

 protected:
  static nsresult InternalUnlabeledDispatch(
      TaskCategory aCategory, already_AddRefed<Runnable>&& aRunnable);

  static nsresult LabeledDispatch(TaskCategory aCategory,
                                  already_AddRefed<nsIRunnable>&& aRunnable,
                                  dom::DocGroup* aDocGroup);

  // Shuts down this dispatcher. If aXPCOMShutdown is true, invalidates this
  // dispatcher.
  void Shutdown(bool aXPCOMShutdown);

  bool mIsRunning;

  // Number of events that are currently enqueued for this SchedulerGroup
  // (across all queues).
  size_t mEventCount = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SchedulerGroup::Runnable,
                              NS_SCHEDULERGROUPRUNNABLE_IID);

}  // namespace mozilla

#endif  // mozilla_SchedulerGroup_h
