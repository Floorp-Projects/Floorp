// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)] // This lint doesn't work here.
#![allow(clippy::unseparated_literal_suffix, clippy::used_underscore_binding)] // For bindgen code.

mod aead;
#[cfg(feature = "fuzzing")]
mod aead_fuzzing;
pub mod agent;
mod agentio;
mod auth;
mod cert;
pub mod constants;
mod ech;
mod err;
#[macro_use]
mod exp;
pub mod ext;
pub mod hkdf;
pub mod hp;
#[macro_use]
mod p11;
mod prio;
mod replay;
mod secrets;
pub mod selfencrypt;
mod ssl;
mod time;

use std::{ffi::CString, path::PathBuf, ptr::null, sync::OnceLock};

#[cfg(not(feature = "fuzzing"))]
pub use self::aead::RealAead as Aead;
#[cfg(feature = "fuzzing")]
pub use self::aead::RealAead;
#[cfg(feature = "fuzzing")]
pub use self::aead_fuzzing::FuzzingAead as Aead;
pub use self::{
    agent::{
        Agent, AllowZeroRtt, Client, HandshakeState, Record, RecordList, ResumptionToken,
        SecretAgent, SecretAgentInfo, SecretAgentPreInfo, Server, ZeroRttCheckResult,
        ZeroRttChecker,
    },
    auth::AuthenticationStatus,
    constants::*,
    ech::{
        encode_config as encode_ech_config, generate_keys as generate_ech_keys, AeadId, KdfId,
        KemId, SymmetricSuite,
    },
    err::{Error, PRErrorCode, Res},
    ext::{ExtensionHandler, ExtensionHandlerResult, ExtensionWriterResult},
    p11::{random, randomize, PrivateKey, PublicKey, SymKey},
    replay::AntiReplay,
    secrets::SecretDirection,
    ssl::Opt,
};

const MINIMUM_NSS_VERSION: &str = "3.97";

#[allow(non_upper_case_globals, clippy::redundant_static_lifetimes)]
#[allow(clippy::upper_case_acronyms)]
#[allow(unknown_lints, clippy::borrow_as_ptr)]
mod nss {
    include!(concat!(env!("OUT_DIR"), "/nss_init.rs"));
}

// Need to map the types through.
fn secstatus_to_res(code: nss::SECStatus) -> Res<()> {
    crate::err::secstatus_to_res(code as crate::ssl::SECStatus)
}

enum NssLoaded {
    External,
    NoDb,
    Db,
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

static INITIALIZED: OnceLock<NssLoaded> = OnceLock::new();

fn already_initialized() -> bool {
    unsafe { nss::NSS_IsInitialized() != 0 }
}

fn version_check() {
    let min_ver = CString::new(MINIMUM_NSS_VERSION).unwrap();
    assert_ne!(
        unsafe { nss::NSS_VersionCheck(min_ver.as_ptr()) },
        0,
        "Minimum NSS version of {MINIMUM_NSS_VERSION} not supported",
    );
}

/// Initialize NSS.  This only executes the initialization routines once, so if there is any chance
/// that
///
/// # Panics
///
/// When NSS initialization fails.
pub fn init() {
    // Set time zero.
    time::init();
    _ = INITIALIZED.get_or_init(|| {
        version_check();
        if already_initialized() {
            return NssLoaded::External;
        }

        secstatus_to_res(unsafe { nss::NSS_NoDB_Init(null()) }).expect("NSS_NoDB_Init failed");
        secstatus_to_res(unsafe { nss::NSS_SetDomesticPolicy() })
            .expect("NSS_SetDomesticPolicy failed");

        NssLoaded::NoDb
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
///
/// # Panics
///
/// If NSS cannot be initialized.
pub fn init_db<P: Into<PathBuf>>(dir: P) {
    time::init();
    _ = INITIALIZED.get_or_init(|| {
        version_check();
        if already_initialized() {
            return NssLoaded::External;
        }

        let path = dir.into();
        assert!(path.is_dir());
        let pathstr = path.to_str().expect("path converts to string").to_string();
        let dircstr = CString::new(pathstr).unwrap();
        let empty = CString::new("").unwrap();
        secstatus_to_res(unsafe {
            nss::NSS_Initialize(
                dircstr.as_ptr(),
                empty.as_ptr(),
                empty.as_ptr(),
                nss::SECMOD_DB.as_ptr().cast(),
                nss::NSS_INIT_READONLY,
            )
        })
        .expect("NSS_Initialize failed");

        secstatus_to_res(unsafe { nss::NSS_SetDomesticPolicy() })
            .expect("NSS_SetDomesticPolicy failed");
        secstatus_to_res(unsafe {
            ssl::SSL_ConfigServerSessionIDCache(1024, 0, 0, dircstr.as_ptr())
        })
        .expect("SSL_ConfigServerSessionIDCache failed");

        #[cfg(debug_assertions)]
        enable_ssl_trace();

        NssLoaded::Db
    });
}

/// # Panics
///
/// If NSS isn't initialized.
pub fn assert_initialized() {
    INITIALIZED
        .get()
        .expect("NSS not initialized with init or init_db");
}

/// NSS tends to return empty "slices" with a null pointer, which will cause
/// `std::slice::from_raw_parts` to panic if passed directly.  This wrapper avoids
/// that issue.  It also performs conversion for lengths, as a convenience.
///
/// # Panics
/// If the provided length doesn't fit into a `usize`.
///
/// # Safety
/// The caller must adhere to the safety constraints of `std::slice::from_raw_parts`,
/// except that this will accept a null value for `data`.
unsafe fn null_safe_slice<'a, T>(data: *const u8, len: T) -> &'a [u8]
where
    usize: TryFrom<T>,
{
    if data.is_null() {
        &[]
    } else if let Ok(len) = usize::try_from(len) {
        #[allow(clippy::disallowed_methods)]
        std::slice::from_raw_parts(data, len)
    } else {
        panic!("null_safe_slice: size overflow");
    }
}
