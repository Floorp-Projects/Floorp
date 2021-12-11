// Copyright 2017 Marcus Heese
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#![allow(non_camel_case_types, non_snake_case)]

// Cryptoki's packed structs interfere with the Clone trait, so we implement Copy and use this
macro_rules! packed_clone {
    ($name:ty) => (
        impl Clone for $name { fn clone(&self) -> $name { *self } }
    )
}

// packed structs only needed on Windows, pkcs11.h mentions
macro_rules! cryptoki_aligned {
    ($decl:item) => {
        #[cfg(windows)]
        #[repr(packed, C)]
        $decl
        #[cfg(not(windows))]
        #[repr(C)]
        $decl
    }
}

use std;
use std::mem;
use std::slice;
use std::ptr;
use num_bigint::BigUint;

use functions::*;
use super::CkFrom;


pub const CK_TRUE: CK_BBOOL = 1;
pub const CK_FALSE: CK_BBOOL = 0;

//// an unsigned 8-bit value
pub type CK_BYTE = u8;
pub type CK_BYTE_PTR = *mut CK_BYTE;

/// an unsigned 8-bit character
pub type CK_CHAR = CK_BYTE;
pub type CK_CHAR_PTR = *mut CK_CHAR;

/// an 8-bit UTF-8 character
pub type CK_UTF8CHAR = CK_BYTE;
pub type CK_UTF8CHAR_PTR = *mut CK_UTF8CHAR;

/// a BYTE-sized Boolean flag
pub type CK_BBOOL = CK_BYTE;

/// an unsigned value, at least 32 bits long
#[cfg(windows)]
pub type CK_ULONG = u32;
#[cfg(not(windows))]
pub type CK_ULONG = usize;
pub type CK_ULONG_PTR = *mut CK_ULONG;

/// a signed value, the same size as a CK_ULONG
#[cfg(windows)]
pub type CK_LONG = i32;
#[cfg(not(windows))]
pub type CK_LONG = isize;


/// at least 32 bits; each bit is a Boolean flag
pub type CK_FLAGS = CK_ULONG;

/* some special values for certain CK_ULONG variables */
pub const CK_UNAVAILABLE_INFORMATION: CK_ULONG = !0;
pub const CK_EFFECTIVELY_INFINITE: CK_ULONG = 0;

#[derive(Debug)]
#[repr(u8)]
pub enum CK_VOID {
  #[doc(hidden)] __Variant1,
  #[doc(hidden)] __Variant2,
}
pub type CK_VOID_PTR = *mut CK_VOID;

/// Pointer to a CK_VOID_PTR-- i.e., pointer to pointer to void
pub type CK_VOID_PTR_PTR = *mut CK_VOID_PTR;

/// The following value is always invalid if used as a session
/// handle or object handle
pub const CK_INVALID_HANDLE: CK_ULONG = 0;

cryptoki_aligned! {
  #[derive(Debug, Copy, Default)]
  pub struct CK_VERSION {
    pub major: CK_BYTE, /* integer portion of version number */
    pub minor: CK_BYTE, /* 1/100ths portion of version number */
  }
}
packed_clone!(CK_VERSION);

pub type CK_VERSION_PTR = *mut CK_VERSION;

cryptoki_aligned! {
  #[derive(Debug, Copy, Default)]
  pub struct CK_INFO {
    /* manufacturerID and libraryDecription have been changed from
    * CK_CHAR to CK_UTF8CHAR for v2.10 */
    pub cryptokiVersion: CK_VERSION,           /* Cryptoki interface ver */
    pub manufacturerID: [CK_UTF8CHAR; 32],     /* blank padded */
    pub flags: CK_FLAGS,                       /* must be zero */
    pub libraryDescription: [CK_UTF8CHAR; 32], /* blank padded */
    pub libraryVersion: CK_VERSION,            /* version of library */
  }
}
packed_clone!(CK_INFO);

impl CK_INFO {
  pub fn new() -> CK_INFO {
    CK_INFO {
      cryptokiVersion: Default::default(),
      manufacturerID: [32; 32],
      flags: 0,
      libraryDescription: [32; 32],
      libraryVersion: Default::default(),
    }
  }
}

pub type CK_INFO_PTR = *mut CK_INFO;

/// CK_NOTIFICATION enumerates the types of notifications that
/// Cryptoki provides to an application
pub type CK_NOTIFICATION = CK_ULONG;

pub const CKN_SURRENDER: CK_NOTIFICATION = 0;
pub const CKN_OTP_CHANGED: CK_NOTIFICATION = 1;

pub type CK_SLOT_ID = CK_ULONG;
pub type CK_SLOT_ID_PTR = *mut CK_SLOT_ID;

cryptoki_aligned! {
  /// CK_SLOT_INFO provides information about a slot
  pub struct CK_SLOT_INFO {
    /// slotDescription and manufacturerID have been changed from
    /// CK_CHAR to CK_UTF8CHAR for v2.10
    pub slotDescription: [CK_UTF8CHAR; 64], /* blank padded */
    pub manufacturerID: [CK_UTF8CHAR; 32], /* blank padded */
    pub flags: CK_FLAGS,

    /// version of hardware
    pub hardwareVersion: CK_VERSION, /* version of hardware */
    /// version of firmware
    pub firmwareVersion: CK_VERSION, /* version of firmware */
  }
}

impl Default for CK_SLOT_INFO {
  fn default() -> CK_SLOT_INFO {
    CK_SLOT_INFO {
      slotDescription: [32; 64],
      manufacturerID: [32; 32],
      flags: 0,
      hardwareVersion: Default::default(),
      firmwareVersion: Default::default(),
    }
  }
}

impl std::fmt::Debug for CK_SLOT_INFO {
  fn fmt(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
    let sd = self.slotDescription.to_vec();
    fmt
      .debug_struct("CK_SLOT_INFO")
      .field("slotDescription", &sd)
      .field("manufacturerID", &self.manufacturerID)
      .field("flags", &self.flags)
      .field("hardwareVersion", &self.hardwareVersion)
      .field("firmwareVersion", &self.firmwareVersion)
      .finish()
  }
}

/// a token is there
pub const CKF_TOKEN_PRESENT: CK_FLAGS = 0x00000001;
/// removable devices
pub const CKF_REMOVABLE_DEVICE: CK_FLAGS = 0x00000002;
/// hardware slot
pub const CKF_HW_SLOT: CK_FLAGS = 0x00000004;

pub type CK_SLOT_INFO_PTR = *mut CK_SLOT_INFO;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_TOKEN_INFO {
    /* label, manufacturerID, and model have been changed from
    * CK_CHAR to CK_UTF8CHAR for v2.10 */
    pub label: [CK_UTF8CHAR; 32],          /* blank padded */
    pub manufacturerID: [CK_UTF8CHAR; 32], /* blank padded */
    pub model: [CK_UTF8CHAR; 16],          /* blank padded */
    pub serialNumber: [CK_CHAR; 16],       /* blank padded */
    pub flags: CK_FLAGS,                   /* see below */
    pub ulMaxSessionCount: CK_ULONG,       /* max open sessions */
    pub ulSessionCount: CK_ULONG,          /* sess. now open */
    pub ulMaxRwSessionCount: CK_ULONG,     /* max R/W sessions */
    pub ulRwSessionCount: CK_ULONG,        /* R/W sess. now open */
    pub ulMaxPinLen: CK_ULONG,             /* in bytes */
    pub ulMinPinLen: CK_ULONG,             /* in bytes */
    pub ulTotalPublicMemory: CK_ULONG,     /* in bytes */
    pub ulFreePublicMemory: CK_ULONG,      /* in bytes */
    pub ulTotalPrivateMemory: CK_ULONG,    /* in bytes */
    pub ulFreePrivateMemory: CK_ULONG,     /* in bytes */
    pub hardwareVersion: CK_VERSION,       /* version of hardware */
    pub firmwareVersion: CK_VERSION,       /* version of firmware */
    pub utcTime: [CK_CHAR; 16],            /* time */
  }
}
packed_clone!(CK_TOKEN_INFO);

impl Default for CK_TOKEN_INFO {
  fn default() -> CK_TOKEN_INFO {
    CK_TOKEN_INFO {
      label: [32; 32],
      manufacturerID: [32; 32],
      model: [32; 16],
      serialNumber: [32; 16],
      flags: 0,
      ulMaxSessionCount: 0,
      ulSessionCount: 0,
      ulMaxRwSessionCount: 0,
      ulRwSessionCount: 0,
      ulMaxPinLen: 0,
      ulMinPinLen: 0,
      ulTotalPublicMemory: 0,
      ulFreePublicMemory: 0,
      ulTotalPrivateMemory: 0,
      ulFreePrivateMemory: 0,
      hardwareVersion: Default::default(),
      firmwareVersion: Default::default(),
      utcTime: [0; 16],
    }
  }
}

/// has random # generator
pub const CKF_RNG: CK_FLAGS = 0x00000001;

/// token is write-protected
pub const CKF_WRITE_PROTECTED: CK_FLAGS = 0x00000002;

/// user must login
pub const CKF_LOGIN_REQUIRED: CK_FLAGS = 0x00000004;

/// normal user's PIN is set
pub const CKF_USER_PIN_INITIALIZED: CK_FLAGS = 0x00000008;

/// CKF_RESTORE_KEY_NOT_NEEDED.  If it is set,
/// that means that *every* time the state of cryptographic
/// operations of a session is successfully saved, all keys
/// needed to continue those operations are stored in the state
pub const CKF_RESTORE_KEY_NOT_NEEDED: CK_FLAGS = 0x00000020;

/// CKF_CLOCK_ON_TOKEN.  If it is set, that means
/// that the token has some sort of clock.  The time on that
/// clock is returned in the token info structure
pub const CKF_CLOCK_ON_TOKEN: CK_FLAGS = 0x00000040;

/// CKF_PROTECTED_AUTHENTICATION_PATH.  If it is
/// set, that means that there is some way for the user to login
/// without sending a PIN through the Cryptoki library itself
pub const CKF_PROTECTED_AUTHENTICATION_PATH: CK_FLAGS = 0x00000100;

/// CKF_DUAL_CRYPTO_OPERATIONS.  If it is true,
/// that means that a single session with the token can perform
/// dual simultaneous cryptographic operations (digest and
/// encrypt; decrypt and digest; sign and encrypt; and decrypt
/// and sign)
pub const CKF_DUAL_CRYPTO_OPERATIONS: CK_FLAGS = 0x00000200;

/// CKF_TOKEN_INITIALIZED. If it is true, the
/// token has been initialized using C_InitializeToken or an
/// equivalent mechanism outside the scope of PKCS #11.
/// Calling C_InitializeToken when this flag is set will cause
/// the token to be reinitialized.
pub const CKF_TOKEN_INITIALIZED: CK_FLAGS = 0x00000400;

/// CKF_SECONDARY_AUTHENTICATION. If it is
/// true, the token supports secondary authentication for
/// private key objects.
pub const CKF_SECONDARY_AUTHENTICATION: CK_FLAGS = 0x00000800;

/// CKF_USER_PIN_COUNT_LOW. If it is true, an
/// incorrect user login PIN has been entered at least once
/// since the last successful authentication.
pub const CKF_USER_PIN_COUNT_LOW: CK_FLAGS = 0x00010000;

/// CKF_USER_PIN_FINAL_TRY. If it is true,
/// supplying an incorrect user PIN will it to become locked.
pub const CKF_USER_PIN_FINAL_TRY: CK_FLAGS = 0x00020000;

/// CKF_USER_PIN_LOCKED. If it is true, the
/// user PIN has been locked. User login to the token is not
/// possible.
pub const CKF_USER_PIN_LOCKED: CK_FLAGS = 0x00040000;

/// CKF_USER_PIN_TO_BE_CHANGED. If it is true,
/// the user PIN value is the default value set by token
/// initialization or manufacturing, or the PIN has been
/// expired by the card.
pub const CKF_USER_PIN_TO_BE_CHANGED: CK_FLAGS = 0x00080000;

/// CKF_SO_PIN_COUNT_LOW. If it is true, an
/// incorrect SO login PIN has been entered at least once since
/// the last successful authentication.
pub const CKF_SO_PIN_COUNT_LOW: CK_FLAGS = 0x00100000;

/// CKF_SO_PIN_FINAL_TRY. If it is true,
/// supplying an incorrect SO PIN will it to become locked.
pub const CKF_SO_PIN_FINAL_TRY: CK_FLAGS = 0x00200000;

/// CKF_SO_PIN_LOCKED. If it is true, the SO
/// PIN has been locked. SO login to the token is not possible.
pub const CKF_SO_PIN_LOCKED: CK_FLAGS = 0x00400000;

/// CKF_SO_PIN_TO_BE_CHANGED. If it is true,
/// the SO PIN value is the default value set by token
/// initialization or manufacturing, or the PIN has been
/// expired by the card.
pub const CKF_SO_PIN_TO_BE_CHANGED: CK_FLAGS = 0x00800000;

pub const CKF_ERROR_STATE: CK_FLAGS = 0x01000000;

pub type CK_TOKEN_INFO_PTR = *mut CK_TOKEN_INFO;

/// CK_SESSION_HANDLE is a Cryptoki-assigned value that
/// identifies a session
pub type CK_SESSION_HANDLE = CK_ULONG;
pub type CK_SESSION_HANDLE_PTR = *mut CK_SESSION_HANDLE;

/// CK_USER_TYPE enumerates the types of Cryptoki users
pub type CK_USER_TYPE = CK_ULONG;

/// Security Officer
pub const CKU_SO: CK_USER_TYPE = 0;
/// Normal user
pub const CKU_USER: CK_USER_TYPE = 1;
/// Context specific
pub const CKU_CONTEXT_SPECIFIC: CK_USER_TYPE = 2;

/// CK_STATE enumerates the session states
type CK_STATE = CK_ULONG;
pub const CKS_RO_PUBLIC_SESSION: CK_STATE = 0;
pub const CKS_RO_USER_FUNCTIONS: CK_STATE = 1;
pub const CKS_RW_PUBLIC_SESSION: CK_STATE = 2;
pub const CKS_RW_USER_FUNCTIONS: CK_STATE = 3;
pub const CKS_RW_SO_FUNCTIONS: CK_STATE = 4;

cryptoki_aligned! {
  #[derive(Debug, Default, Copy)]
  pub struct CK_SESSION_INFO {
    pub slotID: CK_SLOT_ID,
    pub state: CK_STATE,
    pub flags: CK_FLAGS,
    /// device-dependent error code
    pub ulDeviceError: CK_ULONG,
  }
}
packed_clone!(CK_SESSION_INFO);

/// session is r/w
pub const CKF_RW_SESSION: CK_FLAGS = 0x00000002;
/// no parallel
pub const CKF_SERIAL_SESSION: CK_FLAGS = 0x00000004;

pub type CK_SESSION_INFO_PTR = *mut CK_SESSION_INFO;

/// CK_OBJECT_HANDLE is a token-specific identifier for an
/// object
pub type CK_OBJECT_HANDLE = CK_ULONG;
pub type CK_OBJECT_HANDLE_PTR = *mut CK_OBJECT_HANDLE;

/// CK_OBJECT_CLASS is a value that identifies the classes (or
/// types) of objects that Cryptoki recognizes.  It is defined
/// as follows:
pub type CK_OBJECT_CLASS = CK_ULONG;

/// The following classes of objects are defined:
pub const CKO_DATA: CK_OBJECT_CLASS = 0x00000000;
pub const CKO_CERTIFICATE: CK_OBJECT_CLASS = 0x00000001;
pub const CKO_PUBLIC_KEY: CK_OBJECT_CLASS = 0x00000002;
pub const CKO_PRIVATE_KEY: CK_OBJECT_CLASS = 0x00000003;
pub const CKO_SECRET_KEY: CK_OBJECT_CLASS = 0x00000004;
pub const CKO_HW_FEATURE: CK_OBJECT_CLASS = 0x00000005;
pub const CKO_DOMAIN_PARAMETERS: CK_OBJECT_CLASS = 0x00000006;
pub const CKO_MECHANISM: CK_OBJECT_CLASS = 0x00000007;
pub const CKO_OTP_KEY: CK_OBJECT_CLASS = 0x00000008;
pub const CKO_VENDOR_DEFINED: CK_OBJECT_CLASS = 0x80000000;

pub type CK_OBJECT_CLASS_PTR = *mut CK_OBJECT_CLASS;

/// CK_HW_FEATURE_TYPE is a value that identifies the hardware feature type
/// of an object with CK_OBJECT_CLASS equal to CKO_HW_FEATURE.
pub type CK_HW_FEATURE_TYPE = CK_ULONG;

/// The following hardware feature types are defined
pub const CKH_MONOTONIC_COUNTER: CK_HW_FEATURE_TYPE = 0x00000001;
pub const CKH_CLOCK: CK_HW_FEATURE_TYPE = 0x00000002;
pub const CKH_USER_INTERFACE: CK_HW_FEATURE_TYPE = 0x00000003;
pub const CKH_VENDOR_DEFINED: CK_HW_FEATURE_TYPE = 0x80000000;

