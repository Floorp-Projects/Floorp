/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamFilter.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "xpcpublic.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/extensions/StreamFilterChild.h"
#include "mozilla/extensions/StreamFilterEvents.h"
#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/dom/ContentChild.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsLiteralString.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"

using namespace JS;
using namespace mozilla::dom;

namespace mozilla {
namespace extensions {

/*****************************************************************************
 * Initialization
 *****************************************************************************/

StreamFilter::StreamFilter(nsIGlobalObject* aParent, uint64_t aRequestId,
                           const nsAString& aAddonId)
    : mParent(aParent), mChannelId(aRequestId), mAddonId(NS_Atomize(aAddonId)) {
  MOZ_ASSERT(aParent);

  Connect();
};

StreamFilter::~StreamFilter() { ForgetActor(); }

void StreamFilter::ForgetActor() {
  if (mActor) {
    mActor->Cleanup();
    mActor->SetStreamFilter(nullptr);
  }
}

/* static */
already_AddRefed<StreamFilter> StreamFilter::Create(GlobalObject& aGlobal,
                                                    uint64_t aRequestId,
                                                    const nsAString& aAddonId) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  RefPtr<StreamFilter> filter = new StreamFilter(global, aRequestId, aAddonId);
  return filter.forget();
}

/*****************************************************************************
 * Actor allocation
 *****************************************************************************/

void StreamFilter::Connect() {
  MOZ_ASSERT(!mActor);

  mActor = new StreamFilterChild();
  mActor->SetStreamFilter(this);

  nsAutoString addonId;
  mAddonId->ToString(addonId);

  ContentChild* cc = ContentChild::GetSingleton();
  if (cc) {
    RefPtr<StreamFilter> self(this);

    cc->SendInitStreamFilter(mChannelId, addonId)
        ->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
            [=](mozilla::ipc::Endpoint<PStreamFilterChild>&& aEndpoint) {
              self->FinishConnect(std::move(aEndpoint));
            },
            [=](mozilla::ipc::ResponseRejectReason&& aReason) {
              self->mActor->RecvInitialized(false);
            });
  } else {
    mozilla::ipc::Endpoint<PStreamFilterChild> endpoint;
    Unused << StreamFilterParent::Create(nullptr, mChannelId, addonId,
                                         &endpoint);

    // Always dispatch asynchronously so JS callers have a chance to attach
    // event listeners before we dispatch events.
    NS_DispatchToCurrentThread(
        NewRunnableMethod<mozilla::ipc::Endpoint<PStreamFilterChild>&&>(
            "StreamFilter::FinishConnect", this, &StreamFilter::FinishConnect,
            std::move(endpoint)));
  }
}

void StreamFilter::FinishConnect(
    mozilla::ipc::Endpoint<PStreamFilterChild>&& aEndpoint) {
  if (aEndpoint.IsValid()) {
    MOZ_RELEASE_ASSERT(aEndpoint.Bind(mActor));
    mActor->RecvInitialized(true);

    // IPC now owns this reference.
    Unused << do_AddRef(mActor);
  } else {
    mActor->RecvInitialized(false);
  }
}

bool StreamFilter::CheckAlive() {
  // Check whether the global that owns this StreamFitler is still scriptable
  // and, if not, disconnect the actor so that it can be cleaned up.
  JSObject* wrapper = GetWrapperPreserveColor();
  if (!wrapper || !xpc::Scriptability::Get(wrapper).Allowed()) {
    ForgetActor();
    return false;
  }
  return true;
}

/*****************************************************************************
 * Binding methods
 *****************************************************************************/

template <typename T>
static inline bool ReadTypedArrayData(nsTArray<uint8_t>& aData, const T& aArray,
                                      ErrorResult& aRv) {
  aArray.ComputeState();
  if (!aData.SetLength(aArray.Length(), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return false;
  }
  memcpy(aData.Elements(), aArray.Data(), aArray.Length());
  return true;
}

void StreamFilter::Write(const ArrayBufferOrUint8Array& aData,
                         ErrorResult& aRv) {
  if (!mActor) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  nsTArray<uint8_t> data;

  bool ok;
  if (aData.IsArrayBuffer()) {
    ok = ReadTypedArrayData(data, aData.GetAsArrayBuffer(), aRv);
  } else if (aData.IsUint8Array()) {
    ok = ReadTypedArrayData(data, aData.GetAsUint8Array(), aRv);
  } else {
    MOZ_ASSERT_UNREACHABLE("Argument should be ArrayBuffer or Uint8Array");
    return;
  }

  if (ok) {
    mActor->Write(std::move(data), aRv);
  }
}

StreamFilterStatus StreamFilter::Status() const {
  if (!mActor) {
    return StreamFilterStatus::Uninitialized;
  }
  return mActor->Status();
}

void StreamFilter::Suspend(ErrorResult& aRv) {
  if (mActor) {
    mActor->Suspend(aRv);
  } else {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
  }
}

void StreamFilter::Resume(ErrorResult& aRv) {
  if (mActor) {
    mActor->Resume(aRv);
  } else {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
  }
}

void StreamFilter::Disconnect(ErrorResult& aRv) {
  if (mActor) {
    mActor->Disconnect(aRv);
  } else {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
  }
}

void StreamFilter::Close(ErrorResult& aRv) {
  if (mActor) {
    mActor->Close(aRv);
  } else {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
  }
}

/*****************************************************************************
 * Event emitters
 *****************************************************************************/

void StreamFilter::FireEvent(const nsAString& aType) {
  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event = Event::Constructor(this, aType, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

void StreamFilter::FireDataEvent(const nsTArray<uint8_t>& aData) {
  AutoEntryScript aes(mParent, "StreamFilter data event");
  JSContext* cx = aes.cx();

  RootedDictionary<StreamFilterDataEventInit> init(cx);
  init.mBubbles = false;
  init.mCancelable = false;

  auto buffer = ArrayBuffer::Create(cx, aData.Length(), aData.Elements());
  if (!buffer) {
    // TODO: There is no way to recover from this. This chunk of data is lost.
    FireErrorEvent(NS_LITERAL_STRING("Out of memory"));
    return;
  }

  init.mData.Init(buffer);

  RefPtr<StreamFilterDataEvent> event =
      StreamFilterDataEvent::Constructor(this, NS_LITERAL_STRING("data"), init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

void StreamFilter::FireErrorEvent(const nsAString& aError) {
  MOZ_ASSERT(mError.IsEmpty());

  mError = aError;
  FireEvent(NS_LITERAL_STRING("error"));
}

/*****************************************************************************
 * Glue
 *****************************************************************************/

/* static */
bool StreamFilter::IsAllowedInContext(JSContext* aCx, JSObject* /* unused */) {
  return nsContentUtils::CallerHasPermission(aCx,
                                             nsGkAtoms::webRequestBlocking);
}

JSObject* StreamFilter::WrapObject(JSContext* aCx, HandleObject aGivenProto) {
  return StreamFilter_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(StreamFilter)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StreamFilter)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(StreamFilter,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(StreamFilter,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(StreamFilter,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(StreamFilter, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(StreamFilter, DOMEventTargetHelper)

}  // namespace extensions
}  // namespace mozilla
