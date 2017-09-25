/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRequestService.h"

#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIDOMWindowUtils.h"
#include "nsISupports.h"
#include "nsITabParent.h"
#include "nsITraceableChannel.h"
#include "nsTArray.h"

#include "mozilla/dom/TabParent.h"

using namespace mozilla;
using namespace mozilla::dom;

static WebRequestService* sWeakWebRequestService;

WebRequestService::WebRequestService()
  : mDataLock("WebRequest service data lock")
{
}

WebRequestService::~WebRequestService()
{
  // Store existing entries in an array, since Detach can't safely remove them
  // while we're iterating the hashtable.
  AutoTArray<ChannelEntry*, 32> entries;
  for (auto iter = mChannelEntries.Iter(); !iter.Done(); iter.Next()) {
    entries.AppendElement(iter.Data());
  }

  for (auto channel : entries) {
    channel->DetachAll();
  }

  sWeakWebRequestService = nullptr;
}

NS_IMPL_ISUPPORTS(WebRequestService, mozIWebRequestService)

/* static */ WebRequestService&
WebRequestService::GetSingleton()
{
  static RefPtr<WebRequestService> instance;
  if (!sWeakWebRequestService) {
    instance = new WebRequestService();
    ClearOnShutdown(&instance);

    // A separate weak instance that we keep a reference to as long as the
    // original service is alive, even after our strong reference is cleared to
    // allow the service to be destroyed.
    sWeakWebRequestService = instance;
  }
  return *sWeakWebRequestService;
}


NS_IMETHODIMP
WebRequestService::RegisterTraceableChannel(uint64_t aChannelId,
                                            nsIChannel* aChannel,
                                            const nsAString& aAddonId,
                                            nsITabParent* aTabParent,
                                            nsIJSRAIIHelper** aHelper)
{
  nsCOMPtr<nsITraceableChannel> traceableChannel = do_QueryInterface(aChannel);
  NS_ENSURE_TRUE(traceableChannel, NS_ERROR_INVALID_ARG);

  RefPtr<nsIAtom> addonId = NS_Atomize(aAddonId);
  ChannelParent* entry = new ChannelParent(aChannelId, aChannel,
                                           addonId, aTabParent);

  RefPtr<Destructor> destructor = new Destructor(entry);
  destructor.forget(aHelper);

  return NS_OK;
}

already_AddRefed<nsIChannel>
WebRequestService::GetTraceableChannel(uint64_t aChannelId,
                                       nsIAtom* aAddonId,
                                       nsIContentParent* aContentParent)
{
  MutexAutoLock al(mDataLock);

  auto entry = mChannelEntries.Get(aChannelId);
  if (!entry) {
    return nullptr;
  }

  for (auto channelEntry : entry->mTabParents) {
    nsIContentParent* contentParent = nullptr;
    if (channelEntry->mTabParent) {
      contentParent = static_cast<nsIContentParent*>(
        channelEntry->mTabParent->Manager());
    }

    if (channelEntry->mAddonId == aAddonId && contentParent == aContentParent) {
      nsCOMPtr<nsIChannel> channel = do_QueryReferent(entry->mChannel);
      return channel.forget();
    }
  }

  return nullptr;
}


WebRequestService::ChannelParent::ChannelParent(uint64_t aChannelId, nsIChannel* aChannel,
                                                nsIAtom* aAddonId, nsITabParent* aTabParent)
  : mTabParent(static_cast<TabParent*>(aTabParent))
  , mAddonId(aAddonId)
  , mChannelId(aChannelId)
{
  auto service = &GetSingleton();
  MutexAutoLock al(service->mDataLock);

  auto entry = service->mChannelEntries.LookupOrAdd(mChannelId);

  entry->mChannel = do_GetWeakReference(aChannel);
  entry->mTabParents.insertBack(this);
}

WebRequestService::ChannelParent::~ChannelParent()
{
  MOZ_ASSERT(mDetached);
}

void
WebRequestService::ChannelParent::Detach()
{
  if (mDetached) {
    return;
  }
  auto service = &GetSingleton();
  MutexAutoLock al(service->mDataLock);

  auto& map = service->mChannelEntries;
  auto entry = map.Get(mChannelId);
  MOZ_ASSERT(entry);

  removeFrom(entry->mTabParents);
  if (entry->mTabParents.isEmpty()) {
    map.Remove(mChannelId);
  }
  mDetached = true;
}

void
WebRequestService::ChannelEntry::DetachAll()
{
  // Store the next link gecore calling Detach(), since the last Detach() call
  // will destroy this instance and poison our mTabParents member.
  for (ChannelParent *parent, *next = mTabParents.getFirst();
       (parent = next);) {
    next = parent->getNext();
    parent->Detach();
  }
}

WebRequestService::Destructor::~Destructor()
{
  if (NS_WARN_IF(!mDestructCalled)) {
    Destruct();
  }
}

NS_IMETHODIMP
WebRequestService::Destructor::Destruct()
{
  if (NS_WARN_IF(mDestructCalled)) {
    return NS_ERROR_FAILURE;
  }
  mDestructCalled = true;

  mChannelParent->Detach();
  delete mChannelParent;

  return NS_OK;
}

NS_IMPL_ISUPPORTS(WebRequestService::Destructor, nsIJSRAIIHelper)
