/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NeckoTargetHolder.h"

#include "nsContentUtils.h"
#include "nsILoadInfo.h"

namespace mozilla {
namespace net {

already_AddRefed<nsIEventTarget>
NeckoTargetHolder::GetNeckoTarget()
{
  nsCOMPtr<nsIEventTarget> target = mNeckoTarget;

  if (!target) {
    target = GetMainThreadEventTarget();
  }
  return target.forget();
}

nsresult
NeckoTargetHolder::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable,
                            uint32_t aDispatchFlags)
{
  if (mNeckoTarget) {
    return mNeckoTarget->Dispatch(std::move(aRunnable), aDispatchFlags);
  }

  nsCOMPtr<nsIEventTarget> mainThreadTarget = GetMainThreadEventTarget();
  MOZ_ASSERT(mainThreadTarget);

  return mainThreadTarget->Dispatch(std::move(aRunnable), aDispatchFlags);
}

} // namespace net
} // namespace mozilla
