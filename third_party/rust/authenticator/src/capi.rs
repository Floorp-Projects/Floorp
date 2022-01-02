/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::AuthenticatorService;
use crate::errors;
use crate::statecallback::StateCallback;
use crate::{RegisterResult, SignResult};
use libc::size_t;
use rand::{thread_rng, Rng};
use std::collections::HashMap;
use std::sync::mpsc::channel;
use std::thread;
use std::{ptr, slice};

type U2FAppIds = Vec<crate::AppId>;
type U2FKeyHandles = Vec<crate::KeyHandle>;
type U2FCallback = extern "C" fn(u64, *mut U2FResult);

pub enum U2FResult {
    Success(HashMap<u8, Vec<u8>>),
    Error(errors::AuthenticatorError),
}

const RESBUF_ID_REGISTRATION: u8 = 0;
const RESBUF_ID_KEYHANDLE: u8 = 1;
const RESBUF_ID_SIGNATURE: u8 = 2;
const RESBUF_ID_APPID: u8 = 3;
const RESBUF_ID_VENDOR_NAME: u8 = 4;
const RESBUF_ID_DEVICE_NAME: u8 = 5;
const RESBUF_ID_FIRMWARE_MAJOR: u8 = 6;
const RESBUF_ID_FIRMWARE_MINOR: u8 = 7;
const RESBUF_ID_FIRMWARE_BUILD: u8 = 8;

// Generates a new 64-bit transaction id with collision probability 2^-32.
fn new_tid() -> u64 {
    thread_rng().gen::<u64>()
}

unsafe fn from_raw(ptr: *const u8, len: usize) -> Vec<u8> {
    slice::from_raw_parts(ptr, len).to_vec()
}

/// # Safety
///
/// The handle returned by this method must be freed by the caller.
#[no_mangle]
pub extern "C" fn rust_u2f_mgr_new() -> *mut AuthenticatorService {
    if let Ok(mut mgr) = AuthenticatorService::new() {
        mgr.add_detected_transports();
        Box::into_raw(Box::new(mgr))
    } else {
        ptr::null_mut()
    }
}

/// # Safety
///
/// This method must not be called on a handle twice, and the handle is unusable
/// after.
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_free(mgr: *mut AuthenticatorService) {
    if !mgr.is_null() {
        Box::from_raw(mgr);
    }
}

/// # Safety
///
/// The handle returned by this method must be freed by the caller.
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_app_ids_new() -> *mut U2FAppIds {
    Box::into_raw(Box::new(vec![]))
}

/// # Safety
///
/// This method must be used on an actual U2FAppIds handle
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_app_ids_add(
    ids: *mut U2FAppIds,
    id_ptr: *const u8,
    id_len: usize,
) {
    (*ids).push(from_raw(id_ptr, id_len));
}

/// # Safety
///
/// This method must not be called on a handle twice, and the handle is unusable
/// after.
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_app_ids_free(ids: *mut U2FAppIds) {
    if !ids.is_null() {
        Box::from_raw(ids);
    }
}

/// # Safety
///
/// The handle returned by this method must be freed by the caller.
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_khs_new() -> *mut U2FKeyHandles {
    Box::into_raw(Box::new(vec![]))
}

/// # Safety
///
/// This method must be used on an actual U2FKeyHandles handle
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_khs_add(
    khs: *mut U2FKeyHandles,
    key_handle_ptr: *const u8,
    key_handle_len: usize,
    transports: u8,
) {
    (*khs).push(crate::KeyHandle {
        credential: from_raw(key_handle_ptr, key_handle_len),
        transports: crate::AuthenticatorTransports::from_bits_truncate(transports),
    });
}

/// # Safety
///
/// This method must not be called on a handle twice, and the handle is unusable
/// after.
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_khs_free(khs: *mut U2FKeyHandles) {
    if !khs.is_null() {
        Box::from_raw(khs);
    }
}

/// # Safety
///
/// This method must be used on an actual U2FResult handle
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_result_error(res: *const U2FResult) -> u8 {
    if res.is_null() {
        return errors::U2FTokenError::Unknown as u8;
    }

    if let U2FResult::Error(ref err) = *res {
        return err.as_u2f_errorcode();
    }

    0 /* No error, the request succeeded. */
}

/// # Safety
///
/// This method must be used before rust_u2f_resbuf_copy
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_resbuf_length(
    res: *const U2FResult,
    bid: u8,
    len: *mut size_t,
) -> bool {
    if res.is_null() {
        return false;
    }

    if let U2FResult::Success(ref bufs) = *res {
        if let Some(buf) = bufs.get(&bid) {
            *len = buf.len();
            return true;
        }
    }

    false
}

/// # Safety
///
/// This method does not ensure anything about dst before copying, so
/// ensure it is long enough (using rust_u2f_resbuf_length)
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_resbuf_copy(
    res: *const U2FResult,
    bid: u8,
    dst: *mut u8,
) -> bool {
    if res.is_null() {
        return false;
    }

    if let U2FResult::Success(ref bufs) = *res {
        if let Some(buf) = bufs.get(&bid) {
            ptr::copy_nonoverlapping(buf.as_ptr(), dst, buf.len());
            return true;
        }
    }

    false
}

