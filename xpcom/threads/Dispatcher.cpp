/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Dispatcher.h"

#include "jsfriendapi.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Move.h"
#include "nsINamed.h"
#include "nsQueryObject.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsThreadUtils.h"

using namespace mozilla;

class ValidatingDispatcher::Runnable final : public mozilla::Runnable
{
public:
  Runnable(already_AddRefed<nsIRunnable>&& aRunnable,
           ValidatingDispatcher* aDispatcher);

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsIRunnable> mRunnable;
  RefPtr<ValidatingDispatcher> mDispatcher;
};

/* DispatcherEventTarget */

namespace {

#define NS_DISPATCHEREVENTTARGET_IID \
{ 0xbf4e36c8, 0x7d04, 0x4ef4, \
  { 0xbb, 0xd8, 0x11, 0x09, 0x0a, 0xdb, 0x4d, 0xf7 } }

class DispatcherEventTarget final : public nsIEventTarget
{
  RefPtr<ValidatingDispatcher> mDispatcher;
  TaskCategory mCategory;

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DISPATCHEREVENTTARGET_IID)

  DispatcherEventTarget(ValidatingDispatcher* aDispatcher, TaskCategory aCategory)
   : mDispatcher(aDispatcher)
   , mCategory(aCategory)
  {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET

  ValidatingDispatcher* Dispatcher() const { return mDispatcher; }

private:
  ~DispatcherEventTarget() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(DispatcherEventTarget, NS_DISPATCHEREVENTTARGET_IID)

} // namespace

NS_IMPL_ISUPPORTS(DispatcherEventTarget, DispatcherEventTarget, nsIEventTarget)

NS_IMETHODIMP
DispatcherEventTarget::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags)
{
  return Dispatch(do_AddRef(aRunnable), aFlags);
}

NS_IMETHODIMP
DispatcherEventTarget::Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags)
{
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }
  return mDispatcher->Dispatch(nullptr, mCategory, Move(aRunnable));
}

NS_IMETHODIMP
DispatcherEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DispatcherEventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  *aIsOnCurrentThread = NS_IsMainThread();
  return NS_OK;
}

AbstractThread*
Dispatcher::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return AbstractMainThreadForImpl(aCategory);
}

/* static */ nsresult
Dispatcher::UnlabeledDispatch(const char* aName,
                              TaskCategory aCategory,
                              already_AddRefed<nsIRunnable>&& aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  if (aName) {
    if (nsCOMPtr<nsINamed> named = do_QueryInterface(runnable)) {
      named->SetName(aName);
    }
  }
  if (NS_IsMainThread()) {
    return NS_DispatchToCurrentThread(runnable.forget());
  } else {
    return NS_DispatchToMainThread(runnable.forget());
  }
}

ValidatingDispatcher* ValidatingDispatcher::sRunningDispatcher;

ValidatingDispatcher::ValidatingDispatcher()
 : mAccessValid(false)
{
}

nsresult
ValidatingDispatcher::Dispatch(const char* aName,
                               TaskCategory aCategory,
                               already_AddRefed<nsIRunnable>&& aRunnable)
{
  return LabeledDispatch(aName, aCategory, Move(aRunnable));
}

nsIEventTarget*
ValidatingDispatcher::EventTargetFor(TaskCategory aCategory) const
{
  MOZ_ASSERT(aCategory != TaskCategory::Count);
  return mEventTargets[size_t(aCategory)];
}

AbstractThread*
ValidatingDispatcher::AbstractMainThreadForImpl(TaskCategory aCategory)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCategory != TaskCategory::Count);

  if (!mAbstractThreads[size_t(aCategory)]) {
    mAbstractThreads[size_t(aCategory)] =
      AbstractThread::CreateEventTargetWrapper(mEventTargets[size_t(aCategory)],
                                               /* aDrainDirectTasks = */ true);
  }

  return mAbstractThreads[size_t(aCategory)];
}

void
ValidatingDispatcher::CreateEventTargets(bool aNeedValidation)
{
  for (size_t i = 0; i < size_t(TaskCategory::Count); i++) {
    TaskCategory category = static_cast<TaskCategory>(i);
    if (!aNeedValidation) {
      // The chrome TabGroup dispatches directly to the main thread. This means
      // that we don't have to worry about cyclical references when cleaning up
      // the chrome TabGroup.
      mEventTargets[i] = do_GetMainThread();
    } else {
      mEventTargets[i] = CreateEventTargetFor(category);
    }
  }
}

void
ValidatingDispatcher::Shutdown()
{
  // There is a RefPtr cycle TabGroup -> DispatcherEventTarget -> TabGroup. To
  // avoid leaks, we need to break the chain somewhere. We shouldn't be using
  // the ThrottledEventQueue for this TabGroup when no windows belong to it,
  // so it's safe to null out the queue here.
  for (size_t i = 0; i < size_t(TaskCategory::Count); i++) {
    mEventTargets[i] = do_GetMainThread();
    mAbstractThreads[i] = nullptr;
  }
}

already_AddRefed<nsIEventTarget>
ValidatingDispatcher::CreateEventTargetFor(TaskCategory aCategory)
{
  RefPtr<DispatcherEventTarget> target =
    new DispatcherEventTarget(this, aCategory);
  return target.forget();
}

/* static */ ValidatingDispatcher*
ValidatingDispatcher::FromEventTarget(nsIEventTarget* aEventTarget)
{
  RefPtr<DispatcherEventTarget> target = do_QueryObject(aEventTarget);
  if (!target) {
    return nullptr;
  }
  return target->Dispatcher();
}

nsresult
ValidatingDispatcher::LabeledDispatch(const char* aName,
                                      TaskCategory aCategory,
                                      already_AddRefed<nsIRunnable>&& aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  if (XRE_IsContentProcess()) {
    runnable = new Runnable(runnable.forget(), this);
  }
  return UnlabeledDispatch(aName, aCategory, runnable.forget());
}

void
ValidatingDispatcher::SetValidatingAccess(ValidationType aType)
{
  sRunningDispatcher = aType == StartValidation ? this : nullptr;
  mAccessValid = aType == StartValidation;

  dom::AutoJSAPI jsapi;
  jsapi.Init();
  js::EnableAccessValidation(jsapi.cx(), !!sRunningDispatcher);
}

ValidatingDispatcher::Runnable::Runnable(already_AddRefed<nsIRunnable>&& aRunnable,
                                         ValidatingDispatcher* aDispatcher)
 : mRunnable(Move(aRunnable)),
   mDispatcher(aDispatcher)
{
}

NS_IMETHODIMP
ValidatingDispatcher::Runnable::Run()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mDispatcher->SetValidatingAccess(StartValidation);

  nsresult result = mRunnable->Run();

  // The runnable's destructor can have side effects, so try to execute it in
  // the scope of the TabGroup.
  mRunnable = nullptr;

  mDispatcher->SetValidatingAccess(EndValidation);
  return result;
}

ValidatingDispatcher::AutoProcessEvent::AutoProcessEvent()
 : mPrevRunningDispatcher(ValidatingDispatcher::sRunningDispatcher)
{
  ValidatingDispatcher* prev = sRunningDispatcher;
  if (prev) {
    MOZ_ASSERT(prev->mAccessValid);
    prev->SetValidatingAccess(EndValidation);
  }
}

ValidatingDispatcher::AutoProcessEvent::~AutoProcessEvent()
{
  MOZ_ASSERT(!sRunningDispatcher);

  ValidatingDispatcher* prev = mPrevRunningDispatcher;
  if (prev) {
    prev->SetValidatingAccess(StartValidation);
  }
}
