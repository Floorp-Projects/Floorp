/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::{
    AuthenticatorService, CtapVersion, RegisterArgsCtap2, SignArgsCtap2,
};
use crate::ctap2::attestation::AttestationStatement;
use crate::ctap2::commands::get_assertion::{Assertion, AssertionObject, GetAssertionOptions};
use crate::ctap2::commands::make_credentials::MakeCredentialsOptions;
use crate::ctap2::server::{
    PublicKeyCredentialDescriptor, PublicKeyCredentialParameters, RelyingParty, User,
};
use crate::errors::{AuthenticatorError, U2FTokenError};
use crate::statecallback::StateCallback;
use crate::{AttestationObject, CollectedClientDataWrapper, Pin, StatusUpdate};
use crate::{RegisterResult, SignResult};
use libc::size_t;
use rand::{thread_rng, Rng};
use serde_cbor;
use std::convert::TryFrom;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::mpsc::channel;
use std::thread;
use std::{ptr, slice};

type Ctap2RegisterResult = Result<(AttestationObject, CString), AuthenticatorError>;
type Ctap2PubKeyCredDescriptors = Vec<PublicKeyCredentialDescriptor>;
type Ctap2RegisterCallback = extern "C" fn(u64, *mut Ctap2RegisterResult);
type Ctap2SignResult = Result<(AssertionObject, CString), AuthenticatorError>;
type Ctap2SignCallback = extern "C" fn(u64, *mut Ctap2SignResult);
type Ctap2StatusUpdateCallback = extern "C" fn(*mut StatusUpdate);

const SIGN_RESULT_PUBKEY_CRED_ID: u8 = 1;
const SIGN_RESULT_AUTH_DATA: u8 = 2;
const SIGN_RESULT_SIGNATURE: u8 = 3;
const SIGN_RESULT_USER_ID: u8 = 4;
const SIGN_RESULT_USER_NAME: u8 = 5;

#[repr(C)]
pub struct AuthenticatorArgsUser {
    id_ptr: *const u8,
    id_len: usize,
    name: *const c_char,
}

#[repr(C)]
pub struct AuthenticatorArgsChallenge {
    ptr: *const u8,
    len: usize,
}

#[repr(C)]
pub struct AuthenticatorArgsPubCred {
    ptr: *const i32,
    len: usize,
}

#[repr(C)]
pub struct AuthenticatorArgsOptions {
    resident_key: bool,
    user_verification: bool,
    user_presence: bool,
    force_none_attestation: bool,
}

// Generates a new 64-bit transaction id with collision probability 2^-32.
fn new_tid() -> u64 {
    thread_rng().gen::<u64>()
}

unsafe fn from_raw(ptr: *const u8, len: usize) -> Vec<u8> {
    slice::from_raw_parts(ptr, len).to_vec()
}

/// # Safety
///
/// This method must not be called on a handle twice, and the handle is unusable
/// after.
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_mgr_free(mgr: *mut AuthenticatorService) {
    if !mgr.is_null() {
        drop(Box::from_raw(mgr));
    }
}

/// # Safety
///
/// The handle returned by this method must be freed by the caller.
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_pkcd_new() -> *mut Ctap2PubKeyCredDescriptors {
    Box::into_raw(Box::new(vec![]))
}

/// # Safety
///
/// This method must be used on an actual Ctap2PubKeyCredDescriptors CredDescriptorse
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_pkcd_add(
    pkcd: *mut Ctap2PubKeyCredDescriptors,
    id_ptr: *const u8,
    id_len: usize,
    transports: u8,
) {
    (*pkcd).push(PublicKeyCredentialDescriptor {
        id: from_raw(id_ptr, id_len),
        transports: crate::AuthenticatorTransports::from_bits_truncate(transports).into(),
    });
}

/// # Safety
///
/// This method must not be called on a handle twice, and the handle is unusable
/// after.
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_pkcd_free(khs: *mut Ctap2PubKeyCredDescriptors) {
    if !khs.is_null() {
        drop(Box::from_raw(khs));
    }
}