/// # Safety
///
/// This method should not be called on U2FResult handles after they've been
/// freed or a double-free will occur
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_res_free(res: *mut U2FResult) {
    if !res.is_null() {
        Box::from_raw(res);
    }
}

/// # Safety
///
/// This method should not be called on AuthenticatorService handles after
/// they've been freed
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_register(
    mgr: *mut AuthenticatorService,
    flags: u64,
    timeout: u64,
    callback: U2FCallback,
    challenge_ptr: *const u8,
    challenge_len: usize,
    application_ptr: *const u8,
    application_len: usize,
    khs: *const U2FKeyHandles,
) -> u64 {
    if mgr.is_null() {
        return 0;
    }

    // Check buffers.
    if challenge_ptr.is_null() || application_ptr.is_null() {
        return 0;
    }

    let flags = crate::RegisterFlags::from_bits_truncate(flags);
    let challenge = from_raw(challenge_ptr, challenge_len);
    let application = from_raw(application_ptr, application_len);
    let key_handles = (*khs).clone();

    let (status_tx, status_rx) = channel::<crate::StatusUpdate>();
    thread::spawn(move || loop {
        // Issue https://github.com/mozilla/authenticator-rs/issues/132 will
        // plumb the status channel through to the actual C API signatures
        match status_rx.recv() {
            Ok(_) => {}
            Err(_recv_error) => return,
        }
    });

    let tid = new_tid();

    let state_callback = StateCallback::<crate::Result<RegisterResult>>::new(Box::new(move |rv| {
        let result = match rv {
            Ok((registration, dev_info)) => {
                let mut bufs = HashMap::new();
                bufs.insert(RESBUF_ID_REGISTRATION, registration);
                bufs.insert(RESBUF_ID_VENDOR_NAME, dev_info.vendor_name);
                bufs.insert(RESBUF_ID_DEVICE_NAME, dev_info.device_name);
                bufs.insert(RESBUF_ID_FIRMWARE_MAJOR, vec![dev_info.version_major]);
                bufs.insert(RESBUF_ID_FIRMWARE_MINOR, vec![dev_info.version_minor]);
                bufs.insert(RESBUF_ID_FIRMWARE_BUILD, vec![dev_info.version_build]);
                U2FResult::Success(bufs)
            }
            Err(e) => U2FResult::Error(e),
        };

        callback(tid, Box::into_raw(Box::new(result)));
    }));

    let res = (*mgr).register(
        flags,
        timeout,
        challenge,
        application,
        key_handles,
        status_tx,
        state_callback,
    );

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
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_sign(
    mgr: *mut AuthenticatorService,
    flags: u64,
    timeout: u64,
    callback: U2FCallback,
    challenge_ptr: *const u8,
    challenge_len: usize,
    app_ids: *const U2FAppIds,
    khs: *const U2FKeyHandles,
) -> u64 {
    if mgr.is_null() || khs.is_null() {
        return 0;
    }

    // Check buffers.
    if challenge_ptr.is_null() {
        return 0;
    }

    // Need at least one app_id.
    if (*app_ids).is_empty() {
        return 0;
    }

    let flags = crate::SignFlags::from_bits_truncate(flags);
    let challenge = from_raw(challenge_ptr, challenge_len);
    let app_ids = (*app_ids).clone();
    let key_handles = (*khs).clone();

    let (status_tx, status_rx) = channel::<crate::StatusUpdate>();
    thread::spawn(move || loop {
        // Issue https://github.com/mozilla/authenticator-rs/issues/132 will
        // plumb the status channel through to the actual C API signatures
        match status_rx.recv() {
            Ok(_) => {}
            Err(_recv_error) => return,
        }
    });

    let tid = new_tid();
    let state_callback = StateCallback::<crate::Result<SignResult>>::new(Box::new(move |rv| {
        let result = match rv {
            Ok((app_id, key_handle, signature, dev_info)) => {
                let mut bufs = HashMap::new();
                bufs.insert(RESBUF_ID_KEYHANDLE, key_handle);
                bufs.insert(RESBUF_ID_SIGNATURE, signature);
                bufs.insert(RESBUF_ID_APPID, app_id);
                bufs.insert(RESBUF_ID_VENDOR_NAME, dev_info.vendor_name);
                bufs.insert(RESBUF_ID_DEVICE_NAME, dev_info.device_name);
                bufs.insert(RESBUF_ID_FIRMWARE_MAJOR, vec![dev_info.version_major]);
                bufs.insert(RESBUF_ID_FIRMWARE_MINOR, vec![dev_info.version_minor]);
                bufs.insert(RESBUF_ID_FIRMWARE_BUILD, vec![dev_info.version_build]);
                U2FResult::Success(bufs)
            }
            Err(e) => U2FResult::Error(e),
        };

        callback(tid, Box::into_raw(Box::new(result)));
    }));

    let res = (*mgr).sign(
        flags,
        timeout,
        challenge,
        app_ids,
        key_handles,
        status_tx,
        state_callback,
    );

    if res.is_ok() {
        tid
    } else {
        0
    }
}

/// # Safety
///
/// This method should not be called AuthenticatorService handles after they've
/// been freed
#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_cancel(mgr: *mut AuthenticatorService) {
    if !mgr.is_null() {
        // Ignore return value.
        let _ = (*mgr).cancel();
    }
}
