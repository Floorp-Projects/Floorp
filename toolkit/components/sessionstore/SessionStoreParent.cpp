/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SessionStoreParent.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowserSessionStore.h"
#include "mozilla/dom/BrowserSessionStoreBinding.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/InProcessChild.h"
#include "mozilla/dom/InProcessParent.h"
#include "mozilla/dom/SessionStoreChild.h"
#include "mozilla/dom/SessionStoreUtilsBinding.h"
#include "SessionStoreFunctions.h"
#include "nsISupports.h"
#include "nsIXULRuntime.h"
#include "nsImportModule.h"
#include "nsIXPConnect.h"

using namespace mozilla;
using namespace mozilla::dom;

SessionStoreParent::SessionStoreParent(
    CanonicalBrowsingContext* aBrowsingContext,
    BrowserSessionStore* aSessionStore)
    : mBrowsingContext(aBrowsingContext), mSessionStore(aSessionStore) {}

static void SessionStoreUpdate(CanonicalBrowsingContext* aBrowsingContext,
                               const Maybe<nsCString>& aDocShellCaps,
                               const Maybe<bool>& aPrivatedMode,
                               bool aNeedCollectSHistory, uint32_t aEpoch) {
  UpdateSessionStoreData data;
  if (aDocShellCaps.isSome()) {
    auto& disallow = data.mDisallow.Construct();
    if (!aDocShellCaps->IsEmpty()) {
      disallow = aDocShellCaps.value();
    } else {
      disallow.SetIsVoid(true);
    }
  }

  if (aPrivatedMode.isSome()) {
    data.mIsPrivate.Construct() = aPrivatedMode.value();
  }

  RefPtr<BrowserSessionStore> sessionStore =
      BrowserSessionStore::GetOrCreate(aBrowsingContext->Top());

  SessionStoreFormData* formData = sessionStore->GetFormdata();
  data.mFormdata.Construct(formData);

  SessionStoreScrollData* scroll = sessionStore->GetScroll();
  data.mScroll.Construct(scroll);

  nsCOMPtr<nsISessionStoreFunctions> funcs = do_ImportModule(
      "resource://gre/modules/SessionStoreFunctions.jsm", fallible);
  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(funcs);
  if (!wrapped) {
    return;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(wrapped->GetJSObjectGlobal())) {
    return;
  }

  JS::Rooted<JS::Value> update(jsapi.cx());
  if (!ToJSValue(jsapi.cx(), data, &update)) {
    return;
  }

  JS::RootedValue key(jsapi.cx(), aBrowsingContext->Top()->PermanentKey());

  Unused << funcs->UpdateSessionStore(nullptr, aBrowsingContext, key, aEpoch,
                                      aNeedCollectSHistory, update);
}

void SessionStoreParent::FlushAllSessionStoreChildren(
    const std::function<void()>& aDone) {
  if (!mBrowsingContext) {
    aDone();
    return;
  }

  nsTArray<RefPtr<SessionStoreParent::FlushTabStatePromise>> flushPromises;

  // We're special casing this for when the SessionStore{Child, Parent} have
  // been created in the same process. This is only ever true for the parent
  // process session store actor, and is needed because
  // nsFrameLoader::RequestTabStateFlush expect flushes to happen faster than we
  // can manage by using the common path of sending a message the
  // SessionStoreChild. Ideally we should be able to do just that, but not
  // without more work.
  if (InProcessParent::ChildActorFor(this)) {
    // Here we assume that the session store data collection only collect for in
    // (parent-)process content type browsing contexts, which we only flush one
    // session store actor.
    flushPromises.AppendElement(FlushSessionStore());
  } else {
    // While here we flush all participating actors.
    BrowserParent* browserParent = static_cast<BrowserParent*>(Manager());
    browserParent->VisitAll([&flushPromises](BrowserParent* aBrowser) {
      if (PSessionStoreParent* sessionStoreParent =
              SingleManagedOrNull(aBrowser->ManagedPSessionStoreParent())) {
        flushPromises.AppendElement(
            static_cast<SessionStoreParent*>(sessionStoreParent)
                ->FlushSessionStore());
      }
    });
  }

  RefPtr<SessionStoreParent::FlushTabStatePromise::AllPromiseType>
      flushPromise = SessionStoreParent::FlushTabStatePromise::All(
          GetMainThreadSerialEventTarget(), flushPromises);

  mBrowsingContext->UpdateSessionStoreSessionStorage([aDone, flushPromise]() {
    flushPromise->Then(GetCurrentSerialEventTarget(), __func__,
                       [aDone]() { aDone(); });
  });
}

already_AddRefed<SessionStoreParent::FlushTabStatePromise>
SessionStoreParent::FlushSessionStore() {
  if (!mBrowsingContext) {
    return nullptr;
  }

  RefPtr<SessionStoreParent::FlushTabStatePromise> promise =
      SendFlushTabState();
  return promise.forget();
}

void SessionStoreParent::FinalFlushAllSessionStoreChildren(
    const std::function<void()>& aDone) {
  if (!mBrowsingContext) {
    aDone();
    return;
  }

  SessionStoreChild* sessionStoreChild =
      static_cast<SessionStoreChild*>(InProcessParent::ChildActorFor(this));
  if (!sessionStoreChild || mozilla::SessionHistoryInParent()) {
    return FlushAllSessionStoreChildren(aDone);
  }

  sessionStoreChild->FlushSessionStore();
  mBrowsingContext->UpdateSessionStoreSessionStorage(aDone);
}

mozilla::ipc::IPCResult SessionStoreParent::RecvSessionStoreUpdate(
    const Maybe<nsCString>& aDocShellCaps, const Maybe<bool>& aPrivatedMode,
    const bool aNeedCollectSHistory, const uint32_t& aEpoch) {
  if (!mBrowsingContext) {
    return IPC_OK();
  }

  SessionStoreUpdate(mBrowsingContext, aDocShellCaps, aPrivatedMode,
                     aNeedCollectSHistory, aEpoch);
  return IPC_OK();
}

mozilla::ipc::IPCResult SessionStoreParent::RecvIncrementalSessionStoreUpdate(
    const MaybeDiscarded<BrowsingContext>& aBrowsingContext,
    const Maybe<FormData>& aFormData, const Maybe<nsPoint>& aScrollPosition,
    uint32_t aEpoch) {
  if (!aBrowsingContext.IsNull()) {
    mSessionStore->UpdateSessionStore(
        aBrowsingContext.GetMaybeDiscarded()->Canonical(), aFormData,
        aScrollPosition, aEpoch);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult SessionStoreParent::RecvResetSessionStore(
    const MaybeDiscarded<BrowsingContext>& aBrowsingContext, uint32_t aEpoch) {
  if (!aBrowsingContext.IsNull()) {
    mSessionStore->RemoveSessionStore(
        aBrowsingContext.GetMaybeDiscarded()->Canonical());
  }
  return IPC_OK();
}

NS_IMPL_CYCLE_COLLECTION(SessionStoreParent, mBrowsingContext, mSessionStore)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SessionStoreParent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SessionStoreParent, Release)
