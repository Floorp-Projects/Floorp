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

use neqo_common::hex;

use std::convert::TryInto;
use std::ops::{Deref, DerefMut};
use std::ptr::NonNull;

#[allow(clippy::unreadable_literal)]
mod nss_p11 {
    include!(concat!(env!("OUT_DIR"), "/nss_p11.rs"));
}

pub use nss_p11::*;

macro_rules! scoped_ptr {
    ($scoped:ident, $target:ty, $dtor:path) => {
        pub struct $scoped {
            ptr: *mut $target,
        }

        impl $scoped {
            #[must_use]
            pub fn new(ptr: NonNull<$target>) -> Self {
                Self { ptr: ptr.as_ptr() }
            }
        }

        impl Deref for $scoped {
            type Target = *mut $target;
            #[must_use]
            fn deref(&self) -> &*mut $target {
                &self.ptr
            }
        }

        impl DerefMut for $scoped {
            fn deref_mut(&mut self) -> &mut *mut $target {
                &mut self.ptr
            }
        }

        impl Drop for $scoped {
            fn drop(&mut self) {
                let _ = unsafe { $dtor(self.ptr) };
            }
        }
    };
}

scoped_ptr!(Certificate, CERTCertificate, CERT_DestroyCertificate);
scoped_ptr!(CertList, CERTCertList, CERT_DestroyCertList);
scoped_ptr!(PrivateKey, SECKEYPrivateKey, SECKEY_DestroyPrivateKey);
scoped_ptr!(SymKey, PK11SymKey, PK11_FreeSymKey);
scoped_ptr!(Slot, PK11SlotInfo, PK11_FreeSlot);

impl SymKey {
    /// You really don't want to use this.
    pub fn as_bytes<'a>(&'a self) -> Res<&'a [u8]> {
        secstatus_to_res(unsafe { PK11_ExtractKeyValue(self.ptr) })?;

        let key_item = unsafe { PK11_GetKeyData(self.ptr) };
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
            write!(f, "SymKey {}", hex(b))
        } else {
            write!(f, "Opaque SymKey")
        }
    }
}

/// Generate a randomized buffer.
pub fn random(size: usize) -> Res<Vec<u8>> {
    let mut buf = vec![0; size];
    secstatus_to_res(unsafe { PK11_GenerateRandom(buf.as_mut_ptr(), buf.len().try_into()?) })?;
    Ok(buf)
}

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
