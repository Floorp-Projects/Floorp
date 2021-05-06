#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

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
#[allow(
    dead_code,
    non_upper_case_globals,
    clippy::redundant_static_lifetimes,
    clippy::unseparated_literal_suffix,
    clippy::upper_case_acronyms
)]
#[allow(unknown_lints, renamed_and_removed_lints, clippy::unknown_clippy_lints)] // Until we require rust 1.51.
mod nss {
    include!(concat!(env!("OUT_DIR"), "/nss_init.rs"));
}

#[cfg(nss_nodb)]
#[test]
fn init_nodb() {
    init();
    assert_initialized();
    unsafe {
        assert!(nss::NSS_IsInitialized() != 0);
    }
}

#[cfg(not(nss_nodb))]
#[test]
fn init_withdb() {
    init_db(::test_fixture::NSS_DB_PATH);
    assert_initialized();
    unsafe {
        assert!(nss::NSS_IsInitialized() != 0);
    }
}
