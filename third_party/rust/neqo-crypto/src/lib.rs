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
mod exp;
#[macro_use]
mod p11;

#[cfg(not(feature = "fuzzing"))]
mod aead;

#[cfg(feature = "fuzzing")]
mod aead_fuzzing;

pub mod agent;
mod agentio;
mod auth;
mod cert;
pub mod constants;
mod err;
pub mod ext;
pub mod hkdf;
pub mod hp;
mod once;
mod prio;
mod replay;
mod secrets;
pub mod selfencrypt;
mod ssl;
mod time;

#[cfg(not(feature = "fuzzing"))]
pub use self::aead::Aead;

#[cfg(feature = "fuzzing")]
pub use self::aead_fuzzing::Aead;

#[cfg(feature = "fuzzing")]
pub use self::aead_fuzzing::FIXED_TAG_FUZZING;

pub use self::agent::{
    Agent, AllowZeroRtt, Client, HandshakeState, Record, RecordList, ResumptionToken, SecretAgent,
    SecretAgentInfo, SecretAgentPreInfo, Server, ZeroRttCheckResult, ZeroRttChecker,
};
pub use self::auth::AuthenticationStatus;
pub use self::constants::*;
pub use self::err::{Error, PRErrorCode, Res};
pub use self::ext::{ExtensionHandler, ExtensionHandlerResult, ExtensionWriterResult};
pub use self::p11::{random, SymKey};
pub use self::replay::AntiReplay;
pub use self::secrets::SecretDirection;
pub use self::ssl::Opt;

use self::once::OnceResult;

use std::ffi::CString;
use std::path::{Path, PathBuf};
use std::ptr::null;

mod nss {
    #![allow(
        non_upper_case_globals,
        clippy::redundant_static_lifetimes,
        clippy::upper_case_acronyms
    )]
    #![allow(unknown_lints, renamed_and_removed_lints, clippy::unknown_clippy_lints)] // Until we require rust 1.51.
    include!(concat!(env!("OUT_DIR"), "/nss_init.rs"));
}

// Need to map the types through.
fn secstatus_to_res(code: nss::SECStatus) -> Res<()> {
    crate::err::secstatus_to_res(code as crate::ssl::SECStatus)
}

enum NssLoaded {
    External,
    NoDb,
    Db(Box<Path>),
}

impl Drop for NssLoaded {
    fn drop(&mut self) {
        match self {
            Self::NoDb | Self::Db(_) => unsafe {
                secstatus_to_res(nss::NSS_Shutdown()).expect("NSS Shutdown failed")
            },
            _ => {}
        }
    }
}

static mut INITIALIZED: OnceResult<NssLoaded> = OnceResult::new();

fn already_initialized() -> bool {
    unsafe { nss::NSS_IsInitialized() != 0 }
}

/// Initialize NSS.  This only executes the initialization routines once, so if there is any chance that
pub fn init() {
    // Set time zero.
    time::init();
    unsafe {
        INITIALIZED.call_once(|| {
            if already_initialized() {
                return NssLoaded::External;
            }

            secstatus_to_res(nss::NSS_NoDB_Init(null())).expect("NSS_NoDB_Init failed");
            secstatus_to_res(nss::NSS_SetDomesticPolicy()).expect("NSS_SetDomesticPolicy failed");

            NssLoaded::NoDb
        });
    }
}

/// This enables SSLTRACE by calling a simple, harmless function to trigger its
/// side effects.  SSLTRACE is not enabled in NSS until a socket is made or
/// global options are accessed.  Reading an option is the least impact approach.
/// This allows us to use SSLTRACE in all of our unit tests and programs.
#[cfg(debug_assertions)]
fn enable_ssl_trace() {
    let opt = ssl::Opt::Locking.as_int();
    let mut _v: ::std::os::raw::c_int = 0;
    secstatus_to_res(unsafe { ssl::SSL_OptionGetDefault(opt, &mut _v) })
        .expect("SSL_OptionGetDefault failed");
}

/// Initialize with a database.
/// # Panics
/// If NSS cannot be initialized.
pub fn init_db<P: Into<PathBuf>>(dir: P) {
    time::init();
    unsafe {
        INITIALIZED.call_once(|| {
            if already_initialized() {
                return NssLoaded::External;
            }

            let path = dir.into();
            assert!(path.is_dir());
            let pathstr = path.to_str().expect("path converts to string").to_string();
            let dircstr = CString::new(pathstr).expect("new CString");
            let empty = CString::new("").expect("new empty CString");
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
        });
    }
}

/// # Panics
/// If NSS isn't initialized.
pub fn assert_initialized() {
    unsafe {
        INITIALIZED.call_once(|| {
            panic!("NSS not initialized with init or init_db");
        });
    }
}
