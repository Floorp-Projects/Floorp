/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

use pkcs11_bindings::*;
use std::slice;

use std::collections::btree_map::Entry;
use std::collections::{BTreeMap, BTreeSet};
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::{Mutex, MutexGuard};

use crate::internal::{get_attribute, search};
use crate::internal::{ObjectHandle, Query, SearchResult};

use crate::version::*;

const BUILTINS_VERSION: CK_VERSION = CK_VERSION {
    major: NSS_BUILTINS_LIBRARY_VERSION_MAJOR,
    minor: NSS_BUILTINS_LIBRARY_VERSION_MINOR,
};

const FIRMWARE_VERSION: CK_VERSION = CK_VERSION {
    major: NSS_BUILTINS_FIRMWARE_VERSION_MAJOR,
    minor: NSS_BUILTINS_FIRMWARE_VERSION_MINOR,
};

const CRYPTOKI_VERSION: CK_VERSION = CK_VERSION {
    major: NSS_BUILTINS_CRYPTOKI_VERSION_MAJOR,
    minor: NSS_BUILTINS_CRYPTOKI_VERSION_MINOR,
};
const HARDWARE_VERSION: CK_VERSION = CK_VERSION {
    major: NSS_BUILTINS_HARDWARE_VERSION_MAJOR,
    minor: NSS_BUILTINS_HARDWARE_VERSION_MINOR,
};

const MANUFACTURER_ID_BYTES: &[u8; 32] = b"Mozilla Foundation              ";
const LIBRARY_DESCRIPTION_BYTES: &[u8; 32] = b"NSS Builtin Object Cryptoki Modu";

const SLOT_COUNT: CK_ULONG = 1;
const SLOT_ID_ROOTS: CK_SLOT_ID = 1;
const SLOT_DESCRIPTION_ROOTS_BYTES: &[u8; 64] =
    b"NSS Builtin Objects                                             ";

const TOKEN_LABEL_ROOTS_BYTES: &[u8; 32] = b"Builtin Object Token            ";
const TOKEN_MODEL_BYTES: &[u8; 16] = b"1               ";
const TOKEN_SERIAL_NUMBER_BYTES: &[u8; 16] = b"1               ";
const TOKEN_UTC_TIME: &[u8; 16] = b"                ";

#[derive(Debug)]
struct PK11Error(CK_RV);

// The token assigns session handles using a counter. It would make sense to use a 64 bit counter,
// as there would then be no risk of exhausting the session handle space. However,
// CK_SESSION_HANDLE is defined as a C unsigned long, which is a u32 on some platforms.
//
// We start the counter at 1 since PKCS#11 reserves 0 to signal an invalid handle
//
type SessionHandle = u32;
static NEXT_HANDLE: AtomicU32 = AtomicU32::new(1);

// The token needs to keep track of which sessions are open.
//
type SessionSet = BTreeSet<SessionHandle>;
static OPEN_SESSIONS: Mutex<Option<SessionSet>> = Mutex::new(None);

// Helper functions for accessing OPEN_SESSIONS
//
type SessionSetGuard = MutexGuard<'static, Option<SessionSet>>;

fn get_open_sessions_guard() -> Result<SessionSetGuard, PK11Error> {
    OPEN_SESSIONS
        .lock()
        .map_err(|_| PK11Error(CKR_DEVICE_ERROR))
}

fn get_open_sessions(guard: &mut SessionSetGuard) -> Result<&mut SessionSet, PK11Error> {
    let sessions = guard
        .as_mut()
        .ok_or(PK11Error(CKR_CRYPTOKI_NOT_INITIALIZED))?;
    Ok(sessions)
}

// The token needs to cache search results until the client reads them or closes the session.
//
type SearchCache = BTreeMap<SessionHandle, SearchResult>;
static SEARCHES: Mutex<Option<SearchCache>> = Mutex::new(None);

// Helper functions for accessing SEARCHES
//
type SearchCacheGuard = MutexGuard<'static, Option<SearchCache>>;

fn get_search_cache_guard() -> Result<SearchCacheGuard, PK11Error> {
    SEARCHES.lock().map_err(|_| PK11Error(CKR_DEVICE_ERROR))
}

fn get_search_cache(guard: &mut SearchCacheGuard) -> Result<&mut SearchCache, PK11Error> {
    let searches = guard
        .as_mut()
        .ok_or(PK11Error(CKR_CRYPTOKI_NOT_INITIALIZED))?;
    Ok(searches)
}

fn validate_session(handle: SessionHandle) -> Result<(), PK11Error> {
    let mut guard = get_open_sessions_guard()?;
    let sessions = get_open_sessions(&mut guard)?;
    if sessions.contains(&handle) {
        return Ok(());
    }
    if handle < NEXT_HANDLE.load(Ordering::SeqCst) {
        Err(PK11Error(CKR_SESSION_CLOSED))
    } else {
        // Possible that NEXT_HANDLE wrapped and we should return CKR_SESSION_CLOSED.
        // But this is best-effort.
        Err(PK11Error(CKR_SESSION_HANDLE_INVALID))
    }
}

