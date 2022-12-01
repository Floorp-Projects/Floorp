// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use crate::err::{secstatus_to_res, Error, Res};
use crate::util::SECItemMut;

use pkcs11_bindings::CKA_VALUE;

use std::convert::TryFrom;
use std::os::raw::{c_int, c_uint};

#[must_use]
pub fn hex_with_len(buf: impl AsRef<[u8]>) -> String {
    use std::fmt::Write;
    let buf = buf.as_ref();
    let mut ret = String::with_capacity(10 + buf.len() * 2);
    write!(&mut ret, "[{}]: ", buf.len()).unwrap();
    for b in buf {
        write!(&mut ret, "{:02x}", b).unwrap();
    }
    ret
}

#[allow(clippy::upper_case_acronyms)]
#[allow(clippy::unreadable_literal)]
#[allow(unknown_lints, clippy::borrow_as_ptr)]
mod nss_p11 {
    use crate::prtypes::*;
    use crate::nss_prelude::*;
    include!(concat!(env!("OUT_DIR"), "/nss_p11.rs"));
}

use crate::prtypes::*;
pub use nss_p11::*;

// Shadow these bindgen created values to correct their type.
pub const SHA256_LENGTH: usize = nss_p11::SHA256_LENGTH as usize;
pub const AES_BLOCK_SIZE: usize = nss_p11::AES_BLOCK_SIZE as usize;

scoped_ptr!(Certificate, CERTCertificate, CERT_DestroyCertificate);
scoped_ptr!(CertList, CERTCertList, CERT_DestroyCertList);

scoped_ptr!(SubjectPublicKeyInfo, CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo);

scoped_ptr!(PublicKey, SECKEYPublicKey, SECKEY_DestroyPublicKey);
impl_clone!(PublicKey, SECKEY_CopyPublicKey);

impl PublicKey {
    /// Get the HPKE serialization of the public key.
    ///
    /// # Errors
    /// When the key cannot be exported, which can be because the type is not supported.
    /// # Panics
    /// When keys are too large to fit in `c_uint/usize`.  So only on programming error.
    pub fn key_data(&self) -> Res<Vec<u8>> {
        let mut buf = vec![0; 100];
        let mut len: c_uint = 0;
        secstatus_to_res(unsafe {
            PK11_HPKE_Serialize(
                **self,
                buf.as_mut_ptr(),
                &mut len,
                c_uint::try_from(buf.len()).unwrap(),
            )
        })?;
        buf.truncate(usize::try_from(len).unwrap());
        Ok(buf)
    }
}

impl std::fmt::Debug for PublicKey {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if let Ok(b) = self.key_data() {
            write!(f, "PublicKey {}", hex_with_len(b))
        } else {
            write!(f, "Opaque PublicKey")
        }
    }
}

scoped_ptr!(PrivateKey, SECKEYPrivateKey, SECKEY_DestroyPrivateKey);
impl_clone!(PrivateKey, SECKEY_CopyPrivateKey);

impl PrivateKey {
    /// Get the bits of the private key.
    ///
    /// # Errors
    /// When the key cannot be exported, which can be because the type is not supported
    /// or because the key data cannot be extracted from the PKCS#11 module.
    /// # Panics
    /// When the values are too large to fit.  So never.
    pub fn key_data(&self) -> Res<Vec<u8>> {
        let mut key_item = SECItemMut::make_empty();
        secstatus_to_res(unsafe {
            PK11_ReadRawAttribute(
                PK11ObjectType::PK11_TypePrivKey,
                (**self).cast(),
                CKA_VALUE,
                key_item.as_mut(),
            )
        })?;
        Ok(key_item.as_slice().to_owned())
    }
}

impl std::fmt::Debug for PrivateKey {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if let Ok(b) = self.key_data() {
            write!(f, "PrivateKey {}", hex_with_len(b))
        } else {
            write!(f, "Opaque PrivateKey")
        }
    }
}

scoped_ptr!(Slot, PK11SlotInfo, PK11_FreeSlot);

impl Slot {
    pub fn internal() -> Res<Self> {
        unsafe { Slot::from_ptr(PK11_GetInternalSlot()) }
    }
}

// Note: PK11SymKey is internally reference counted
scoped_ptr!(SymKey, PK11SymKey, PK11_FreeSymKey);
impl_clone!(SymKey, PK11_ReferenceSymKey);

impl SymKey {
    /// You really don't want to use this.
    ///
    /// # Errors
    /// Internal errors in case of failures in NSS.
    pub fn as_bytes(&self) -> Res<&[u8]> {
        secstatus_to_res(unsafe { PK11_ExtractKeyValue(**self) })?;

        let key_item = unsafe { PK11_GetKeyData(**self) };
        // This is accessing a value attached to the key, so we can treat this as a borrow.
        match unsafe { key_item.as_mut() } {
            None => Err(Error::InternalError),
            Some(key) => Ok(unsafe { std::slice::from_raw_parts(key.data, key.len as usize) }),
        }
    }
}

impl std::fmt::Debug for SymKey {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if let Ok(b) = self.as_bytes() {
            write!(f, "SymKey {}", hex_with_len(b))
        } else {
            write!(f, "Opaque SymKey")
        }
    }
}

unsafe fn destroy_pk11_context(ctxt: *mut PK11Context) {
    PK11_DestroyContext(ctxt, PRBool::from(true));
}
scoped_ptr!(Context, PK11Context, destroy_pk11_context);

/// Generate a randomized buffer.
/// # Panics
/// When `size` is too large or NSS fails.
#[must_use]
pub fn random(size: usize) -> Vec<u8> {
    let mut buf = vec![0; size];
    secstatus_to_res(unsafe {
        PK11_GenerateRandom(buf.as_mut_ptr(), c_int::try_from(buf.len()).unwrap())
    })
    .unwrap();
    buf
}

impl_into_result!(SECOidData);

#[cfg(test)]
mod test {
    use super::random;
    use test_fixture::fixture_init;

    #[test]
    fn randomness() {
        fixture_init();
        // If this ever fails, there is either a bug, or it's time to buy a lottery ticket.
        assert_ne!(random(16), random(16));
    }
}
