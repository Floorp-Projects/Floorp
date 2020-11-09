/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::util::{ensure_nss_initialized, map_nss_secstatus, sec_item_as_slice, ScopedPtr};
use crate::{
    error::*,
    pk11::{
        slot::get_internal_slot,
        types::{AlgorithmID, SymKey},
    },
};

// Expose for consumers to choose the hashing algorithm
// Currently only SHA256 supported
pub use crate::pk11::context::HashAlgorithm;
use nss_sys::SECOidTag;
use std::convert::TryFrom;

// ***** BASED ON THE FOLLOWING IMPLEMENTATION *****
// https://searchfox.org/mozilla-central/rev/8ccea36c4fb09412609fb738c722830d7098602b/dom/crypto/WebCryptoTask.cpp#2567

pub fn pbkdf2_key_derive(
    password: &[u8],
    salt: &[u8],
    iterations: u32,
    hash_algorithm: HashAlgorithm,
    out: &mut [u8],
) -> Result<()> {
    ensure_nss_initialized();
    let oid_tag = match hash_algorithm {
        HashAlgorithm::SHA256 => SECOidTag::SEC_OID_HMAC_SHA256 as u32,
        HashAlgorithm::SHA384 => SECOidTag::SEC_OID_HMAC_SHA384 as u32,
    };
    let mut sec_salt = nss_sys::SECItem {
        len: u32::try_from(salt.len())?,
        data: salt.as_ptr() as *mut u8,
        type_: 0,
    };
    let alg_id = unsafe {
        AlgorithmID::from_ptr(nss_sys::PK11_CreatePBEV2AlgorithmID(
            SECOidTag::SEC_OID_PKCS5_PBKDF2 as u32,
            SECOidTag::SEC_OID_HMAC_SHA1 as u32,
            oid_tag,
            i32::try_from(out.len())?,
            i32::try_from(iterations)?,
            &mut sec_salt as *mut nss_sys::SECItem,
        ))?
    };

    let slot = get_internal_slot()?;
    let mut sec_pw = nss_sys::SECItem {
        len: u32::try_from(password.len())?,
        data: password.as_ptr() as *mut u8,
        type_: 0,
    };
    let sym_key = unsafe {
        SymKey::from_ptr(nss_sys::PK11_PBEKeyGen(
            slot.as_mut_ptr(),
            alg_id.as_mut_ptr(),
            &mut sec_pw as *mut nss_sys::SECItem,
            nss_sys::PR_FALSE,
            std::ptr::null_mut(),
        ))?
    };
    map_nss_secstatus(|| unsafe { nss_sys::PK11_ExtractKeyValue(sym_key.as_mut_ptr()) })?;

    // This doesn't leak, because the SECItem* returned by PK11_GetKeyData
    // just refers to a buffer managed by `sym_key` which we copy into `buf`
    let mut key_data = unsafe { *nss_sys::PK11_GetKeyData(sym_key.as_mut_ptr()) };
    let buf = unsafe { sec_item_as_slice(&mut key_data)? };
    // Stop panic in swap_with_slice by returning an error if the sizes mismatch
    if buf.len() != out.len() {
        return Err(ErrorKind::InternalError.into());
    }
    out.swap_with_slice(buf);
    Ok(())
}
