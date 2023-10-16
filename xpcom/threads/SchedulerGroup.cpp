/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SchedulerGroup.h"

#include <utility>

#include "jsfriendapi.h"
#include "mozilla/Atomics.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsINamed.h"
#include "nsQueryObject.h"
#include "nsThreadUtils.h"

using namespace mozilla;

/* static */
nsresult SchedulerGroup::Dispatch(TaskCategory aCategory,
                                  already_AddRefed<nsIRunnable>&& aRunnable) {
  if (NS_IsMainThread()) {
    return NS_DispatchToCurrentThread(std::move(aRunnable));
  } else {
    return NS_DispatchToMainThread(std::move(aRunnable));
  }
}