/// # Safety
///
/// The handle returned by this method must be freed by the caller.
/// The returned handle can be used with all rust_u2f_mgr_*-functions as well
/// but uses CTAP2 as the underlying protocol. CTAP1 requests will be repackaged
/// into CTAP2 (if the device supports it)
#[no_mangle]
pub extern "C" fn rust_ctap2_mgr_new() -> *mut AuthenticatorService {
    if let Ok(mut mgr) = AuthenticatorService::new(CtapVersion::CTAP2) {
        mgr.add_detected_transports();
        Box::into_raw(Box::new(mgr))
    } else {
        ptr::null_mut()
    }
}

fn rewrap_client_data(
    client_data: CollectedClientDataWrapper,
) -> Result<CString, AuthenticatorError> {
    let s = CString::new(client_data.serialized_data.clone()).map_err(|_| {
        AuthenticatorError::Custom("Failed to transform client_data to C String".to_string())
    })?;
    Ok(s)
}

fn rewrap_register_result(
    attestation_object: AttestationObject,
    client_data: CollectedClientDataWrapper,
) -> Ctap2RegisterResult {
    let s = rewrap_client_data(client_data)?;
    Ok((attestation_object, s))
}

fn rewrap_sign_result(
    assertion_object: AssertionObject,
    client_data: CollectedClientDataWrapper,
) -> Ctap2SignResult {
    let s = rewrap_client_data(client_data)?;
    Ok((assertion_object, s))
}

/// # Safety
///
/// This method should not be called on AuthenticatorService handles after
/// they've been freed
/// All input is copied and it is the callers responsibility to free appropriately.
/// Note: `KeyHandles` are used as `PublicKeyCredentialDescriptor`s for the exclude_list
///       to keep the API smaller, as they are essentially the same thing.
///       `PublicKeyCredentialParameters` in pub_cred_params are represented as i32 with
///       their COSE value (see: https://www.iana.org/assignments/cose/cose.xhtml#table-algorithms)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_mgr_register(
    mgr: *mut AuthenticatorService,
    timeout: u64,
    callback: Ctap2RegisterCallback,
    status_callback: Ctap2StatusUpdateCallback,
    challenge: AuthenticatorArgsChallenge,
    relying_party_id: *const c_char,
    origin_ptr: *const c_char,
    user: AuthenticatorArgsUser,
    pub_cred_params: AuthenticatorArgsPubCred,
    exclude_list: *const Ctap2PubKeyCredDescriptors,
    options: AuthenticatorArgsOptions,
    pin_ptr: *const c_char,
) -> u64 {
    if mgr.is_null() {
        return 0;
    }

    // Check buffers.
    if challenge.ptr.is_null()
        || origin_ptr.is_null()
        || relying_party_id.is_null()
        || user.id_ptr.is_null()
        || user.name.is_null()
        || exclude_list.is_null()
    {
        return 0;
    }

    let pub_cred_params = match slice::from_raw_parts(pub_cred_params.ptr, pub_cred_params.len)
        .iter()
        .map(|x| PublicKeyCredentialParameters::try_from(*x))
        .collect()
    {
        Ok(x) => x,
        Err(_) => {
            return 0;
        }
    };
    let pin = if pin_ptr.is_null() {
        None
    } else {
        Some(Pin::new(&CStr::from_ptr(pin_ptr).to_string_lossy()))
    };
    let user = User {
        id: from_raw(user.id_ptr, user.id_len),
        name: Some(CStr::from_ptr(user.name).to_string_lossy().to_string()), // TODO(MS): Use to_str() and error out on failure?
        display_name: None,
        icon: None,
    };
    let rp = RelyingParty {
        id: CStr::from_ptr(relying_party_id)
            .to_string_lossy()
            .to_string(),
        name: None,
        icon: None,
    };
    let origin = CStr::from_ptr(origin_ptr).to_string_lossy().to_string();
    let challenge = from_raw(challenge.ptr, challenge.len);
    let exclude_list = (*exclude_list).clone();
    let force_none_attestation = options.force_none_attestation;

    let (status_tx, status_rx) = channel::<crate::StatusUpdate>();
    thread::spawn(move || loop {
        match status_rx.recv() {
            Ok(r) => {
                let rb = Box::new(r);
                status_callback(Box::into_raw(rb));
            }
            Err(e) => {
                status_callback(ptr::null_mut());
                error!("Error when receiving status update: {:?}", e);
                return;
            }
        }
    });

    let tid = new_tid();

    let state_callback = StateCallback::<crate::Result<RegisterResult>>::new(Box::new(move |rv| {
        let res = match rv {
            Ok(RegisterResult::CTAP1(..)) => Err(AuthenticatorError::VersionMismatch(
                "rust_ctap2_mgr_register",
                2,
            )),
            Ok(RegisterResult::CTAP2(mut attestation_object, client_data)) => {
                if force_none_attestation {
                    attestation_object.att_statement = AttestationStatement::None;
                }
                rewrap_register_result(attestation_object, client_data)
            }
            Err(e) => Err(e),
        };

        callback(tid, Box::into_raw(Box::new(res)));
    }));

    let ctap_args = RegisterArgsCtap2 {
        challenge,
        relying_party: rp,
        origin,
        user,
        pub_cred_params,
        exclude_list,
        options: MakeCredentialsOptions {
            resident_key: options.resident_key.then(|| true),
            user_verification: options.user_verification.then(|| true),
        },
        extensions: Default::default(),
        pin,
    };

    let res = (*mgr).register(timeout, ctap_args.into(), status_tx, state_callback);

    if res.is_ok() {
        tid
    } else {
        0
    }
}