/// CK_KEY_TYPE is a value that identifies a key type
pub type CK_KEY_TYPE = CK_ULONG;

/// the following key types are defined:
pub const CKK_RSA: CK_KEY_TYPE = 0x00000000;
pub const CKK_DSA: CK_KEY_TYPE = 0x00000001;
pub const CKK_DH: CK_KEY_TYPE = 0x00000002;
pub const CKK_ECDSA: CK_KEY_TYPE = CKK_EC;
pub const CKK_EC: CK_KEY_TYPE = 0x00000003;
pub const CKK_X9_42_DH: CK_KEY_TYPE = 0x00000004;
pub const CKK_KEA: CK_KEY_TYPE = 0x00000005;
pub const CKK_GENERIC_SECRET: CK_KEY_TYPE = 0x00000010;
pub const CKK_RC2: CK_KEY_TYPE = 0x00000011;
pub const CKK_RC4: CK_KEY_TYPE = 0x00000012;
pub const CKK_DES: CK_KEY_TYPE = 0x00000013;
pub const CKK_DES2: CK_KEY_TYPE = 0x00000014;
pub const CKK_DES3: CK_KEY_TYPE = 0x00000015;
pub const CKK_CAST: CK_KEY_TYPE = 0x00000016;
pub const CKK_CAST3: CK_KEY_TYPE = 0x00000017;
pub const CKK_CAST5: CK_KEY_TYPE = CKK_CAST128;
pub const CKK_CAST128: CK_KEY_TYPE = 0x00000018;
pub const CKK_RC5: CK_KEY_TYPE = 0x00000019;
pub const CKK_IDEA: CK_KEY_TYPE = 0x0000001A;
pub const CKK_SKIPJACK: CK_KEY_TYPE = 0x0000001B;
pub const CKK_BATON: CK_KEY_TYPE = 0x0000001C;
pub const CKK_JUNIPER: CK_KEY_TYPE = 0x0000001D;
pub const CKK_CDMF: CK_KEY_TYPE = 0x0000001E;
pub const CKK_AES: CK_KEY_TYPE = 0x0000001F;
pub const CKK_BLOWFISH: CK_KEY_TYPE = 0x00000020;
pub const CKK_TWOFISH: CK_KEY_TYPE = 0x00000021;
pub const CKK_SECURID: CK_KEY_TYPE = 0x00000022;
pub const CKK_HOTP: CK_KEY_TYPE = 0x00000023;
pub const CKK_ACTI: CK_KEY_TYPE = 0x00000024;
pub const CKK_CAMELLIA: CK_KEY_TYPE = 0x00000025;
pub const CKK_ARIA: CK_KEY_TYPE = 0x00000026;
pub const CKK_MD5_HMAC: CK_KEY_TYPE = 0x00000027;
pub const CKK_SHA_1_HMAC: CK_KEY_TYPE = 0x00000028;
pub const CKK_RIPEMD128_HMAC: CK_KEY_TYPE = 0x00000029;
pub const CKK_RIPEMD160_HMAC: CK_KEY_TYPE = 0x0000002A;
pub const CKK_SHA256_HMAC: CK_KEY_TYPE = 0x0000002B;
pub const CKK_SHA384_HMAC: CK_KEY_TYPE = 0x0000002C;
pub const CKK_SHA512_HMAC: CK_KEY_TYPE = 0x0000002D;
pub const CKK_SHA224_HMAC: CK_KEY_TYPE = 0x0000002E;
pub const CKK_SEED: CK_KEY_TYPE = 0x0000002F;
pub const CKK_GOSTR3410: CK_KEY_TYPE = 0x00000030;
pub const CKK_GOSTR3411: CK_KEY_TYPE = 0x00000031;
pub const CKK_GOST28147: CK_KEY_TYPE = 0x00000032;
pub const CKK_VENDOR_DEFINED: CK_KEY_TYPE = 0x80000000;

/// CK_CERTIFICATE_TYPE is a value that identifies a certificate
/// type
pub type CK_CERTIFICATE_TYPE = CK_ULONG;

pub const CK_CERTIFICATE_CATEGORY_UNSPECIFIED: CK_ULONG = 0;
pub const CK_CERTIFICATE_CATEGORY_TOKEN_USER: CK_ULONG = 1;
pub const CK_CERTIFICATE_CATEGORY_AUTHORITY: CK_ULONG = 2;
pub const CK_CERTIFICATE_CATEGORY_OTHER_ENTITY: CK_ULONG = 3;

pub const CK_SECURITY_DOMAIN_UNSPECIFIED: CK_ULONG = 0;
pub const CK_SECURITY_DOMAIN_MANUFACTURER: CK_ULONG = 1;
pub const CK_SECURITY_DOMAIN_OPERATOR: CK_ULONG = 2;
pub const CK_SECURITY_DOMAIN_THIRD_PARTY: CK_ULONG = 3;

/// The following certificate types are defined:
pub const CKC_X_509: CK_CERTIFICATE_TYPE = 0x00000000;
pub const CKC_X_509_ATTR_CERT: CK_CERTIFICATE_TYPE = 0x00000001;
pub const CKC_WTLS: CK_CERTIFICATE_TYPE = 0x00000002;
pub const CKC_VENDOR_DEFINED: CK_CERTIFICATE_TYPE = 0x80000000;

/// CK_ATTRIBUTE_TYPE is a value that identifies an attribute
/// type
pub type CK_ATTRIBUTE_TYPE = CK_ULONG;

/// The CKF_ARRAY_ATTRIBUTE flag identifies an attribute which
/// consists of an array of values.
pub const CKF_ARRAY_ATTRIBUTE: CK_FLAGS = 0x40000000;

/// The following OTP-related defines relate to the CKA_OTP_FORMAT attribute
pub const CK_OTP_FORMAT_DECIMAL: CK_ULONG = 0;
pub const CK_OTP_FORMAT_HEXADECIMAL: CK_ULONG = 1;
pub const CK_OTP_FORMAT_ALPHANUMERIC: CK_ULONG = 2;
pub const CK_OTP_FORMAT_BINARY: CK_ULONG = 3;

/// The following OTP-related defines relate to the CKA_OTP_..._REQUIREMENT
/// attributes
pub const CK_OTP_PARAM_IGNORED: CK_ULONG = 0;
pub const CK_OTP_PARAM_OPTIONAL: CK_ULONG = 1;
pub const CK_OTP_PARAM_MANDATORY: CK_ULONG = 2;

/// The following attribute types are defined:
pub const CKA_CLASS: CK_ATTRIBUTE_TYPE = 0x00000000;
pub const CKA_TOKEN: CK_ATTRIBUTE_TYPE = 0x00000001;
pub const CKA_PRIVATE: CK_ATTRIBUTE_TYPE = 0x00000002;
pub const CKA_LABEL: CK_ATTRIBUTE_TYPE = 0x00000003;
pub const CKA_APPLICATION: CK_ATTRIBUTE_TYPE = 0x00000010;
pub const CKA_VALUE: CK_ATTRIBUTE_TYPE = 0x00000011;
pub const CKA_OBJECT_ID: CK_ATTRIBUTE_TYPE = 0x00000012;
pub const CKA_CERTIFICATE_TYPE: CK_ATTRIBUTE_TYPE = 0x00000080;
pub const CKA_ISSUER: CK_ATTRIBUTE_TYPE = 0x00000081;
pub const CKA_SERIAL_NUMBER: CK_ATTRIBUTE_TYPE = 0x00000082;
pub const CKA_AC_ISSUER: CK_ATTRIBUTE_TYPE = 0x00000083;
pub const CKA_OWNER: CK_ATTRIBUTE_TYPE = 0x00000084;
pub const CKA_ATTR_TYPES: CK_ATTRIBUTE_TYPE = 0x00000085;
pub const CKA_TRUSTED: CK_ATTRIBUTE_TYPE = 0x00000086;
pub const CKA_CERTIFICATE_CATEGORY: CK_ATTRIBUTE_TYPE = 0x00000087;
pub const CKA_JAVA_MIDP_SECURITY_DOMAIN: CK_ATTRIBUTE_TYPE = 0x00000088;
pub const CKA_URL: CK_ATTRIBUTE_TYPE = 0x00000089;
pub const CKA_HASH_OF_SUBJECT_PUBLIC_KEY: CK_ATTRIBUTE_TYPE = 0x0000008A;
pub const CKA_HASH_OF_ISSUER_PUBLIC_KEY: CK_ATTRIBUTE_TYPE = 0x0000008B;
pub const CKA_NAME_HASH_ALGORITHM: CK_ATTRIBUTE_TYPE = 0x0000008C;
pub const CKA_CHECK_VALUE: CK_ATTRIBUTE_TYPE = 0x00000090;

pub const CKA_KEY_TYPE: CK_ATTRIBUTE_TYPE = 0x00000100;
pub const CKA_SUBJECT: CK_ATTRIBUTE_TYPE = 0x00000101;
pub const CKA_ID: CK_ATTRIBUTE_TYPE = 0x00000102;
pub const CKA_SENSITIVE: CK_ATTRIBUTE_TYPE = 0x00000103;
pub const CKA_ENCRYPT: CK_ATTRIBUTE_TYPE = 0x00000104;
pub const CKA_DECRYPT: CK_ATTRIBUTE_TYPE = 0x00000105;
pub const CKA_WRAP: CK_ATTRIBUTE_TYPE = 0x00000106;
pub const CKA_UNWRAP: CK_ATTRIBUTE_TYPE = 0x00000107;
pub const CKA_SIGN: CK_ATTRIBUTE_TYPE = 0x00000108;
pub const CKA_SIGN_RECOVER: CK_ATTRIBUTE_TYPE = 0x00000109;
pub const CKA_VERIFY: CK_ATTRIBUTE_TYPE = 0x0000010A;
pub const CKA_VERIFY_RECOVER: CK_ATTRIBUTE_TYPE = 0x0000010B;
pub const CKA_DERIVE: CK_ATTRIBUTE_TYPE = 0x0000010C;
pub const CKA_START_DATE: CK_ATTRIBUTE_TYPE = 0x00000110;
pub const CKA_END_DATE: CK_ATTRIBUTE_TYPE = 0x00000111;
pub const CKA_MODULUS: CK_ATTRIBUTE_TYPE = 0x00000120;
pub const CKA_MODULUS_BITS: CK_ATTRIBUTE_TYPE = 0x00000121;
pub const CKA_PUBLIC_EXPONENT: CK_ATTRIBUTE_TYPE = 0x00000122;
pub const CKA_PRIVATE_EXPONENT: CK_ATTRIBUTE_TYPE = 0x00000123;
pub const CKA_PRIME_1: CK_ATTRIBUTE_TYPE = 0x00000124;
pub const CKA_PRIME_2: CK_ATTRIBUTE_TYPE = 0x00000125;
pub const CKA_EXPONENT_1: CK_ATTRIBUTE_TYPE = 0x00000126;
pub const CKA_EXPONENT_2: CK_ATTRIBUTE_TYPE = 0x00000127;
pub const CKA_COEFFICIENT: CK_ATTRIBUTE_TYPE = 0x00000128;
pub const CKA_PUBLIC_KEY_INFO: CK_ATTRIBUTE_TYPE = 0x00000129;
pub const CKA_PRIME: CK_ATTRIBUTE_TYPE = 0x00000130;
pub const CKA_SUBPRIME: CK_ATTRIBUTE_TYPE = 0x00000131;
pub const CKA_BASE: CK_ATTRIBUTE_TYPE = 0x00000132;

pub const CKA_PRIME_BITS: CK_ATTRIBUTE_TYPE = 0x00000133;
pub const CKA_SUBPRIME_BITS: CK_ATTRIBUTE_TYPE = 0x00000134;
pub const CKA_SUB_PRIME_BITS: CK_ATTRIBUTE_TYPE = CKA_SUBPRIME_BITS;

pub const CKA_VALUE_BITS: CK_ATTRIBUTE_TYPE = 0x00000160;
pub const CKA_VALUE_LEN: CK_ATTRIBUTE_TYPE = 0x00000161;
pub const CKA_EXTRACTABLE: CK_ATTRIBUTE_TYPE = 0x00000162;
pub const CKA_LOCAL: CK_ATTRIBUTE_TYPE = 0x00000163;
pub const CKA_NEVER_EXTRACTABLE: CK_ATTRIBUTE_TYPE = 0x00000164;
pub const CKA_ALWAYS_SENSITIVE: CK_ATTRIBUTE_TYPE = 0x00000165;
pub const CKA_KEY_GEN_MECHANISM: CK_ATTRIBUTE_TYPE = 0x00000166;

pub const CKA_MODIFIABLE: CK_ATTRIBUTE_TYPE = 0x00000170;
pub const CKA_COPYABLE: CK_ATTRIBUTE_TYPE = 0x00000171;

pub const CKA_DESTROYABLE: CK_ATTRIBUTE_TYPE = 0x00000172;

pub const CKA_ECDSA_PARAMS: CK_ATTRIBUTE_TYPE = CKA_EC_PARAMS;
pub const CKA_EC_PARAMS: CK_ATTRIBUTE_TYPE = 0x00000180;

pub const CKA_EC_POINT: CK_ATTRIBUTE_TYPE = 0x00000181;

pub const CKA_SECONDARY_AUTH: CK_ATTRIBUTE_TYPE = 0x00000200; /* Deprecated */
pub const CKA_AUTH_PIN_FLAGS: CK_ATTRIBUTE_TYPE = 0x00000201; /* Deprecated */

pub const CKA_ALWAYS_AUTHENTICATE: CK_ATTRIBUTE_TYPE = 0x00000202;

pub const CKA_WRAP_WITH_TRUSTED: CK_ATTRIBUTE_TYPE = 0x00000210;
pub const CKA_WRAP_TEMPLATE: CK_ATTRIBUTE_TYPE = (CKF_ARRAY_ATTRIBUTE | 0x00000211);
pub const CKA_UNWRAP_TEMPLATE: CK_ATTRIBUTE_TYPE = (CKF_ARRAY_ATTRIBUTE | 0x00000212);
pub const CKA_DERIVE_TEMPLATE: CK_ATTRIBUTE_TYPE = (CKF_ARRAY_ATTRIBUTE | 0x00000213);

pub const CKA_OTP_FORMAT: CK_ATTRIBUTE_TYPE = 0x00000220;
pub const CKA_OTP_LENGTH: CK_ATTRIBUTE_TYPE = 0x00000221;
pub const CKA_OTP_TIME_INTERVAL: CK_ATTRIBUTE_TYPE = 0x00000222;
pub const CKA_OTP_USER_FRIENDLY_MODE: CK_ATTRIBUTE_TYPE = 0x00000223;
pub const CKA_OTP_CHALLENGE_REQUIREMENT: CK_ATTRIBUTE_TYPE = 0x00000224;
pub const CKA_OTP_TIME_REQUIREMENT: CK_ATTRIBUTE_TYPE = 0x00000225;
pub const CKA_OTP_COUNTER_REQUIREMENT: CK_ATTRIBUTE_TYPE = 0x00000226;
pub const CKA_OTP_PIN_REQUIREMENT: CK_ATTRIBUTE_TYPE = 0x00000227;
pub const CKA_OTP_COUNTER: CK_ATTRIBUTE_TYPE = 0x0000022E;
pub const CKA_OTP_TIME: CK_ATTRIBUTE_TYPE = 0x0000022F;
pub const CKA_OTP_USER_IDENTIFIER: CK_ATTRIBUTE_TYPE = 0x0000022A;
pub const CKA_OTP_SERVICE_IDENTIFIER: CK_ATTRIBUTE_TYPE = 0x0000022B;
pub const CKA_OTP_SERVICE_LOGO: CK_ATTRIBUTE_TYPE = 0x0000022C;
pub const CKA_OTP_SERVICE_LOGO_TYPE: CK_ATTRIBUTE_TYPE = 0x0000022D;

pub const CKA_GOSTR3410_PARAMS: CK_ATTRIBUTE_TYPE = 0x00000250;
pub const CKA_GOSTR3411_PARAMS: CK_ATTRIBUTE_TYPE = 0x00000251;
pub const CKA_GOST28147_PARAMS: CK_ATTRIBUTE_TYPE = 0x00000252;

pub const CKA_HW_FEATURE_TYPE: CK_ATTRIBUTE_TYPE = 0x00000300;
pub const CKA_RESET_ON_INIT: CK_ATTRIBUTE_TYPE = 0x00000301;
pub const CKA_HAS_RESET: CK_ATTRIBUTE_TYPE = 0x00000302;

