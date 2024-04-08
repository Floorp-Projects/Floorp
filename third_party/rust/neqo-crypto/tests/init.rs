// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// This uses external interfaces to neqo_crypto rather than being a module
// inside of lib.rs. Because all other code uses the test_fixture module,
// they will be calling into the public version of init_db().  Calling into
// the version exposed to an inner module in lib.rs would result in calling
// a different version of init_db.  That causes explosions as they get
// different versions of the Once instance they use and they initialize NSS
// twice, probably likely in parallel.  That doesn't work out well.
use neqo_crypto::{assert_initialized, init_db};

// Pull in the NSS internals so that we can ask NSS if it thinks that
// it is properly initialized.
#[allow(dead_code, non_upper_case_globals)]
mod nss {
    include!(concat!(env!("OUT_DIR"), "/nss_init.rs"));
}

#[cfg(nss_nodb)]
#[test]
fn init_nodb() {
    neqo_crypto::init().unwrap();
    assert_initialized();
    unsafe {
        assert_ne!(nss::NSS_IsInitialized(), 0);
    }
}

#[cfg(nss_nodb)]
#[test]
fn init_twice_nodb() {
    unsafe {
        nss::NSS_NoDB_Init(std::ptr::null());
        assert_ne!(nss::NSS_IsInitialized(), 0);
    }
    // Now do it again
    init_nodb();
}

#[cfg(not(nss_nodb))]
#[test]
fn init_withdb() {
    init_db(::test_fixture::NSS_DB_PATH).unwrap();
    assert_initialized();
    unsafe {
        assert_ne!(nss::NSS_IsInitialized(), 0);
    }
}

#[cfg(not(nss_nodb))]
#[test]
fn init_twice_withdb() {
    use std::{ffi::CString, path::PathBuf};

    let empty = CString::new("").unwrap();
    let path: PathBuf = ::test_fixture::NSS_DB_PATH.into();
    assert!(path.is_dir());
    let pathstr = path.to_str().unwrap();
    let dircstr = CString::new(pathstr).unwrap();
    unsafe {
        nss::NSS_Initialize(
            dircstr.as_ptr(),
            empty.as_ptr(),
            empty.as_ptr(),
            nss::SECMOD_DB.as_ptr().cast(),
            nss::NSS_INIT_READONLY,
        );
        assert_ne!(nss::NSS_IsInitialized(), 0);
    }
    // Now do it again
    init_withdb();
}