/// # Safety
///
/// This method should not be called on AuthenticatorService handles after
/// they've been freed
/// Note: `KeyHandles` are used as `PublicKeyCredentialDescriptor`s for the exclude_list
///       to keep the API smaller, as they are essentially the same thing.
///       `PublicKeyCredentialParameters` in pub_cred_params are represented as i32 with
///       their COSE value (see: https://www.iana.org/assignments/cose/cose.xhtml#table-algorithms)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_mgr_sign(
    mgr: *mut AuthenticatorService,
    timeout: u64,
    callback: Ctap2SignCallback,
    status_callback: Ctap2StatusUpdateCallback,
    challenge: AuthenticatorArgsChallenge,
    relying_party_id: *const c_char,
    origin_ptr: *const c_char,
    allow_list: *const Ctap2PubKeyCredDescriptors,
    options: AuthenticatorArgsOptions,
    pin_ptr: *const c_char,
) -> u64 {
    if mgr.is_null() {
        return 0;
    }

    // Check buffers.
    if challenge.ptr.is_null()
        || origin_ptr.is_null()
        || relying_party_id.is_null()
        || allow_list.is_null()
    {
        return 0;
    }

    let pin = if pin_ptr.is_null() {
        None
    } else {
        Some(Pin::new(&CStr::from_ptr(pin_ptr).to_string_lossy()))
    };
    let rpid = CStr::from_ptr(relying_party_id)
        .to_string_lossy()
        .to_string();
    let origin = CStr::from_ptr(origin_ptr).to_string_lossy().to_string();
    let challenge = from_raw(challenge.ptr, challenge.len);
    let allow_list: Vec<_> = (*allow_list).clone();

    let (status_tx, status_rx) = channel::<crate::StatusUpdate>();
    thread::spawn(move || loop {
        match status_rx.recv() {
            Ok(r) => {
                let rb = Box::new(r);
                status_callback(Box::into_raw(rb));
            }
            Err(e) => {
                status_callback(ptr::null_mut());
                error!("Error when receiving status update: {:?}", e);
                return;
            }
        }
    });

    let single_key_handle = if allow_list.len() == 1 {
        Some(allow_list.first().unwrap().clone())
    } else {
        None
    };

    let tid = new_tid();
    let state_callback = StateCallback::<crate::Result<SignResult>>::new(Box::new(move |rv| {
        let res = match rv {
            Ok(SignResult::CTAP1(..)) => Err(AuthenticatorError::VersionMismatch(
                "rust_ctap2_mgr_register",
                2,
            )),
            Ok(SignResult::CTAP2(mut assertion_object, client_data)) => {
                // The token can omit sending back credentials, if the allow_list had only one
                // entry. Thus we re-add that here now for all found assertions before handing it out.
                assertion_object.0.iter_mut().for_each(|x| {
                    x.credentials = x.credentials.clone().or(single_key_handle.clone());
                });
                rewrap_sign_result(assertion_object, client_data)
            }
            Err(e) => Err(e),
        };

        callback(tid, Box::into_raw(Box::new(res)));
    }));
    let ctap_args = SignArgsCtap2 {
        challenge,
        origin,
        relying_party_id: rpid,
        allow_list,
        options: GetAssertionOptions {
            user_presence: options.user_presence.then(|| true),
            user_verification: options.user_verification.then(|| true),
        },
        extensions: Default::default(),
        pin,
    };

    let res = (*mgr).sign(timeout, ctap_args.into(), status_tx, state_callback);

    if res.is_ok() {
        tid
    } else {
        0
    }
}