pub const CKA_PIXEL_X: CK_ATTRIBUTE_TYPE = 0x00000400;
pub const CKA_PIXEL_Y: CK_ATTRIBUTE_TYPE = 0x00000401;
pub const CKA_RESOLUTION: CK_ATTRIBUTE_TYPE = 0x00000402;
pub const CKA_CHAR_ROWS: CK_ATTRIBUTE_TYPE = 0x00000403;
pub const CKA_CHAR_COLUMNS: CK_ATTRIBUTE_TYPE = 0x00000404;
pub const CKA_COLOR: CK_ATTRIBUTE_TYPE = 0x00000405;
pub const CKA_BITS_PER_PIXEL: CK_ATTRIBUTE_TYPE = 0x00000406;
pub const CKA_CHAR_SETS: CK_ATTRIBUTE_TYPE = 0x00000480;
pub const CKA_ENCODING_METHODS: CK_ATTRIBUTE_TYPE = 0x00000481;
pub const CKA_MIME_TYPES: CK_ATTRIBUTE_TYPE = 0x00000482;
pub const CKA_MECHANISM_TYPE: CK_ATTRIBUTE_TYPE = 0x00000500;
pub const CKA_REQUIRED_CMS_ATTRIBUTES: CK_ATTRIBUTE_TYPE = 0x00000501;
pub const CKA_DEFAULT_CMS_ATTRIBUTES: CK_ATTRIBUTE_TYPE = 0x00000502;
pub const CKA_SUPPORTED_CMS_ATTRIBUTES: CK_ATTRIBUTE_TYPE = 0x00000503;
pub const CKA_ALLOWED_MECHANISMS: CK_ATTRIBUTE_TYPE = (CKF_ARRAY_ATTRIBUTE | 0x00000600);

pub const CKA_VENDOR_DEFINED: CK_ATTRIBUTE_TYPE = 0x80000000;

cryptoki_aligned! {
  /// CK_ATTRIBUTE is a structure that includes the type, length
  /// and value of an attribute
  #[derive(Copy)]
  pub struct CK_ATTRIBUTE {
    pub attrType: CK_ATTRIBUTE_TYPE,
    pub pValue: CK_VOID_PTR,
    /// in bytes
    pub ulValueLen: CK_ULONG,
  }
}
packed_clone!(CK_ATTRIBUTE);

pub type CK_ATTRIBUTE_PTR = *mut CK_ATTRIBUTE;

impl Default for CK_ATTRIBUTE {
  fn default() -> Self {
    Self {
      attrType: CKA_VENDOR_DEFINED,
      pValue: ptr::null_mut(),
      ulValueLen: 0,
    }
  }
}

impl std::fmt::Debug for CK_ATTRIBUTE {
  fn fmt(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
    let attrType = format!("0x{:x}", self.attrType);
    let data = unsafe { slice::from_raw_parts(self.pValue as *const u8, self.ulValueLen as usize) };
    fmt
      .debug_struct("CK_ATTRIBUTE")
      .field("attrType", &attrType)
      .field("pValue", &data)
      .field("ulValueLen", &self.ulValueLen)
      .finish()
  }
}

impl CK_ATTRIBUTE {
  pub fn new(attrType: CK_ATTRIBUTE_TYPE) -> Self {
    Self {
      attrType,
      pValue: ptr::null_mut(),
      ulValueLen: 0,
    }
  }

  pub fn with_bool(mut self, b: &CK_BBOOL) -> Self {
    self.pValue = b as *const CK_BBOOL as CK_VOID_PTR;
    self.ulValueLen = 1;
    self
  }

  pub fn set_bool(&mut self, b: &CK_BBOOL) {
    self.pValue = b as *const CK_BBOOL as CK_VOID_PTR;
    if self.ulValueLen == 0 {
      self.ulValueLen = 1;
    }
  }

  pub fn get_bool(&self) -> bool {
    let data: CK_BBOOL = unsafe { mem::transmute_copy(&*self.pValue) };
    CkFrom::from(data)
  }

  pub fn with_ck_ulong(mut self, val: &CK_ULONG) -> Self {
    self.pValue = val as *const _ as CK_VOID_PTR;
    self.ulValueLen = std::mem::size_of::<CK_ULONG>() as CK_ULONG;
    self
  }

  pub fn set_ck_ulong(&mut self, val: &CK_ULONG) {
    self.pValue = val as *const _ as CK_VOID_PTR;
    if self.ulValueLen == 0 {
      self.ulValueLen = std::mem::size_of::<CK_ULONG>() as CK_ULONG;
    }
  }

  pub fn get_ck_ulong(&self) -> CK_ULONG {
    unsafe { mem::transmute_copy(&*self.pValue) }
  }

  pub fn with_ck_long(mut self, val: &CK_LONG) -> Self {
    self.pValue = val as *const _ as CK_VOID_PTR;
    self.ulValueLen = std::mem::size_of::<CK_LONG>() as CK_ULONG;
    self
  }

  pub fn set_ck_long(&mut self, val: &CK_LONG) {
    self.pValue = val as *const _ as CK_VOID_PTR;
    if self.ulValueLen == 0 {
      self.ulValueLen = std::mem::size_of::<CK_LONG>() as CK_ULONG;
    }
  }

  pub fn get_ck_long(&self) -> CK_LONG {
    unsafe { mem::transmute_copy(&*self.pValue) }
  }

  pub fn with_biginteger(mut self, val: &[u8]) -> Self {
    self.pValue = val.as_ptr() as CK_VOID_PTR;
    self.ulValueLen = val.len() as CK_ULONG;
    self
  }

  pub fn set_biginteger(&mut self, val: &[u8]) {
    self.pValue = val.as_ptr() as CK_VOID_PTR;
    if self.ulValueLen == 0 {
      self.ulValueLen = val.len() as CK_ULONG;
    }
  }

  pub fn get_biginteger(&self) -> BigUint {
    let slice = unsafe { slice::from_raw_parts(self.pValue as CK_BYTE_PTR, self.ulValueLen as usize) };
    BigUint::from_bytes_le(slice)
  }

  pub fn with_bytes(mut self, val: &[CK_BYTE]) -> Self {
    self.pValue = val.as_ptr() as CK_VOID_PTR;
    self.ulValueLen = val.len() as CK_ULONG;
    self
  }

  pub fn set_bytes(&mut self, val: &[CK_BYTE]) {
    self.pValue = val.as_ptr() as CK_VOID_PTR;
    if self.ulValueLen == 0 {
      self.ulValueLen = val.len() as CK_ULONG;
    }
  }

  pub fn get_bytes(&self) -> Vec<CK_BYTE> {
    let slice = unsafe { slice::from_raw_parts(self.pValue as CK_BYTE_PTR, self.ulValueLen as usize) };
    Vec::from(slice)
  }

  pub fn with_string(mut self, str: &String) -> Self {
    self.pValue = str.as_ptr() as CK_VOID_PTR;
    self.ulValueLen = str.len() as CK_ULONG;
    self
  }

  pub fn set_string(&mut self, str: &String) {
    self.pValue = str.as_ptr() as CK_VOID_PTR;
    if self.ulValueLen == 0 {
      self.ulValueLen = str.len() as CK_ULONG;
    }
  }

  pub fn get_string(&self) -> String {
    let slice = unsafe { slice::from_raw_parts(self.pValue as CK_BYTE_PTR, self.ulValueLen as usize) };
    String::from_utf8_lossy(slice).into_owned()
  }

  pub fn with_date(mut self, date: &CK_DATE) -> Self {
    self.pValue = (date as *const CK_DATE) as CK_VOID_PTR;
    self.ulValueLen = mem::size_of::<CK_DATE>() as CK_ULONG;
    self
  }

  pub fn set_date(&mut self, date: &CK_DATE) {
    self.pValue = (date as *const CK_DATE) as CK_VOID_PTR;
    if self.ulValueLen == 0 {
      self.ulValueLen = mem::size_of::<CK_DATE>() as CK_ULONG;
    }
  }

  pub fn get_date(&self) -> CK_DATE {
    unsafe { mem::transmute_copy(&*self.pValue) }
  }

  // this works for C structs and primitives, but not for vectors, slices, strings
  //pub fn set_ptr<T>(&mut self, val: &T) {
  //  self.pValue = (val as *const T) as CK_VOID_PTR;
  //}
}

//trait CkAttributeFrom<T> {
//    fn from_ck(T, CK_ATTRIBUTE_TYPE) -> Self;
//}
//
//trait CkAttributeInto<T> {
//    fn into_attribute(self, CK_ATTRIBUTE_TYPE) -> CK_ATTRIBUTE;
//}
//
//impl<T> CkAttributeInto<T> for T where CK_ATTRIBUTE: CkAttributeFrom<T> {
//    fn into_attribute(self, attrType: CK_ATTRIBUTE_TYPE) -> CK_ATTRIBUTE {
//        CkAttributeFrom::from_ck(self, attrType)
//    }
//}
//
//impl CkAttributeFrom<bool> for CK_ATTRIBUTE {
//    fn from_ck(b: bool, attrType: CK_ATTRIBUTE_TYPE) -> CK_ATTRIBUTE {
//        let val: CK_BBOOL = if b { 1 } else { 0 };
//        let ret = Self {
//            attrType: attrType,
//            pValue: &val as *const u8 as *const CK_VOID,
//            ulValueLen: 1,
//        };
//        println!("{:?}", ret);
//        ret
//    }
//}

cryptoki_aligned! {
  /// CK_DATE is a structure that defines a date
  #[derive(Debug, Default, Copy)]
  pub struct CK_DATE {
    /// the year ("1900" - "9999")
    pub year: [CK_CHAR; 4],
    /// the month ("01" - "12")
    pub month: [CK_CHAR; 2],
    /// the day   ("01" - "31")
    pub day: [CK_CHAR; 2],
  }
}
packed_clone!(CK_DATE);

/// CK_MECHANISM_TYPE is a value that identifies a mechanism
/// type
pub type CK_MECHANISM_TYPE = CK_ULONG;

/// the following mechanism types are defined:
pub const CKM_RSA_PKCS_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x00000000;
pub const CKM_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000001;
pub const CKM_RSA_9796: CK_MECHANISM_TYPE = 0x00000002;
pub const CKM_RSA_X_509: CK_MECHANISM_TYPE = 0x00000003;

pub const CKM_MD2_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000004;
pub const CKM_MD5_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000005;
pub const CKM_SHA1_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000006;

pub const CKM_RIPEMD128_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000007;
pub const CKM_RIPEMD160_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000008;
pub const CKM_RSA_PKCS_OAEP: CK_MECHANISM_TYPE = 0x00000009;

pub const CKM_RSA_X9_31_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x0000000A;
pub const CKM_RSA_X9_31: CK_MECHANISM_TYPE = 0x0000000B;
pub const CKM_SHA1_RSA_X9_31: CK_MECHANISM_TYPE = 0x0000000C;
pub const CKM_RSA_PKCS_PSS: CK_MECHANISM_TYPE = 0x0000000D;
pub const CKM_SHA1_RSA_PKCS_PSS: CK_MECHANISM_TYPE = 0x0000000E;

pub const CKM_DSA_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x00000010;
pub const CKM_DSA: CK_MECHANISM_TYPE = 0x00000011;
pub const CKM_DSA_SHA1: CK_MECHANISM_TYPE = 0x00000012;
pub const CKM_DSA_SHA224: CK_MECHANISM_TYPE = 0x00000013;
pub const CKM_DSA_SHA256: CK_MECHANISM_TYPE = 0x00000014;
pub const CKM_DSA_SHA384: CK_MECHANISM_TYPE = 0x00000015;
pub const CKM_DSA_SHA512: CK_MECHANISM_TYPE = 0x00000016;

pub const CKM_DH_PKCS_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x00000020;
pub const CKM_DH_PKCS_DERIVE: CK_MECHANISM_TYPE = 0x00000021;

pub const CKM_X9_42_DH_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x00000030;
pub const CKM_X9_42_DH_DERIVE: CK_MECHANISM_TYPE = 0x00000031;
pub const CKM_X9_42_DH_HYBRID_DERIVE: CK_MECHANISM_TYPE = 0x00000032;
pub const CKM_X9_42_MQV_DERIVE: CK_MECHANISM_TYPE = 0x00000033;

pub const CKM_SHA256_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000040;
pub const CKM_SHA384_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000041;
pub const CKM_SHA512_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000042;
pub const CKM_SHA256_RSA_PKCS_PSS: CK_MECHANISM_TYPE = 0x00000043;
pub const CKM_SHA384_RSA_PKCS_PSS: CK_MECHANISM_TYPE = 0x00000044;
pub const CKM_SHA512_RSA_PKCS_PSS: CK_MECHANISM_TYPE = 0x00000045;

pub const CKM_SHA224_RSA_PKCS: CK_MECHANISM_TYPE = 0x00000046;
pub const CKM_SHA224_RSA_PKCS_PSS: CK_MECHANISM_TYPE = 0x00000047;

pub const CKM_SHA512_224: CK_MECHANISM_TYPE = 0x00000048;
pub const CKM_SHA512_224_HMAC: CK_MECHANISM_TYPE = 0x00000049;
pub const CKM_SHA512_224_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x0000004A;
pub const CKM_SHA512_224_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x0000004B;
pub const CKM_SHA512_256: CK_MECHANISM_TYPE = 0x0000004C;
pub const CKM_SHA512_256_HMAC: CK_MECHANISM_TYPE = 0x0000004D;
pub const CKM_SHA512_256_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x0000004E;
pub const CKM_SHA512_256_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x0000004F;

pub const CKM_SHA512_T: CK_MECHANISM_TYPE = 0x00000050;
pub const CKM_SHA512_T_HMAC: CK_MECHANISM_TYPE = 0x00000051;
pub const CKM_SHA512_T_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000052;
pub const CKM_SHA512_T_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000053;

pub const CKM_RC2_KEY_GEN: CK_MECHANISM_TYPE = 0x00000100;
pub const CKM_RC2_ECB: CK_MECHANISM_TYPE = 0x00000101;
pub const CKM_RC2_CBC: CK_MECHANISM_TYPE = 0x00000102;
pub const CKM_RC2_MAC: CK_MECHANISM_TYPE = 0x00000103;

pub const CKM_RC2_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000104;
pub const CKM_RC2_CBC_PAD: CK_MECHANISM_TYPE = 0x00000105;

pub const CKM_RC4_KEY_GEN: CK_MECHANISM_TYPE = 0x00000110;
pub const CKM_RC4: CK_MECHANISM_TYPE = 0x00000111;
pub const CKM_DES_KEY_GEN: CK_MECHANISM_TYPE = 0x00000120;
pub const CKM_DES_ECB: CK_MECHANISM_TYPE = 0x00000121;
pub const CKM_DES_CBC: CK_MECHANISM_TYPE = 0x00000122;
pub const CKM_DES_MAC: CK_MECHANISM_TYPE = 0x00000123;

pub const CKM_DES_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000124;
pub const CKM_DES_CBC_PAD: CK_MECHANISM_TYPE = 0x00000125;

pub const CKM_DES2_KEY_GEN: CK_MECHANISM_TYPE = 0x00000130;
pub const CKM_DES3_KEY_GEN: CK_MECHANISM_TYPE = 0x00000131;
pub const CKM_DES3_ECB: CK_MECHANISM_TYPE = 0x00000132;
pub const CKM_DES3_CBC: CK_MECHANISM_TYPE = 0x00000133;
pub const CKM_DES3_MAC: CK_MECHANISM_TYPE = 0x00000134;

pub const CKM_DES3_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000135;
pub const CKM_DES3_CBC_PAD: CK_MECHANISM_TYPE = 0x00000136;
pub const CKM_DES3_CMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000137;
pub const CKM_DES3_CMAC: CK_MECHANISM_TYPE = 0x00000138;
pub const CKM_CDMF_KEY_GEN: CK_MECHANISM_TYPE = 0x00000140;
pub const CKM_CDMF_ECB: CK_MECHANISM_TYPE = 0x00000141;
pub const CKM_CDMF_CBC: CK_MECHANISM_TYPE = 0x00000142;
pub const CKM_CDMF_MAC: CK_MECHANISM_TYPE = 0x00000143;
pub const CKM_CDMF_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000144;
pub const CKM_CDMF_CBC_PAD: CK_MECHANISM_TYPE = 0x00000145;

pub const CKM_DES_OFB64: CK_MECHANISM_TYPE = 0x00000150;
pub const CKM_DES_OFB8: CK_MECHANISM_TYPE = 0x00000151;
pub const CKM_DES_CFB64: CK_MECHANISM_TYPE = 0x00000152;
pub const CKM_DES_CFB8: CK_MECHANISM_TYPE = 0x00000153;

pub const CKM_MD2: CK_MECHANISM_TYPE = 0x00000200;

pub const CKM_MD2_HMAC: CK_MECHANISM_TYPE = 0x00000201;
pub const CKM_MD2_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000202;

pub const CKM_MD5: CK_MECHANISM_TYPE = 0x00000210;

pub const CKM_MD5_HMAC: CK_MECHANISM_TYPE = 0x00000211;
pub const CKM_MD5_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000212;

