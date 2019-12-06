/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/WinDllServices.h"

#include <windows.h>
#include <psapi.h>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/StaticLocalPtr.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/UntrustedModulesProcessor.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsString.h"

namespace mozilla {

const char* DllServices::kTopicDllLoadedMainThread = "dll-loaded-main-thread";
const char* DllServices::kTopicDllLoadedNonMainThread =
    "dll-loaded-non-main-thread";

DllServices* DllServices::Get() {
  static StaticLocalRefPtr<DllServices> sInstance(
      []() -> already_AddRefed<DllServices> {
        RefPtr<DllServices> dllSvc(new DllServices());
        dllSvc->EnableFull();

        auto setClearOnShutdown = [ptr = &sInstance]() -> void {
          ClearOnShutdown(ptr);
        };

        if (NS_IsMainThread()) {
          setClearOnShutdown();
          return dllSvc.forget();
        }

        SystemGroup::Dispatch(
            TaskCategory::Other,
            NS_NewRunnableFunction("mozilla::DllServices::Get",
                                   std::move(setClearOnShutdown)));

        return dllSvc.forget();
      }());

  return sInstance;
}

DllServices::DllServices()
    : mUntrustedModulesProcessor(UntrustedModulesProcessor::Create()) {}

RefPtr<UntrustedModulesPromise> DllServices::GetUntrustedModulesData() {
  if (!mUntrustedModulesProcessor) {
    return UntrustedModulesPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED,
                                                    __func__);
  }

  return mUntrustedModulesProcessor->GetProcessedData();
}

void DllServices::NotifyDllLoad(glue::EnhancedModuleLoadInfo&& aModLoadInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  const char* topic;

  if (aModLoadInfo.mNtLoadInfo.mThreadId == ::GetCurrentThreadId()) {
    topic = kTopicDllLoadedMainThread;
  } else {
    topic = kTopicDllLoadedNonMainThread;
  }

  // We save the path to a nsAutoString because once we have submitted
  // aModLoadInfo for processing there is no guarantee that the original
  // buffer will continue to be valid.
  nsAutoString dllFilePath(aModLoadInfo.GetSectionName());

  if (mUntrustedModulesProcessor) {
    mUntrustedModulesProcessor->Enqueue(std::move(aModLoadInfo));
  }

  nsCOMPtr<nsIObserverService> obsServ(mozilla::services::GetObserverService());
  obsServ->NotifyObservers(nullptr, topic, dllFilePath.get());
}

void DllServices::NotifyModuleLoadBacklog(ModuleLoadInfoVec&& aEvents) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mUntrustedModulesProcessor) {
    return;
  }

  mUntrustedModulesProcessor->Enqueue(std::move(aEvents));
}

}  // namespace mozilla
