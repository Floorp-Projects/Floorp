/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SchedulerGroup_h
#define mozilla_SchedulerGroup_h

#include "mozilla/RefPtr.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/PerformanceCounter.h"
#include "nsCOMPtr.h"
#include "nsID.h"
#include "nsIRunnable.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsThreadUtils.h"
#include "nscore.h"

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

class SchedulerGroup {
 public:
  class Runnable final : public mozilla::Runnable, public nsIRunnablePriority {
   public:
    Runnable(already_AddRefed<nsIRunnable>&& aRunnable,
             mozilla::PerformanceCounter* aPerformanceCounter);

    mozilla::PerformanceCounter* GetPerformanceCounter() const;

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
    RefPtr<mozilla::PerformanceCounter> mPerformanceCounter;
  };
  friend class Runnable;

  static nsresult Dispatch(TaskCategory aCategory,
                           already_AddRefed<nsIRunnable>&& aRunnable);

  static nsresult UnlabeledDispatch(TaskCategory aCategory,
                                    already_AddRefed<nsIRunnable>&& aRunnable);

  static nsresult LabeledDispatch(
      TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable,
      mozilla::PerformanceCounter* aPerformanceCounter);

 protected:
  static nsresult InternalUnlabeledDispatch(
      TaskCategory aCategory, already_AddRefed<Runnable>&& aRunnable);
};

NS_DEFINE_STATIC_IID_ACCESSOR(SchedulerGroup::Runnable,
                              NS_SCHEDULERGROUPRUNNABLE_IID);

}  // namespace mozilla

#endif  // mozilla_SchedulerGroup_h
