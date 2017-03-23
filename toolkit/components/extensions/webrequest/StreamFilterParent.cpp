/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamFilterParent.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/nsIContentParent.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace extensions {

using namespace mozilla::dom;

StreamFilterParent::StreamFilterParent(uint64_t aChannelId, const nsAString& aAddonId)
  : mChannelId(aChannelId)
  , mAddonId(NS_Atomize(aAddonId))
{
}

StreamFilterParent::~StreamFilterParent()
{
}

void
StreamFilterParent::Init(already_AddRefed<nsIContentParent> aContentParent)
{
  NS_ReleaseOnMainThread(contentParent.forget());
}

void
StreamFilterParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

NS_IMPL_ISUPPORTS0(StreamFilterParent)

} // namespace extensions
} // namespace mozilla

