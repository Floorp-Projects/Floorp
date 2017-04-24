/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundChannelRegistrar.h"

#include "HttpBackgroundChannelParent.h"
#include "HttpChannelParent.h"
#include "nsIInterfaceRequestor.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(BackgroundChannelRegistrar, nsIBackgroundChannelRegistrar)

BackgroundChannelRegistrar::BackgroundChannelRegistrar()
{
  // BackgroundChannelRegistrar is a main-thread-only object.
  // All the operations should be run on main thread.
  // It should be used on chrome process only.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

BackgroundChannelRegistrar::~BackgroundChannelRegistrar()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
BackgroundChannelRegistrar::NotifyChannelLinked(
  HttpChannelParent* aChannelParent,
  HttpBackgroundChannelParent* aBgParent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChannelParent);
  MOZ_ASSERT(aBgParent);

  aBgParent->LinkToChannel(aChannelParent);
  aChannelParent->OnBackgroundParentReady(aBgParent);
}

// nsIBackgroundChannelRegistrar
void
BackgroundChannelRegistrar::DeleteChannel(uint64_t aKey)
{
  MOZ_ASSERT(NS_IsMainThread());

  mChannels.Remove(aKey);
  mBgChannels.Remove(aKey);
}

void
BackgroundChannelRegistrar::LinkHttpChannel(
  uint64_t aKey,
  HttpChannelParent* aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChannel);

  RefPtr<HttpBackgroundChannelParent> bgParent;
  bool found = mBgChannels.Remove(aKey, getter_AddRefs(bgParent));

  if (!found) {
    mChannels.Put(aKey, aChannel);
    return;
  }

  MOZ_ASSERT(bgParent);
  NotifyChannelLinked(aChannel, bgParent);
}

void
BackgroundChannelRegistrar::LinkBackgroundChannel(
  uint64_t aKey,
  HttpBackgroundChannelParent* aBgChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBgChannel);

  RefPtr<HttpChannelParent> parent;
  bool found = mChannels.Remove(aKey, getter_AddRefs(parent));

  if (!found) {
    mBgChannels.Put(aKey, aBgChannel);
    return;
  }

  MOZ_ASSERT(parent);
  NotifyChannelLinked(parent, aBgChannel);
}

} // namespace net
} // namespace mozilla