// /// # Safety
// ///
// /// This method must be used on an actual U2FResult handle
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_register_result_error(res: *const Ctap2RegisterResult) -> u8 {
    if res.is_null() {
        return U2FTokenError::Unknown as u8;
    }

    match &*res {
        Ok(..) => 0, // No error, the request succeeded.
        Err(e) => e.as_u2f_errorcode(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_error(res: *const Ctap2SignResult) -> u8 {
    if res.is_null() {
        return U2FTokenError::Unknown as u8;
    }

    match &*res {
        Ok(..) => 0, // No error, the request succeeded.
        Err(e) => e.as_u2f_errorcode(),
    }
}

/// # Safety
///
/// This method should not be called on RegisterResult handles after they've been
/// freed or a double-free will occur
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_register_res_free(res: *mut Ctap2RegisterResult) {
    if !res.is_null() {
        drop(Box::from_raw(res));
    }
}

/// # Safety
///
/// This method should not be called on SignResult handles after they've been
/// freed or a double-free will occur
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_res_free(res: *mut Ctap2SignResult) {
    if !res.is_null() {
        drop(Box::from_raw(res));
    }
}

/// # Safety
///
/// This method should not be called AuthenticatorService handles after they've
/// been freed
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_mgr_cancel(mgr: *mut AuthenticatorService) {
    if !mgr.is_null() {
        // Ignore return value.
        let _ = (*mgr).cancel();
    }
}

unsafe fn client_data_len<T>(
    res: *const Result<(T, CString), AuthenticatorError>,
    len: *mut size_t,
) -> bool {
    if res.is_null() || len.is_null() {
        return false;
    }

    match &*res {
        Ok((_, client_data)) => {
            *len = client_data.as_bytes().len();
            true
        }
        Err(_) => false,
    }
}

/// This function is used to get the length, prior to calling
/// rust_ctap2_register_result_client_data_copy()
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_register_result_client_data_len(
    res: *const Ctap2RegisterResult,
    len: *mut size_t,
) -> bool {
    client_data_len(res, len)
}

/// This function is used to get the length, prior to calling
/// rust_ctap2_sign_result_client_data_copy()
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_client_data_len(
    res: *const Ctap2SignResult,
    len: *mut size_t,
) -> bool {
    client_data_len(res, len)
}

unsafe fn client_data_copy<T>(
    res: *const Result<(T, CString), AuthenticatorError>,
    dst: *mut c_char,
) -> bool {
    if dst.is_null() || res.is_null() {
        return false;
    }

    match &*res {
        Ok((_, client_data)) => {
            ptr::copy_nonoverlapping(client_data.as_ptr(), dst, client_data.as_bytes().len());
            return true;
        }
        Err(_) => false,
    }
}

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_register_result_client_data_len)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_register_result_client_data_copy(
    res: *const Ctap2RegisterResult,
    dst: *mut c_char,
) -> bool {
    client_data_copy(res, dst)
}

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_register_result_client_data_len)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_client_data_copy(
    res: *const Ctap2SignResult,
    dst: *mut c_char,
) -> bool {
    client_data_copy(res, dst)
}

/// # Safety
///
/// This function is used to get how long the specific item is.
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_register_result_attestation_len(
    res: *const Ctap2RegisterResult,
    len: *mut size_t,
) -> bool {
    if res.is_null() || len.is_null() {
        return false;
    }

    if let Ok((attestation, _)) = &*res {
        if let Some(item_len) = serde_cbor::to_vec(&attestation).ok().map(|x| x.len()) {
            *len = item_len;
            return true;
        }
    }
    false
}

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_register_result_item_len)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_register_result_attestation_copy(
    res: *const Ctap2RegisterResult,
    dst: *mut u8,
) -> bool {
    if res.is_null() || dst.is_null() {
        return false;
    }

    if let Ok((attestation, _)) = &*res {
        if let Ok(item) = serde_cbor::to_vec(&attestation) {
            ptr::copy_nonoverlapping(item.as_ptr(), dst, item.len());
            return true;
        }
    }
    false
}

