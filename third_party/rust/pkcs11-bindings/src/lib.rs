/* -*- Mode: rust; rust-indent-offset: 4 -*- */

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(overflowing_literals)]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

/// Constants from NSS [pkcs11n.h](https://hg.mozilla.org/projects/nss/file/tip/lib/util/pkcs11n.h)
pub mod nss {
    use crate::{CKA_VENDOR_DEFINED, CKO_VENDOR_DEFINED, CK_ULONG};

    pub const NSSCK_VENDOR_NSS: CK_ULONG = 0x4E534350;

    pub const CKT_VENDOR_DEFINED: CK_ULONG = 0x80000000;
    pub const CKT_NSS: CK_ULONG = CKT_VENDOR_DEFINED | NSSCK_VENDOR_NSS;

    pub const CKT_NSS_TRUSTED: CK_ULONG = CKT_NSS + 1;
    pub const CKT_NSS_TRUSTED_DELEGATOR: CK_ULONG = CKT_NSS + 2;
    pub const CKT_NSS_MUST_VERIFY_TRUST: CK_ULONG = CKT_NSS + 3;
    pub const CKT_NSS_NOT_TRUSTED: CK_ULONG = CKT_NSS + 10;
    pub const CKT_NSS_TRUST_UNKNOWN: CK_ULONG = CKT_NSS + 5;

    pub const CKA_NSS: CK_ULONG = CKA_VENDOR_DEFINED | NSSCK_VENDOR_NSS;

    pub const CKA_TRUST: CK_ULONG = CKA_NSS + 0x2000;
    pub const CKA_TRUST_SERVER_AUTH: CK_ULONG = CKA_TRUST + 8;
    pub const CKA_TRUST_CLIENT_AUTH: CK_ULONG = CKA_TRUST + 9;
    pub const CKA_TRUST_CODE_SIGNING: CK_ULONG = CKA_TRUST + 10;
    pub const CKA_TRUST_EMAIL_PROTECTION: CK_ULONG = CKA_TRUST + 11;
    pub const CKA_TRUST_STEP_UP_APPROVED: CK_ULONG = CKA_TRUST + 16;

    pub const CKA_CERT_SHA1_HASH: CK_ULONG = CKA_TRUST + 100;
    pub const CKA_CERT_MD5_HASH: CK_ULONG = CKA_TRUST + 101;

    pub const CKA_NSS_MOZILLA_CA_POLICY: CK_ULONG = CKA_NSS + 34;
    pub const CKA_NSS_SERVER_DISTRUST_AFTER: CK_ULONG = CKA_NSS + 35;
    pub const CKA_NSS_EMAIL_DISTRUST_AFTER: CK_ULONG = CKA_NSS + 36;

    pub const CKO_NSS: CK_ULONG = CKO_VENDOR_DEFINED | NSSCK_VENDOR_NSS;
    pub const CKO_NSS_TRUST: CK_ULONG = CKO_NSS + 3;
    pub const CKO_NSS_BUILTIN_ROOT_LIST: CK_ULONG = CKO_NSS + 4;
}
