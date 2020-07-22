/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

use std::ffi::{CStr, OsString};
use std::os::raw::c_char;

#[macro_use]
extern crate serde_derive;
#[macro_use]
extern crate log;
use winapi::shared::ntdef::HRESULT;
use winapi::shared::winerror::{HRESULT_FROM_WIN32, S_OK};
use wio::wide::FromWide;

mod viaduct_wininet;
use viaduct_wininet::WinInetBackend;

// HRESULT with 0x80000000 is an error, 0x20000000 set is a customer error code.
#[allow(overflowing_literals)]
const HR_NETWORK_ERROR: HRESULT = 0x80000000 | 0x20000000 | 0x1;
#[allow(overflowing_literals)]
const HR_SETTINGS_ERROR: HRESULT = 0x80000000 | 0x20000000 | 0x2;

#[derive(Debug, Deserialize)]
pub struct EnabledRecord {
    // Unknown fields are ignored by serde: see the docs for `#[serde(deny_unknown_fields)]`.
    pub(crate) enabled: bool,
}

pub enum Error {
    /// A backend error with an attached Windows error code from `GetLastError()`.
    WindowsError(u32),

    /// A network or otherwise transient error.
    NetworkError,

    /// A configuration or settings data error that probably requires code, configuration, or
    /// server-side changes to address.
    SettingsError,
}

impl From<viaduct::UnexpectedStatus> for Error {
    fn from(_err: viaduct::UnexpectedStatus) -> Self {
        Error::NetworkError
    }
}

impl From<viaduct::Error> for Error {
    fn from(err: viaduct::Error) -> Self {
        match err {
            viaduct::Error::NetworkError(_) => Error::NetworkError,
            viaduct::Error::BackendError(raw) => {
                // If we have a string that's a hex error code like
                // "0xabcde", that's a Windows error.
                if raw.starts_with("0x") {
                    let without_prefix = raw.trim_start_matches("0x");
                    let parse_result = u32::from_str_radix(without_prefix, 16);
                    if let Ok(parsed) = parse_result {
                        return Error::WindowsError(parsed);
                    }
                }
                Error::SettingsError
            }
            _ => Error::SettingsError,
        }
    }
}

impl From<serde_json::Error> for Error {
    fn from(_err: serde_json::Error) -> Self {
        Error::SettingsError
    }
}

impl From<url::ParseError> for Error {
    fn from(_err: url::ParseError) -> Self {
        Error::SettingsError
    }
}

fn is_agent_remote_disabled<S: AsRef<str>>(url: S) -> Result<bool, Error> {
    // Be careful setting the viaduct backend twice.  If the backend
    // has been set already, assume that it's our backend: we may as
    // well at least try to continue.
    match viaduct::set_backend(&WinInetBackend) {
        Ok(_) => {}
        Err(viaduct::Error::SetBackendError) => {}
        e => e?,
    }

    let url = url::Url::parse(url.as_ref())?;
    let req = viaduct::Request::new(viaduct::Method::Get, url);
    let resp = req.send()?;

    let resp = resp.require_success()?;

    let body: serde_json::Value = resp.json()?;
    let data = body.get("data").ok_or(Error::SettingsError)?;
    let record: EnabledRecord = serde_json::from_value(data.clone())?;

    let disabled = !record.enabled;
    Ok(disabled)
}

// This is an easy way to consume `MOZ_APP_DISPLAYNAME` from Rust code.
extern "C" {
    #[no_mangle]
    static gWinEventLogSourceName: *const u16;
}

#[allow(dead_code)]
#[no_mangle]
extern "C" fn IsAgentRemoteDisabledRust(szUrl: *const c_char, lpdwDisabled: *mut u32) -> HRESULT {
    let wineventlog_name = unsafe { OsString::from_wide_ptr_null(gWinEventLogSourceName) };
    let logger = wineventlog::EventLogger::new(&wineventlog_name);
    // It's fine to initialize logging twice.
    let _ = log::set_boxed_logger(Box::new(logger));
    log::set_max_level(log::LevelFilter::Info);

    // Use an IIFE for `?`.
    let disabled_result = (|| {
        if lpdwDisabled.is_null() {
            return Err(Error::SettingsError);
        }

        let url = unsafe { CStr::from_ptr(szUrl).to_str().map(|x| x.to_string()) }
            .map_err(|_| Error::SettingsError)?;

        info!("Using remote settings URL: {}", url);

        is_agent_remote_disabled(url)
    })();

    match disabled_result {
        Err(e) => {
            return match e {
                Error::WindowsError(errno) => {
                    let hr = HRESULT_FROM_WIN32(errno);
                    error!("Error::WindowsError({}) (HRESULT: 0x{:x})", errno, hr);
                    hr
                }
                Error::NetworkError => {
                    let hr = HR_NETWORK_ERROR;
                    error!("Error::NetworkError (HRESULT: 0x{:x})", hr);
                    hr
                }
                Error::SettingsError => {
                    let hr = HR_SETTINGS_ERROR;
                    error!("Error::SettingsError (HRESULT: 0x{:x})", hr);
                    hr
                }
            };
        }

        Ok(remote_disabled) => {
            // We null-checked `lpdwDisabled` earlier, but just to be safe.
            if !lpdwDisabled.is_null() {
                unsafe { *lpdwDisabled = if remote_disabled { 1 } else { 0 } };
            }
            return S_OK;
        }
    }
}