pub const CKM_SHA_1: CK_MECHANISM_TYPE = 0x00000220;

pub const CKM_SHA_1_HMAC: CK_MECHANISM_TYPE = 0x00000221;
pub const CKM_SHA_1_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000222;

pub const CKM_RIPEMD128: CK_MECHANISM_TYPE = 0x00000230;
pub const CKM_RIPEMD128_HMAC: CK_MECHANISM_TYPE = 0x00000231;
pub const CKM_RIPEMD128_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000232;
pub const CKM_RIPEMD160: CK_MECHANISM_TYPE = 0x00000240;
pub const CKM_RIPEMD160_HMAC: CK_MECHANISM_TYPE = 0x00000241;
pub const CKM_RIPEMD160_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000242;

pub const CKM_SHA256: CK_MECHANISM_TYPE = 0x00000250;
pub const CKM_SHA256_HMAC: CK_MECHANISM_TYPE = 0x00000251;
pub const CKM_SHA256_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000252;
pub const CKM_SHA224: CK_MECHANISM_TYPE = 0x00000255;
pub const CKM_SHA224_HMAC: CK_MECHANISM_TYPE = 0x00000256;
pub const CKM_SHA224_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000257;
pub const CKM_SHA384: CK_MECHANISM_TYPE = 0x00000260;
pub const CKM_SHA384_HMAC: CK_MECHANISM_TYPE = 0x00000261;
pub const CKM_SHA384_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000262;
pub const CKM_SHA512: CK_MECHANISM_TYPE = 0x00000270;
pub const CKM_SHA512_HMAC: CK_MECHANISM_TYPE = 0x00000271;
pub const CKM_SHA512_HMAC_GENERAL: CK_MECHANISM_TYPE = 0x00000272;
pub const CKM_SECURID_KEY_GEN: CK_MECHANISM_TYPE = 0x00000280;
pub const CKM_SECURID: CK_MECHANISM_TYPE = 0x00000282;
pub const CKM_HOTP_KEY_GEN: CK_MECHANISM_TYPE = 0x00000290;
pub const CKM_HOTP: CK_MECHANISM_TYPE = 0x00000291;
pub const CKM_ACTI: CK_MECHANISM_TYPE = 0x000002A0;
pub const CKM_ACTI_KEY_GEN: CK_MECHANISM_TYPE = 0x000002A1;

pub const CKM_CAST_KEY_GEN: CK_MECHANISM_TYPE = 0x00000300;
pub const CKM_CAST_ECB: CK_MECHANISM_TYPE = 0x00000301;
pub const CKM_CAST_CBC: CK_MECHANISM_TYPE = 0x00000302;
pub const CKM_CAST_MAC: CK_MECHANISM_TYPE = 0x00000303;
pub const CKM_CAST_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000304;
pub const CKM_CAST_CBC_PAD: CK_MECHANISM_TYPE = 0x00000305;
pub const CKM_CAST3_KEY_GEN: CK_MECHANISM_TYPE = 0x00000310;
pub const CKM_CAST3_ECB: CK_MECHANISM_TYPE = 0x00000311;
pub const CKM_CAST3_CBC: CK_MECHANISM_TYPE = 0x00000312;
pub const CKM_CAST3_MAC: CK_MECHANISM_TYPE = 0x00000313;
pub const CKM_CAST3_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000314;
pub const CKM_CAST3_CBC_PAD: CK_MECHANISM_TYPE = 0x00000315;
/// Note that CAST128 and CAST5 are the same algorithm
pub const CKM_CAST5_KEY_GEN: CK_MECHANISM_TYPE = 0x00000320;
pub const CKM_CAST128_KEY_GEN: CK_MECHANISM_TYPE = 0x00000320;
pub const CKM_CAST5_ECB: CK_MECHANISM_TYPE = 0x00000321;
pub const CKM_CAST128_ECB: CK_MECHANISM_TYPE = 0x00000321;
pub const CKM_CAST5_CBC: CK_MECHANISM_TYPE = CKM_CAST128_CBC;
pub const CKM_CAST128_CBC: CK_MECHANISM_TYPE = 0x00000322;
pub const CKM_CAST5_MAC: CK_MECHANISM_TYPE = CKM_CAST128_MAC;
pub const CKM_CAST128_MAC: CK_MECHANISM_TYPE = 0x00000323;
pub const CKM_CAST5_MAC_GENERAL: CK_MECHANISM_TYPE = CKM_CAST128_MAC_GENERAL;
pub const CKM_CAST128_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000324;
pub const CKM_CAST5_CBC_PAD: CK_MECHANISM_TYPE = CKM_CAST128_CBC_PAD;
pub const CKM_CAST128_CBC_PAD: CK_MECHANISM_TYPE = 0x00000325;
pub const CKM_RC5_KEY_GEN: CK_MECHANISM_TYPE = 0x00000330;
pub const CKM_RC5_ECB: CK_MECHANISM_TYPE = 0x00000331;
pub const CKM_RC5_CBC: CK_MECHANISM_TYPE = 0x00000332;
pub const CKM_RC5_MAC: CK_MECHANISM_TYPE = 0x00000333;
pub const CKM_RC5_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000334;
pub const CKM_RC5_CBC_PAD: CK_MECHANISM_TYPE = 0x00000335;
pub const CKM_IDEA_KEY_GEN: CK_MECHANISM_TYPE = 0x00000340;
pub const CKM_IDEA_ECB: CK_MECHANISM_TYPE = 0x00000341;
pub const CKM_IDEA_CBC: CK_MECHANISM_TYPE = 0x00000342;
pub const CKM_IDEA_MAC: CK_MECHANISM_TYPE = 0x00000343;
pub const CKM_IDEA_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000344;
pub const CKM_IDEA_CBC_PAD: CK_MECHANISM_TYPE = 0x00000345;
pub const CKM_GENERIC_SECRET_KEY_GEN: CK_MECHANISM_TYPE = 0x00000350;
pub const CKM_CONCATENATE_BASE_AND_KEY: CK_MECHANISM_TYPE = 0x00000360;
pub const CKM_CONCATENATE_BASE_AND_DATA: CK_MECHANISM_TYPE = 0x00000362;
pub const CKM_CONCATENATE_DATA_AND_BASE: CK_MECHANISM_TYPE = 0x00000363;
pub const CKM_XOR_BASE_AND_DATA: CK_MECHANISM_TYPE = 0x00000364;
pub const CKM_EXTRACT_KEY_FROM_KEY: CK_MECHANISM_TYPE = 0x00000365;
pub const CKM_SSL3_PRE_MASTER_KEY_GEN: CK_MECHANISM_TYPE = 0x00000370;
pub const CKM_SSL3_MASTER_KEY_DERIVE: CK_MECHANISM_TYPE = 0x00000371;
pub const CKM_SSL3_KEY_AND_MAC_DERIVE: CK_MECHANISM_TYPE = 0x00000372;

pub const CKM_SSL3_MASTER_KEY_DERIVE_DH: CK_MECHANISM_TYPE = 0x00000373;
pub const CKM_TLS_PRE_MASTER_KEY_GEN: CK_MECHANISM_TYPE = 0x00000374;
pub const CKM_TLS_MASTER_KEY_DERIVE: CK_MECHANISM_TYPE = 0x00000375;
pub const CKM_TLS_KEY_AND_MAC_DERIVE: CK_MECHANISM_TYPE = 0x00000376;
pub const CKM_TLS_MASTER_KEY_DERIVE_DH: CK_MECHANISM_TYPE = 0x00000377;

pub const CKM_TLS_PRF: CK_MECHANISM_TYPE = 0x00000378;

pub const CKM_SSL3_MD5_MAC: CK_MECHANISM_TYPE = 0x00000380;
pub const CKM_SSL3_SHA1_MAC: CK_MECHANISM_TYPE = 0x00000381;
pub const CKM_MD5_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000390;
pub const CKM_MD2_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000391;
pub const CKM_SHA1_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000392;

pub const CKM_SHA256_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000393;
pub const CKM_SHA384_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000394;
pub const CKM_SHA512_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000395;
pub const CKM_SHA224_KEY_DERIVATION: CK_MECHANISM_TYPE = 0x00000396;

pub const CKM_PBE_MD2_DES_CBC: CK_MECHANISM_TYPE = 0x000003A0;
pub const CKM_PBE_MD5_DES_CBC: CK_MECHANISM_TYPE = 0x000003A1;
pub const CKM_PBE_MD5_CAST_CBC: CK_MECHANISM_TYPE = 0x000003A2;
pub const CKM_PBE_MD5_CAST3_CBC: CK_MECHANISM_TYPE = 0x000003A3;
pub const CKM_PBE_MD5_CAST5_CBC: CK_MECHANISM_TYPE = CKM_PBE_MD5_CAST128_CBC;
pub const CKM_PBE_MD5_CAST128_CBC: CK_MECHANISM_TYPE = 0x000003A4;
pub const CKM_PBE_SHA1_CAST5_CBC: CK_MECHANISM_TYPE = CKM_PBE_SHA1_CAST128_CBC;
pub const CKM_PBE_SHA1_CAST128_CBC: CK_MECHANISM_TYPE = 0x000003A5;
pub const CKM_PBE_SHA1_RC4_128: CK_MECHANISM_TYPE = 0x000003A6;
pub const CKM_PBE_SHA1_RC4_40: CK_MECHANISM_TYPE = 0x000003A7;
pub const CKM_PBE_SHA1_DES3_EDE_CBC: CK_MECHANISM_TYPE = 0x000003A8;
pub const CKM_PBE_SHA1_DES2_EDE_CBC: CK_MECHANISM_TYPE = 0x000003A9;
pub const CKM_PBE_SHA1_RC2_128_CBC: CK_MECHANISM_TYPE = 0x000003AA;
pub const CKM_PBE_SHA1_RC2_40_CBC: CK_MECHANISM_TYPE = 0x000003AB;

pub const CKM_PKCS5_PBKD2: CK_MECHANISM_TYPE = 0x000003B0;

pub const CKM_PBA_SHA1_WITH_SHA1_HMAC: CK_MECHANISM_TYPE = 0x000003C0;

pub const CKM_WTLS_PRE_MASTER_KEY_GEN: CK_MECHANISM_TYPE = 0x000003D0;
pub const CKM_WTLS_MASTER_KEY_DERIVE: CK_MECHANISM_TYPE = 0x000003D1;
pub const CKM_WTLS_MASTER_KEY_DERIVE_DH_ECC: CK_MECHANISM_TYPE = 0x000003D2;
pub const CKM_WTLS_PRF: CK_MECHANISM_TYPE = 0x000003D3;
pub const CKM_WTLS_SERVER_KEY_AND_MAC_DERIVE: CK_MECHANISM_TYPE = 0x000003D4;
pub const CKM_WTLS_CLIENT_KEY_AND_MAC_DERIVE: CK_MECHANISM_TYPE = 0x000003D5;

pub const CKM_TLS10_MAC_SERVER: CK_MECHANISM_TYPE = 0x000003D6;
pub const CKM_TLS10_MAC_CLIENT: CK_MECHANISM_TYPE = 0x000003D7;
pub const CKM_TLS12_MAC: CK_MECHANISM_TYPE = 0x000003D8;
pub const CKM_TLS12_KDF: CK_MECHANISM_TYPE = 0x000003D9;
pub const CKM_TLS12_MASTER_KEY_DERIVE: CK_MECHANISM_TYPE = 0x000003E0;
pub const CKM_TLS12_KEY_AND_MAC_DERIVE: CK_MECHANISM_TYPE = 0x000003E1;
pub const CKM_TLS12_MASTER_KEY_DERIVE_DH: CK_MECHANISM_TYPE = 0x000003E2;
pub const CKM_TLS12_KEY_SAFE_DERIVE: CK_MECHANISM_TYPE = 0x000003E3;
pub const CKM_TLS_MAC: CK_MECHANISM_TYPE = 0x000003E4;
pub const CKM_TLS_KDF: CK_MECHANISM_TYPE = 0x000003E5;

pub const CKM_KEY_WRAP_LYNKS: CK_MECHANISM_TYPE = 0x00000400;
pub const CKM_KEY_WRAP_SET_OAEP: CK_MECHANISM_TYPE = 0x00000401;

pub const CKM_CMS_SIG: CK_MECHANISM_TYPE = 0x00000500;
pub const CKM_KIP_DERIVE: CK_MECHANISM_TYPE = 0x00000510;
pub const CKM_KIP_WRAP: CK_MECHANISM_TYPE = 0x00000511;
pub const CKM_KIP_MAC: CK_MECHANISM_TYPE = 0x00000512;

pub const CKM_CAMELLIA_KEY_GEN: CK_MECHANISM_TYPE = 0x00000550;
pub const CKM_CAMELLIA_ECB: CK_MECHANISM_TYPE = 0x00000551;
pub const CKM_CAMELLIA_CBC: CK_MECHANISM_TYPE = 0x00000552;
pub const CKM_CAMELLIA_MAC: CK_MECHANISM_TYPE = 0x00000553;
pub const CKM_CAMELLIA_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000554;
pub const CKM_CAMELLIA_CBC_PAD: CK_MECHANISM_TYPE = 0x00000555;
pub const CKM_CAMELLIA_ECB_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00000556;
pub const CKM_CAMELLIA_CBC_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00000557;
pub const CKM_CAMELLIA_CTR: CK_MECHANISM_TYPE = 0x00000558;

pub const CKM_ARIA_KEY_GEN: CK_MECHANISM_TYPE = 0x00000560;
pub const CKM_ARIA_ECB: CK_MECHANISM_TYPE = 0x00000561;
pub const CKM_ARIA_CBC: CK_MECHANISM_TYPE = 0x00000562;
pub const CKM_ARIA_MAC: CK_MECHANISM_TYPE = 0x00000563;
pub const CKM_ARIA_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000564;
pub const CKM_ARIA_CBC_PAD: CK_MECHANISM_TYPE = 0x00000565;
pub const CKM_ARIA_ECB_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00000566;
pub const CKM_ARIA_CBC_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00000567;

pub const CKM_SEED_KEY_GEN: CK_MECHANISM_TYPE = 0x00000650;
pub const CKM_SEED_ECB: CK_MECHANISM_TYPE = 0x00000651;
pub const CKM_SEED_CBC: CK_MECHANISM_TYPE = 0x00000652;
pub const CKM_SEED_MAC: CK_MECHANISM_TYPE = 0x00000653;
pub const CKM_SEED_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00000654;
pub const CKM_SEED_CBC_PAD: CK_MECHANISM_TYPE = 0x00000655;
pub const CKM_SEED_ECB_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00000656;
pub const CKM_SEED_CBC_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00000657;

pub const CKM_SKIPJACK_KEY_GEN: CK_MECHANISM_TYPE = 0x00001000;
pub const CKM_SKIPJACK_ECB64: CK_MECHANISM_TYPE = 0x00001001;
pub const CKM_SKIPJACK_CBC64: CK_MECHANISM_TYPE = 0x00001002;
pub const CKM_SKIPJACK_OFB64: CK_MECHANISM_TYPE = 0x00001003;
pub const CKM_SKIPJACK_CFB64: CK_MECHANISM_TYPE = 0x00001004;
pub const CKM_SKIPJACK_CFB32: CK_MECHANISM_TYPE = 0x00001005;
pub const CKM_SKIPJACK_CFB16: CK_MECHANISM_TYPE = 0x00001006;
pub const CKM_SKIPJACK_CFB8: CK_MECHANISM_TYPE = 0x00001007;
pub const CKM_SKIPJACK_WRAP: CK_MECHANISM_TYPE = 0x00001008;
pub const CKM_SKIPJACK_PRIVATE_WRAP: CK_MECHANISM_TYPE = 0x00001009;
pub const CKM_SKIPJACK_RELAYX: CK_MECHANISM_TYPE = 0x0000100a;
pub const CKM_KEA_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x00001010;
pub const CKM_KEA_KEY_DERIVE: CK_MECHANISM_TYPE = 0x00001011;
pub const CKM_KEA_DERIVE: CK_MECHANISM_TYPE = 0x00001012;
pub const CKM_FORTEZZA_TIMESTAMP: CK_MECHANISM_TYPE = 0x00001020;
pub const CKM_BATON_KEY_GEN: CK_MECHANISM_TYPE = 0x00001030;
pub const CKM_BATON_ECB128: CK_MECHANISM_TYPE = 0x00001031;
pub const CKM_BATON_ECB96: CK_MECHANISM_TYPE = 0x00001032;
pub const CKM_BATON_CBC128: CK_MECHANISM_TYPE = 0x00001033;
pub const CKM_BATON_COUNTER: CK_MECHANISM_TYPE = 0x00001034;
pub const CKM_BATON_SHUFFLE: CK_MECHANISM_TYPE = 0x00001035;
pub const CKM_BATON_WRAP: CK_MECHANISM_TYPE = 0x00001036;

