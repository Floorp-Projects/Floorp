/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UniFFICallbacks_h
#define mozilla_UniFFICallbacks_h

#include "mozilla/StaticPtr.h"
#include "mozilla/dom/UniFFIRust.h"
#include "mozilla/dom/UniFFIScaffolding.h"

namespace mozilla::uniffi {

/**
 * UniFFI-generated scaffolding function to initialize a callback interface
 *
 * The Rust code expests us to pass it a ForeignCallback entrypoint for each
 * callback interface.
 */
typedef void (*CallbackInitFunc)(ForeignCallback, RustCallStatus*);

/**
 * All the information needed to handle a callback interface.
 *
 * The generated code includes a function that maps interface ids to one of
 * these structs
 */
struct CallbackInterfaceInfo {
  // Human-friendly name
  const char* mName;
  // JS handler
  StaticRefPtr<dom::UniFFICallbackHandler>* mHandler;
  // Handler function, defined in the generated C++ code
  ForeignCallback mForeignCallback;
  // Init function, defined in the generated Rust scaffolding.  mForeignCallback
  // should be passed to this.
  CallbackInitFunc mInitForeignCallback;
};

Maybe<CallbackInterfaceInfo> UniFFIGetCallbackInterfaceInfo(
    uint64_t aInterfaceId);

#ifdef MOZ_UNIFFI_FIXTURES
Maybe<CallbackInterfaceInfo> UniFFIFixturesGetCallbackInterfaceInfo(
    uint64_t aInterfaceId);
#endif

/**
 * Register the JS handler for a callback interface
 */
void RegisterCallbackHandler(uint64_t aInterfaceId,
                             dom::UniFFICallbackHandler& aCallbackHandler,
                             ErrorResult& aError);

/**
 * Deregister the JS handler for a callback interface
 */
void DeregisterCallbackHandler(uint64_t aInterfaceId, ErrorResult& aError);

/**
 * Queue a UniFFI callback interface function to run at some point
 *
 * This function is for fire-and-forget callbacks where the caller doesn't care
 * about the return value and doesn't want to wait for the call to finish.  A
 * good use case for this is logging.
 */
MOZ_CAN_RUN_SCRIPT
void QueueCallback(uint64_t aInterfaceId, uint64_t aHandle, uint32_t aMethod,
                   const uint8_t* aArgsData, int32_t aArgsLen);

}  // namespace mozilla::uniffi

#endif  // mozilla_UniFFICallbacks_h
