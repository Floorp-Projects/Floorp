/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/WinDllServices.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsString.h"

namespace mozilla {

const char* DllServices::kTopicDllLoadedMainThread = "dll-loaded-main-thread";
const char* DllServices::kTopicDllLoadedNonMainThread = "dll-loaded-non-main-thread";

static StaticRefPtr<DllServices> sInstance;

DllServices*
DllServices::Get()
{
  if (sInstance) {
    return sInstance;
  }

  sInstance = new DllServices();
  ClearOnShutdown(&sInstance);
  return sInstance;
}

DllServices::DllServices()
{
  Enable();
}

void
DllServices::NotifyDllLoad(const bool aIsMainThread, const nsString& aDllName)
{
  const char* topic;

  if (aIsMainThread) {
    topic = kTopicDllLoadedMainThread;
  } else {
    topic = kTopicDllLoadedNonMainThread;
  }

  nsCOMPtr<nsIObserverService> obsServ(mozilla::services::GetObserverService());
  obsServ->NotifyObservers(nullptr, topic, aDllName.get());
}

void
DllServices::NotifyUntrustedModuleLoads(
  const Vector<glue::ModuleLoadEvent, 0, InfallibleAllocPolicy>& aEvents)
{
}

}// namespace mozilla