pub const CKM_ECDSA_KEY_PAIR_GEN: CK_MECHANISM_TYPE = CKM_EC_KEY_PAIR_GEN;
pub const CKM_EC_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x00001040;

pub const CKM_ECDSA: CK_MECHANISM_TYPE = 0x00001041;
pub const CKM_ECDSA_SHA1: CK_MECHANISM_TYPE = 0x00001042;
pub const CKM_ECDSA_SHA224: CK_MECHANISM_TYPE = 0x00001043;
pub const CKM_ECDSA_SHA256: CK_MECHANISM_TYPE = 0x00001044;
pub const CKM_ECDSA_SHA384: CK_MECHANISM_TYPE = 0x00001045;
pub const CKM_ECDSA_SHA512: CK_MECHANISM_TYPE = 0x00001046;

pub const CKM_ECDH1_DERIVE: CK_MECHANISM_TYPE = 0x00001050;
pub const CKM_ECDH1_COFACTOR_DERIVE: CK_MECHANISM_TYPE = 0x00001051;
pub const CKM_ECMQV_DERIVE: CK_MECHANISM_TYPE = 0x00001052;

pub const CKM_ECDH_AES_KEY_WRAP: CK_MECHANISM_TYPE = 0x00001053;
pub const CKM_RSA_AES_KEY_WRAP: CK_MECHANISM_TYPE = 0x00001054;

pub const CKM_JUNIPER_KEY_GEN: CK_MECHANISM_TYPE = 0x00001060;
pub const CKM_JUNIPER_ECB128: CK_MECHANISM_TYPE = 0x00001061;
pub const CKM_JUNIPER_CBC128: CK_MECHANISM_TYPE = 0x00001062;
pub const CKM_JUNIPER_COUNTER: CK_MECHANISM_TYPE = 0x00001063;
pub const CKM_JUNIPER_SHUFFLE: CK_MECHANISM_TYPE = 0x00001064;
pub const CKM_JUNIPER_WRAP: CK_MECHANISM_TYPE = 0x00001065;
pub const CKM_FASTHASH: CK_MECHANISM_TYPE = 0x00001070;

pub const CKM_AES_KEY_GEN: CK_MECHANISM_TYPE = 0x00001080;
pub const CKM_AES_ECB: CK_MECHANISM_TYPE = 0x00001081;
pub const CKM_AES_CBC: CK_MECHANISM_TYPE = 0x00001082;
pub const CKM_AES_MAC: CK_MECHANISM_TYPE = 0x00001083;
pub const CKM_AES_MAC_GENERAL: CK_MECHANISM_TYPE = 0x00001084;
pub const CKM_AES_CBC_PAD: CK_MECHANISM_TYPE = 0x00001085;
pub const CKM_AES_CTR: CK_MECHANISM_TYPE = 0x00001086;
pub const CKM_AES_GCM: CK_MECHANISM_TYPE = 0x00001087;
pub const CKM_AES_CCM: CK_MECHANISM_TYPE = 0x00001088;
pub const CKM_AES_CTS: CK_MECHANISM_TYPE = 0x00001089;
pub const CKM_AES_CMAC: CK_MECHANISM_TYPE = 0x0000108A;
pub const CKM_AES_CMAC_GENERAL: CK_MECHANISM_TYPE = 0x0000108B;

pub const CKM_AES_XCBC_MAC: CK_MECHANISM_TYPE = 0x0000108C;
pub const CKM_AES_XCBC_MAC_96: CK_MECHANISM_TYPE = 0x0000108D;
pub const CKM_AES_GMAC: CK_MECHANISM_TYPE = 0x0000108E;

pub const CKM_BLOWFISH_KEY_GEN: CK_MECHANISM_TYPE = 0x00001090;
pub const CKM_BLOWFISH_CBC: CK_MECHANISM_TYPE = 0x00001091;
pub const CKM_TWOFISH_KEY_GEN: CK_MECHANISM_TYPE = 0x00001092;
pub const CKM_TWOFISH_CBC: CK_MECHANISM_TYPE = 0x00001093;
pub const CKM_BLOWFISH_CBC_PAD: CK_MECHANISM_TYPE = 0x00001094;
pub const CKM_TWOFISH_CBC_PAD: CK_MECHANISM_TYPE = 0x00001095;

pub const CKM_DES_ECB_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00001100;
pub const CKM_DES_CBC_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00001101;
pub const CKM_DES3_ECB_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00001102;
pub const CKM_DES3_CBC_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00001103;
pub const CKM_AES_ECB_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00001104;
pub const CKM_AES_CBC_ENCRYPT_DATA: CK_MECHANISM_TYPE = 0x00001105;

pub const CKM_GOSTR3410_KEY_PAIR_GEN: CK_MECHANISM_TYPE = 0x00001200;
pub const CKM_GOSTR3410: CK_MECHANISM_TYPE = 0x00001201;
pub const CKM_GOSTR3410_WITH_GOSTR3411: CK_MECHANISM_TYPE = 0x00001202;
pub const CKM_GOSTR3410_KEY_WRAP: CK_MECHANISM_TYPE = 0x00001203;
pub const CKM_GOSTR3410_DERIVE: CK_MECHANISM_TYPE = 0x00001204;
pub const CKM_GOSTR3411: CK_MECHANISM_TYPE = 0x00001210;
pub const CKM_GOSTR3411_HMAC: CK_MECHANISM_TYPE = 0x00001211;
pub const CKM_GOST28147_KEY_GEN: CK_MECHANISM_TYPE = 0x00001220;
pub const CKM_GOST28147_ECB: CK_MECHANISM_TYPE = 0x00001221;
pub const CKM_GOST28147: CK_MECHANISM_TYPE = 0x00001222;
pub const CKM_GOST28147_MAC: CK_MECHANISM_TYPE = 0x00001223;
pub const CKM_GOST28147_KEY_WRAP: CK_MECHANISM_TYPE = 0x00001224;

pub const CKM_DSA_PARAMETER_GEN: CK_MECHANISM_TYPE = 0x00002000;
pub const CKM_DH_PKCS_PARAMETER_GEN: CK_MECHANISM_TYPE = 0x00002001;
pub const CKM_X9_42_DH_PARAMETER_GEN: CK_MECHANISM_TYPE = 0x00002002;
pub const CKM_DSA_PROBABLISTIC_PARAMETER_GEN: CK_MECHANISM_TYPE = 0x00002003;
pub const CKM_DSA_SHAWE_TAYLOR_PARAMETER_GEN: CK_MECHANISM_TYPE = 0x00002004;

pub const CKM_AES_OFB: CK_MECHANISM_TYPE = 0x00002104;
pub const CKM_AES_CFB64: CK_MECHANISM_TYPE = 0x00002105;
pub const CKM_AES_CFB8: CK_MECHANISM_TYPE = 0x00002106;
pub const CKM_AES_CFB128: CK_MECHANISM_TYPE = 0x00002107;

pub const CKM_AES_CFB1: CK_MECHANISM_TYPE = 0x00002108;
/// WAS: 0x00001090
pub const CKM_AES_KEY_WRAP: CK_MECHANISM_TYPE = 0x00002109;
/// WAS: 0x00001091
pub const CKM_AES_KEY_WRAP_PAD: CK_MECHANISM_TYPE = 0x0000210A;

pub const CKM_RSA_PKCS_TPM_1_1: CK_MECHANISM_TYPE = 0x00004001;
pub const CKM_RSA_PKCS_OAEP_TPM_1_1: CK_MECHANISM_TYPE = 0x00004002;

pub const CKM_VENDOR_DEFINED: CK_MECHANISM_TYPE = 0x80000000;

pub type CK_MECHANISM_TYPE_PTR = *mut CK_MECHANISM_TYPE;

cryptoki_aligned! {
  /// CK_MECHANISM is a structure that specifies a particular
  /// mechanism
  #[derive(Debug, Copy)]
  pub struct CK_MECHANISM {
    pub mechanism: CK_MECHANISM_TYPE,
    pub pParameter: CK_VOID_PTR,
    /// in bytes
    pub ulParameterLen: CK_ULONG,
  }
}
packed_clone!(CK_MECHANISM);

pub type CK_MECHANISM_PTR = *mut CK_MECHANISM;

cryptoki_aligned! {
  /// CK_MECHANISM_INFO provides information about a particular
  /// mechanism
  #[derive(Debug, Default, Copy)]
  pub struct CK_MECHANISM_INFO {
    pub ulMinKeySize: CK_ULONG,
    pub ulMaxKeySize: CK_ULONG,
    pub flags: CK_FLAGS,
  }
}
packed_clone!(CK_MECHANISM_INFO);

/// The flags are defined as follows:
pub const CKF_HW: CK_FLAGS = 0x00000001; /* performed by HW */

/// Specify whether or not a mechanism can be used for a particular task
pub const CKF_ENCRYPT: CK_FLAGS = 0x00000100;
pub const CKF_DECRYPT: CK_FLAGS = 0x00000200;
pub const CKF_DIGEST: CK_FLAGS = 0x00000400;
pub const CKF_SIGN: CK_FLAGS = 0x00000800;
pub const CKF_SIGN_RECOVER: CK_FLAGS = 0x00001000;
pub const CKF_VERIFY: CK_FLAGS = 0x00002000;
pub const CKF_VERIFY_RECOVER: CK_FLAGS = 0x00004000;
pub const CKF_GENERATE: CK_FLAGS = 0x00008000;
pub const CKF_GENERATE_KEY_PAIR: CK_FLAGS = 0x00010000;
pub const CKF_WRAP: CK_FLAGS = 0x00020000;
pub const CKF_UNWRAP: CK_FLAGS = 0x00040000;
pub const CKF_DERIVE: CK_FLAGS = 0x00080000;

/// Describe a token's EC capabilities not available in mechanism
/// information.
pub const CKF_EC_F_P: CK_FLAGS = 0x00100000;
pub const CKF_EC_F_2M: CK_FLAGS = 0x00200000;
pub const CKF_EC_ECPARAMETERS: CK_FLAGS = 0x00400000;
pub const CKF_EC_NAMEDCURVE: CK_FLAGS = 0x00800000;
pub const CKF_EC_UNCOMPRESS: CK_FLAGS = 0x01000000;
pub const CKF_EC_COMPRESS: CK_FLAGS = 0x02000000;

pub const CKF_EXTENSION: CK_FLAGS = 0x80000000;

pub type CK_MECHANISM_INFO_PTR = *mut CK_MECHANISM_INFO;

/// CK_RV is a value that identifies the return value of a
/// Cryptoki function
pub type CK_RV = CK_ULONG;
pub const CKR_OK: CK_RV = 0x00000000;
pub const CKR_CANCEL: CK_RV = 0x00000001;
pub const CKR_HOST_MEMORY: CK_RV = 0x00000002;
pub const CKR_SLOT_ID_INVALID: CK_RV = 0x00000003;
pub const CKR_GENERAL_ERROR: CK_RV = 0x00000005;
pub const CKR_FUNCTION_FAILED: CK_RV = 0x00000006;
pub const CKR_ARGUMENTS_BAD: CK_RV = 0x00000007;
pub const CKR_NO_EVENT: CK_RV = 0x00000008;
pub const CKR_NEED_TO_CREATE_THREADS: CK_RV = 0x00000009;
pub const CKR_CANT_LOCK: CK_RV = 0x0000000A;
pub const CKR_ATTRIBUTE_READ_ONLY: CK_RV = 0x00000010;
pub const CKR_ATTRIBUTE_SENSITIVE: CK_RV = 0x00000011;
pub const CKR_ATTRIBUTE_TYPE_INVALID: CK_RV = 0x00000012;
pub const CKR_ATTRIBUTE_VALUE_INVALID: CK_RV = 0x00000013;
pub const CKR_ACTION_PROHIBITED: CK_RV = 0x0000001B;
pub const CKR_DATA_INVALID: CK_RV = 0x00000020;
pub const CKR_DATA_LEN_RANGE: CK_RV = 0x00000021;
pub const CKR_DEVICE_ERROR: CK_RV = 0x00000030;
pub const CKR_DEVICE_MEMORY: CK_RV = 0x00000031;
pub const CKR_DEVICE_REMOVED: CK_RV = 0x00000032;
pub const CKR_ENCRYPTED_DATA_INVALID: CK_RV = 0x00000040;
pub const CKR_ENCRYPTED_DATA_LEN_RANGE: CK_RV = 0x00000041;
pub const CKR_FUNCTION_CANCELED: CK_RV = 0x00000050;
pub const CKR_FUNCTION_NOT_PARALLEL: CK_RV = 0x00000051;
pub const CKR_FUNCTION_NOT_SUPPORTED: CK_RV = 0x00000054;
pub const CKR_KEY_HANDLE_INVALID: CK_RV = 0x00000060;
pub const CKR_KEY_SIZE_RANGE: CK_RV = 0x00000062;
pub const CKR_KEY_TYPE_INCONSISTENT: CK_RV = 0x00000063;
pub const CKR_KEY_NOT_NEEDED: CK_RV = 0x00000064;
pub const CKR_KEY_CHANGED: CK_RV = 0x00000065;
pub const CKR_KEY_NEEDED: CK_RV = 0x00000066;
pub const CKR_KEY_INDIGESTIBLE: CK_RV = 0x00000067;
pub const CKR_KEY_FUNCTION_NOT_PERMITTED: CK_RV = 0x00000068;
pub const CKR_KEY_NOT_WRAPPABLE: CK_RV = 0x00000069;
pub const CKR_KEY_UNEXTRACTABLE: CK_RV = 0x0000006A;
pub const CKR_MECHANISM_INVALID: CK_RV = 0x00000070;
pub const CKR_MECHANISM_PARAM_INVALID: CK_RV = 0x00000071;
pub const CKR_OBJECT_HANDLE_INVALID: CK_RV = 0x00000082;
pub const CKR_OPERATION_ACTIVE: CK_RV = 0x00000090;
pub const CKR_OPERATION_NOT_INITIALIZED: CK_RV = 0x00000091;
pub const CKR_PIN_INCORRECT: CK_RV = 0x000000A0;
pub const CKR_PIN_INVALID: CK_RV = 0x000000A1;
pub const CKR_PIN_LEN_RANGE: CK_RV = 0x000000A2;
pub const CKR_PIN_EXPIRED: CK_RV = 0x000000A3;
pub const CKR_PIN_LOCKED: CK_RV = 0x000000A4;
pub const CKR_SESSION_CLOSED: CK_RV = 0x000000B0;
pub const CKR_SESSION_COUNT: CK_RV = 0x000000B1;
pub const CKR_SESSION_HANDLE_INVALID: CK_RV = 0x000000B3;
pub const CKR_SESSION_PARALLEL_NOT_SUPPORTED: CK_RV = 0x000000B4;
pub const CKR_SESSION_READ_ONLY: CK_RV = 0x000000B5;
pub const CKR_SESSION_EXISTS: CK_RV = 0x000000B6;
pub const CKR_SESSION_READ_ONLY_EXISTS: CK_RV = 0x000000B7;
pub const CKR_SESSION_READ_WRITE_SO_EXISTS: CK_RV = 0x000000B8;
pub const CKR_SIGNATURE_INVALID: CK_RV = 0x000000C0;
pub const CKR_SIGNATURE_LEN_RANGE: CK_RV = 0x000000C1;
pub const CKR_TEMPLATE_INCOMPLETE: CK_RV = 0x000000D0;
pub const CKR_TEMPLATE_INCONSISTENT: CK_RV = 0x000000D1;
pub const CKR_TOKEN_NOT_PRESENT: CK_RV = 0x000000E0;
pub const CKR_TOKEN_NOT_RECOGNIZED: CK_RV = 0x000000E1;
pub const CKR_TOKEN_WRITE_PROTECTED: CK_RV = 0x000000E2;
pub const CKR_UNWRAPPING_KEY_HANDLE_INVALID: CK_RV = 0x000000F0;
pub const CKR_UNWRAPPING_KEY_SIZE_RANGE: CK_RV = 0x000000F1;
pub const CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT: CK_RV = 0x000000F2;
pub const CKR_USER_ALREADY_LOGGED_IN: CK_RV = 0x00000100;
pub const CKR_USER_NOT_LOGGED_IN: CK_RV = 0x00000101;
pub const CKR_USER_PIN_NOT_INITIALIZED: CK_RV = 0x00000102;
pub const CKR_USER_TYPE_INVALID: CK_RV = 0x00000103;
pub const CKR_USER_ANOTHER_ALREADY_LOGGED_IN: CK_RV = 0x00000104;
pub const CKR_USER_TOO_MANY_TYPES: CK_RV = 0x00000105;
pub const CKR_WRAPPED_KEY_INVALID: CK_RV = 0x00000110;
pub const CKR_WRAPPED_KEY_LEN_RANGE: CK_RV = 0x00000112;
pub const CKR_WRAPPING_KEY_HANDLE_INVALID: CK_RV = 0x00000113;
pub const CKR_WRAPPING_KEY_SIZE_RANGE: CK_RV = 0x00000114;
pub const CKR_WRAPPING_KEY_TYPE_INCONSISTENT: CK_RV = 0x00000115;
pub const CKR_RANDOM_SEED_NOT_SUPPORTED: CK_RV = 0x00000120;
pub const CKR_RANDOM_NO_RNG: CK_RV = 0x00000121;
pub const CKR_DOMAIN_PARAMS_INVALID: CK_RV = 0x00000130;
pub const CKR_CURVE_NOT_SUPPORTED: CK_RV = 0x00000140;
pub const CKR_BUFFER_TOO_SMALL: CK_RV = 0x00000150;
pub const CKR_SAVED_STATE_INVALID: CK_RV = 0x00000160;
pub const CKR_INFORMATION_SENSITIVE: CK_RV = 0x00000170;
pub const CKR_STATE_UNSAVEABLE: CK_RV = 0x00000180;
pub const CKR_CRYPTOKI_NOT_INITIALIZED: CK_RV = 0x00000190;
pub const CKR_CRYPTOKI_ALREADY_INITIALIZED: CK_RV = 0x00000191;
pub const CKR_MUTEX_BAD: CK_RV = 0x000001A0;
pub const CKR_MUTEX_NOT_LOCKED: CK_RV = 0x000001A1;
pub const CKR_NEW_PIN_MODE: CK_RV = 0x000001B0;
pub const CKR_NEXT_OTP: CK_RV = 0x000001B1;
pub const CKR_EXCEEDED_MAX_ITERATIONS: CK_RV = 0x000001B5;
pub const CKR_FIPS_SELF_TEST_FAILED: CK_RV = 0x000001B6;
pub const CKR_LIBRARY_LOAD_FAILED: CK_RV = 0x000001B7;
pub const CKR_PIN_TOO_WEAK: CK_RV = 0x000001B8;
pub const CKR_PUBLIC_KEY_INVALID: CK_RV = 0x000001B9;
pub const CKR_FUNCTION_REJECTED: CK_RV = 0x00000200;
pub const CKR_VENDOR_DEFINED: CK_RV = 0x80000000;