// The internal implementation of C_Initialize
fn initialize() -> Result<(), PK11Error> {
    {
        let mut search_cache_guard = get_search_cache_guard()?;
        if (*search_cache_guard).is_some() {
            return Err(PK11Error(CKR_CRYPTOKI_ALREADY_INITIALIZED));
        }
        *search_cache_guard = Some(SearchCache::default());
    }

    {
        let mut session_guard = get_open_sessions_guard()?;
        if (*session_guard).is_some() {
            return Err(PK11Error(CKR_CRYPTOKI_ALREADY_INITIALIZED));
        }
        *session_guard = Some(SessionSet::default());
    }

    Ok(())
}

// The internal implementation of C_Finalize
fn finalize() -> Result<(), PK11Error> {
    {
        let mut guard = get_search_cache_guard()?;
        // Try to access the search cache to ensure we're initialized.
        // Returns CKR_CRYPTOKI_NOT_INITIALIZED if we're not.
        let _ = get_search_cache(&mut guard)?;
        *guard = None;
    }

    let mut guard = get_open_sessions_guard()?;
    let _ = get_open_sessions(&mut guard)?;
    *guard = None;

    Ok(())
}

// Internal implementation of C_OpenSession
fn open_session() -> Result<SessionHandle, PK11Error> {
    let mut handle = NEXT_HANDLE.fetch_add(1, Ordering::SeqCst);
    if handle == 0 {
        // skip handle 0 if the addition wraps
        handle = NEXT_HANDLE.fetch_add(1, Ordering::SeqCst);
    }

    let mut guard = get_open_sessions_guard()?;
    let sessions = get_open_sessions(&mut guard)?;
    while !sessions.insert(handle) {
        // this only executes if NEXT_HANDLE wraps while sessions with
        // small handles are still open.
        handle = NEXT_HANDLE.fetch_add(1, Ordering::SeqCst);
    }

    Ok(handle)
}

// Internal implementation of C_CloseSession
fn close_session(session: SessionHandle) -> Result<(), PK11Error> {
    {
        let mut guard = get_search_cache_guard()?;
        let searches = get_search_cache(&mut guard)?;
        searches.remove(&session);
    }

    {
        let mut guard = get_open_sessions_guard()?;
        let sessions = get_open_sessions(&mut guard)?;
        if sessions.remove(&session) {
            Ok(())
        } else if session < NEXT_HANDLE.load(Ordering::SeqCst) {
            Err(PK11Error(CKR_SESSION_CLOSED))
        } else {
            Err(PK11Error(CKR_SESSION_HANDLE_INVALID))
        }
    }
}

// Internal implementation of C_CloseAllSessions
fn close_all_sessions() -> Result<(), PK11Error> {
    {
        let mut guard = get_search_cache_guard()?;
        let searches = get_search_cache(&mut guard)?;
        searches.clear();
    }

    {
        let mut guard = get_open_sessions_guard()?;
        let sessions = get_open_sessions(&mut guard)?;
        sessions.clear();
    }

    Ok(())
}

// Internal implementation of C_FindObjectsInit
fn find_objects_init(session: SessionHandle, query: &Query) -> Result<usize, PK11Error> {
    validate_session(session)?;

    let results = search(query);
    let count = results.len();

    let mut guard = get_search_cache_guard()?;
    let searches = get_search_cache(&mut guard)?;
    match searches.entry(session) {
        Entry::Occupied(_) => Err(PK11Error(CKR_OPERATION_ACTIVE)),
        Entry::Vacant(v) => {
            v.insert(results);
            Ok(count)
        }
    }
}

// Internal implementation of C_FindObjects
fn find_objects(session: SessionHandle, out: &mut [CK_OBJECT_HANDLE]) -> Result<usize, PK11Error> {
    validate_session(session)?;

    let mut guard = get_search_cache_guard()?;
    let searches = get_search_cache(&mut guard)?;
    if let Some(objects) = searches.get_mut(&session) {
        for (i, out_i) in out.iter_mut().enumerate() {
            match objects.pop() {
                Some(object) => *out_i = object.into(),
                None => return Ok(i),
            }
        }
        Ok(out.len())
    } else {
        Ok(0)
    }
}

// Internal implementation of C_FindObjectsFinal
fn find_objects_final(session: SessionHandle) -> Result<(), PK11Error> {
    validate_session(session)?;

    let mut guard = get_search_cache_guard()?;
    let searches = get_search_cache(&mut guard)?;
    searches.remove(&session);
    Ok(())
}

extern "C" fn C_Initialize(_pInitArgs: CK_VOID_PTR) -> CK_RV {
    match initialize() {
        Ok(_) => CKR_OK,
        Err(PK11Error(e)) => e,
    }
}

extern "C" fn C_Finalize(pReserved: CK_VOID_PTR) -> CK_RV {
    if !pReserved.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    match finalize() {
        Ok(_) => CKR_OK,
        Err(PK11Error(e)) => e,
    }
}

extern "C" fn C_GetInfo(pInfo: CK_INFO_PTR) -> CK_RV {
    if pInfo.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    unsafe {
        *pInfo = CK_INFO {
            cryptokiVersion: CRYPTOKI_VERSION,
            manufacturerID: *MANUFACTURER_ID_BYTES,
            flags: 0,
            libraryDescription: *LIBRARY_DESCRIPTION_BYTES,
            libraryVersion: BUILTINS_VERSION,
        };
    }
    CKR_OK
}

