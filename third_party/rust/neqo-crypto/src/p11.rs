// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(dead_code)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::{
    cell::RefCell,
    convert::TryFrom,
    mem,
    ops::{Deref, DerefMut},
    os::raw::{c_int, c_uint},
    ptr::null_mut,
};

use neqo_common::hex_with_len;

use crate::{
    err::{secstatus_to_res, Error, Res},
    null_safe_slice,
};

#[allow(clippy::upper_case_acronyms)]
#[allow(clippy::unreadable_literal)]
#[allow(unknown_lints, clippy::borrow_as_ptr)]
mod nss_p11 {
    include!(concat!(env!("OUT_DIR"), "/nss_p11.rs"));
}

pub use nss_p11::*;

#[macro_export]
macro_rules! scoped_ptr {
    ($scoped:ident, $target:ty, $dtor:path) => {
        pub struct $scoped {
            ptr: *mut $target,
        }

        impl $scoped {
            /// Create a new instance of `$scoped` from a pointer.
            ///
            /// # Errors
            ///
            /// When passed a null pointer generates an error.
            pub fn from_ptr(ptr: *mut $target) -> Result<Self, $crate::err::Error> {
                if ptr.is_null() {
                    Err($crate::err::Error::last_nss_error())
                } else {
                    Ok(Self { ptr })
                }
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
            #[allow(unused_must_use)]
            fn drop(&mut self) {
                unsafe { $dtor(self.ptr) };
            }
        }
    };
}

scoped_ptr!(Certificate, CERTCertificate, CERT_DestroyCertificate);
scoped_ptr!(CertList, CERTCertList, CERT_DestroyCertList);
scoped_ptr!(PublicKey, SECKEYPublicKey, SECKEY_DestroyPublicKey);

impl PublicKey {
    /// Get the HPKE serialization of the public key.
    ///
    /// # Errors
    ///
    /// When the key cannot be exported, which can be because the type is not supported.
    ///
    /// # Panics
    ///
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

impl Clone for PublicKey {
    #[must_use]
    fn clone(&self) -> Self {
        let ptr = unsafe { SECKEY_CopyPublicKey(self.ptr) };
        assert!(!ptr.is_null());
        Self { ptr }
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

impl PrivateKey {
    /// Get the bits of the private key.
    ///
    /// # Errors
    ///
    /// When the key cannot be exported, which can be because the type is not supported
    /// or because the key data cannot be extracted from the PKCS#11 module.
    ///
    /// # Panics
    ///
    /// When the values are too large to fit.  So never.
    pub fn key_data(&self) -> Res<Vec<u8>> {
        let mut key_item = Item::make_empty();
        secstatus_to_res(unsafe {
            PK11_ReadRawAttribute(
                PK11ObjectType::PK11_TypePrivKey,
                (**self).cast(),
                CK_ATTRIBUTE_TYPE::from(CKA_VALUE),
                &mut key_item,
            )
        })?;
        let slc = unsafe { null_safe_slice(key_item.data, key_item.len) };
        let key = Vec::from(slc);
        // The data that `key_item` refers to needs to be freed, but we can't
        // use the scoped `Item` implementation.  This is OK as long as nothing
        // panics between `PK11_ReadRawAttribute` succeeding and here.
        unsafe {
            SECITEM_FreeItem(&mut key_item, PRBool::from(false));
        }
        Ok(key)
    }
}
unsafe impl Send for PrivateKey {}

impl Clone for PrivateKey {
    #[must_use]
    fn clone(&self) -> Self {
        let ptr = unsafe { SECKEY_CopyPrivateKey(self.ptr) };
        assert!(!ptr.is_null());
        Self { ptr }
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
        let p = unsafe { PK11_GetInternalSlot() };
        Slot::from_ptr(p)
    }
}

scoped_ptr!(SymKey, PK11SymKey, PK11_FreeSymKey);

impl SymKey {
    /// You really don't want to use this.
    ///
    /// # Errors
    ///
    /// Internal errors in case of failures in NSS.
    pub fn as_bytes(&self) -> Res<&[u8]> {
        secstatus_to_res(unsafe { PK11_ExtractKeyValue(self.ptr) })?;

        let key_item = unsafe { PK11_GetKeyData(self.ptr) };
        // This is accessing a value attached to the key, so we can treat this as a borrow.
        match unsafe { key_item.as_mut() } {
            None => Err(Error::InternalError),
            Some(key) => Ok(unsafe { null_safe_slice(key.data, key.len) }),
        }
    }
}

impl Clone for SymKey {
    #[must_use]
    fn clone(&self) -> Self {
        let ptr = unsafe { PK11_ReferenceSymKey(self.ptr) };
        assert!(!ptr.is_null());
        Self { ptr }
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

unsafe fn destroy_secitem(item: *mut SECItem) {
    SECITEM_FreeItem(item, PRBool::from(true));
}
scoped_ptr!(Item, SECItem, destroy_secitem);

impl Item {
    /// Create a wrapper for a slice of this object.
    /// Creating this object is technically safe, but using it is extremely dangerous.
    /// Minimally, it can only be passed as a `const SECItem*` argument to functions,
    /// or those that treat their argument as `const`.
    pub fn wrap(buf: &[u8]) -> SECItem {
        SECItem {
            type_: SECItemType::siBuffer,
            data: buf.as_ptr().cast_mut(),
            len: c_uint::try_from(buf.len()).unwrap(),
        }
    }

    /// Create a wrapper for a struct.
    /// Creating this object is technically safe, but using it is extremely dangerous.
    /// Minimally, it can only be passed as a `const SECItem*` argument to functions,
    /// or those that treat their argument as `const`.
    pub fn wrap_struct<T>(v: &T) -> SECItem {
        let data: *const T = v;
        SECItem {
            type_: SECItemType::siBuffer,
            data: data.cast_mut().cast(),
            len: c_uint::try_from(mem::size_of::<T>()).unwrap(),
        }
    }

    /// Make an empty `SECItem` for passing as a mutable `SECItem*` argument.
    pub fn make_empty() -> SECItem {
        SECItem {
            type_: SECItemType::siBuffer,
            data: null_mut(),
            len: 0,
        }
    }

    /// This dereferences the pointer held by the item and makes a copy of the
    /// content that is referenced there.
    ///
    /// # Safety
    ///
    /// This dereferences two pointers.  It doesn't get much less safe.
    pub unsafe fn into_vec(self) -> Vec<u8> {
        let b = self.ptr.as_ref().unwrap();
        // Sanity check the type, as some types don't count bytes in `Item::len`.
        assert_eq!(b.type_, SECItemType::siBuffer);
        let slc = null_safe_slice(b.data, b.len);
        Vec::from(slc)
    }
}

/// Fill a buffer with randomness.
///
/// # Panics
///
/// When `size` is too large or NSS fails.
pub fn randomize<B: AsMut<[u8]>>(mut buf: B) -> B {
    let m_buf = buf.as_mut();
    let len = c_int::try_from(m_buf.len()).unwrap();
    secstatus_to_res(unsafe { PK11_GenerateRandom(m_buf.as_mut_ptr(), len) }).unwrap();
    buf
}

struct RandomCache {
    cache: [u8; Self::SIZE],
    used: usize,
}

impl RandomCache {
    const SIZE: usize = 256;
    const CUTOFF: usize = 32;

    fn new() -> Self {
        RandomCache {
            cache: [0; Self::SIZE],
            used: Self::SIZE,
        }
    }

    fn randomize<B: AsMut<[u8]>>(&mut self, mut buf: B) -> B {
        let m_buf = buf.as_mut();
        debug_assert!(m_buf.len() <= Self::CUTOFF);
        let avail = Self::SIZE - self.used;
        if m_buf.len() <= avail {
            m_buf.copy_from_slice(&self.cache[self.used..self.used + m_buf.len()]);
            self.used += m_buf.len();
        } else {
            if avail > 0 {
                m_buf[..avail].copy_from_slice(&self.cache[self.used..]);
            }
            randomize(&mut self.cache[..]);
            self.used = m_buf.len() - avail;
            m_buf[avail..].copy_from_slice(&self.cache[..self.used]);
        }
        buf
    }
}

/// Generate a randomized array.
///
/// # Panics
///
/// When `size` is too large or NSS fails.
#[must_use]
pub fn random<const N: usize>() -> [u8; N] {
    thread_local! { static CACHE: RefCell<RandomCache> = RefCell::new(RandomCache::new()) };

    let buf = [0; N];
    if N <= RandomCache::CUTOFF {
        CACHE.with_borrow_mut(|c| c.randomize(buf))
    } else {
        randomize(buf)
    }
}

#[cfg(test)]
mod test {
    use test_fixture::fixture_init;

    use super::RandomCache;
    use crate::{random, randomize};

    #[test]
    fn randomness() {
        fixture_init();
        // If any of these ever fail, there is either a bug, or it's time to buy a lottery ticket.
        assert_ne!(random::<16>(), randomize([0; 16]));
        assert_ne!([0; 16], random::<16>());
        assert_ne!([0; 64], random::<64>());
    }

    #[test]
    fn cache_random_lengths() {
        const ZERO: [u8; 256] = [0; 256];

        fixture_init();
        let mut cache = RandomCache::new();
        let mut buf = [0; 256];
        let bits = usize::BITS - (RandomCache::CUTOFF - 1).leading_zeros();
        let mask = 0xff >> (u8::BITS - bits);

        for _ in 0..100 {
            let len = loop {
                let len = usize::from(random::<1>()[0] & mask) + 1;
                if len <= RandomCache::CUTOFF {
                    break len;
                }
            };
            buf.fill(0);
            if len >= 16 {
                assert_ne!(&cache.randomize(&mut buf[..len])[..len], &ZERO[..len]);
            }
        }
    }
}
