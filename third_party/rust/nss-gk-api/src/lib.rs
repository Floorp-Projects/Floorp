// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]
// Bindgen auto generated code
// won't adhere to the clippy rules below
#![allow(clippy::module_name_repetitions)]
#![allow(clippy::unseparated_literal_suffix)]
#![allow(clippy::used_underscore_binding)]

#[macro_use]
pub mod err;
#[macro_use]
mod exp;
#[macro_use]
mod util;

pub mod p11;
mod prio;
mod ssl;
pub mod time;

pub use err::{Error, IntoResult, secstatus_to_res};
pub use p11::{PrivateKey, PublicKey, SymKey};
pub use util::*;

use once_cell::sync::OnceCell;

use std::ffi::CString;
use std::path::{Path, PathBuf};
use std::ptr::null;

const MINIMUM_NSS_VERSION: &str = "3.74";

#[allow(non_snake_case)]
#[allow(non_upper_case_globals)]
pub mod nss_prelude {
    pub use crate::prtypes::*;
    pub use _SECStatus::*;
    include!(concat!(env!("OUT_DIR"), "/nss_prelude.rs"));
}
pub use nss_prelude::{SECItem, SECItemArray, SECItemType, SECStatus};

#[allow(non_upper_case_globals, clippy::redundant_static_lifetimes)]
#[allow(clippy::upper_case_acronyms)]
#[allow(unknown_lints, clippy::borrow_as_ptr)]
mod nss {
    use crate::nss_prelude::*;
    include!(concat!(env!("OUT_DIR"), "/nss_init.rs"));
}

pub mod prtypes;
pub use prtypes::*;

// Shadow these bindgen created values to correct their type.
pub const PR_FALSE: PRBool = prtypes::PR_FALSE as PRBool;
pub const PR_TRUE: PRBool = prtypes::PR_TRUE as PRBool;

enum NssLoaded {
    External,
    NoDb,
    Db(Box<Path>),
}

impl Drop for NssLoaded {
    fn drop(&mut self) {
        if !matches!(self, Self::External) {
            unsafe {
                secstatus_to_res(nss::NSS_Shutdown()).expect("NSS Shutdown failed");
            }
        }
    }
}

static INITIALIZED: OnceCell<NssLoaded> = OnceCell::new();

fn already_initialized() -> bool {
    unsafe { nss::NSS_IsInitialized() != 0 }
}

fn version_check() {
    let min_ver = CString::new(MINIMUM_NSS_VERSION).unwrap();
    assert_ne!(
        unsafe { nss::NSS_VersionCheck(min_ver.as_ptr()) },
        0,
        "Minimum NSS version of {} not supported",
        MINIMUM_NSS_VERSION,
    );
}

/// Initialize NSS.  This only executes the initialization routines once, so if there is any chance that
pub fn init() {
    // Set time zero.
    time::init();
    INITIALIZED.get_or_init(|| {
        unsafe {
            version_check();
            if already_initialized() {
                return NssLoaded::External;
            }

            secstatus_to_res(nss::NSS_NoDB_Init(null())).expect("NSS_NoDB_Init failed");
            secstatus_to_res(nss::NSS_SetDomesticPolicy()).expect("NSS_SetDomesticPolicy failed");

            NssLoaded::NoDb
        }
    });
}

/// This enables SSLTRACE by calling a simple, harmless function to trigger its
/// side effects.  SSLTRACE is not enabled in NSS until a socket is made or
/// global options are accessed.  Reading an option is the least impact approach.
/// This allows us to use SSLTRACE in all of our unit tests and programs.
#[cfg(debug_assertions)]
fn enable_ssl_trace() {
    let opt = ssl::Opt::Locking.as_int();
    let mut v: ::std::os::raw::c_int = 0;
    secstatus_to_res(unsafe { ssl::SSL_OptionGetDefault(opt, &mut v) })
        .expect("SSL_OptionGetDefault failed");
}

/// Initialize with a database.
/// # Panics
/// If NSS cannot be initialized.
pub fn init_db<P: Into<PathBuf>>(dir: P) {
    time::init();
    INITIALIZED.get_or_init(|| {
        unsafe {
            version_check();
            if already_initialized() {
                return NssLoaded::External;
            }

            let path = dir.into();
            assert!(path.is_dir());
            let pathstr = path.to_str().expect("path converts to string").to_string();
            let dircstr = CString::new(pathstr).unwrap();
            let empty = CString::new("").unwrap();
            secstatus_to_res(nss::NSS_Initialize(
                dircstr.as_ptr(),
                empty.as_ptr(),
                empty.as_ptr(),
                nss::SECMOD_DB.as_ptr().cast(),
                nss::NSS_INIT_READONLY,
            ))
            .expect("NSS_Initialize failed");

            secstatus_to_res(nss::NSS_SetDomesticPolicy()).expect("NSS_SetDomesticPolicy failed");
            secstatus_to_res(ssl::SSL_ConfigServerSessionIDCache(
                1024,
                0,
                0,
                dircstr.as_ptr(),
            ))
            .expect("SSL_ConfigServerSessionIDCache failed");

            #[cfg(debug_assertions)]
            enable_ssl_trace();

            NssLoaded::Db(path.into_boxed_path())
        }
    });
}

/// # Panics
/// If NSS isn't initialized.
pub fn assert_initialized() {
    INITIALIZED.get().expect("NSS not initialized with init or init_db");
}
