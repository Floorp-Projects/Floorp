/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use crate::*;

// https://searchfox.org/nss/rev/4d480919bbf204df5e199b9fdedec8f2a6295778/lib/util/pkcs11n.h#27
pub const NSSCK_VENDOR_NSS: u32 = 0x4E534350;

pub const CKM_NSS: u32 = CKM_VENDOR_DEFINED | NSSCK_VENDOR_NSS;
pub const CKM_NSS_HKDF_SHA256: u32 = CKM_NSS + 4;
pub const CKM_NSS_HKDF_SHA384: u32 = CKM_NSS + 5;

pub type CK_GCM_PARAMS = CK_GCM_PARAMS_V3;
#[repr(C)]
pub struct CK_GCM_PARAMS_V3 {
    pub pIv: CK_BYTE_PTR,
    pub ulIvLen: CK_ULONG,
    pub ulIvBits: CK_ULONG,
    pub pAAD: CK_BYTE_PTR,
    pub ulAADLen: CK_ULONG,
    pub ulTagBits: CK_ULONG,
}
#[repr(C)]
pub struct CK_NSS_HKDFParams {
    pub bExtract: CK_BBOOL,
    pub pSalt: CK_BYTE_PTR,
    pub ulSaltLen: CK_ULONG,
    pub bExpand: CK_BBOOL,
    pub pInfo: CK_BYTE_PTR,
    pub ulInfoLen: CK_ULONG,
}
