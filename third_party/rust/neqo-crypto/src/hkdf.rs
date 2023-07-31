// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    constants::{
        Cipher, Version, TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384,
        TLS_CHACHA20_POLY1305_SHA256, TLS_VERSION_1_3,
    },
    err::{Error, Res},
    p11::{
        random, Item, PK11Origin, PK11SymKey, PK11_ImportDataKey, Slot, SymKey, CKA_DERIVE,
        CKM_HKDF_DERIVE, CK_ATTRIBUTE_TYPE, CK_MECHANISM_TYPE,
    },
};

use std::{
    convert::TryFrom,
    os::raw::{c_char, c_uint},
    ptr::null_mut,
};

experimental_api!(SSL_HkdfExtract(
    version: Version,
    cipher: Cipher,
    salt: *mut PK11SymKey,
    ikm: *mut PK11SymKey,
    prk: *mut *mut PK11SymKey,
));
experimental_api!(SSL_HkdfExpandLabel(
    version: Version,
    cipher: Cipher,
    prk: *mut PK11SymKey,
    handshake_hash: *const u8,
    handshake_hash_len: c_uint,
    label: *const c_char,
    label_len: c_uint,
    secret: *mut *mut PK11SymKey,
));

fn key_size(version: Version, cipher: Cipher) -> Res<usize> {
    if version != TLS_VERSION_1_3 {
        return Err(Error::UnsupportedVersion);
    }
    Ok(match cipher {
        TLS_AES_128_GCM_SHA256 | TLS_CHACHA20_POLY1305_SHA256 => 32,
        TLS_AES_256_GCM_SHA384 => 48,
        _ => return Err(Error::UnsupportedCipher),
    })
}

/// Generate a random key of the right size for the given suite.
///
/// # Errors
/// Only if NSS fails.
pub fn generate_key(version: Version, cipher: Cipher) -> Res<SymKey> {
    import_key(version, &random(key_size(version, cipher)?))
}

/// Import a symmetric key for use with HKDF.
///
/// # Errors
/// Errors returned if the key buffer is an incompatible size or the NSS functions fail.
pub fn import_key(version: Version, buf: &[u8]) -> Res<SymKey> {
    if version != TLS_VERSION_1_3 {
        return Err(Error::UnsupportedVersion);
    }
    let slot = Slot::internal()?;
    let key_ptr = unsafe {
        PK11_ImportDataKey(
            *slot,
            CK_MECHANISM_TYPE::from(CKM_HKDF_DERIVE),
            PK11Origin::PK11_OriginUnwrap,
            CK_ATTRIBUTE_TYPE::from(CKA_DERIVE),
            &mut Item::wrap(buf),
            null_mut(),
        )
    };
    SymKey::from_ptr(key_ptr)
}

/// Extract a PRK from the given salt and IKM using the algorithm defined in RFC 5869.
///
/// # Errors
/// Errors returned if inputs are too large or the NSS functions fail.
pub fn extract(
    version: Version,
    cipher: Cipher,
    salt: Option<&SymKey>,
    ikm: &SymKey,
) -> Res<SymKey> {
    let mut prk: *mut PK11SymKey = null_mut();
    let salt_ptr: *mut PK11SymKey = match salt {
        Some(s) => **s,
        None => null_mut(),
    };
    unsafe { SSL_HkdfExtract(version, cipher, salt_ptr, **ikm, &mut prk) }?;
    SymKey::from_ptr(prk)
}

/// Expand a PRK using the HKDF-Expand-Label function defined in RFC 8446.
///
/// # Errors
/// Errors returned if inputs are too large or the NSS functions fail.
pub fn expand_label(
    version: Version,
    cipher: Cipher,
    prk: &SymKey,
    handshake_hash: &[u8],
    label: &str,
) -> Res<SymKey> {
    let l = label.as_bytes();
    let mut secret: *mut PK11SymKey = null_mut();

    // Note that this doesn't allow for passing null() for the handshake hash.
    // A zero-length slice produces an identical result.
    unsafe {
        SSL_HkdfExpandLabel(
            version,
            cipher,
            **prk,
            handshake_hash.as_ptr(),
            c_uint::try_from(handshake_hash.len())?,
            l.as_ptr().cast(),
            c_uint::try_from(l.len())?,
            &mut secret,
        )
    }?;
    SymKey::from_ptr(secret)
}
