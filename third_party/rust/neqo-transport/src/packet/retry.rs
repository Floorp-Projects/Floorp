// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(clippy::pedantic)]

use crate::version::Version;
use crate::{Error, Res};

use neqo_common::qerror;
use neqo_crypto::{hkdf, Aead, TLS_AES_128_GCM_SHA256, TLS_VERSION_1_3};

use std::cell::RefCell;

/// The AEAD used for Retry is fixed, so use thread local storage.
fn make_aead(version: Version) -> Aead {
    #[cfg(debug_assertions)]
    ::neqo_crypto::assert_initialized();

    let secret = hkdf::import_key(TLS_VERSION_1_3, version.retry_secret()).unwrap();
    Aead::new(
        false,
        TLS_VERSION_1_3,
        TLS_AES_128_GCM_SHA256,
        &secret,
        version.label_prefix(),
    )
    .unwrap()
}
thread_local!(static RETRY_AEAD_29: RefCell<Aead> = RefCell::new(make_aead(Version::Draft29)));
thread_local!(static RETRY_AEAD_V1: RefCell<Aead> = RefCell::new(make_aead(Version::Version1)));
thread_local!(static RETRY_AEAD_V2: RefCell<Aead> = RefCell::new(make_aead(Version::Version2)));

/// Run a function with the appropriate Retry AEAD.
pub fn use_aead<F, T>(version: Version, f: F) -> Res<T>
where
    F: FnOnce(&Aead) -> Res<T>,
{
    match version {
        Version::Version2 => &RETRY_AEAD_V2,
        Version::Version1 => &RETRY_AEAD_V1,
        Version::Draft29 | Version::Draft30 | Version::Draft31 | Version::Draft32 => &RETRY_AEAD_29,
    }
    .try_with(|aead| f(&aead.borrow()))
    .map_err(|e| {
        qerror!("Unable to access Retry AEAD: {:?}", e);
        Error::InternalError(6)
    })?
}

/// Determine how large the expansion is for a given key.
pub fn expansion(version: Version) -> usize {
    if let Ok(ex) = use_aead(version, |aead| Ok(aead.expansion())) {
        ex
    } else {
        panic!("Unable to access Retry AEAD")
    }
}
