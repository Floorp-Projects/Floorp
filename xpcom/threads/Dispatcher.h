/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Dispatcher_h
#define mozilla_Dispatcher_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/TaskCategory.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

class nsIEventTarget;
class nsIRunnable;

namespace mozilla {
class AbstractThread;
namespace dom {
class TabGroup;
}

// The "main thread" in Gecko will soon be a set of cooperatively scheduled
// "fibers". Global state in Gecko will be partitioned into a series of "groups"
// (with roughly one group per tab). Runnables will be annotated with the set of
// groups that they touch. Two runnables may run concurrently on different
// fibers as long as they touch different groups.
//
// A Dispatcher is an abstract class to represent a "group". Essentially the
// only functionality offered by a Dispatcher is the ability to dispatch
// runnables to the group. TabGroup, DocGroup, and SystemGroup are the concrete
// implementations of Dispatcher.
class Dispatcher
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Implementations of this method must be safe to call off the main thread.
  virtual nsresult Dispatch(const char* aName,
                            TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) = 0;

  // Implementations of this method must be safe to call off the main thread.
  // The returned nsIEventTarget must also be usable from any thread.
  virtual nsIEventTarget* EventTargetFor(TaskCategory aCategory) const = 0;

  // Must always be called on the main thread. The returned AbstractThread can
  // always be used off the main thread.
  AbstractThread* AbstractMainThreadFor(TaskCategory aCategory);

  // This method performs a safe cast. It returns null if |this| is not of the
  // requested type.
  virtual dom::TabGroup* AsTabGroup() { return nullptr; }

  static nsresult UnlabeledDispatch(const char* aName,
                                    TaskCategory aCategory,
                                    already_AddRefed<nsIRunnable>&& aRunnable);

protected:
  // Implementations are guaranteed that this method is called on the main
  // thread.
  virtual AbstractThread* AbstractMainThreadForImpl(TaskCategory aCategory) = 0;
};

class ValidatingDispatcher : public Dispatcher
{
public:
  ValidatingDispatcher();

  nsresult Dispatch(const char* aName,
                    TaskCategory aCategory,
                    already_AddRefed<nsIRunnable>&& aRunnable) override;

  nsIEventTarget* EventTargetFor(TaskCategory aCategory) const override;

protected:
  // Implementations are guaranteed that this method is called on the main
  // thread.
  AbstractThread* AbstractMainThreadForImpl(TaskCategory aCategory) override;

  // Helper method to create an event target specific to a particular TaskCategory.
  virtual already_AddRefed<nsIEventTarget>
  CreateEventTargetFor(TaskCategory aCategory);

  // Given an event target returned by |dispatcher->CreateEventTargetFor|, this
  // function returns |dispatcher|.
  static ValidatingDispatcher* FromEventTarget(nsIEventTarget* aEventTarget);

  void CreateEventTargets(bool aNeedValidation);
  void Shutdown();

  nsCOMPtr<nsIEventTarget> mEventTargets[size_t(TaskCategory::Count)];
  RefPtr<AbstractThread> mAbstractThreads[size_t(TaskCategory::Count)];
};

} // namespace mozilla

#endif // mozilla_dom_Dispatcher_h
