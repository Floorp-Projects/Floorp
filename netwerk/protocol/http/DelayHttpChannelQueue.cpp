/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et cin ts=4 sw=2 sts=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayHttpChannelQueue.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsIObserverService.h"
#include "nsHttpChannel.h"
#include "nsThreadManager.h"

using namespace mozilla;
using namespace mozilla::net;

namespace {
StaticRefPtr<DelayHttpChannelQueue> sDelayHttpChannelQueue;
}

bool DelayHttpChannelQueue::AttemptQueueChannel(nsHttpChannel* aChannel) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(NS_IsMainThread());

  if (!TimeStamp::GetFuzzyfoxEnabled()) {
    return false;
  }

  if (!sDelayHttpChannelQueue) {
    RefPtr<DelayHttpChannelQueue> queue = new DelayHttpChannelQueue();
    if (!queue->Initialize()) {
      return false;
    }

    sDelayHttpChannelQueue = queue;
  }

  if (NS_WARN_IF(
          !sDelayHttpChannelQueue->mQueue.AppendElement(aChannel, fallible))) {
    return false;
  }

  return true;
}

DelayHttpChannelQueue::DelayHttpChannelQueue() {
  MOZ_ASSERT(NS_IsMainThread());
}

DelayHttpChannelQueue::~DelayHttpChannelQueue() {
  MOZ_ASSERT(NS_IsMainThread());
}

bool DelayHttpChannelQueue::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return false;
  }

  nsresult rv = obs->AddObserver(this, "fuzzyfox-fire-outbound", false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = obs->AddObserver(this, "xpcom-shutdown", false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

NS_IMETHODIMP
DelayHttpChannelQueue::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "fuzzyfox-fire-outbound")) {
    FireQueue();
    return NS_OK;
  }

  MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_OK;
  }

  obs->RemoveObserver(this, "fuzzyfox-fire-outbound");
  obs->RemoveObserver(this, "xpcom-shutdown");

  return NS_OK;
}

void DelayHttpChannelQueue::FireQueue() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mQueue.IsEmpty()) {
    return;
  }

  // TODO: get this from the DOM clock?
  TimeStamp ts = TimeStamp::Now();

  FallibleTArray<RefPtr<nsHttpChannel>> queue;
  queue.SwapElements(mQueue);

  for (RefPtr<nsHttpChannel>& channel : queue) {
    channel->AsyncOpenFinal(ts);
  }
}

NS_INTERFACE_MAP_BEGIN(DelayHttpChannelQueue)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(DelayHttpChannelQueue)
NS_IMPL_RELEASE(DelayHttpChannelQueue)
