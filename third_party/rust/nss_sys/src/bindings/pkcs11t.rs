/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::os::raw::{c_uchar, c_ulong, c_void};

pub const CK_TRUE: CK_BBOOL = 1;
pub const CK_FALSE: CK_BBOOL = 0;
pub type CK_BYTE = c_uchar;
pub type CK_BBOOL = CK_BYTE;
pub type CK_ULONG = c_ulong;
pub type CK_BYTE_PTR = *mut CK_BYTE;
pub type CK_VOID_PTR = *mut c_void;
pub type CK_OBJECT_HANDLE = CK_ULONG;
pub type CK_OBJECT_CLASS = CK_ULONG;
pub type CK_KEY_TYPE = CK_ULONG;
pub type CK_ATTRIBUTE_TYPE = CK_ULONG;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CK_ATTRIBUTE {
    pub type_: CK_ATTRIBUTE_TYPE,
    pub pValue: CK_VOID_PTR,
    pub ulValueLen: CK_ULONG,
}
pub type CK_MECHANISM_TYPE = CK_ULONG;

pub const CK_INVALID_HANDLE: u32 = 0;
pub const CKO_PRIVATE_KEY: u32 = 3;
pub const CKK_EC: u32 = 3;
pub const CKA_CLASS: u32 = 0;
pub const CKA_TOKEN: u32 = 1;
pub const CKA_PRIVATE: u32 = 2;
pub const CKA_VALUE: u32 = 17;
pub const CKA_KEY_TYPE: u32 = 256;
pub const CKA_ID: u32 = 258;
pub const CKA_SENSITIVE: u32 = 259;
pub const CKA_ENCRYPT: u32 = 260;
pub const CKA_WRAP: u32 = 262;
pub const CKA_SIGN: u32 = 264;
pub const CKA_EC_PARAMS: u32 = 384;
pub const CKA_EC_POINT: u32 = 385;
// https://searchfox.org/nss/rev/4d480919bbf204df5e199b9fdedec8f2a6295778/lib/util/pkcs11t.h#1244
pub const CKM_VENDOR_DEFINED: u32 = 0x80000000;
pub const CKM_SHA256_HMAC: u32 = 593;
pub const CKM_SHA384_HMAC: u32 = 609;
pub const CKM_SHA512_HMAC: u32 = 625;
pub const CKM_EC_KEY_PAIR_GEN: u32 = 4160;
pub const CKM_ECDH1_DERIVE: u32 = 4176;
pub const CKM_AES_CBC_PAD: u32 = 4229;
pub const CKM_AES_GCM: u32 = 4231;
pub const CKD_NULL: u32 = 1;
