/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Viaduct_h
#define mozilla_Viaduct_h

/**
 * Viaduct is a way for Application Services Rust components
 * (https://github.com/mozilla/application-services) to make network requests
 * using a trusted stack (gecko).
 *
 * The way it works is roughly as follows:
 * - First we register a callback using `viaduct_initialize`
 * (InitializeViaduct). This callback is stored on the Rust side
 * in a static variable, therefore InitializeViaduct() must be called only once.
 *
 * - When the Rust code needs to make a network request, our callback
 * (ViaductCallback) will be called with a protocol buffer describing
 * the request to make on their behalf. Note 1: The callback MUST be called from
 * a background thread as it is blocking. Note 2: It is our side responsibility
 * to call `viaduct_destroy_bytebuffer` on the buffer.
 *
 * - We set a semaphore to make the background thread wait while we make the
 * request on the main thread using nsIChannel. (ViaductRequest::MakeRequest)
 *
 * - Once a response is received, we allocate a bytebuffer to store the
 * response using `viaduct_alloc_bytebuffer` and unlock the semaphore.
 * (ViaductRequest::OnStopRequest)
 *
 * - The background thread is unlocked, and the callback returns the response to
 * the Rust caller. (ViaductCallback)
 *
 * - The Rust caller will free the response buffer we allocated earlier.
 *
 * Reference:
 * https://github.com/mozilla/application-services/blob/master/components/viaduct/README.md
 */

namespace mozilla {

// Should only be called once during startup to initialize the Viaduct callback.
void InitializeViaduct();

}  // namespace mozilla

#endif  // mozilla_Viaduct_h
