/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FOGIPC.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/MozPromise.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

using mozilla::dom::ContentParent;
using mozilla::ipc::ByteBuf;
using FlushFOGDataPromise = mozilla::dom::ContentParent::FlushFOGDataPromise;

namespace mozilla {
namespace glean {

extern "C" {
uint32_t fog_serialize_ipc_buf();
uint32_t fog_give_ipc_buf(uint8_t* buf, uint32_t buf_len);
void fog_use_ipc_buf(uint8_t* buf, uint32_t buf_len);
}

/**
 * The parent process is asking you to flush your data ASAP.
 *
 * @param aResolver - The function you need to call with the bincoded,
 *                    serialized payload that the Rust impl hands you.
 */
void FlushFOGData(std::function<void(ipc::ByteBuf&&)>&& aResolver) {
  ByteBuf buf;
  uint32_t ipcBufferSize = fog_serialize_ipc_buf();
  bool ok = buf.Allocate(ipcBufferSize);
  if (!ok) {
    return;
  }
  uint32_t writtenLen = fog_give_ipc_buf(buf.mData, buf.mLen);
  if (writtenLen != ipcBufferSize) {
    return;
  }
  aResolver(std::move(buf));
}

/**
 * Called by FOG on the parent process when it wants to flush all its
 * children's data.
 * @param aResolver - The function that'll be called with the results.
 */
void FlushAllChildData(
    std::function<void(const nsTArray<ipc::ByteBuf>&&)>&& aResolver) {
  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  if (parents.Length() == 0) {
    nsTArray<ipc::ByteBuf> results;
    aResolver(std::move(results));
    return;
  }

  nsTArray<RefPtr<FlushFOGDataPromise>> promises;
  for (auto parent : parents) {
    promises.EmplaceBack(parent->SendFlushFOGData());
  }
  // TODO: Don't throw away resolved data if some of the promises reject.
  // (not sure how, but it'll mean not using ::All... maybe a custom copy of
  // AllPromiseHolder? Might be impossible outside MozPromise.h)
  FlushFOGDataPromise::All(GetCurrentSerialEventTarget(), promises)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [&aResolver](
                 FlushFOGDataPromise::AllPromiseType::ResolveOrRejectValue&&
                     aValue) {
               if (aValue.IsResolve()) {
                 aResolver(std::move(aValue.ResolveValue()));
               } else {
                 nsTArray<ipc::ByteBuf> results;
                 aResolver(std::move(results));
               }
             });
}

/**
 * A child process has sent you this buf as a treat.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void FOGData(ipc::ByteBuf&& buf) { fog_use_ipc_buf(buf.mData, buf.mLen); }

/**
 * Called by FOG on a child process when it wants to send a buf to the parent.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void SendFOGData(ipc::ByteBuf&& buf) {
  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content:
      mozilla::dom::ContentChild::GetSingleton()->SendFOGData(std::move(buf));
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsuppored process type");
  }
}

}  // namespace glean
}  // namespace mozilla
