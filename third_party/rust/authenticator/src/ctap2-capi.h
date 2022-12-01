/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CTAP2_CAPI
#define __CTAP2_CAPI
#include <stdlib.h>
#include "nsString.h"

extern "C" {
const uint8_t CTAP2_SIGN_RESULT_PUBKEY_CRED_ID = 1;
const uint8_t CTAP2_SIGN_RESULT_AUTH_DATA = 2;
const uint8_t CTAP2_SIGN_RESULT_SIGNATURE = 3;
const uint8_t CTAP2_SIGN_RESULT_USER_ID = 4;
const uint8_t CTAP2_SIGN_RESULT_USER_NAME = 5;

typedef struct {
    const uint8_t *id_ptr;
    size_t id_len;
    const char *name;
} AuthenticatorArgsUser;

typedef struct {
    const uint8_t *ptr;
    size_t len;
} AuthenticatorArgsChallenge;

typedef struct {
    const int32_t *ptr;
    size_t len;
} AuthenticatorArgsPubCred;

typedef struct {
    bool resident_key;
    bool user_verification;
    bool user_presence;
    bool force_none_attestation;
} AuthenticatorArgsOptions;

// NOTE: Preconditions
// * All rust_u2f_mgr* pointers must refer to pointers which are returned
//   by rust_u2f_mgr_new, and must be freed with rust_u2f_mgr_free.
// * All rust_u2f_khs* pointers must refer to pointers which are returned
//   by rust_u2f_pkcd_new, and must be freed with rust_u2f_pkcd_free.
// * All rust_u2f_res* pointers must refer to pointers passed to the
//   register() and sign() callbacks. They can be null on failure.

// The `rust_u2f_key_handles` opaque type is equivalent to the rust type
// `Ctap2PubKeyCredDescriptors`
struct rust_ctap2_pub_key_cred_descriptors;

/// Ctap2PubKeyCredDescriptors functions.
rust_ctap2_pub_key_cred_descriptors* rust_ctap2_pkcd_new();
void rust_ctap2_pkcd_add(rust_ctap2_pub_key_cred_descriptors* pkcd, const uint8_t* id_ptr,
                      size_t id_len, uint8_t transports);
/* unsafe */ void rust_ctap2_pkcd_free(rust_ctap2_pub_key_cred_descriptors* khs);

// The `rust_ctap2_mgr` opaque type is equivalent to the rust type `Ctap2Manager`
// struct rust_ctap_manager;

// The `rust_ctap2_result` opaque type is equivalent to the rust type `RegisterResult`
struct rust_ctap2_register_result;

// The `rust_ctap2_result` opaque type is equivalent to the rust type `RegisterResult`
struct rust_ctap2_sign_result;

// Ctap2 exposes the results directly without repackaging them. Use getter-functions.
typedef void (*rust_ctap2_register_callback)(uint64_t, rust_ctap2_register_result*);
typedef void (*rust_ctap2_sign_callback)(uint64_t, rust_ctap2_sign_result*);

// Status updates get sent, if a device needs a PIN, if a device needs to be selected, etc.
struct rust_ctap2_status_update_res;
// May be called with NULL, in case of an error
typedef void (*rust_ctap2_status_update_callback)(rust_ctap2_status_update_res*);

rust_ctap_manager* rust_ctap2_mgr_new();
/* unsafe */ void rust_ctap2_mgr_free(rust_ctap_manager* mgr);

/* unsafe */ void rust_ctap2_register_res_free(rust_ctap2_register_result* res);
/* unsafe */ void rust_ctap2_sign_res_free(rust_ctap2_sign_result* res);

uint64_t rust_ctap2_mgr_register(
    rust_ctap_manager* mgr, uint64_t timeout, rust_ctap2_register_callback, rust_ctap2_status_update_callback,
    AuthenticatorArgsChallenge challenge,
    const char* relying_party_id, const char *origin_ptr,
    AuthenticatorArgsUser user, AuthenticatorArgsPubCred pub_cred_params,
    const rust_ctap2_pub_key_cred_descriptors* exclude_list, AuthenticatorArgsOptions options,
    const char *pin
);

uint64_t rust_ctap2_mgr_sign(
    rust_ctap_manager* mgr, uint64_t timeout, rust_ctap2_sign_callback, rust_ctap2_status_update_callback,
    AuthenticatorArgsChallenge challenge,
    const char* relying_party_id, const char *origin_ptr,
    const rust_ctap2_pub_key_cred_descriptors* allow_list, AuthenticatorArgsOptions options,
    const char *pin
);

void rust_ctap2_mgr_cancel(rust_ctap_manager* mgr);

// Returns 0 for success, or the U2F_ERROR error code >= 1.
uint8_t rust_ctap2_register_result_error(const rust_ctap2_register_result* res);
uint8_t rust_ctap2_sign_result_error(const rust_ctap2_sign_result* res);

/// # Safety
///
/// This function is used to get the length, prior to calling
/// rust_ctap2_register_result_client_data_copy()
bool rust_ctap2_register_result_client_data_len(
    const rust_ctap2_register_result *res,
    size_t *len
);

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_register_result_client_data_len)
bool rust_ctap2_register_result_client_data_copy(
    const rust_ctap2_register_result *res,
    const char *dst
);