extern "C" fn C_GetSlotList(
    _tokenPresent: CK_BBOOL,
    pSlotList: CK_SLOT_ID_PTR,
    pulCount: CK_ULONG_PTR,
) -> CK_RV {
    if pulCount.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    if !pSlotList.is_null() {
        if unsafe { *pulCount } < SLOT_COUNT {
            return CKR_BUFFER_TOO_SMALL;
        }
        unsafe {
            *pSlotList = SLOT_ID_ROOTS;
        }
    }
    unsafe {
        *pulCount = SLOT_COUNT;
    }
    CKR_OK
}

extern "C" fn C_GetSlotInfo(slotID: CK_SLOT_ID, pInfo: CK_SLOT_INFO_PTR) -> CK_RV {
    if (slotID != SLOT_ID_ROOTS) || pInfo.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    unsafe {
        *pInfo = CK_SLOT_INFO {
            slotDescription: *SLOT_DESCRIPTION_ROOTS_BYTES,
            manufacturerID: *MANUFACTURER_ID_BYTES,
            flags: CKF_TOKEN_PRESENT,
            hardwareVersion: HARDWARE_VERSION,
            firmwareVersion: FIRMWARE_VERSION,
        };
    }
    CKR_OK
}

extern "C" fn C_GetTokenInfo(slotID: CK_SLOT_ID, pInfo: CK_TOKEN_INFO_PTR) -> CK_RV {
    if (slotID != SLOT_ID_ROOTS) || pInfo.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    unsafe {
        *pInfo = CK_TOKEN_INFO {
            label: *TOKEN_LABEL_ROOTS_BYTES,
            manufacturerID: *MANUFACTURER_ID_BYTES,
            model: *TOKEN_MODEL_BYTES,
            serialNumber: *TOKEN_SERIAL_NUMBER_BYTES,
            flags: CKF_WRITE_PROTECTED,
            ulMaxSessionCount: CK_UNAVAILABLE_INFORMATION,
            ulSessionCount: 0,
            ulMaxRwSessionCount: CK_UNAVAILABLE_INFORMATION,
            ulRwSessionCount: 0,
            ulMaxPinLen: CK_UNAVAILABLE_INFORMATION,
            ulMinPinLen: CK_UNAVAILABLE_INFORMATION,
            ulTotalPublicMemory: CK_UNAVAILABLE_INFORMATION,
            ulFreePublicMemory: CK_UNAVAILABLE_INFORMATION,
            ulTotalPrivateMemory: CK_UNAVAILABLE_INFORMATION,
            ulFreePrivateMemory: CK_UNAVAILABLE_INFORMATION,
            hardwareVersion: HARDWARE_VERSION,
            firmwareVersion: FIRMWARE_VERSION,
            utcTime: *TOKEN_UTC_TIME,
        };
    }
    CKR_OK
}

extern "C" fn C_GetMechanismList(
    slotID: CK_SLOT_ID,
    _pMechanismList: CK_MECHANISM_TYPE_PTR,
    pulCount: CK_ULONG_PTR,
) -> CK_RV {
    if slotID != SLOT_ID_ROOTS || pulCount.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    unsafe {
        *pulCount = 0;
    }
    CKR_OK
}

