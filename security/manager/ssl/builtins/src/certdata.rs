/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use pkcs11_bindings::nss::*;
use pkcs11_bindings::*;

// We need to expand some PKCS#11 / NSS constants as byte arrays for pattern matching and
// C_GetAttributeValue queries. We use native endianness, because PKCS#11 sits between an
// application and a device driver that are running on the same machine.
pub const CKC_X_509_BYTES: &[u8] = &CKC_X_509.to_ne_bytes();
pub const CKO_CERTIFICATE_BYTES: &[u8] = &CKO_CERTIFICATE.to_ne_bytes();
pub const CKO_NSS_BUILTIN_ROOT_LIST_BYTES: &[u8] = &CKO_NSS_BUILTIN_ROOT_LIST.to_ne_bytes();
pub const CKO_NSS_TRUST_BYTES: &[u8] = &CKO_NSS_TRUST.to_ne_bytes();
pub const CKT_NSS_MUST_VERIFY_TRUST_BYTES: &[u8] = &CKT_NSS_MUST_VERIFY_TRUST.to_ne_bytes();
pub const CKT_NSS_NOT_TRUSTED_BYTES: &[u8] = &CKT_NSS_NOT_TRUSTED.to_ne_bytes();
pub const CKT_NSS_TRUSTED_DELEGATOR_BYTES: &[u8] = &CKT_NSS_TRUSTED_DELEGATOR.to_ne_bytes();
pub const CK_FALSE_BYTES: &[u8] = &CK_FALSE.to_ne_bytes();
pub const CK_TRUE_BYTES: &[u8] = &CK_TRUE.to_ne_bytes();

#[derive(PartialEq, Eq)]
pub struct Root {
    pub label: &'static str,
    pub der_name: (u8, u8),
    pub der_serial: (u8, u8),
    pub der_cert: &'static [u8],
    pub mozilla_ca_policy: Option<&'static [u8]>,
    pub server_distrust_after: Option<&'static [u8]>,
    pub email_distrust_after: Option<&'static [u8]>,
    pub sha1: [u8; 20],
    pub md5: [u8; 16],
    pub trust_server: &'static [u8],
    pub trust_email: &'static [u8],
}

impl Root {
    pub fn der_name(&self) -> &'static [u8] {
        &self.der_cert[self.der_name.0 as usize..][..self.der_name.1 as usize]
    }
    pub fn der_serial(&self) -> &'static [u8] {
        &self.der_cert[self.der_serial.0 as usize..][..self.der_serial.1 as usize]
    }
}

impl PartialOrd for Root {
    fn partial_cmp(&self, other: &Root) -> Option<std::cmp::Ordering> {
        self.der_name().partial_cmp(other.der_name())
    }
}

include!(concat!(env!("OUT_DIR"), "/builtins.rs"));