/// CK_NOTIFY is an application callback that processes events
pub type CK_NOTIFY = Option<extern "C" fn(CK_SESSION_HANDLE, CK_NOTIFICATION, CK_VOID_PTR) -> CK_RV>;

cryptoki_aligned! {
  /// CK_FUNCTION_LIST is a structure holding a Cryptoki spec
  /// version and pointers of appropriate types to all the
  /// Cryptoki functions
  #[derive(Debug, Copy)]
  pub struct CK_FUNCTION_LIST {
    pub version: CK_VERSION,
    pub C_Initialize: Option<C_Initialize>,
    pub C_Finalize: Option<C_Finalize>,
    pub C_GetInfo: Option<C_GetInfo>,
    pub C_GetFunctionList: Option<C_GetFunctionList>,
    pub C_GetSlotList: Option<C_GetSlotList>,
    pub C_GetSlotInfo: Option<C_GetSlotInfo>,
    pub C_GetTokenInfo: Option<C_GetTokenInfo>,
    pub C_GetMechanismList: Option<C_GetMechanismList>,
    pub C_GetMechanismInfo: Option<C_GetMechanismInfo>,
    pub C_InitToken: Option<C_InitToken>,
    pub C_InitPIN: Option<C_InitPIN>,
    pub C_SetPIN: Option<C_SetPIN>,
    pub C_OpenSession: Option<C_OpenSession>,
    pub C_CloseSession: Option<C_CloseSession>,
    pub C_CloseAllSessions: Option<C_CloseAllSessions>,
    pub C_GetSessionInfo: Option<C_GetSessionInfo>,
    pub C_GetOperationState: Option<C_GetOperationState>,
    pub C_SetOperationState: Option<C_SetOperationState>,
    pub C_Login: Option<C_Login>,
    pub C_Logout: Option<C_Logout>,
    pub C_CreateObject: Option<C_CreateObject>,
    pub C_CopyObject: Option<C_CopyObject>,
    pub C_DestroyObject: Option<C_DestroyObject>,
    pub C_GetObjectSize: Option<C_GetObjectSize>,
    pub C_GetAttributeValue: Option<C_GetAttributeValue>,
    pub C_SetAttributeValue: Option<C_SetAttributeValue>,
    pub C_FindObjectsInit: Option<C_FindObjectsInit>,
    pub C_FindObjects: Option<C_FindObjects>,
    pub C_FindObjectsFinal: Option<C_FindObjectsFinal>,
    pub C_EncryptInit: Option<C_EncryptInit>,
    pub C_Encrypt: Option<C_Encrypt>,
    pub C_EncryptUpdate: Option<C_EncryptUpdate>,
    pub C_EncryptFinal: Option<C_EncryptFinal>,
    pub C_DecryptInit: Option<C_DecryptInit>,
    pub C_Decrypt: Option<C_Decrypt>,
    pub C_DecryptUpdate: Option<C_DecryptUpdate>,
    pub C_DecryptFinal: Option<C_DecryptFinal>,
    pub C_DigestInit: Option<C_DigestInit>,
    pub C_Digest: Option<C_Digest>,
    pub C_DigestUpdate: Option<C_DigestUpdate>,
    pub C_DigestKey: Option<C_DigestKey>,
    pub C_DigestFinal: Option<C_DigestFinal>,
    pub C_SignInit: Option<C_SignInit>,
    pub C_Sign: Option<C_Sign>,
    pub C_SignUpdate: Option<C_SignUpdate>,
    pub C_SignFinal: Option<C_SignFinal>,
    pub C_SignRecoverInit: Option<C_SignRecoverInit>,
    pub C_SignRecover: Option<C_SignRecover>,
    pub C_VerifyInit: Option<C_VerifyInit>,
    pub C_Verify: Option<C_Verify>,
    pub C_VerifyUpdate: Option<C_VerifyUpdate>,
    pub C_VerifyFinal: Option<C_VerifyFinal>,
    pub C_VerifyRecoverInit: Option<C_VerifyRecoverInit>,
    pub C_VerifyRecover: Option<C_VerifyRecover>,
    pub C_DigestEncryptUpdate: Option<C_DigestEncryptUpdate>,
    pub C_DecryptDigestUpdate: Option<C_DecryptDigestUpdate>,
    pub C_SignEncryptUpdate: Option<C_SignEncryptUpdate>,
    pub C_DecryptVerifyUpdate: Option<C_DecryptVerifyUpdate>,
    pub C_GenerateKey: Option<C_GenerateKey>,
    pub C_GenerateKeyPair: Option<C_GenerateKeyPair>,
    pub C_WrapKey: Option<C_WrapKey>,
    pub C_UnwrapKey: Option<C_UnwrapKey>,
    pub C_DeriveKey: Option<C_DeriveKey>,
    pub C_SeedRandom: Option<C_SeedRandom>,
    pub C_GenerateRandom: Option<C_GenerateRandom>,
    pub C_GetFunctionStatus: Option<C_GetFunctionStatus>,
    pub C_CancelFunction: Option<C_CancelFunction>,
    pub C_WaitForSlotEvent: Option<C_WaitForSlotEvent>,
  }
}
packed_clone!(CK_FUNCTION_LIST);
pub type CK_FUNCTION_LIST_PTR = *mut CK_FUNCTION_LIST;
pub type CK_FUNCTION_LIST_PTR_PTR = *mut CK_FUNCTION_LIST_PTR;

/// CK_CREATEMUTEX is an application callback for creating a
/// mutex object
pub type CK_CREATEMUTEX = Option<extern "C" fn(CK_VOID_PTR_PTR) -> CK_RV>;
/// CK_DESTROYMUTEX is an application callback for destroying a
/// mutex object
pub type CK_DESTROYMUTEX = Option<extern "C" fn(CK_VOID_PTR) -> CK_RV>;
/// CK_LOCKMUTEX is an application callback for locking a mutex
pub type CK_LOCKMUTEX = Option<extern "C" fn(CK_VOID_PTR) -> CK_RV>;
/// CK_UNLOCKMUTEX is an application callback for unlocking a
/// mutex
pub type CK_UNLOCKMUTEX = Option<extern "C" fn(CK_VOID_PTR) -> CK_RV>;

cryptoki_aligned! {
  /// CK_C_INITIALIZE_ARGS provides the optional arguments to
  /// C_Initialize
  #[derive(Debug, Copy)]
  pub struct CK_C_INITIALIZE_ARGS {
    pub CreateMutex: CK_CREATEMUTEX,
    pub DestroyMutex: CK_DESTROYMUTEX,
    pub LockMutex: CK_LOCKMUTEX,
    pub UnlockMutex: CK_UNLOCKMUTEX,
    pub flags: CK_FLAGS,
    pub pReserved: CK_VOID_PTR,
  }
}
packed_clone!(CK_C_INITIALIZE_ARGS);

// TODO: we need to make this the default and implement a new
// function
impl Default for CK_C_INITIALIZE_ARGS {
    fn default() -> Self {
        Self::new()
    }
}

impl CK_C_INITIALIZE_ARGS {
  pub fn new() -> CK_C_INITIALIZE_ARGS {
    CK_C_INITIALIZE_ARGS {
      flags: CKF_OS_LOCKING_OK,
      CreateMutex: None,
      DestroyMutex: None,
      LockMutex: None,
      UnlockMutex: None,
      pReserved: ptr::null_mut(),
    }
  }
}

pub const CKF_LIBRARY_CANT_CREATE_OS_THREADS: CK_FLAGS = 0x00000001;
pub const CKF_OS_LOCKING_OK: CK_FLAGS = 0x00000002;

pub type CK_C_INITIALIZE_ARGS_PTR = *mut CK_C_INITIALIZE_ARGS;

/// CKF_DONT_BLOCK is for the function C_WaitForSlotEvent
pub const CKF_DONT_BLOCK: CK_FLAGS = 1;

/// CK_RSA_PKCS_MGF_TYPE  is used to indicate the Message
/// Generation Function (MGF) applied to a message block when
/// formatting a message block for the PKCS #1 OAEP encryption
/// scheme.
pub type CK_RSA_PKCS_MGF_TYPE = CK_ULONG;

pub type CK_RSA_PKCS_MGF_TYPE_PTR = *mut CK_RSA_PKCS_MGF_TYPE;

/// The following MGFs are defined
pub const CKG_MGF1_SHA1: CK_RSA_PKCS_MGF_TYPE = 0x00000001;
pub const CKG_MGF1_SHA256: CK_RSA_PKCS_MGF_TYPE = 0x00000002;
pub const CKG_MGF1_SHA384: CK_RSA_PKCS_MGF_TYPE = 0x00000003;
pub const CKG_MGF1_SHA512: CK_RSA_PKCS_MGF_TYPE = 0x00000004;
pub const CKG_MGF1_SHA224: CK_RSA_PKCS_MGF_TYPE = 0x00000005;

/// CK_RSA_PKCS_OAEP_SOURCE_TYPE  is used to indicate the source
/// of the encoding parameter when formatting a message block
/// for the PKCS #1 OAEP encryption scheme.
pub type CK_RSA_PKCS_OAEP_SOURCE_TYPE = CK_ULONG;

pub type CK_RSA_PKCS_OAEP_SOURCE_TYPE_PTR = *mut CK_RSA_PKCS_OAEP_SOURCE_TYPE;

/// The following encoding parameter sources are defined
pub const CKZ_DATA_SPECIFIED: CK_RSA_PKCS_OAEP_SOURCE_TYPE = 0x00000001;

cryptoki_aligned! {
  /// CK_RSA_PKCS_OAEP_PARAMS provides the parameters to the
  /// CKM_RSA_PKCS_OAEP mechanism.
  #[derive(Debug, Copy)]
  pub struct CK_RSA_PKCS_OAEP_PARAMS {
    pub hashAlg: CK_MECHANISM_TYPE,
    pub mgf: CK_RSA_PKCS_MGF_TYPE,
    pub source: CK_RSA_PKCS_OAEP_SOURCE_TYPE,
    pub pSourceData: CK_VOID_PTR,
    pub ulSourceDataLen: CK_ULONG,
  }
}
packed_clone!(CK_RSA_PKCS_OAEP_PARAMS);

pub type CK_RSA_PKCS_OAEP_PARAMS_PTR = *mut CK_RSA_PKCS_OAEP_PARAMS;

cryptoki_aligned! {
  /// CK_RSA_PKCS_PSS_PARAMS provides the parameters to the
  /// CKM_RSA_PKCS_PSS mechanism(s).
  #[derive(Debug, Copy)]
  pub struct CK_RSA_PKCS_PSS_PARAMS {
    pub hashAlg: CK_MECHANISM_TYPE,
    pub mgf: CK_RSA_PKCS_MGF_TYPE,
    pub sLen: CK_ULONG,
  }
}
packed_clone!(CK_RSA_PKCS_PSS_PARAMS);

pub type CK_RSA_PKCS_PSS_PARAMS_PTR = *mut CK_RSA_PKCS_PSS_PARAMS;

pub type CK_EC_KDF_TYPE = CK_ULONG;

/* The following EC Key Derivation Functions are defined */
pub const CKD_NULL: CK_EC_KDF_TYPE = 0x00000001;
pub const CKD_SHA1_KDF: CK_EC_KDF_TYPE = 0x00000002;

/* The following X9.42 DH key derivation functions are defined */
pub const CKD_SHA1_KDF_ASN1: CK_X9_42_DH_KDF_TYPE = 0x00000003;
pub const CKD_SHA1_KDF_CONCATENATE: CK_X9_42_DH_KDF_TYPE = 0x00000004;
pub const CKD_SHA224_KDF: CK_X9_42_DH_KDF_TYPE = 0x00000005;
pub const CKD_SHA256_KDF: CK_X9_42_DH_KDF_TYPE = 0x00000006;
pub const CKD_SHA384_KDF: CK_X9_42_DH_KDF_TYPE = 0x00000007;
pub const CKD_SHA512_KDF: CK_X9_42_DH_KDF_TYPE = 0x00000008;
pub const CKD_CPDIVERSIFY_KDF: CK_X9_42_DH_KDF_TYPE = 0x00000009;

cryptoki_aligned! {
  /// CK_ECDH1_DERIVE_PARAMS provides the parameters to the
  /// CKM_ECDH1_DERIVE and CKM_ECDH1_COFACTOR_DERIVE mechanisms,
  /// where each party contributes one key pair.
  pub struct CK_ECDH1_DERIVE_PARAMS {
    pub kdf: CK_EC_KDF_TYPE,
    pub ulSharedDataLen: CK_ULONG,
    pub pSharedData: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
  }
}

pub type CK_ECDH1_DERIVE_PARAMS_PTR = *mut CK_ECDH1_DERIVE_PARAMS;

cryptoki_aligned! {
  /// CK_ECDH2_DERIVE_PARAMS provides the parameters to the
  /// CKM_ECMQV_DERIVE mechanism, where each party contributes two key pairs.
  #[derive(Debug, Copy)]
  pub struct CK_ECDH2_DERIVE_PARAMS {
    pub kdf: CK_EC_KDF_TYPE,
    pub ulSharedDataLen: CK_ULONG,
    pub pSharedData: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
    pub ulPrivateDataLen: CK_ULONG,
    pub hPrivateData: CK_OBJECT_HANDLE,
    pub ulPublicDataLen2: CK_ULONG,
    pub pPublicData2: CK_BYTE_PTR,
  }
}
packed_clone!(CK_ECDH2_DERIVE_PARAMS);

pub type CK_ECDH2_DERIVE_PARAMS_PTR = *mut CK_ECDH2_DERIVE_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_ECMQV_DERIVE_PARAMS {
    pub kdf: CK_EC_KDF_TYPE,
    pub ulSharedDataLen: CK_ULONG,
    pub pSharedData: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
    pub ulPrivateDataLen: CK_ULONG,
    pub hPrivateData: CK_OBJECT_HANDLE,
    pub ulPublicDataLen2: CK_ULONG,
    pub pPublicData2: CK_BYTE_PTR,
    pub publicKey: CK_OBJECT_HANDLE,
  }
}
packed_clone!(CK_ECMQV_DERIVE_PARAMS);

pub type CK_ECMQV_DERIVE_PARAMS_PTR = *mut CK_ECMQV_DERIVE_PARAMS;

/// Typedefs and defines for the CKM_X9_42_DH_KEY_PAIR_GEN and the
/// CKM_X9_42_DH_PARAMETER_GEN mechanisms
pub type CK_X9_42_DH_KDF_TYPE = CK_ULONG;
pub type CK_X9_42_DH_KDF_TYPE_PTR = *mut CK_X9_42_DH_KDF_TYPE;

