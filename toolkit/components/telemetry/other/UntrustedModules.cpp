/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UntrustedModules.h"

#include "GMPServiceParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/net/SocketProcessParent.h"
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/RDDChild.h"
#include "mozilla/RDDProcessManager.h"
#include "mozilla/WinDllServices.h"
#include "nsISupportsImpl.h"
#include "nsProxyRelease.h"
#include "nsXULAppAPI.h"
#include "UntrustedModulesDataSerializer.h"

namespace mozilla {
namespace Telemetry {

static const uint32_t kMaxModulesArrayLen = 100;

using UntrustedModulesIpcPromise =
    MozPromise<Maybe<UntrustedModulesData>, ipc::ResponseRejectReason, true>;

using MultiGetUntrustedModulesPromise =
    MozPromise<bool /*aIgnored*/, nsresult, true>;

class MOZ_HEAP_CLASS MultiGetUntrustedModulesData final {
 public:
  /**
   * @param aFlags [in] Combinations of the flags defined under nsITelemetry.
   *               (See "Flags for getUntrustedModuleLoadEvents"
   *                in nsITelemetry.idl)
   */
  explicit MultiGetUntrustedModulesData(uint32_t aFlags)
      : mFlags(aFlags),
        mBackupSvc(UntrustedModulesBackupService::Get()),
        mPromise(new MultiGetUntrustedModulesPromise::Private(__func__)),
        mNumPending(0) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MultiGetUntrustedModulesData)

  RefPtr<MultiGetUntrustedModulesPromise> GetUntrustedModuleLoadEvents();
  void Serialize(RefPtr<dom::Promise>&& aPromise);

  MultiGetUntrustedModulesData(const MultiGetUntrustedModulesData&) = delete;
  MultiGetUntrustedModulesData(MultiGetUntrustedModulesData&&) = delete;
  MultiGetUntrustedModulesData& operator=(const MultiGetUntrustedModulesData&) =
      delete;
  MultiGetUntrustedModulesData& operator=(MultiGetUntrustedModulesData&&) =
      delete;

 private:
  ~MultiGetUntrustedModulesData() = default;

  void AddPending(RefPtr<UntrustedModulesPromise>&& aNewPending) {
    MOZ_ASSERT(NS_IsMainThread());

    ++mNumPending;

    RefPtr<MultiGetUntrustedModulesData> self(this);
    aNewPending->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self](Maybe<UntrustedModulesData>&& aResult) {
          self->OnCompletion(std::move(aResult));
        },
        [self](nsresult aReason) { self->OnCompletion(); });
  }

  void AddPending(RefPtr<UntrustedModulesIpcPromise>&& aNewPending) {
    MOZ_ASSERT(NS_IsMainThread());

    ++mNumPending;

    RefPtr<MultiGetUntrustedModulesData> self(this);
    aNewPending->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self](Maybe<UntrustedModulesData>&& aResult) {
          self->OnCompletion(std::move(aResult));
        },
        [self](ipc::ResponseRejectReason&& aReason) { self->OnCompletion(); });
  }

  void OnCompletion() {
    MOZ_ASSERT(NS_IsMainThread() && mNumPending > 0);

    --mNumPending;
    if (mNumPending) {
      return;
    }

    mPromise->Resolve(true, __func__);
  }

  void OnCompletion(Maybe<UntrustedModulesData>&& aResult) {
    MOZ_ASSERT(NS_IsMainThread());

    if (aResult.isSome()) {
      mBackupSvc->Backup(std::move(aResult.ref()));
    }

    OnCompletion();
  }

 private:
  // Combinations of the flags defined under nsITelemetry.
  // (See "Flags for getUntrustedModuleLoadEvents" in nsITelemetry.idl)
  uint32_t mFlags;

  RefPtr<UntrustedModulesBackupService> mBackupSvc;
  RefPtr<MultiGetUntrustedModulesPromise::Private> mPromise;
  size_t mNumPending;
};

RefPtr<MultiGetUntrustedModulesPromise>
MultiGetUntrustedModulesData::GetUntrustedModuleLoadEvents() {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  // Parent process
  RefPtr<DllServices> dllSvc(DllServices::Get());
  AddPending(dllSvc->GetUntrustedModulesData());

  // Child processes
  nsTArray<dom::ContentParent*> contentParents;
  dom::ContentParent::GetAll(contentParents);
  for (auto&& contentParent : contentParents) {
    AddPending(contentParent->SendGetUntrustedModulesData());
  }

  if (auto* socketActor = net::SocketProcessParent::GetSingleton()) {
    AddPending(socketActor->SendGetUntrustedModulesData());
  }

  if (RDDProcessManager* rddMgr = RDDProcessManager::Get()) {
    if (RDDChild* rddChild = rddMgr->GetRDDChild()) {
      AddPending(rddChild->SendGetUntrustedModulesData());
    }
  }

  if (RefPtr<ipc::UtilityProcessManager> utilityManager =
          ipc::UtilityProcessManager::GetIfExists()) {
    for (RefPtr<ipc::UtilityProcessParent>& parent :
         utilityManager->GetAllProcessesProcessParent()) {
      AddPending(parent->SendGetUntrustedModulesData());
    }
  }

  if (RefPtr<gmp::GeckoMediaPluginServiceParent> gmps =
          gmp::GeckoMediaPluginServiceParent::GetSingleton()) {
    nsTArray<RefPtr<
        gmp::GeckoMediaPluginServiceParent::GetUntrustedModulesDataPromise>>
        promises;
    gmps->SendGetUntrustedModulesData(promises);
    for (auto& promise : promises) {
      AddPending(std::move(promise));
    }
  }

  return mPromise;
}

