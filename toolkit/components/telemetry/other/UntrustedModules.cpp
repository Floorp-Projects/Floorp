/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UntrustedModules.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/MozPromise.h"
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
  MultiGetUntrustedModulesData()
      : mBackupSvc(UntrustedModulesBackupService::Get()),
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

    if (mResults.empty()) {
      mPromise->Reject(NS_ERROR_NOT_AVAILABLE, __func__);
      return;
    }

    mPromise->Resolve(true, __func__);
  }

  void OnCompletion(Maybe<UntrustedModulesData>&& aResult) {
    MOZ_ASSERT(NS_IsMainThread());

    if (aResult.isSome()) {
      Unused << mResults.emplaceBack(
          MakeRefPtr<mozilla::UntrustedModulesDataContainer>(
              std::move(aResult.ref())));
    }

    OnCompletion();
  }

 private:
  RefPtr<UntrustedModulesBackupService> mBackupSvc;
  RefPtr<MultiGetUntrustedModulesPromise::Private> mPromise;
  Vector<RefPtr<UntrustedModulesDataContainer>> mResults;
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

  if (RDDProcessManager* rddMgr = RDDProcessManager::Get()) {
    if (RDDChild* rddChild = rddMgr->GetRDDChild()) {
      AddPending(rddChild->SendGetUntrustedModulesData());
    }
  }

  Unused << mResults.reserve(mNumPending);

  return mPromise;
}

void MultiGetUntrustedModulesData::Serialize(RefPtr<dom::Promise>&& aPromise) {
  MOZ_ASSERT(NS_IsMainThread());

  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(aPromise->GetGlobalObject()))) {
    aPromise->MaybeReject(NS_ERROR_FAILURE);
    return;
  }

  if (mResults.empty()) {
    aPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  JSContext* cx = jsapi.cx();
  UntrustedModulesDataSerializer serializer(cx, kMaxModulesArrayLen);
  if (!serializer) {
    aPromise->MaybeReject(NS_ERROR_FAILURE);
    return;
  }

  nsresult rv = serializer.Add(mResults);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aPromise->MaybeReject(rv);
    return;
  }

  JS::RootedValue jsval(cx);
  serializer.GetObject(&jsval);
  aPromise->MaybeResolve(jsval);
}

nsresult GetUntrustedModuleLoadEvents(JSContext* cx, dom::Promise** aPromise) {
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

  RefPtr<MultiGetUntrustedModulesData> multi(
      new MultiGetUntrustedModulesData());

  multi->GetUntrustedModuleLoadEvents()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise, multi](bool) mutable { multi->Serialize(std::move(promise)); },
      [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aPromise);
  return NS_OK;
}

}  // namespace Telemetry
}  // namespace mozilla
