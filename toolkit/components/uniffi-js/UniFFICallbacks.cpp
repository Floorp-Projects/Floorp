/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/OwnedRustBuffer.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/UniFFIBinding.h"
#include "mozilla/dom/UniFFICallbacks.h"
#include "mozilla/Maybe.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"

static mozilla::LazyLogModule UNIFFI_INVOKE_CALLBACK_LOGGER("uniffi");

namespace mozilla::uniffi {

using dom::ArrayBuffer;
using dom::RootedDictionary;
using dom::UniFFICallbackHandler;
using dom::UniFFIScaffoldingCallCode;

static Maybe<CallbackInterfaceInfo> GetCallbackInterfaceInfo(
    uint64_t aInterfaceId) {
  const Maybe<CallbackInterfaceInfo> cbiInfo =
      UniFFIGetCallbackInterfaceInfo(aInterfaceId);
#ifdef MOZ_UNIFFI_FIXTURES
  if (!cbiInfo) {
    return UniFFIFixturesGetCallbackInterfaceInfo(aInterfaceId);
  }
#endif
  return cbiInfo;
}

void RegisterCallbackHandler(uint64_t aInterfaceId,
                             UniFFICallbackHandler& aCallbackHandler,
                             ErrorResult& aError) {
  // We should only be mutating mHandler on the main thread
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<CallbackInterfaceInfo> cbiInfo = GetCallbackInterfaceInfo(aInterfaceId);
  if (!cbiInfo.isSome()) {
    aError.ThrowUnknownError(
        nsPrintfCString("Unknown interface id: %" PRIu64, aInterfaceId));
    return;
  }

  if (*cbiInfo->mHandler) {
    MOZ_LOG(UNIFFI_INVOKE_CALLBACK_LOGGER, LogLevel::Error,
            ("[UniFFI] Callback handler already registered for %s",
             cbiInfo->mName));
    return;
  }
  *cbiInfo->mHandler = &aCallbackHandler;
  RustCallStatus status = {0};
  cbiInfo->mInitForeignCallback(cbiInfo->mForeignCallback, &status);
  // Just use MOZ_DIAGNOSTIC_ASSERT to check the status, since the call should
  // always succeed. The RustCallStatus out param only exists because UniFFI
  // adds it to all FFI calls.
  MOZ_DIAGNOSTIC_ASSERT(status.code == RUST_CALL_SUCCESS);
}

void DeregisterCallbackHandler(uint64_t aInterfaceId, ErrorResult& aError) {
  // We should only be mutating mHandler on the main thread
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<CallbackInterfaceInfo> cbiInfo = GetCallbackInterfaceInfo(aInterfaceId);
  if (!cbiInfo.isSome()) {
    aError.ThrowUnknownError(
        nsPrintfCString("Unknown interface id: %" PRIu64, aInterfaceId));
    return;
  }
  *cbiInfo->mHandler = nullptr;
}

MOZ_CAN_RUN_SCRIPT
static void QueueCallbackInner(uint64_t aInterfaceId, uint64_t aHandle,
                               uint32_t aMethod, OwnedRustBuffer aArgs) {
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<CallbackInterfaceInfo> cbiInfo = GetCallbackInterfaceInfo(aInterfaceId);
  if (!cbiInfo.isSome()) {
    MOZ_LOG(UNIFFI_INVOKE_CALLBACK_LOGGER, LogLevel::Error,
            ("[UniFFI] Unknown inferface id: %" PRIu64, aInterfaceId));
    return;
  }

  // Take our own reference to the callback handler to ensure that it stays
  // alive for the duration of this call
  RefPtr<UniFFICallbackHandler> ihandler = *cbiInfo->mHandler;

  if (!ihandler) {
    MOZ_LOG(UNIFFI_INVOKE_CALLBACK_LOGGER, LogLevel::Error,
            ("[UniFFI] JS handler for %s not registered", cbiInfo->mName));
    return;
  }

  JSObject* global = ihandler->CallbackGlobalOrNull();
  if (!global) {
    MOZ_LOG(UNIFFI_INVOKE_CALLBACK_LOGGER, LogLevel::Error,
            ("[UniFFI] JS handler for %s has null global", cbiInfo->mName));
    return;
  }

  dom::AutoEntryScript aes(global, cbiInfo->mName);
  IgnoredErrorResult error;
  JS::Rooted<JSObject*> args(aes.cx(), aArgs.IntoArrayBuffer(aes.cx()));

  ihandler->Call(aHandle, aMethod, args, error);

  if (error.Failed()) {
    MOZ_LOG(UNIFFI_INVOKE_CALLBACK_LOGGER, LogLevel::Error,
            ("[UniFFI] Error invoking JS handler for %s", cbiInfo->mName));
    return;
  }
}

void QueueCallback(uint64_t aInterfaceId, uint64_t aHandle, uint32_t aMethod,
                   RustBuffer aArgs) {
  nsresult dispatchResult =
      GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
          "UniFFI callback", [=]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
            QueueCallbackInner(aInterfaceId, aHandle, aMethod,
                               OwnedRustBuffer(aArgs));
          }));

  if (NS_FAILED(dispatchResult)) {
    MOZ_LOG(UNIFFI_INVOKE_CALLBACK_LOGGER, LogLevel::Error,
            ("[UniFFI] Error dispatching UniFFI callback task"));
  }
}

}  // namespace mozilla::uniffi
