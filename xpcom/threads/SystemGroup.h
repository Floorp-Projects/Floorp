/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SystemGroup_h
#define mozilla_SystemGroup_h

#include "mozilla/TaskCategory.h"
#include "mozilla/SchedulerGroup.h"

// The SystemGroup should be used for dispatching runnables that don't need to
// touch web content. Runnables dispatched to the SystemGroup are run in order
// relative to each other, but their order relative to other runnables is
// undefined.

namespace mozilla {

class SystemGroup
{
 public:
  // This method is safe to use from any thread.
  static nsresult Dispatch(const char* aName,
                           TaskCategory aCategory,
                           already_AddRefed<nsIRunnable>&& aRunnable);

  // This method is safe to use from any thread.
  static nsISerialEventTarget* EventTargetFor(TaskCategory aCategory);

  // Must be called on the main thread. The AbstractThread can always be used
  // off the main thread.
  static AbstractThread* AbstractMainThreadFor(TaskCategory aCategory);

  static void InitStatic();
  static void Shutdown();

  // Returns true if SystemGroup has been initialized.
  static bool Initialized();
};

} // namespace mozilla

#endif // mozilla_SystemGroup_h