cryptoki_aligned! {
  /// CK_X9_42_DH1_DERIVE_PARAMS provides the parameters to the
  /// CKM_X9_42_DH_DERIVE key derivation mechanism, where each party
  /// contributes one key pair
  #[derive(Debug, Copy)]
  pub struct CK_X9_42_DH1_DERIVE_PARAMS {
    pub kdf: CK_X9_42_DH_KDF_TYPE,
    pub ulOtherInfoLen: CK_ULONG,
    pub pOtherInfo: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
  }
}
packed_clone!(CK_X9_42_DH1_DERIVE_PARAMS);

pub type CK_X9_42_DH1_DERIVE_PARAMS_PTR = *mut CK_X9_42_DH1_DERIVE_PARAMS;

cryptoki_aligned! {
  /// CK_X9_42_DH2_DERIVE_PARAMS provides the parameters to the
  /// CKM_X9_42_DH_HYBRID_DERIVE and CKM_X9_42_MQV_DERIVE key derivation
  /// mechanisms, where each party contributes two key pairs
  #[derive(Debug, Copy)]
  pub struct CK_X9_42_DH2_DERIVE_PARAMS {
    pub kdf: CK_X9_42_DH_KDF_TYPE,
    pub ulOtherInfoLen: CK_ULONG,
    pub pOtherInfo: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
    pub ulPrivateDataLen: CK_ULONG,
    pub hPrivateData: CK_OBJECT_HANDLE,
    pub ulPublicDataLen2: CK_ULONG,
    pub pPublicData2: CK_BYTE_PTR,
  }
}
packed_clone!(CK_X9_42_DH2_DERIVE_PARAMS);

pub type CK_X9_42_DH2_DERIVE_PARAMS_PTR = *mut CK_X9_42_DH2_DERIVE_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_X9_42_MQV_DERIVE_PARAMS {
    pub kdf: CK_X9_42_DH_KDF_TYPE,
    pub ulOtherInfoLen: CK_ULONG,
    pub pOtherInfo: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
    pub ulPrivateDataLen: CK_ULONG,
    pub hPrivateData: CK_OBJECT_HANDLE,
    pub ulPublicDataLen2: CK_ULONG,
    pub pPublicData2: CK_BYTE_PTR,
    pub publicKey: CK_OBJECT_HANDLE,
  }
}
packed_clone!(CK_X9_42_MQV_DERIVE_PARAMS);

pub type CK_X9_42_MQV_DERIVE_PARAMS_PTR = *mut CK_X9_42_MQV_DERIVE_PARAMS;

cryptoki_aligned! {
  /// CK_KEA_DERIVE_PARAMS provides the parameters to the
  /// CKM_KEA_DERIVE mechanism
  #[derive(Debug, Copy)]
  pub struct CK_KEA_DERIVE_PARAMS {
    pub isSender: CK_BBOOL,
    pub ulRandomLen: CK_ULONG,
    pub pRandomA: CK_BYTE_PTR,
    pub pRandomB: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
  }
}
packed_clone!(CK_KEA_DERIVE_PARAMS);

pub type CK_KEA_DERIVE_PARAMS_PTR = *mut CK_KEA_DERIVE_PARAMS;

/// CK_RC2_PARAMS provides the parameters to the CKM_RC2_ECB and
/// CKM_RC2_MAC mechanisms.  An instance of CK_RC2_PARAMS just
/// holds the effective keysize
pub type CK_RC2_PARAMS = CK_ULONG;

pub type CK_RC2_PARAMS_PTR = *mut CK_RC2_PARAMS;

cryptoki_aligned! {
  /// CK_RC2_CBC_PARAMS provides the parameters to the CKM_RC2_CBC
  /// mechanism
  #[derive(Debug, Copy)]
  pub struct CK_RC2_CBC_PARAMS {
    /// effective bits (1-1024)
    pub ulEffectiveBits: CK_ULONG,
    /// IV for CBC mode
    pub iv: [CK_BYTE; 8],
  }
}
packed_clone!(CK_RC2_CBC_PARAMS);

pub type CK_RC2_CBC_PARAMS_PTR = *mut CK_RC2_CBC_PARAMS;


cryptoki_aligned! {
  /// CK_RC2_MAC_GENERAL_PARAMS provides the parameters for the
  /// CKM_RC2_MAC_GENERAL mechanism
  #[derive(Debug, Copy)]
  pub struct CK_RC2_MAC_GENERAL_PARAMS {
    /// effective bits (1-1024)
    pub ulEffectiveBits: CK_ULONG,
    /// Length of MAC in bytes
    pub ulMacLength: CK_ULONG,
  }
}
packed_clone!(CK_RC2_MAC_GENERAL_PARAMS);

pub type CK_RC2_MAC_GENERAL_PARAMS_PTR = *mut CK_RC2_MAC_GENERAL_PARAMS;


cryptoki_aligned! {
  /// CK_RC5_PARAMS provides the parameters to the CKM_RC5_ECB and
  /// CKM_RC5_MAC mechanisms
  #[derive(Debug, Copy)]
  pub struct CK_RC5_PARAMS {
    /// wordsize in bits
    pub ulWordsize: CK_ULONG,
    /// number of rounds
    pub ulRounds: CK_ULONG,
  }
}
packed_clone!(CK_RC5_PARAMS);

pub type CK_RC5_PARAMS_PTR = *mut CK_RC5_PARAMS;


cryptoki_aligned! {
  /// CK_RC5_CBC_PARAMS provides the parameters to the CKM_RC5_CBC
  /// mechanism
  #[derive(Debug, Copy)]
  pub struct CK_RC5_CBC_PARAMS {
    /// wordsize in bits
    pub ulWordsize: CK_ULONG,
    /// number of rounds
    pub ulRounds: CK_ULONG,
    /// pointer to IV
    pub pIv: CK_BYTE_PTR,
    /// length of IV in bytes
    pub ulIvLen: CK_ULONG,
  }
}
packed_clone!(CK_RC5_CBC_PARAMS);

pub type CK_RC5_CBC_PARAMS_PTR = *mut CK_RC5_CBC_PARAMS;


cryptoki_aligned! {
  /// CK_RC5_MAC_GENERAL_PARAMS provides the parameters for the
  /// CKM_RC5_MAC_GENERAL mechanism
  #[derive(Debug, Copy)]
  pub struct CK_RC5_MAC_GENERAL_PARAMS {
    /// wordsize in bits
    pub ulWordsize: CK_ULONG,
    /// number of rounds
    pub ulRounds: CK_ULONG,
    /// Length of MAC in bytes
    pub ulMacLength: CK_ULONG,
  }
}
packed_clone!(CK_RC5_MAC_GENERAL_PARAMS);

pub type CK_RC5_MAC_GENERAL_PARAMS_PTR = *mut CK_RC5_MAC_GENERAL_PARAMS;

/// CK_MAC_GENERAL_PARAMS provides the parameters to most block
/// ciphers' MAC_GENERAL mechanisms.  Its value is the length of
/// the MAC
pub type CK_MAC_GENERAL_PARAMS = CK_ULONG;

pub type CK_MAC_GENERAL_PARAMS_PTR = *mut CK_MAC_GENERAL_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_DES_CBC_ENCRYPT_DATA_PARAMS {
    pub iv: [CK_BYTE; 8],
    pub pData: CK_BYTE_PTR,
    pub length: CK_ULONG,
  }
}
packed_clone!(CK_DES_CBC_ENCRYPT_DATA_PARAMS);

pub type CK_DES_CBC_ENCRYPT_DATA_PARAMS_PTR = *mut CK_DES_CBC_ENCRYPT_DATA_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_AES_CBC_ENCRYPT_DATA_PARAMS {
    pub iv: [CK_BYTE; 16],
    pub pData: CK_BYTE_PTR,
    pub length: CK_ULONG,
  }
}
packed_clone!(CK_AES_CBC_ENCRYPT_DATA_PARAMS);

pub type CK_AES_CBC_ENCRYPT_DATA_PARAMS_PTR = *mut CK_AES_CBC_ENCRYPT_DATA_PARAMS;

cryptoki_aligned! {
  /// CK_SKIPJACK_PRIVATE_WRAP_PARAMS provides the parameters to the
  /// CKM_SKIPJACK_PRIVATE_WRAP mechanism
  #[derive(Debug, Copy)]
  pub struct CK_SKIPJACK_PRIVATE_WRAP_PARAMS {
    pub ulPasswordLen: CK_ULONG,
    pub pPassword: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pPublicData: CK_BYTE_PTR,
    pub ulPAndGLen: CK_ULONG,
    pub ulQLen: CK_ULONG,
    pub ulRandomLen: CK_ULONG,
    pub pRandomA: CK_BYTE_PTR,
    pub pPrimeP: CK_BYTE_PTR,
    pub pBaseG: CK_BYTE_PTR,
    pub pSubprimeQ: CK_BYTE_PTR,
  }
}
packed_clone!(CK_SKIPJACK_PRIVATE_WRAP_PARAMS);

pub type CK_SKIPJACK_PRIVATE_WRAP_PARAMS_PTR = *mut CK_SKIPJACK_PRIVATE_WRAP_PARAMS;

cryptoki_aligned! {
  /// CK_SKIPJACK_RELAYX_PARAMS provides the parameters to the
  /// CKM_SKIPJACK_RELAYX mechanism
  #[derive(Debug, Copy)]
  pub struct CK_SKIPJACK_RELAYX_PARAMS {
    pub ulOldWrappedXLen: CK_ULONG,
    pub pOldWrappedX: CK_BYTE_PTR,
    pub ulOldPasswordLen: CK_ULONG,
    pub pOldPassword: CK_BYTE_PTR,
    pub ulOldPublicDataLen: CK_ULONG,
    pub pOldPublicData: CK_BYTE_PTR,
    pub ulOldRandomLen: CK_ULONG,
    pub pOldRandomA: CK_BYTE_PTR,
    pub ulNewPasswordLen: CK_ULONG,
    pub pNewPassword: CK_BYTE_PTR,
    pub ulNewPublicDataLen: CK_ULONG,
    pub pNewPublicData: CK_BYTE_PTR,
    pub ulNewRandomLen: CK_ULONG,
    pub pNewRandomA: CK_BYTE_PTR,
  }
}
packed_clone!(CK_SKIPJACK_RELAYX_PARAMS);

pub type CK_SKIPJACK_RELAYX_PARAMS_PTR = *mut CK_SKIPJACK_RELAYX_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_PBE_PARAMS {
    pub pInitVector: CK_BYTE_PTR,
    pub pPassword: CK_UTF8CHAR_PTR,
    pub ulPasswordLen: CK_ULONG,
    pub pSalt: CK_BYTE_PTR,
    pub ulSaltLen: CK_ULONG,
    pub ulIteration: CK_ULONG,
  }
}
packed_clone!(CK_PBE_PARAMS);

pub type CK_PBE_PARAMS_PTR = *mut CK_PBE_PARAMS;

cryptoki_aligned! {
  /// CK_KEY_WRAP_SET_OAEP_PARAMS provides the parameters to the
  /// CKM_KEY_WRAP_SET_OAEP mechanism
  #[derive(Debug, Copy)]
  pub struct CK_KEY_WRAP_SET_OAEP_PARAMS {
    /// block contents byte
    pub bBC: CK_BYTE,
    /// extra data
    pub pX: CK_BYTE_PTR,
    /// length of extra data in bytes
    pub ulXLen: CK_ULONG,
  }
}
packed_clone!(CK_KEY_WRAP_SET_OAEP_PARAMS);

pub type CK_KEY_WRAP_SET_OAEP_PARAMS_PTR = *mut CK_KEY_WRAP_SET_OAEP_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_SSL3_RANDOM_DATA {
    pub pClientRandom: CK_BYTE_PTR,
    pub ulClientRandomLen: CK_ULONG,
    pub pServerRandom: CK_BYTE_PTR,
    pub ulServerRandomLen: CK_ULONG,
  }
}
packed_clone!(CK_SSL3_RANDOM_DATA);

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_SSL3_MASTER_KEY_DERIVE_PARAMS {
    pub RandomInfo: CK_SSL3_RANDOM_DATA,
    pub pVersion: CK_VERSION_PTR,
  }
}
packed_clone!(CK_SSL3_MASTER_KEY_DERIVE_PARAMS);

pub type CK_SSL3_MASTER_KEY_DERIVE_PARAMS_PTR = *mut CK_SSL3_MASTER_KEY_DERIVE_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_SSL3_KEY_MAT_OUT {
    pub hClientMacSecret: CK_OBJECT_HANDLE,
    pub hServerMacSecret: CK_OBJECT_HANDLE,
    pub hClientKey: CK_OBJECT_HANDLE,
    pub hServerKey: CK_OBJECT_HANDLE,
    pub pIVClient: CK_BYTE_PTR,
    pub pIVServer: CK_BYTE_PTR,
  }
}
packed_clone!(CK_SSL3_KEY_MAT_OUT);

pub type CK_SSL3_KEY_MAT_OUT_PTR = *mut CK_SSL3_KEY_MAT_OUT;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_SSL3_KEY_MAT_PARAMS {
    pub ulMacSizeInBits: CK_ULONG,
    pub ulKeySizeInBits: CK_ULONG,
    pub ulIVSizeInBits: CK_ULONG,
    pub bIsExport: CK_BBOOL,
    pub RandomInfo: CK_SSL3_RANDOM_DATA,
    pub pReturnedKeyMaterial: CK_SSL3_KEY_MAT_OUT_PTR,
  }
}
packed_clone!(CK_SSL3_KEY_MAT_PARAMS);

pub type CK_SSL3_KEY_MAT_PARAMS_PTR = *mut CK_SSL3_KEY_MAT_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_TLS_PRF_PARAMS {
    pub pSeed: CK_BYTE_PTR,
    pub ulSeedLen: CK_ULONG,
    pub pLabel: CK_BYTE_PTR,
    pub ulLabelLen: CK_ULONG,
    pub pOutput: CK_BYTE_PTR,
    pub pulOutputLen: CK_ULONG_PTR,
  }
}
packed_clone!(CK_TLS_PRF_PARAMS);

pub type CK_TLS_PRF_PARAMS_PTR = *mut CK_TLS_PRF_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_WTLS_RANDOM_DATA {
    pub pClientRandom: CK_BYTE_PTR,
    pub ulClientRandomLen: CK_ULONG,
    pub pServerRandom: CK_BYTE_PTR,
    pub ulServerRandomLen: CK_ULONG,
  }
}
packed_clone!(CK_WTLS_RANDOM_DATA);

pub type CK_WTLS_RANDOM_DATA_PTR = *mut CK_WTLS_RANDOM_DATA;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_WTLS_MASTER_KEY_DERIVE_PARAMS {
    pub DigestMechanism: CK_MECHANISM_TYPE,
    pub RandomInfo: CK_WTLS_RANDOM_DATA,
    pub pVersion: CK_BYTE_PTR,
  }
}
packed_clone!(CK_WTLS_MASTER_KEY_DERIVE_PARAMS);

pub type CK_WTLS_MASTER_KEY_DERIVE_PARAMS_PTR = *mut CK_WTLS_MASTER_KEY_DERIVE_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_WTLS_PRF_PARAMS {
    pub DigestMechanism: CK_MECHANISM_TYPE,
    pub pSeed: CK_BYTE_PTR,
    pub ulSeedLen: CK_ULONG,
    pub pLabel: CK_BYTE_PTR,
    pub ulLabelLen: CK_ULONG,
    pub pOutput: CK_BYTE_PTR,
    pub pulOutputLen: CK_ULONG_PTR,
  }
}
packed_clone!(CK_WTLS_PRF_PARAMS);

pub type CK_WTLS_PRF_PARAMS_PTR = *mut CK_WTLS_PRF_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_WTLS_KEY_MAT_OUT {
    pub hMacSecret: CK_OBJECT_HANDLE,
    pub hKey: CK_OBJECT_HANDLE,
    pub pIV: CK_BYTE_PTR,
  }
}
packed_clone!(CK_WTLS_KEY_MAT_OUT);

pub type CK_WTLS_KEY_MAT_OUT_PTR = *mut CK_WTLS_KEY_MAT_OUT;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_WTLS_KEY_MAT_PARAMS {
    pub DigestMechanism: CK_MECHANISM_TYPE,
    pub ulMacSizeInBits: CK_ULONG,
    pub ulKeySizeInBits: CK_ULONG,
    pub ulIVSizeInBits: CK_ULONG,
    pub ulSequenceNumber: CK_ULONG,
    pub bIsExport: CK_BBOOL,
    pub RandomInfo: CK_WTLS_RANDOM_DATA,
    pub pReturnedKeyMaterial: CK_WTLS_KEY_MAT_OUT_PTR,
  }
}
packed_clone!(CK_WTLS_KEY_MAT_PARAMS);

pub type CK_WTLS_KEY_MAT_PARAMS_PTR = *mut CK_WTLS_KEY_MAT_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_CMS_SIG_PARAMS {
    pub certificateHandle: CK_OBJECT_HANDLE,
    pub pSigningMechanism: CK_MECHANISM_PTR,
    pub pDigestMechanism: CK_MECHANISM_PTR,
    pub pContentType: CK_UTF8CHAR_PTR,
    pub pRequestedAttributes: CK_BYTE_PTR,
    pub ulRequestedAttributesLen: CK_ULONG,
    pub pRequiredAttributes: CK_BYTE_PTR,
    pub ulRequiredAttributesLen: CK_ULONG,
  }
}
packed_clone!(CK_CMS_SIG_PARAMS);

