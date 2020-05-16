/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::*;
use std::os::raw::{c_int, c_uchar, c_void};

pub type SECKEYPublicKey = SECKEYPublicKeyStr;
#[repr(C)]
pub struct SECKEYPublicKeyStr {
    pub arena: *mut PLArenaPool,
    pub keyType: u32, /* KeyType */
    pub pkcs11Slot: *mut PK11SlotInfo,
    pub pkcs11ID: CK_OBJECT_HANDLE,
    pub u: SECKEYPublicKeyStr_u,
}

#[repr(C)]
pub union SECKEYPublicKeyStr_u {
    pub rsa: SECKEYRSAPublicKey,
    pub dsa: SECKEYDSAPublicKey,
    pub dh: SECKEYDHPublicKey,
    pub kea: SECKEYKEAPublicKey,
    pub fortezza: SECKEYFortezzaPublicKey,
    pub ec: SECKEYECPublicKey,
}

pub type SECKEYPrivateKey = SECKEYPrivateKeyStr;
#[repr(C)]
pub struct SECKEYPrivateKeyStr {
    pub arena: *mut PLArenaPool,
    pub keyType: u32, /* KeyType */
    pub pkcs11Slot: *mut PK11SlotInfo,
    pub pkcs11ID: CK_OBJECT_HANDLE,
    pub pkcs11IsTemp: PRBool,
    pub wincx: *mut c_void,
    pub staticflags: PRUint32,
}

#[repr(u32)]
pub enum KeyType {
    nullKey = 0,
    rsaKey = 1,
    dsaKey = 2,
    fortezzaKey = 3,
    dhKey = 4,
    keaKey = 5,
    ecKey = 6,
    rsaPssKey = 7,
    rsaOaepKey = 8,
}

pub type SECKEYRSAPublicKey = SECKEYRSAPublicKeyStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYRSAPublicKeyStr {
    pub arena: *mut PLArenaPool,
    pub modulus: SECItem,
    pub publicExponent: SECItem,
}

pub type SECKEYDSAPublicKey = SECKEYDSAPublicKeyStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYDSAPublicKeyStr {
    pub params: SECKEYPQGParams,
    pub publicValue: SECItem,
}

pub type SECKEYPQGParams = SECKEYPQGParamsStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYPQGParamsStr {
    pub arena: *mut PLArenaPool,
    pub prime: SECItem,
    pub subPrime: SECItem,
    pub base: SECItem,
}

pub type SECKEYDHPublicKey = SECKEYDHPublicKeyStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYDHPublicKeyStr {
    pub arena: *mut PLArenaPool,
    pub prime: SECItem,
    pub base: SECItem,
    pub publicValue: SECItem,
}

pub type SECKEYKEAPublicKey = SECKEYKEAPublicKeyStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYKEAPublicKeyStr {
    pub params: SECKEYKEAParams,
    pub publicValue: SECItem,
}

pub type SECKEYKEAParams = SECKEYKEAParamsStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYKEAParamsStr {
    pub arena: *mut PLArenaPool,
    pub hash: SECItem,
}

pub type SECKEYFortezzaPublicKey = SECKEYFortezzaPublicKeyStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYFortezzaPublicKeyStr {
    pub KEAversion: c_int,
    pub DSSversion: c_int,
    pub KMID: [c_uchar; 8usize],
    pub clearance: SECItem,
    pub KEApriviledge: SECItem,
    pub DSSpriviledge: SECItem,
    pub KEAKey: SECItem,
    pub DSSKey: SECItem,
    pub params: SECKEYPQGParams,
    pub keaParams: SECKEYPQGParams,
}

pub type SECKEYECPublicKey = SECKEYECPublicKeyStr;
#[repr(C)]
#[derive(Copy, Clone)]
pub struct SECKEYECPublicKeyStr {
    pub DEREncodedParams: SECKEYECParams,
    pub size: c_int,
    pub publicValue: SECItem,
    pub encoding: u32, /* ECPointEncoding */
}

pub type SECKEYECParams = SECItem;

#[repr(u32)]
#[derive(Copy, Clone)]
pub enum ECPointEncoding {
    ECPoint_Uncompressed = 0,
    ECPoint_XOnly = 1,
    ECPoint_Undefined = 2,
}
