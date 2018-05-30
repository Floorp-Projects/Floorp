/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRequestService.h"

#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsIChannel.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::extensions;

static WebRequestService* sWeakWebRequestService;

WebRequestService::~WebRequestService()
{
  sWeakWebRequestService = nullptr;
}

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


UniquePtr<WebRequestChannelEntry>
WebRequestService::RegisterChannel(ChannelWrapper* aChannel)
{
  UniquePtr<ChannelEntry> entry(new ChannelEntry(aChannel));

  auto key = mChannelEntries.LookupForAdd(entry->mChannelId);
  MOZ_DIAGNOSTIC_ASSERT(!key);
  key.OrInsert([&entry]() { return entry.get(); });

  return std::move(entry);

}

already_AddRefed<nsITraceableChannel>
WebRequestService::GetTraceableChannel(uint64_t aChannelId,
                                       nsAtom* aAddonId,
                                       nsIContentParent* aContentParent)
{
  if (auto entry = mChannelEntries.Get(aChannelId)) {
    if (entry->mChannel) {
      return entry->mChannel->GetTraceableChannel(aAddonId, aContentParent);

    }
  }
  return nullptr;
}

WebRequestChannelEntry::WebRequestChannelEntry(ChannelWrapper* aChannel)
  : mChannelId(aChannel->Id())
  , mChannel(aChannel)
{}

WebRequestChannelEntry::~WebRequestChannelEntry()
{
  if (sWeakWebRequestService) {
    sWeakWebRequestService->mChannelEntries.Remove(mChannelId);
  }
}
