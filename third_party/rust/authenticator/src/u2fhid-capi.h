/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __U2FHID_CAPI
#define __U2FHID_CAPI
#include <stdlib.h>
#include "nsString.h"

extern "C" {

const uint8_t U2F_RESBUF_ID_REGISTRATION = 0;
const uint8_t U2F_RESBUF_ID_KEYHANDLE = 1;
const uint8_t U2F_RESBUF_ID_SIGNATURE = 2;
const uint8_t U2F_RESBUF_ID_APPID = 3;
const uint8_t U2F_RESBUF_ID_VENDOR_NAME = 4;
const uint8_t U2F_RESBUF_ID_DEVICE_NAME = 5;
const uint8_t U2F_RESBUF_ID_FIRMWARE_MAJOR = 6;
const uint8_t U2F_RESBUF_ID_FIRMWARE_MINOR = 7;
const uint8_t U2F_RESBUF_ID_FIRMWARE_BUILD = 8;

const uint64_t U2F_FLAG_REQUIRE_RESIDENT_KEY = 1;
const uint64_t U2F_FLAG_REQUIRE_USER_VERIFICATION = 2;
const uint64_t U2F_FLAG_REQUIRE_PLATFORM_ATTACHMENT = 4;

const uint8_t U2F_AUTHENTICATOR_TRANSPORT_USB = 1;
const uint8_t U2F_AUTHENTICATOR_TRANSPORT_NFC = 2;
const uint8_t U2F_AUTHENTICATOR_TRANSPORT_BLE = 4;
const uint8_t CTAP_AUTHENTICATOR_TRANSPORT_INTERNAL = 8;

const uint8_t U2F_ERROR_UKNOWN = 1;
const uint8_t U2F_ERROR_NOT_SUPPORTED = 2;
const uint8_t U2F_ERROR_INVALID_STATE = 3;
const uint8_t U2F_ERROR_CONSTRAINT = 4;
const uint8_t U2F_ERROR_NOT_ALLOWED = 5;

// NOTE: Preconditions
// * All rust_u2f_mgr* pointers must refer to pointers which are returned
//   by rust_u2f_mgr_new, and must be freed with rust_u2f_mgr_free.
// * All rust_u2f_khs* pointers must refer to pointers which are returned
//   by rust_u2f_khs_new, and must be freed with rust_u2f_khs_free.
// * All rust_u2f_res* pointers must refer to pointers passed to the
//   register() and sign() callbacks. They can be null on failure.

// The `rust_u2f_mgr` opaque type is equivalent to the rust type `U2FManager`
struct rust_u2f_manager;

// The `rust_u2f_app_ids` opaque type is equivalent to the rust type `U2FAppIds`
struct rust_u2f_app_ids;

// The `rust_u2f_key_handles` opaque type is equivalent to the rust type
// `U2FKeyHandles`
struct rust_u2f_key_handles;

// The `rust_u2f_res` opaque type is equivalent to the rust type `U2FResult`
struct rust_u2f_result;

// The callback passed to register() and sign().
typedef void (*rust_u2f_callback)(uint64_t, rust_u2f_result*);

/// U2FManager functions.

rust_u2f_manager* rust_u2f_mgr_new();
/* unsafe */ void rust_u2f_mgr_free(rust_u2f_manager* mgr);

uint64_t rust_u2f_mgr_register(rust_u2f_manager* mgr, uint64_t flags,
                               uint64_t timeout, rust_u2f_callback,
                               const uint8_t* challenge_ptr,
                               size_t challenge_len,
                               const uint8_t* application_ptr,
                               size_t application_len,
                               const rust_u2f_key_handles* khs);

uint64_t rust_u2f_mgr_sign(rust_u2f_manager* mgr, uint64_t flags,
                           uint64_t timeout, rust_u2f_callback,
                           const uint8_t* challenge_ptr, size_t challenge_len,
                           const rust_u2f_app_ids* app_ids,
                           const rust_u2f_key_handles* khs);

void rust_u2f_mgr_cancel(rust_u2f_manager* mgr);

/// U2FAppIds functions.

rust_u2f_app_ids* rust_u2f_app_ids_new();
void rust_u2f_app_ids_add(rust_u2f_app_ids* ids, const uint8_t* id,
                          size_t id_len);
/* unsafe */ void rust_u2f_app_ids_free(rust_u2f_app_ids* ids);

/// U2FKeyHandles functions.

rust_u2f_key_handles* rust_u2f_khs_new();
void rust_u2f_khs_add(rust_u2f_key_handles* khs, const uint8_t* key_handle,
                      size_t key_handle_len, uint8_t transports);
/* unsafe */ void rust_u2f_khs_free(rust_u2f_key_handles* khs);

/// U2FResult functions.

// Returns 0 for success, or the U2F_ERROR error code >= 1.
uint8_t rust_u2f_result_error(const rust_u2f_result* res);

// Call this before `[..]_copy()` to allocate enough space.
bool rust_u2f_resbuf_length(const rust_u2f_result* res, uint8_t bid,
                            size_t* len);
bool rust_u2f_resbuf_copy(const rust_u2f_result* res, uint8_t bid,
                          uint8_t* dst);
/* unsafe */ void rust_u2f_res_free(rust_u2f_result* res);
}

#endif  // __U2FHID_CAPI