/// # Safety
///
/// This function is used to get the length, prior to calling
/// rust_ctap2_register_result_item_copy()
bool rust_ctap2_register_result_attestation_len(
    const rust_ctap2_register_result *res,
    size_t *len
);

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_register_result_item_len)
bool rust_ctap2_register_result_attestation_copy(
    const rust_ctap2_register_result* res,
    uint8_t *dst
);
/// # Safety
///
/// This function is used to get the length, prior to calling
/// rust_ctap2_register_result_client_data_copy()
bool rust_ctap2_sign_result_client_data_len(
    const rust_ctap2_sign_result *res,
    size_t *len
);

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_sign_result_client_data_len)
bool rust_ctap2_sign_result_client_data_copy(
    const rust_ctap2_sign_result *res,
    const char *dst
);

/// # Safety
///
/// This function is used to get the length, prior to calling
/// rust_ctap2_register_result_client_data_copy()
bool rust_ctap2_sign_result_assertions_len(
    const rust_ctap2_sign_result *res,
    size_t *len
);

bool rust_ctap2_sign_result_item_contains(
    const rust_ctap2_sign_result *res,
    size_t assertion_idx,
    uint8_t item_idx
);

/// # Safety
///
/// This function is used to get the length, prior to calling
/// rust_ctap2_sign_result_item_copy()
bool rust_ctap2_sign_result_item_len(
    const rust_ctap2_sign_result *res,
    size_t assertion_idx,
    uint8_t item_idx,
    size_t *len
);

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_sign_result_item_len)
bool rust_ctap2_sign_result_item_copy(
    const rust_ctap2_sign_result* res,
    size_t assertion_idx,
    uint8_t item_idx,
    uint8_t *dst
);

bool rust_ctap2_sign_result_contains_username(
    const rust_ctap2_sign_result *res,
    size_t assertion_idx
);

/// # Safety
///
/// This function is used to get the length, prior to calling
/// rust_ctap2_sign_result_username_copy()
bool rust_ctap2_sign_result_username_len(
    const rust_ctap2_sign_result *res,
    size_t assertion_idx,
    size_t *len
);

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_sign_result_username_len)
bool rust_ctap2_sign_result_username_copy(
    const rust_ctap2_sign_result* res,
    size_t assertion_idx,
    const char *dst
);

/// # Safety
///
/// This function is used to get the length, prior to calling
/// rust_ctap2_status_update_copy_json()
bool rust_ctap2_status_update_len(
    const rust_ctap2_status_update_res *res,
    size_t *len
);

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_status_update_len)
bool rust_ctap2_status_update_copy_json(
    const rust_ctap2_status_update_res *res,
    const char *dst
);

bool rust_ctap2_status_update_send_pin(
    const rust_ctap2_status_update_res *res,
    const char *pin
);


/// # Safety
/// This frees the memory of a status_update_res
bool rust_ctap2_destroy_status_update_res(
    rust_ctap2_status_update_res *res
);


}
#endif  // __CTAP2_CAPI