extern "C" fn C_GetMechanismInfo(
    _slotID: CK_SLOT_ID,
    _type: CK_MECHANISM_TYPE,
    _pInfo: CK_MECHANISM_INFO_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_InitToken(
    _slotID: CK_SLOT_ID,
    _pPin: CK_UTF8CHAR_PTR,
    _ulPinLen: CK_ULONG,
    _pLabel: CK_UTF8CHAR_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_InitPIN(
    _hSession: CK_SESSION_HANDLE,
    _pPin: CK_UTF8CHAR_PTR,
    _ulPinLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SetPIN(
    _hSession: CK_SESSION_HANDLE,
    _pOldPin: CK_UTF8CHAR_PTR,
    _ulOldLen: CK_ULONG,
    _pNewPin: CK_UTF8CHAR_PTR,
    _ulNewLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_OpenSession(
    slotID: CK_SLOT_ID,
    flags: CK_FLAGS,
    _pApplication: CK_VOID_PTR,
    _Notify: CK_NOTIFY,
    phSession: CK_SESSION_HANDLE_PTR,
) -> CK_RV {
    if slotID != SLOT_ID_ROOTS || phSession.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    // [pkcs11-base-v3.0, Section 5.6.1]
    // For legacy reasons, the CKF_SERIAL_SESSION bit MUST always be set; if a call to
    // C_OpenSession does not have this bit set, the call should return unsuccessfully with the
    // error code CKR_SESSION_PARALLEL_NOT_SUPPORTED.
    if flags & CKF_SERIAL_SESSION == 0 {
        return CKR_SESSION_PARALLEL_NOT_SUPPORTED;
    }
    let session_id = match open_session() {
        Ok(session_id) => session_id as CK_SESSION_HANDLE,
        Err(PK11Error(e)) => return e,
    };
    unsafe { *phSession = session_id };
    CKR_OK
}

extern "C" fn C_CloseSession(hSession: CK_SESSION_HANDLE) -> CK_RV {
    let session: SessionHandle = match hSession.try_into() {
        Ok(session) => session,
        Err(_) => return CKR_SESSION_HANDLE_INVALID,
    };
    match close_session(session) {
        Ok(_) => CKR_OK,
        Err(PK11Error(e)) => e,
    }
}

extern "C" fn C_CloseAllSessions(slotID: CK_SLOT_ID) -> CK_RV {
    if slotID != SLOT_ID_ROOTS {
        return CKR_ARGUMENTS_BAD;
    }
    match close_all_sessions() {
        Ok(_) => CKR_OK,
        Err(PK11Error(e)) => e,
    }
}

extern "C" fn C_GetSessionInfo(_hSession: CK_SESSION_HANDLE, _pInfo: CK_SESSION_INFO_PTR) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_GetOperationState(
    _hSession: CK_SESSION_HANDLE,
    _pOperationState: CK_BYTE_PTR,
    _pulOperationStateLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SetOperationState(
    _hSession: CK_SESSION_HANDLE,
    _pOperationState: CK_BYTE_PTR,
    _ulOperationStateLen: CK_ULONG,
    _hEncryptionKey: CK_OBJECT_HANDLE,
    _hAuthenticationKey: CK_OBJECT_HANDLE,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_Login(
    _hSession: CK_SESSION_HANDLE,
    _userType: CK_USER_TYPE,
    _pPin: CK_UTF8CHAR_PTR,
    _ulPinLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_Logout(_hSession: CK_SESSION_HANDLE) -> CK_RV {
    CKR_OK
}

extern "C" fn C_CreateObject(
    _hSession: CK_SESSION_HANDLE,
    _pTemplate: CK_ATTRIBUTE_PTR,
    _ulCount: CK_ULONG,
    _phObject: CK_OBJECT_HANDLE_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_CopyObject(
    _hSession: CK_SESSION_HANDLE,
    _hObject: CK_OBJECT_HANDLE,
    _pTemplate: CK_ATTRIBUTE_PTR,
    _ulCount: CK_ULONG,
    _phNewObject: CK_OBJECT_HANDLE_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DestroyObject(_hSession: CK_SESSION_HANDLE, _hObject: CK_OBJECT_HANDLE) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_GetObjectSize(
    _hSession: CK_SESSION_HANDLE,
    _hObject: CK_OBJECT_HANDLE,
    _pulSize: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_GetAttributeValue(
    _hSession: CK_SESSION_HANDLE,
    hObject: CK_OBJECT_HANDLE,
    pTemplate: CK_ATTRIBUTE_PTR,
    ulCount: CK_ULONG,
) -> CK_RV {
    if pTemplate.is_null() {
        return CKR_ARGUMENTS_BAD;
    }

    let count: usize = match ulCount.try_into() {
        Ok(count) => count,
        Err(_) => return CKR_ARGUMENTS_BAD,
    };

    // C_GetAttributeValue has a session handle parameter because PKCS#11 objects can have
    // session-bound lifetimes and access controls. We don't have any session objects, and all of
    // our token objects are public. So there's no good reason to validate the session handle.
    //
    //let session: SessionHandle = match hSession.try_into() {
    //    Ok(session) => session,
    //    Err(_) => return CKR_SESSION_HANDLE_INVALID,
    //};
    //
    //if let Err(PK11Error(e)) = validate_session(session) {
    //    return e;
    //}

    let handle: ObjectHandle = match hObject.try_into() {
        Ok(handle) => handle,
        Err(_) => return CKR_OBJECT_HANDLE_INVALID,
    };

    let attrs: &mut [CK_ATTRIBUTE] = unsafe { slice::from_raw_parts_mut(pTemplate, count) };

    let mut rv = CKR_OK;

    // Handle requests with null pValue fields
    for attr in attrs.iter_mut().filter(|x| x.pValue.is_null()) {
        attr.ulValueLen = match get_attribute(attr.type_, &handle) {
            None => {
                // [pkcs11-base-v3.0, Section 5.7.5]
                // 2. [...] if the specified value for the object is invalid (the object does not possess
                //    such an attribute), then the ulValueLen field in that triple is modified to hold the
                //    value CK_UNAVAILABLE_INFORMATION.
                rv = CKR_ATTRIBUTE_TYPE_INVALID;
                CK_UNAVAILABLE_INFORMATION
            }
            Some(attr) => {
                // [pkcs11-base-v3.0, Section 5.7.5]
                // 3. [...] if the pValue field has the value NULL_PTR, then the ulValueLen field is modified
                //    to hold the exact length of the specified attribute for the object.
                attr.len() as CK_ULONG
            }
        }
    }

    // Handle requests with non-null pValue fields
    for attr in attrs.iter_mut().filter(|x| !x.pValue.is_null()) {
        let dst_len: usize = match attr.ulValueLen.try_into() {
            Ok(dst_len) => dst_len,
            Err(_) => return CKR_ARGUMENTS_BAD,
        };
        attr.ulValueLen = match get_attribute(attr.type_, &handle) {
            None => {
                // [pkcs11-base-v3.0, Section 5.7.5]
                // 2. [...] if the specified value for the object is invalid (the object does not possess
                //    such an attribute), then the ulValueLen field in that triple is modified to hold the
                //    value CK_UNAVAILABLE_INFORMATION.
                rv = CKR_ATTRIBUTE_TYPE_INVALID;
                CK_UNAVAILABLE_INFORMATION
            }
            Some(src) if dst_len >= src.len() => {
                // [pkcs11-base-v3.0, Section 5.7.5]
                // 4. [...] if the length specified in ulValueLen is large enough to hold the value
                //    of the specified attribute for the object, then that attribute is copied into
                //    the buffer located at pValue, and the ulValueLen field is modified to hold
                //    the exact length of the attribute.
                let dst: &mut [u8] =
                    unsafe { slice::from_raw_parts_mut(attr.pValue as *mut u8, dst_len) };
                dst[..src.len()].copy_from_slice(src);
                src.len() as CK_ULONG
            }
            _ => {
                // [pkcs11-base-v3.0, Section 5.7.5]
                // 5. Otherwise, the ulValueLen field is modified to hold the value
                //    CK_UNAVAILABLE_INFORMATION.
                rv = CKR_BUFFER_TOO_SMALL;
                CK_UNAVAILABLE_INFORMATION
            }
        };
    }

    // [pkcs11-base-v3.0, Section 5.7.5]
    // If case 2 applies to any of the requested attributes, then the call should return the value
    // CKR_ATTRIBUTE_TYPE_INVALID.  If case 5 applies to any of the requested attributes, then the
    // call should return the value CKR_BUFFER_TOO_SMALL.  As usual, if more than one of these
    // error codes is applicable, Cryptoki may return any of them.  Only if none of them applies to
    // any of the requested attributes will CKR_OK be returned.
    rv
}

extern "C" fn C_SetAttributeValue(
    _hSession: CK_SESSION_HANDLE,
    _hObject: CK_OBJECT_HANDLE,
    _pTemplate: CK_ATTRIBUTE_PTR,
    _ulCount: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_FindObjectsInit(
    hSession: CK_SESSION_HANDLE,
    pTemplate: CK_ATTRIBUTE_PTR,
    ulCount: CK_ULONG,
) -> CK_RV {
    if pTemplate.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    let count: usize = match ulCount.try_into() {
        Ok(count) => count,
        Err(_) => return CKR_ARGUMENTS_BAD,
    };
    let session: SessionHandle = match hSession.try_into() {
        Ok(session) => session,
        Err(_) => return CKR_SESSION_HANDLE_INVALID,
    };

    let raw_attrs: &[CK_ATTRIBUTE] = unsafe { slice::from_raw_parts_mut(pTemplate, count) };

    let mut query: Vec<(CK_ATTRIBUTE_TYPE, &[u8])> = Vec::with_capacity(raw_attrs.len());
    for attr in raw_attrs {
        match usize::try_from(attr.ulValueLen) {
            Ok(len) => query.push((attr.type_, unsafe {
                slice::from_raw_parts_mut(attr.pValue as *mut u8, len)
            })),
            Err(_) => return CKR_ARGUMENTS_BAD,
        }
    }

    match find_objects_init(session, &query) {
        Ok(_) => CKR_OK,
        Err(PK11Error(e)) => e,
    }
}

extern "C" fn C_FindObjects(
    hSession: CK_SESSION_HANDLE,
    phObject: CK_OBJECT_HANDLE_PTR,
    ulMaxObjectCount: CK_ULONG,
    pulObjectCount: CK_ULONG_PTR,
) -> CK_RV {
    if phObject.is_null() || pulObjectCount.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    let max_object_count: usize = match ulMaxObjectCount.try_into() {
        Ok(max_object_count) => max_object_count,
        Err(_) => return CKR_ARGUMENTS_BAD,
    };
    let session: SessionHandle = match hSession.try_into() {
        Ok(session) => session,
        Err(_) => return CKR_SESSION_HANDLE_INVALID,
    };
    let out: &mut [CK_OBJECT_HANDLE] =
        unsafe { slice::from_raw_parts_mut(phObject, max_object_count) };
    match find_objects(session, out) {
        Ok(num_found) => {
            unsafe { *pulObjectCount = num_found as CK_ULONG };
            CKR_OK
        }
        Err(PK11Error(e)) => e,
    }
}

extern "C" fn C_FindObjectsFinal(hSession: CK_SESSION_HANDLE) -> CK_RV {
    let session: SessionHandle = match hSession.try_into() {
        Ok(session) => session,
        Err(_) => return CKR_SESSION_HANDLE_INVALID,
    };
    match find_objects_final(session) {
        Ok(()) => CKR_OK,
        Err(PK11Error(e)) => e,
    }
}

extern "C" fn C_EncryptInit(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hKey: CK_OBJECT_HANDLE,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_Encrypt(
    _hSession: CK_SESSION_HANDLE,
    _pData: CK_BYTE_PTR,
    _ulDataLen: CK_ULONG,
    _pEncryptedData: CK_BYTE_PTR,
    _pulEncryptedDataLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_EncryptUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pPart: CK_BYTE_PTR,
    _ulPartLen: CK_ULONG,
    _pEncryptedPart: CK_BYTE_PTR,
    _pulEncryptedPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_EncryptFinal(
    _hSession: CK_SESSION_HANDLE,
    _pLastEncryptedPart: CK_BYTE_PTR,
    _pulLastEncryptedPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DecryptInit(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hKey: CK_OBJECT_HANDLE,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_Decrypt(
    _hSession: CK_SESSION_HANDLE,
    _pEncryptedData: CK_BYTE_PTR,
    _ulEncryptedDataLen: CK_ULONG,
    _pData: CK_BYTE_PTR,
    _pulDataLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DecryptUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pEncryptedPart: CK_BYTE_PTR,
    _ulEncryptedPartLen: CK_ULONG,
    _pPart: CK_BYTE_PTR,
    _pulPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DecryptFinal(
    _hSession: CK_SESSION_HANDLE,
    _pLastPart: CK_BYTE_PTR,
    _pulLastPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DigestInit(_hSession: CK_SESSION_HANDLE, _pMechanism: CK_MECHANISM_PTR) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_Digest(
    _hSession: CK_SESSION_HANDLE,
    _pData: CK_BYTE_PTR,
    _ulDataLen: CK_ULONG,
    _pDigest: CK_BYTE_PTR,
    _pulDigestLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DigestUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pPart: CK_BYTE_PTR,
    _ulPartLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DigestKey(_hSession: CK_SESSION_HANDLE, _hKey: CK_OBJECT_HANDLE) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DigestFinal(
    _hSession: CK_SESSION_HANDLE,
    _pDigest: CK_BYTE_PTR,
    _pulDigestLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SignInit(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hKey: CK_OBJECT_HANDLE,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_Sign(
    _hSession: CK_SESSION_HANDLE,
    _pData: CK_BYTE_PTR,
    _ulDataLen: CK_ULONG,
    _pSignature: CK_BYTE_PTR,
    _pulSignatureLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SignUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pPart: CK_BYTE_PTR,
    _ulPartLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SignFinal(
    _hSession: CK_SESSION_HANDLE,
    _pSignature: CK_BYTE_PTR,
    _pulSignatureLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SignRecoverInit(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hKey: CK_OBJECT_HANDLE,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SignRecover(
    _hSession: CK_SESSION_HANDLE,
    _pData: CK_BYTE_PTR,
    _ulDataLen: CK_ULONG,
    _pSignature: CK_BYTE_PTR,
    _pulSignatureLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_VerifyInit(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hKey: CK_OBJECT_HANDLE,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_Verify(
    _hSession: CK_SESSION_HANDLE,
    _pData: CK_BYTE_PTR,
    _ulDataLen: CK_ULONG,
    _pSignature: CK_BYTE_PTR,
    _ulSignatureLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_VerifyUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pPart: CK_BYTE_PTR,
    _ulPartLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_VerifyFinal(
    _hSession: CK_SESSION_HANDLE,
    _pSignature: CK_BYTE_PTR,
    _ulSignatureLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_VerifyRecoverInit(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hKey: CK_OBJECT_HANDLE,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_VerifyRecover(
    _hSession: CK_SESSION_HANDLE,
    _pSignature: CK_BYTE_PTR,
    _ulSignatureLen: CK_ULONG,
    _pData: CK_BYTE_PTR,
    _pulDataLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DigestEncryptUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pPart: CK_BYTE_PTR,
    _ulPartLen: CK_ULONG,
    _pEncryptedPart: CK_BYTE_PTR,
    _pulEncryptedPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DecryptDigestUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pEncryptedPart: CK_BYTE_PTR,
    _ulEncryptedPartLen: CK_ULONG,
    _pPart: CK_BYTE_PTR,
    _pulPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SignEncryptUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pPart: CK_BYTE_PTR,
    _ulPartLen: CK_ULONG,
    _pEncryptedPart: CK_BYTE_PTR,
    _pulEncryptedPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DecryptVerifyUpdate(
    _hSession: CK_SESSION_HANDLE,
    _pEncryptedPart: CK_BYTE_PTR,
    _ulEncryptedPartLen: CK_ULONG,
    _pPart: CK_BYTE_PTR,
    _pulPartLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_GenerateKey(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _pTemplate: CK_ATTRIBUTE_PTR,
    _ulCount: CK_ULONG,
    _phKey: CK_OBJECT_HANDLE_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_GenerateKeyPair(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _pPublicKeyTemplate: CK_ATTRIBUTE_PTR,
    _ulPublicKeyAttributeCount: CK_ULONG,
    _pPrivateKeyTemplate: CK_ATTRIBUTE_PTR,
    _ulPrivateKeyAttributeCount: CK_ULONG,
    _phPublicKey: CK_OBJECT_HANDLE_PTR,
    _phPrivateKey: CK_OBJECT_HANDLE_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_WrapKey(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hWrappingKey: CK_OBJECT_HANDLE,
    _hKey: CK_OBJECT_HANDLE,
    _pWrappedKey: CK_BYTE_PTR,
    _pulWrappedKeyLen: CK_ULONG_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_UnwrapKey(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hUnwrappingKey: CK_OBJECT_HANDLE,
    _pWrappedKey: CK_BYTE_PTR,
    _ulWrappedKeyLen: CK_ULONG,
    _pTemplate: CK_ATTRIBUTE_PTR,
    _ulAttributeCount: CK_ULONG,
    _phKey: CK_OBJECT_HANDLE_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_DeriveKey(
    _hSession: CK_SESSION_HANDLE,
    _pMechanism: CK_MECHANISM_PTR,
    _hBaseKey: CK_OBJECT_HANDLE,
    _pTemplate: CK_ATTRIBUTE_PTR,
    _ulAttributeCount: CK_ULONG,
    _phKey: CK_OBJECT_HANDLE_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_SeedRandom(
    _hSession: CK_SESSION_HANDLE,
    _pSeed: CK_BYTE_PTR,
    _ulSeedLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_GenerateRandom(
    _hSession: CK_SESSION_HANDLE,
    _RandomData: CK_BYTE_PTR,
    _ulRandomLen: CK_ULONG,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_GetFunctionStatus(_hSession: CK_SESSION_HANDLE) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_CancelFunction(_hSession: CK_SESSION_HANDLE) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

extern "C" fn C_WaitForSlotEvent(
    _flags: CK_FLAGS,
    _pSlot: CK_SLOT_ID_PTR,
    _pRserved: CK_VOID_PTR,
) -> CK_RV {
    CKR_FUNCTION_NOT_SUPPORTED
}

pub static FUNCTION_LIST: CK_FUNCTION_LIST = CK_FUNCTION_LIST {
    version: CRYPTOKI_VERSION,
    C_Initialize: Some(C_Initialize),
    C_Finalize: Some(C_Finalize),
    C_GetInfo: Some(C_GetInfo),
    C_GetFunctionList: None,
    C_GetSlotList: Some(C_GetSlotList),
    C_GetSlotInfo: Some(C_GetSlotInfo),
    C_GetTokenInfo: Some(C_GetTokenInfo),
    C_GetMechanismList: Some(C_GetMechanismList),
    C_GetMechanismInfo: Some(C_GetMechanismInfo),
    C_InitToken: Some(C_InitToken),
    C_InitPIN: Some(C_InitPIN),
    C_SetPIN: Some(C_SetPIN),
    C_OpenSession: Some(C_OpenSession),
    C_CloseSession: Some(C_CloseSession),
    C_CloseAllSessions: Some(C_CloseAllSessions),
    C_GetSessionInfo: Some(C_GetSessionInfo),
    C_GetOperationState: Some(C_GetOperationState),
    C_SetOperationState: Some(C_SetOperationState),
    C_Login: Some(C_Login),
    C_Logout: Some(C_Logout),
    C_CreateObject: Some(C_CreateObject),
    C_CopyObject: Some(C_CopyObject),
    C_DestroyObject: Some(C_DestroyObject),
    C_GetObjectSize: Some(C_GetObjectSize),
    C_GetAttributeValue: Some(C_GetAttributeValue),
    C_SetAttributeValue: Some(C_SetAttributeValue),
    C_FindObjectsInit: Some(C_FindObjectsInit),
    C_FindObjects: Some(C_FindObjects),
    C_FindObjectsFinal: Some(C_FindObjectsFinal),
    C_EncryptInit: Some(C_EncryptInit),
    C_Encrypt: Some(C_Encrypt),
    C_EncryptUpdate: Some(C_EncryptUpdate),
    C_EncryptFinal: Some(C_EncryptFinal),
    C_DecryptInit: Some(C_DecryptInit),
    C_Decrypt: Some(C_Decrypt),
    C_DecryptUpdate: Some(C_DecryptUpdate),
    C_DecryptFinal: Some(C_DecryptFinal),
    C_DigestInit: Some(C_DigestInit),
    C_Digest: Some(C_Digest),
    C_DigestUpdate: Some(C_DigestUpdate),
    C_DigestKey: Some(C_DigestKey),
    C_DigestFinal: Some(C_DigestFinal),
    C_SignInit: Some(C_SignInit),
    C_Sign: Some(C_Sign),
    C_SignUpdate: Some(C_SignUpdate),
    C_SignFinal: Some(C_SignFinal),
    C_SignRecoverInit: Some(C_SignRecoverInit),
    C_SignRecover: Some(C_SignRecover),
    C_VerifyInit: Some(C_VerifyInit),
    C_Verify: Some(C_Verify),
    C_VerifyUpdate: Some(C_VerifyUpdate),
    C_VerifyFinal: Some(C_VerifyFinal),
    C_VerifyRecoverInit: Some(C_VerifyRecoverInit),
    C_VerifyRecover: Some(C_VerifyRecover),
    C_DigestEncryptUpdate: Some(C_DigestEncryptUpdate),
    C_DecryptDigestUpdate: Some(C_DecryptDigestUpdate),
    C_SignEncryptUpdate: Some(C_SignEncryptUpdate),
    C_DecryptVerifyUpdate: Some(C_DecryptVerifyUpdate),
    C_GenerateKey: Some(C_GenerateKey),
    C_GenerateKeyPair: Some(C_GenerateKeyPair),
    C_WrapKey: Some(C_WrapKey),
    C_UnwrapKey: Some(C_UnwrapKey),
    C_DeriveKey: Some(C_DeriveKey),
    C_SeedRandom: Some(C_SeedRandom),
    C_GenerateRandom: Some(C_GenerateRandom),
    C_GetFunctionStatus: Some(C_GetFunctionStatus),
    C_CancelFunction: Some(C_CancelFunction),
    C_WaitForSlotEvent: Some(C_WaitForSlotEvent),
};

#[no_mangle]
pub unsafe fn BUILTINSC_GetFunctionList(ppFunctionList: CK_FUNCTION_LIST_PTR_PTR) -> CK_RV {
    if ppFunctionList.is_null() {
        return CKR_ARGUMENTS_BAD;
    }
    // CK_FUNCTION_LIST_PTR is a *mut CK_FUNCTION_LIST, but as per the
    // specification, the caller must treat it as *const CK_FUNCTION_LIST.
    *ppFunctionList = std::ptr::addr_of!(FUNCTION_LIST) as CK_FUNCTION_LIST_PTR;
    CKR_OK
}

#[cfg(test)]
mod pkcs11_tests {
    use crate::certdata::*;
    use crate::internal::*;
    use crate::pkcs11::*;

    #[test]
    fn test_main() {
        // We need to run tests serially because of C_Initialize / C_Finalize calls.
        test_simple();
        test_c_get_function_list();
        test_c_get_attribute();
    }

    fn test_simple() {
        let query = &[(CKA_CLASS, CKO_CERTIFICATE_BYTES)];
        initialize().expect("initialize should not fail.");
        let hSession = open_session().expect("open_session should not fail.");
        let count = find_objects_init(hSession, query).expect("find_objects_init should not fail.");
        assert_eq!(count, BUILTINS.len());
        let mut results: [CK_OBJECT_HANDLE; 10] = [0; 10];
        let n_read =
            find_objects(hSession, &mut results).expect("find_objects_init should not fail.");
        assert_eq!(n_read, 10);
        finalize().expect("finalize should not fail.");
    }

    fn test_c_get_function_list() {
        let c_null = 0 as *mut std::ffi::c_void;
        let mut pFunctionList: CK_FUNCTION_LIST_PTR = c_null as CK_FUNCTION_LIST_PTR;
        let rv = unsafe { crate::pkcs11::BUILTINSC_GetFunctionList(&mut pFunctionList) };
        assert_eq!(CKR_OK, rv);
        if let Some(pC_Initialize) = unsafe { (*pFunctionList).C_Initialize } {
            let rv = unsafe { pC_Initialize(c_null) };
            assert_eq!(CKR_OK, rv);
        } else {
            assert!(false);
        }

        if let Some(pC_Finalize) = unsafe { (*pFunctionList).C_Finalize } {
            let rv = unsafe { pC_Finalize(c_null) };
            assert_eq!(CKR_OK, rv);
        } else {
            assert!(false);
        }
    }

    fn test_c_get_attribute() {
        let c_null = 0 as *mut std::ffi::c_void;
        let template: &mut [CK_ATTRIBUTE] = &mut [CK_ATTRIBUTE {
            type_: CKA_SUBJECT,
            pValue: c_null,
            ulValueLen: 0,
        }];
        let template_ptr = &mut template[0] as CK_ATTRIBUTE_PTR;
        let object: CK_OBJECT_HANDLE = 2;
        let mut session: CK_SESSION_HANDLE = 0;
        assert_eq!(CKR_OK, C_Initialize(c_null));
        assert_eq!(
            CKR_OK,
            C_OpenSession(
                SLOT_ID_ROOTS,
                CKF_SERIAL_SESSION,
                c_null,
                None,
                &mut session as *mut CK_SESSION_HANDLE
            )
        );
        assert_eq!(
            CKR_OK,
            C_GetAttributeValue(session, object, template_ptr, 1)
        );
        let len = template[0].ulValueLen as usize;
        assert_eq!(len, BUILTINS[0].der_name.len());

        let value: &mut [u8] = &mut vec![0; 1];
        let value_ptr: *mut u8 = &mut value[0] as *mut u8;
        template[0].pValue = value_ptr as *mut std::ffi::c_void;
        template[0].ulValueLen = 1;
        assert_eq!(
            CKR_BUFFER_TOO_SMALL,
            C_GetAttributeValue(session, object, template_ptr, 1)
        );
        assert_eq!(template[0].ulValueLen, CK_UNAVAILABLE_INFORMATION);

        let value: &mut [u8] = &mut vec![0; len];
        let value_ptr: *mut u8 = &mut value[0] as *mut u8;
        template[0].pValue = value_ptr as *mut std::ffi::c_void;
        template[0].ulValueLen = len as CK_ULONG;
        assert_eq!(
            CKR_OK,
            C_GetAttributeValue(session, object, template_ptr, 1)
        );
        assert_eq!(value, BUILTINS[0].der_name);
        assert_eq!(CKR_OK, C_Finalize(c_null));
    }
}