/// This function is used to get how many assertions there are in total
/// The returned number can be used as index-maximum to access individual
/// fields
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_assertions_len(
    res: *const Ctap2SignResult,
    len: *mut size_t,
) -> bool {
    if res.is_null() || len.is_null() {
        return false;
    }

    match &*res {
        Ok((assertions, _)) => {
            *len = assertions.0.len();
            return true;
        }
        Err(_) => false,
    }
}

fn sign_result_item_len(assertion: &Assertion, item_idx: u8) -> Option<usize> {
    match item_idx {
        SIGN_RESULT_PUBKEY_CRED_ID => assertion.credentials.as_ref().map(|x| x.id.len()),
        // This is inefficent! Converting twice here. Once for len, once for copy
        SIGN_RESULT_AUTH_DATA => assertion.auth_data.to_vec().ok().map(|x| x.len()),
        SIGN_RESULT_SIGNATURE => Some(assertion.signature.len()),
        SIGN_RESULT_USER_ID => assertion.user.as_ref().map(|u| u.id.len()),
        SIGN_RESULT_USER_NAME => assertion
            .user
            .as_ref()
            .map(|u| {
                u.display_name
                    .as_ref()
                    .or(u.name.as_ref())
                    .map(|n| n.as_bytes().len())
            })
            .flatten(),
        _ => None,
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_item_contains(
    res: *const Ctap2SignResult,
    assertion_idx: usize,
    item_idx: u8,
) -> bool {
    if res.is_null() {
        return false;
    }

    match &*res {
        Ok((assertions, _)) => {
            if assertion_idx >= assertions.0.len() {
                return false;
            }
            if item_idx == SIGN_RESULT_AUTH_DATA {
                // Short-cut to avoid serializing auth_data
                return true;
            }
            sign_result_item_len(&assertions.0[assertion_idx], item_idx).is_some()
        }
        Err(_) => false,
    }
}

/// # Safety
///
/// This function is used to get how long the specific item is.
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_item_len(
    res: *const Ctap2SignResult,
    assertion_idx: usize,
    item_idx: u8,
    len: *mut size_t,
) -> bool {
    if res.is_null() || len.is_null() {
        return false;
    }

    match &*res {
        Ok((assertions, _)) => {
            if assertion_idx >= assertions.0.len() {
                return false;
            }

            if let Some(item_len) = sign_result_item_len(&assertions.0[assertion_idx], item_idx) {
                *len = item_len;
                true
            } else {
                false
            }
        }
        Err(_) => false,
    }
}

unsafe fn sign_result_item_copy(assertion: &Assertion, item_idx: u8, dst: *mut u8) -> bool {
    if dst.is_null() {
        return false;
    }

    let tmp_val;
    let item = match item_idx {
        SIGN_RESULT_PUBKEY_CRED_ID => assertion.credentials.as_ref().map(|x| x.id.as_ref()),
        // This is inefficent! Converting twice here. Once for len, once for copy
        SIGN_RESULT_AUTH_DATA => {
            tmp_val = assertion.auth_data.to_vec().ok();
            tmp_val.as_ref().map(|x| x.as_ref())
        }
        SIGN_RESULT_SIGNATURE => Some(assertion.signature.as_ref()),
        SIGN_RESULT_USER_ID => assertion.user.as_ref().map(|u| u.id.as_ref()),
        SIGN_RESULT_USER_NAME => assertion
            .user
            .as_ref()
            .map(|u| {
                u.display_name
                    .as_ref()
                    .or(u.name.as_ref())
                    .map(|n| n.as_bytes().as_ref())
            })
            .flatten(),
        _ => None,
    };

    if let Some(item) = item {
        ptr::copy_nonoverlapping(item.as_ptr(), dst, item.len());
        true
    } else {
        false
    }
}

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_sign_result_item_len)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_item_copy(
    res: *const Ctap2SignResult,
    assertion_idx: usize,
    item_idx: u8,
    dst: *mut u8,
) -> bool {
    if res.is_null() || dst.is_null() {
        return false;
    }

    match &*res {
        Ok((assertions, _)) => {
            if assertion_idx >= assertions.0.len() {
                return false;
            }

            sign_result_item_copy(&assertions.0[assertion_idx], item_idx, dst)
        }
        Err(_) => false,
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_contains_username(
    res: *const Ctap2SignResult,
    assertion_idx: usize,
) -> bool {
    if res.is_null() {
        return false;
    }

    match &*res {
        Ok((assertions, _)) => {
            if assertion_idx >= assertions.0.len() {
                return false;
            }
            assertions.0[assertion_idx]
                .user
                .as_ref()
                .map(|u| u.display_name.as_ref().or(u.name.as_ref()))
                .is_some()
        }
        Err(_) => false,
    }
}

/// # Safety
///
/// This function is used to get how long the specific username is.
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_username_len(
    res: *const Ctap2SignResult,
    assertion_idx: usize,
    len: *mut size_t,
) -> bool {
    if res.is_null() || len.is_null() {
        return false;
    }

    match &*res {
        Ok((assertions, _)) => {
            if assertion_idx >= assertions.0.len() {
                return false;
            }

            if let Some(name_len) = assertions.0[assertion_idx]
                .user
                .as_ref()
                .map(|u| u.display_name.as_ref().or(u.name.as_ref()))
                .flatten()
                .map(|x| x.as_bytes().len())
            {
                *len = name_len;
                true
            } else {
                false
            }
        }
        Err(_) => false,
    }
}

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_sign_result_username_len)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_sign_result_username_copy(
    res: *const Ctap2SignResult,
    assertion_idx: usize,
    dst: *mut c_char,
) -> bool {
    if res.is_null() || dst.is_null() {
        return false;
    }

    match &*res {
        Ok((assertions, _)) => {
            if assertion_idx >= assertions.0.len() {
                return false;
            }

            if let Some(name) = assertions.0[assertion_idx]
                .user
                .as_ref()
                .map(|u| u.display_name.as_ref().or(u.name.as_ref()))
                .flatten()
                .map(|u| CString::new(u.clone()).ok())
                .flatten()
            {
                ptr::copy_nonoverlapping(name.as_ptr(), dst, name.as_bytes().len());
                true
            } else {
                false
            }
        }

        Err(_) => false,
    }
}

/// # Safety
///
/// This function is used to get how long the JSON-representation of a status update is.
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_status_update_len(
    res: *const StatusUpdate,
    len: *mut size_t,
) -> bool {
    if res.is_null() || len.is_null() {
        return false;
    }

    match serde_json::to_string(&*res) {
        Ok(s) => {
            *len = s.len();
            true
        }
        Err(e) => {
            error!("Failed to parse {:?} into json: {:?}", &*res, e);
            false
        }
    }
}

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_ctap2_status_update_len)
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_status_update_copy_json(
    res: *const StatusUpdate,
    dst: *mut c_char,
) -> bool {
    if res.is_null() || dst.is_null() {
        return false;
    }

    match serde_json::to_string(&*res) {
        Ok(s) => {
            if let Ok(cs) = CString::new(s) {
                ptr::copy_nonoverlapping(cs.as_ptr(), dst, cs.as_bytes().len());
                true
            } else {
                error!("Failed to convert String to CString");
                false
            }
        }
        Err(e) => {
            error!("Failed to parse {:?} into json: {:?}", &*res, e);
            false
        }
    }
}

/// # Safety
///
/// We copy the pin, so it is the callers responsibility to free the argument
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_status_update_send_pin(
    res: *const StatusUpdate,
    c_pin: *mut c_char,
) -> bool {
    if res.is_null() || c_pin.is_null() {
        return false;
    }

    match &*res {
        StatusUpdate::PinError(_, sender) => {
            if let Ok(pin) = CStr::from_ptr(c_pin).to_str() {
                sender
                    .send(Pin::new(pin))
                    .map_err(|e| {
                        error!("Failed to send PIN to device-thread");
                        e
                    })
                    .is_ok()
            } else {
                error!("Failed to convert PIN from c_char to String");
                false
            }
        }
        _ => {
            error!("Wrong state!");
            false
        }
    }
}

/// # Safety
///
/// This function frees the memory of res!
#[no_mangle]
pub unsafe extern "C" fn rust_ctap2_destroy_status_update_res(res: *mut StatusUpdate) -> bool {
    if res.is_null() {
        return false;
    }
    // Dropping it when we go out of scope
    drop(Box::from_raw(res));
    true
}