pub type CK_CMS_SIG_PARAMS_PTR = *mut CK_CMS_SIG_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_KEY_DERIVATION_STRING_DATA {
    pub pData: CK_BYTE_PTR,
    pub ulLen: CK_ULONG,
  }
}
packed_clone!(CK_KEY_DERIVATION_STRING_DATA);

pub type CK_KEY_DERIVATION_STRING_DATA_PTR = *mut CK_KEY_DERIVATION_STRING_DATA;


/// The CK_EXTRACT_PARAMS is used for the
/// CKM_EXTRACT_KEY_FROM_KEY mechanism.  It specifies which bit
/// of the base key should be used as the first bit of the
/// derived key
pub type CK_EXTRACT_PARAMS = CK_ULONG;

pub type CK_EXTRACT_PARAMS_PTR = *mut CK_EXTRACT_PARAMS;

/// CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE is used to
/// indicate the Pseudo-Random Function (PRF) used to generate
/// key bits using PKCS #5 PBKDF2.
pub type CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = CK_ULONG;

pub type CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE_PTR = *mut CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE;

pub const CKP_PKCS5_PBKD2_HMAC_SHA1: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000001;
pub const CKP_PKCS5_PBKD2_HMAC_GOSTR3411: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000002;
pub const CKP_PKCS5_PBKD2_HMAC_SHA224: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000003;
pub const CKP_PKCS5_PBKD2_HMAC_SHA256: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000004;
pub const CKP_PKCS5_PBKD2_HMAC_SHA384: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000005;
pub const CKP_PKCS5_PBKD2_HMAC_SHA512: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000006;
pub const CKP_PKCS5_PBKD2_HMAC_SHA512_224: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000007;
pub const CKP_PKCS5_PBKD2_HMAC_SHA512_256: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE = 0x00000008;

/// CK_PKCS5_PBKDF2_SALT_SOURCE_TYPE is used to indicate the
/// source of the salt value when deriving a key using PKCS #5
/// PBKDF2.
pub type CK_PKCS5_PBKDF2_SALT_SOURCE_TYPE = CK_ULONG;

pub type CK_PKCS5_PBKDF2_SALT_SOURCE_TYPE_PTR = *mut CK_PKCS5_PBKDF2_SALT_SOURCE_TYPE;

/// The following salt value sources are defined in PKCS #5 v2.0.
pub const CKZ_SALT_SPECIFIED: CK_PKCS5_PBKDF2_SALT_SOURCE_TYPE = 0x00000001;

cryptoki_aligned! {
  /// CK_PKCS5_PBKD2_PARAMS is a structure that provides the
  /// parameters to the CKM_PKCS5_PBKD2 mechanism.
  #[derive(Debug, Copy)]
  pub struct CK_PKCS5_PBKD2_PARAMS {
    pub saltSource: CK_PKCS5_PBKDF2_SALT_SOURCE_TYPE,
    pub pSaltSourceData: CK_VOID_PTR,
    pub ulSaltSourceDataLen: CK_ULONG,
    pub iterations: CK_ULONG,
    pub prf: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE,
    pub pPrfData: CK_VOID_PTR,
    pub ulPrfDataLen: CK_ULONG,
    pub pPassword: CK_UTF8CHAR_PTR,
    pub ulPasswordLen: CK_ULONG_PTR,
  }
}
packed_clone!(CK_PKCS5_PBKD2_PARAMS);

pub type CK_PKCS5_PBKD2_PARAMS_PTR = *mut CK_PKCS5_PBKD2_PARAMS;

cryptoki_aligned! {
  /// CK_PKCS5_PBKD2_PARAMS2 is a corrected version of the CK_PKCS5_PBKD2_PARAMS
  /// structure that provides the parameters to the CKM_PKCS5_PBKD2 mechanism
  /// noting that the ulPasswordLen field is a CK_ULONG and not a CK_ULONG_PTR.
  #[derive(Debug, Copy)]
  pub struct CK_PKCS5_PBKD2_PARAMS2 {
    pub saltSource: CK_PKCS5_PBKDF2_SALT_SOURCE_TYPE,
    pub pSaltSourceData: CK_VOID_PTR,
    pub ulSaltSourceDataLen: CK_ULONG,
    pub iterations: CK_ULONG,
    pub prf: CK_PKCS5_PBKD2_PSEUDO_RANDOM_FUNCTION_TYPE,
    pub pPrfData: CK_VOID_PTR,
    pub ulPrfDataLen: CK_ULONG,
    pub pPassword: CK_UTF8CHAR_PTR,
    pub ulPasswordLen: CK_ULONG,
  }
}
packed_clone!(CK_PKCS5_PBKD2_PARAMS2);

pub type CK_PKCS5_PBKD2_PARAMS2_PTR = *mut CK_PKCS5_PBKD2_PARAMS2;

pub type CK_OTP_PARAM_TYPE = CK_ULONG;
/// backward compatibility
pub type CK_PARAM_TYPE = CK_OTP_PARAM_TYPE;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_OTP_PARAM {
    pub paramType: CK_OTP_PARAM_TYPE,
    pub pValue: CK_VOID_PTR,
    pub ulValueLen: CK_ULONG,
  }
}
packed_clone!(CK_OTP_PARAM);

pub type CK_OTP_PARAM_PTR = *mut CK_OTP_PARAM;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_OTP_PARAMS {
    pub pParams: CK_OTP_PARAM_PTR,
    pub ulCount: CK_ULONG,
  }
}
packed_clone!(CK_OTP_PARAMS);

pub type CK_OTP_PARAMS_PTR = *mut CK_OTP_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_OTP_SIGNATURE_INFO {
    pub pParams: CK_OTP_PARAM_PTR,
    pub ulCount: CK_ULONG,
  }
}
packed_clone!(CK_OTP_SIGNATURE_INFO);

pub type CK_OTP_SIGNATURE_INFO_PTR = *mut CK_OTP_SIGNATURE_INFO;

pub const CK_OTP_VALUE: CK_ULONG = 0;
pub const CK_OTP_PIN: CK_ULONG = 1;
pub const CK_OTP_CHALLENGE: CK_ULONG = 2;
pub const CK_OTP_TIME: CK_ULONG = 3;
pub const CK_OTP_COUNTER: CK_ULONG = 4;
pub const CK_OTP_FLAGS: CK_ULONG = 5;
pub const CK_OTP_OUTPUT_LENGTH: CK_ULONG = 6;
pub const CK_OTP_OUTPUT_FORMAT: CK_ULONG = 7;

pub const CKF_NEXT_OTP: CK_FLAGS = 0x00000001;
pub const CKF_EXCLUDE_TIME: CK_FLAGS = 0x00000002;
pub const CKF_EXCLUDE_COUNTER: CK_FLAGS = 0x00000004;
pub const CKF_EXCLUDE_CHALLENGE: CK_FLAGS = 0x00000008;
pub const CKF_EXCLUDE_PIN: CK_FLAGS = 0x00000010;
pub const CKF_USER_FRIENDLY_OTP: CK_FLAGS = 0x00000020;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_KIP_PARAMS {
    pub pMechanism: CK_MECHANISM_PTR,
    pub hKey: CK_OBJECT_HANDLE,
    pub pSeed: CK_BYTE_PTR,
    pub ulSeedLen: CK_ULONG,
  }
}
packed_clone!(CK_KIP_PARAMS);

pub type CK_KIP_PARAMS_PTR = *mut CK_KIP_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_AES_CTR_PARAMS {
    pub ulCounterBits: CK_ULONG,
    pub cb: [CK_BYTE; 16],
  }
}
packed_clone!(CK_AES_CTR_PARAMS);

pub type CK_AES_CTR_PARAMS_PTR = *mut CK_AES_CTR_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_GCM_PARAMS {
    pub pIv: CK_BYTE_PTR,
    pub ulIvLen: CK_ULONG,
    pub ulIvBits: CK_ULONG,
    pub pAAD: CK_BYTE_PTR,
    pub ulAADLen: CK_ULONG,
    pub ulTagBits: CK_ULONG,
  }
}
packed_clone!(CK_GCM_PARAMS);

pub type CK_GCM_PARAMS_PTR = *mut CK_GCM_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_CCM_PARAMS {
    pub ulDataLen: CK_ULONG,
    pub pNonce: CK_BYTE_PTR,
    pub ulNonceLen: CK_ULONG,
    pub pAAD: CK_BYTE_PTR,
    pub ulAADLen: CK_ULONG,
    pub ulMACLen: CK_ULONG,
  }
}
packed_clone!(CK_CCM_PARAMS);

pub type CK_CCM_PARAMS_PTR = *mut CK_CCM_PARAMS;

cryptoki_aligned! {
  /// Deprecated. Use CK_GCM_PARAMS
  #[derive(Debug, Copy)]
  pub struct CK_AES_GCM_PARAMS {
    pub pIv: CK_BYTE_PTR,
    pub ulIvLen: CK_ULONG,
    pub ulIvBits: CK_ULONG,
    pub pAAD: CK_BYTE_PTR,
    pub ulAADLen: CK_ULONG,
    pub ulTagBits: CK_ULONG,
  }
}
packed_clone!(CK_AES_GCM_PARAMS);

pub type CK_AES_GCM_PARAMS_PTR = *mut CK_AES_GCM_PARAMS;

cryptoki_aligned! {
  /// Deprecated. Use CK_CCM_PARAMS
  #[derive(Debug, Copy)]
  pub struct CK_AES_CCM_PARAMS {
    pub ulDataLen: CK_ULONG,
    pub pNonce: CK_BYTE_PTR,
    pub ulNonceLen: CK_ULONG,
    pub pAAD: CK_BYTE_PTR,
    pub ulAADLen: CK_ULONG,
    pub ulMACLen: CK_ULONG,
  }
}
packed_clone!(CK_AES_CCM_PARAMS);

pub type CK_AES_CCM_PARAMS_PTR = *mut CK_AES_CCM_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_CAMELLIA_CTR_PARAMS {
    pub ulCounterBits: CK_ULONG,
    pub cb: [CK_BYTE; 16],
  }
}
packed_clone!(CK_CAMELLIA_CTR_PARAMS);

pub type CK_CAMELLIA_CTR_PARAMS_PTR = *mut CK_CAMELLIA_CTR_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_CAMELLIA_CBC_ENCRYPT_DATA_PARAMS {
    pub iv: [CK_BYTE; 16],
    pub pData: CK_BYTE_PTR,
    pub length: CK_ULONG,
  }
}
packed_clone!(CK_CAMELLIA_CBC_ENCRYPT_DATA_PARAMS);

pub type CK_CAMELLIA_CBC_ENCRYPT_DATA_PARAMS_PTR = *mut CK_CAMELLIA_CBC_ENCRYPT_DATA_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_ARIA_CBC_ENCRYPT_DATA_PARAMS {
    pub iv: [CK_BYTE; 16],
    pub pData: CK_BYTE_PTR,
    pub length: CK_ULONG,
  }
}
packed_clone!(CK_ARIA_CBC_ENCRYPT_DATA_PARAMS);

pub type CK_ARIA_CBC_ENCRYPT_DATA_PARAMS_PTR = *mut CK_ARIA_CBC_ENCRYPT_DATA_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_DSA_PARAMETER_GEN_PARAM {
    pub hash: CK_MECHANISM_TYPE,
    pub pSeed: CK_BYTE_PTR,
    pub ulSeedLen: CK_ULONG,
    pub ulIndex: CK_ULONG,
  }
}
packed_clone!(CK_DSA_PARAMETER_GEN_PARAM);

pub type CK_DSA_PARAMETER_GEN_PARAM_PTR = *mut CK_DSA_PARAMETER_GEN_PARAM;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_ECDH_AES_KEY_WRAP_PARAMS {
    pub ulAESKeyBits: CK_ULONG,
    pub kdf: CK_EC_KDF_TYPE,
    pub ulSharedDataLen: CK_ULONG,
    pub pSharedData: CK_BYTE_PTR,
  }
}
packed_clone!(CK_ECDH_AES_KEY_WRAP_PARAMS);

pub type CK_ECDH_AES_KEY_WRAP_PARAMS_PTR = *mut CK_ECDH_AES_KEY_WRAP_PARAMS;

pub type CK_JAVA_MIDP_SECURITY_DOMAIN = CK_ULONG;

pub type CK_CERTIFICATE_CATEGORY = CK_ULONG;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_RSA_AES_KEY_WRAP_PARAMS {
    pub ulAESKeyBits: CK_ULONG,
    pub pOAEPParams: CK_RSA_PKCS_OAEP_PARAMS_PTR,
  }
}
packed_clone!(CK_RSA_AES_KEY_WRAP_PARAMS);

pub type CK_RSA_AES_KEY_WRAP_PARAMS_PTR = *mut CK_RSA_AES_KEY_WRAP_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_TLS12_MASTER_KEY_DERIVE_PARAMS {
    pub RandomInfo: CK_SSL3_RANDOM_DATA,
    pub pVersion: CK_VERSION_PTR,
    pub prfHashMechanism: CK_MECHANISM_TYPE,
  }
}
packed_clone!(CK_TLS12_MASTER_KEY_DERIVE_PARAMS);

pub type CK_TLS12_MASTER_KEY_DERIVE_PARAMS_PTR = *mut CK_TLS12_MASTER_KEY_DERIVE_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_TLS12_KEY_MAT_PARAMS {
    pub ulMacSizeInBits: CK_ULONG,
    pub ulKeySizeInBits: CK_ULONG,
    pub ulIVSizeInBits: CK_ULONG,
    pub bIsExport: CK_BBOOL,
    pub RandomInfo: CK_SSL3_RANDOM_DATA,
    pub pReturnedKeyMaterial: CK_SSL3_KEY_MAT_OUT_PTR,
    pub prfHashMechanism: CK_MECHANISM_TYPE,
  }
}
packed_clone!(CK_TLS12_KEY_MAT_PARAMS);

pub type CK_TLS12_KEY_MAT_PARAMS_PTR = *mut CK_TLS12_KEY_MAT_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_TLS_KDF_PARAMS {
    pub prfMechanism: CK_MECHANISM_TYPE,
    pub pLabel: CK_BYTE_PTR,
    pub ulLabelLength: CK_ULONG,
    pub RandomInfo: CK_SSL3_RANDOM_DATA,
    pub pContextData: CK_BYTE_PTR,
    pub ulContextDataLength: CK_ULONG,
  }
}
packed_clone!(CK_TLS_KDF_PARAMS);

pub type CK_TLS_KDF_PARAMS_PTR = *mut CK_TLS_KDF_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_TLS_MAC_PARAMS {
    pub prfHashMechanism: CK_MECHANISM_TYPE,
    pub ulMacLength: CK_ULONG,
    pub ulServerOrClient: CK_ULONG,
  }
}
packed_clone!(CK_TLS_MAC_PARAMS);

pub type CK_TLS_MAC_PARAMS_PTR = *mut CK_TLS_MAC_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_GOSTR3410_DERIVE_PARAMS {
    pub kdf: CK_EC_KDF_TYPE,
    pub pPublicData: CK_BYTE_PTR,
    pub ulPublicDataLen: CK_ULONG,
    pub pUKM: CK_BYTE_PTR,
    pub ulUKMLen: CK_ULONG,
  }
}
packed_clone!(CK_GOSTR3410_DERIVE_PARAMS);

pub type CK_GOSTR3410_DERIVE_PARAMS_PTR = *mut CK_GOSTR3410_DERIVE_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_GOSTR3410_KEY_WRAP_PARAMS {
    pub pWrapOID: CK_BYTE_PTR,
    pub ulWrapOIDLen: CK_ULONG,
    pub pUKM: CK_BYTE_PTR,
    pub ulUKMLen: CK_ULONG,
    pub hKey: CK_OBJECT_HANDLE,
  }
}
packed_clone!(CK_GOSTR3410_KEY_WRAP_PARAMS);

pub type CK_GOSTR3410_KEY_WRAP_PARAMS_PTR = *mut CK_GOSTR3410_KEY_WRAP_PARAMS;

cryptoki_aligned! {
  #[derive(Debug, Copy)]
  pub struct CK_SEED_CBC_ENCRYPT_DATA_PARAMS {
    pub iv: [CK_BYTE; 16],
    pub pData: CK_BYTE_PTR,
    pub length: CK_ULONG,
  }
}
packed_clone!(CK_SEED_CBC_ENCRYPT_DATA_PARAMS);

pub type CK_SEED_CBC_ENCRYPT_DATA_PARAMS_PTR = *mut CK_SEED_CBC_ENCRYPT_DATA_PARAMS;