void MultiGetUntrustedModulesData::Serialize(RefPtr<dom::Promise>&& aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(aPromise->GetGlobalObject()))) {
    aPromise->MaybeReject(NS_ERROR_FAILURE);
    return;
  }

  JSContext* cx = jsapi.cx();
  UntrustedModulesDataSerializer serializer(cx, kMaxModulesArrayLen, mFlags);
  if (!serializer) {
    aPromise->MaybeReject(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv;
  if (mFlags & nsITelemetry::INCLUDE_OLD_LOADEVENTS) {
    // When INCLUDE_OLD_LOADEVENTS is set, we need to return instances
    // from both "Staging" and "Settled" backup.
    if (mFlags & nsITelemetry::KEEP_LOADEVENTS_NEW) {
      // When INCLUDE_OLD_LOADEVENTS and KEEP_LOADEVENTS_NEW are set, we need to
      // return a JS object consisting of all instances from both "Staging" and
      // "Settled" backups, keeping instances in those backups as is.
      if (mFlags & nsITelemetry::EXCLUDE_STACKINFO_FROM_LOADEVENTS) {
        // Without the stack info, we can add multiple UntrustedModulesData to
        // the serializer directly.
        rv = serializer.Add(mBackupSvc->Staging());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          aPromise->MaybeReject(rv);
          return;
        }
        rv = serializer.Add(mBackupSvc->Settled());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          aPromise->MaybeReject(rv);
          return;
        }
      } else {
        // Currently we don't have a method to merge UntrustedModulesData into
        // a serialized JS object because merging CombinedStack will be tricky.
        // Thus we return an error on this flag combination.
        aPromise->MaybeReject(NS_ERROR_INVALID_ARG);
        return;
      }
    } else {
      // When KEEP_LOADEVENTS_NEW is not set, we can move data from "Staging"
      // to "Settled" first, then add "Settled" to the serializer.
      mBackupSvc->SettleAllStagingData();

      const UntrustedModulesBackupData& settledRef = mBackupSvc->Settled();
      if (settledRef.IsEmpty()) {
        aPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
        return;
      }

      rv = serializer.Add(settledRef);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aPromise->MaybeReject(rv);
        return;
      }
    }
  } else {
    // When INCLUDE_OLD_LOADEVENTS is not set, we serialize only the "Staging"
    // into a JS object.
    const UntrustedModulesBackupData& stagingRef = mBackupSvc->Staging();

    if (stagingRef.IsEmpty()) {
      aPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
      return;
    }

    rv = serializer.Add(stagingRef);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aPromise->MaybeReject(rv);
      return;
    }

    // When KEEP_LOADEVENTS_NEW is not set, we move all "Staging" instances
    // to the "Settled".
    if (!(mFlags & nsITelemetry::KEEP_LOADEVENTS_NEW)) {
      mBackupSvc->SettleAllStagingData();
    }
  }

#if defined(XP_WIN)
  RefPtr<DllServices> dllSvc(DllServices::Get());
  nt::SharedSection* sharedSection = dllSvc->GetSharedSection();
  if (sharedSection) {
    auto dynamicBlocklist = sharedSection->GetDynamicBlocklist();

    nsTArray<nsDependentSubstring> blockedModules;
    for (const auto& blockedEntry : dynamicBlocklist) {
      if (!blockedEntry.IsValidDynamicBlocklistEntry()) {
        break;
      }
      blockedModules.AppendElement(
          nsDependentSubstring(blockedEntry.mName.Buffer,
                               blockedEntry.mName.Length / sizeof(wchar_t)));
    }
    rv = serializer.AddBlockedModules(blockedModules);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aPromise->MaybeReject(rv);
      return;
    }
  }
#endif

  JS::Rooted<JS::Value> jsval(cx);
  serializer.GetObject(&jsval);
  aPromise->MaybeResolve(jsval);
}

nsresult GetUntrustedModuleLoadEvents(uint32_t aFlags, JSContext* cx,
                                      dom::Promise** aPromise) {
  // Create a promise using global context.
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(cx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<dom::Promise> promise(dom::Promise::Create(global, result));
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  auto multi = MakeRefPtr<MultiGetUntrustedModulesData>(aFlags);
  multi->GetUntrustedModuleLoadEvents()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise, multi](bool) mutable { multi->Serialize(std::move(promise)); },
      [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

}  // namespace Telemetry
}  // namespace mozilla
