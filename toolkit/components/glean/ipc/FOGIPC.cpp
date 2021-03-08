/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FOGIPC.h"

#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/glean/GleanMetrics.h"
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

/**
 * The parent process is asking you to flush your data ASAP.
 *
 * @param aResolver - The function you need to call with the bincoded,
 *                    serialized payload that the Rust impl hands you.
 */
void FlushFOGData(std::function<void(ipc::ByteBuf&&)>&& aResolver) {
#ifndef MOZ_GLEAN_ANDROID
  ByteBuf buf;
  uint32_t ipcBufferSize = impl::fog_serialize_ipc_buf();
  bool ok = buf.Allocate(ipcBufferSize);
  if (!ok) {
    return;
  }
  uint32_t writtenLen = impl::fog_give_ipc_buf(buf.mData, buf.mLen);
  if (writtenLen != ipcBufferSize) {
    return;
  }
  aResolver(std::move(buf));
#endif
}

/**
 * Called by FOG on the parent process when it wants to flush all its
 * children's data.
 * @param aResolver - The function that'll be called with the results.
 */
void FlushAllChildData(
    std::function<void(nsTArray<ipc::ByteBuf>&&)>&& aResolver) {
#ifndef MOZ_GLEAN_ANDROID
  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  if (parents.Length() == 0) {
    nsTArray<ipc::ByteBuf> results;
    aResolver(std::move(results));
    return;
  }

  auto timerId = fog_ipc::flush_durations.Start();
  nsTArray<RefPtr<FlushFOGDataPromise>> promises;
  for (auto* parent : parents) {
    promises.EmplaceBack(parent->SendFlushFOGData());
  }
  // TODO: Don't throw away resolved data if some of the promises reject.
  // (not sure how, but it'll mean not using ::All... maybe a custom copy of
  // AllPromiseHolder? Might be impossible outside MozPromise.h)
  FlushFOGDataPromise::All(GetCurrentSerialEventTarget(), promises)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [aResolver = std::move(aResolver), timerId](
                 FlushFOGDataPromise::AllPromiseType::ResolveOrRejectValue&&
                     aValue) {
               fog_ipc::flush_durations.StopAndAccumulate(std::move(timerId));
               if (aValue.IsResolve()) {
                 aResolver(std::move(aValue.ResolveValue()));
               } else {
                 nsTArray<ipc::ByteBuf> results;
                 aResolver(std::move(results));
               }
             });
#endif
}

/**
 * A child process has sent you this buf as a treat.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void FOGData(ipc::ByteBuf&& buf) {
#ifndef MOZ_GLEAN_ANDROID
  fog_ipc::buffer_sizes.Accumulate(buf.mLen);
  impl::fog_use_ipc_buf(buf.mData, buf.mLen);
#endif
}

/**
 * Called by FOG on a child process when it wants to send a buf to the parent.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void SendFOGData(ipc::ByteBuf&& buf) {
#ifndef MOZ_GLEAN_ANDROID
  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content:
      mozilla::dom::ContentChild::GetSingleton()->SendFOGData(std::move(buf));
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsuppored process type");
  }
#endif
}

/**
 * Called on the parent process to ask all child processes for data,
 * sending it all down into Rust to be used.
 */
RefPtr<GenericPromise> FlushAndUseFOGData() {
  RefPtr<GenericPromise::Private> ret = new GenericPromise::Private(__func__);
  std::function<void(nsTArray<ByteBuf> &&)> resolver =
      [ret](nsTArray<ByteBuf>&& bufs) {
        for (ByteBuf& buf : bufs) {
          FOGData(std::move(buf));
        }
        ret->Resolve(true, __func__);
      };
  FlushAllChildData(std::move(resolver));
  return ret;
}

}  // namespace glean
}  // namespace mozilla
