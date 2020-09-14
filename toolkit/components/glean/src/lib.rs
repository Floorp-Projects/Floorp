// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Firefox on Glean (FOG) is the name of the layer that integrates the [Glean SDK][glean-sdk] into Firefox Desktop.
//! It is currently being designed and implemented.
//!
//! The [Glean SDK][glean-sdk] is a data collection library built by Mozilla for use in its products.
//! Like [Telemetry][telemetry], it can be used to
//! (in accordance with our [Privacy Policy][privacy-policy])
//! send anonymous usage statistics to Mozilla in order to make better decisions.
//!
//! Documentation can be found online in the [Firefox Source Docs][docs].
//!
//! [glean-sdk]: https://github.com/mozilla/glean/
//! [book-of-glean]: https://mozilla.github.io/glean/book/index.html
//! [privacy-policy]: https://www.mozilla.org/privacy/
//! [docs]: https://firefox-source-docs.mozilla.org/toolkit/components/glean/

// No one is currently using the Glean SDK, so let's export it, so we know it gets
// compiled.
pub extern crate fog;

#[macro_use]
extern crate cstr;
#[macro_use]
extern crate xpcom;

use std::ffi::CStr;
use std::os::raw::c_char;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_NO_CONTENT, NS_OK};
use nsstring::{nsACString, nsAString, nsCStr, nsCString, nsStr, nsString};
use xpcom::interfaces::{
    mozIViaduct, nsIFile, nsIObserver, nsIPrefBranch, nsIPropertyBag2, nsISupports, nsIXULAppInfo,
};
use xpcom::{RefPtr, XpCom};

use client_info::ClientInfo;
use glean_core::Configuration;

mod api;
mod client_info;
mod core_metrics;

/// Project FOG's entry point.
///
/// This assembles client information and the Glean configuration and then initializes the global
/// Glean instance.
#[no_mangle]
pub unsafe extern "C" fn fog_init() -> nsresult {
    log::debug!("Initializing FOG.");

    let data_path = match get_data_path() {
        Ok(dp) => dp,
        Err(e) => return e,
    };

    let (app_build, app_display_version, channel) = match get_app_info() {
        Ok(ai) => ai,
        Err(e) => return e,
    };

    let (os_version, architecture) = match get_system_info() {
        Ok(si) => si,
        Err(e) => return e,
    };

    let client_info = ClientInfo {
        app_build,
        app_display_version,
        channel: Some(channel),
        os_version,
        architecture,
    };
    log::debug!("Client Info: {:#?}", client_info);

    let pref_observer = UploadPrefObserver::allocate(InitUploadPrefObserver {});
    if let Err(e) = pref_observer.begin_observing() {
        log::error!(
            "Could not observe data upload pref. Abandoning FOG init due to {:?}",
            e
        );
        return e;
    }

    let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
    let data_path = data_path.to_string();
    let configuration = Configuration {
        upload_enabled,
        data_path,
        application_id: "firefox.desktop".to_string(),
        max_events: None,
        delay_ping_lifetime_io: false,
        language_binding_name: "Rust".into(),
    };

    log::debug!("Configuration: {:#?}", configuration);

    // Ensure Viaduct is initialized for networking unconditionally so we don't
    // need to check again if upload is later enabled.
    if let Some(viaduct) =
        xpcom::create_instance::<mozIViaduct>(cstr!("@mozilla.org/toolkit/viaduct;1"))
    {
        let result = viaduct.EnsureInitialized();
        if result.failed() {
            log::error!("Failed to ensure viaduct was initialized due to {}. Ping upload may not be available.", result.error_name());
        }
    } else {
        log::error!("Failed to create Viaduct via XPCOM. Ping upload may not be available.");
    }

    if configuration.data_path.len() > 0 {
        std::thread::spawn(move || {
            if let Err(e) = api::initialize(configuration, client_info) {
                log::error!("Failed to init FOG due to {:?}", e);
            }

            if let Err(e) = fog::flush_init() {
                log::error!("Failed to flush pre-init buffer due to {:?}", e);
            }
        });
    }

    NS_OK
}

/// Construct and return the data_path from the profile dir, or return an error.
fn get_data_path() -> Result<String, nsresult> {
    let dir_svc = match xpcom::services::get_DirectoryService() {
        Some(ds) => ds,
        _ => return Err(NS_ERROR_FAILURE),
    };
    let mut profile_dir = xpcom::GetterAddrefs::<nsIFile>::new();
    unsafe {
        dir_svc
            .Get(
                cstr!("ProfD").as_ptr(),
                &nsIFile::IID,
                profile_dir.void_ptr(),
            )
            .to_result()?;
    }
    let profile_dir = profile_dir.refptr().ok_or(NS_ERROR_FAILURE)?;
    let mut profile_path = nsString::new();
    unsafe {
        (*profile_dir).GetPath(&mut *profile_path).to_result()?;
    }
    let profile_path = String::from_utf16(&profile_path[..]).map_err(|_| NS_ERROR_FAILURE)?;
    let data_path = profile_path + "/datareporting/glean";
    Ok(data_path)
}

/// Return a tuple of the build_id, app version, and build channel.
/// If the XUL Runtime isn't a XULAppInfo (e.g. in xpcshell),
/// build_id ad app_version will be "unknown".
/// Other problems result in an error being returned instead.
fn get_app_info() -> Result<(String, String, String), nsresult> {
    let xul = xpcom::services::get_XULRuntime().ok_or(NS_ERROR_FAILURE)?;

    let mut channel = nsCString::new();
    unsafe {
        xul.GetDefaultUpdateChannel(&mut *channel).to_result()?;
    }

    let app_info = match xul.query_interface::<nsIXULAppInfo>() {
        Some(ai) => ai,
        // In e.g. xpcshell the XULRuntime isn't XULAppInfo.
        // We still want to return sensible values so tests don't explode.
        _ => {
            return Ok((
                "unknown".to_owned(),
                "unknown".to_owned(),
                channel.to_string(),
            ))
        }
    };

    let mut build_id = nsCString::new();
    unsafe {
        app_info.GetAppBuildID(&mut *build_id).to_result()?;
    }

    let mut version = nsCString::new();
    unsafe {
        app_info.GetVersion(&mut *version).to_result()?;
    }

    Ok((
        build_id.to_string(),
        version.to_string(),
        channel.to_string(),
    ))
}

/// Return a tuple of os_version and architecture, or an error.
fn get_system_info() -> Result<(String, String), nsresult> {
    let info_service = xpcom::get_service::<nsIPropertyBag2>(cstr!("@mozilla.org/system-info;1"))
        .ok_or(NS_ERROR_FAILURE)?;

    let os_version_key: Vec<u16> = "version".encode_utf16().collect();
    let os_version_key = &nsStr::from(&os_version_key) as &nsAString;
    let mut os_version = nsCString::new();
    unsafe {
        info_service
            .GetPropertyAsACString(os_version_key, &mut *os_version)
            .to_result()?;
    }

    let arch_key: Vec<u16> = "arch".encode_utf16().collect();
    let arch_key = &nsStr::from(&arch_key) as &nsAString;
    let mut arch = nsCString::new();
    unsafe {
        info_service
            .GetPropertyAsACString(arch_key, &mut *arch)
            .to_result()?;
    }

    Ok((os_version.to_string(), arch.to_string()))
}

// Partially cargo-culted from https://searchfox.org/mozilla-central/rev/598e50d2c3cd81cd616654f16af811adceb08f9f/security/manager/ssl/cert_storage/src/lib.rs#1192
#[derive(xpcom)]
#[xpimplements(nsIObserver)]
#[refcnt = "atomic"]
struct InitUploadPrefObserver {}

#[allow(non_snake_case)]
impl UploadPrefObserver {
    unsafe fn begin_observing(&self) -> Result<(), nsresult> {
        let pref_service = xpcom::services::get_PrefService().ok_or(NS_ERROR_FAILURE)?;
        let pref_branch: RefPtr<nsIPrefBranch> =
            (*pref_service).query_interface().ok_or(NS_ERROR_FAILURE)?;
        let pref_nscstr = &nsCStr::from("datareporting.healthreport.uploadEnabled") as &nsACString;
        (*pref_branch)
            .AddObserverImpl(pref_nscstr, self.coerce::<nsIObserver>(), false)
            .to_result()?;
        Ok(())
    }

    unsafe fn Observe(
        &self,
        _subject: *const nsISupports,
        topic: *const c_char,
        pref_name: *const i16,
    ) -> nserror::nsresult {
        let topic = CStr::from_ptr(topic).to_str().unwrap();
        // Conversion utf16 to utf8 is messy.
        // We should only ever observe changes to the one pref we want,
        // but just to be on the safe side let's assert.

        // cargo-culted from https://searchfox.org/mozilla-central/rev/598e50d2c3cd81cd616654f16af811adceb08f9f/security/manager/ssl/cert_storage/src/lib.rs#1606-1612
        // (with a little transformation)
        let len = (0..).take_while(|&i| *pref_name.offset(i) != 0).count(); // find NUL.
        let slice = std::slice::from_raw_parts(pref_name as *const u16, len);
        let pref_name = match String::from_utf16(slice) {
            Ok(name) => name,
            Err(_) => return NS_ERROR_FAILURE,
        };
        log::info!("Observed {:?}, {:?}", topic, pref_name);
        debug_assert!(
            topic == "nsPref:changed" && pref_name == "datareporting.healthreport.uploadEnabled"
        );

        let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
        api::set_upload_enabled(upload_enabled);
        NS_OK
    }
}

static mut PENDING_BUF: Vec<u8> = Vec::new();

// IPC serialization/deserialization methods
// Crucially important that the first two not be called on multiple threads.

/// Only safe if only called on a single thread (the same single thread you call
/// fog_give_ipc_buf on).
#[no_mangle]
pub unsafe extern "C" fn fog_serialize_ipc_buf() -> usize {
    if let Some(buf) = fog::ipc::take_buf() {
        PENDING_BUF = buf;
        PENDING_BUF.len()
    } else {
        PENDING_BUF = vec![];
        0
    }
}

#[no_mangle]
/// Only safe if called on a single thread (the same single thread you call
/// fog_serialize_ipc_buf on), and if buf points to an allocated buffer of at
/// least buf_len bytes.
pub unsafe extern "C" fn fog_give_ipc_buf(buf: *mut u8, buf_len: usize) -> usize {
    let pending_len = PENDING_BUF.len();
    if buf.is_null() || buf_len < pending_len {
        return 0;
    }
    std::ptr::copy_nonoverlapping(PENDING_BUF.as_ptr(), buf, pending_len);
    PENDING_BUF = Vec::new();
    pending_len
}

#[no_mangle]
/// Only safe if buf points to an allocated buffer of at least buf_len bytes.
/// No ownership is transfered to Rust by this method: caller owns the memory at
/// buf before and after this call.
pub unsafe extern "C" fn fog_use_ipc_buf(buf: *const u8, buf_len: usize) {
    let slice = std::slice::from_raw_parts(buf, buf_len);
    let _res = fog::ipc::replay_from_buf(slice);
    /*if res.is_err() {
        // TODO: Record the error.
    }*/
}

#[no_mangle]
/// Sets the debug tag for pings assembled in the future.
/// Returns an error result if the provided value is not a valid tag.
pub unsafe extern "C" fn fog_set_debug_view_tag(value: &nsACString) -> nsresult {
    let result = api::set_debug_view_tag(&value.to_string());
    if result {
        return NS_OK;
    } else {
        return NS_ERROR_FAILURE;
    }
}

#[no_mangle]
/// Submits a ping by name.
/// Returns NS_OK if the ping was successfully submitted, NS_ERROR_NO_CONTENT
/// if the ping wasn't sent, or NS_ERROR_FAILURE if some part of the ping
/// submission mechanism failed.
pub unsafe extern "C" fn fog_submit_ping(ping_name: &nsACString) -> nsresult {
    match api::submit_ping(&ping_name.to_string()) {
        Ok(true) => NS_OK,
        Ok(false) => NS_ERROR_NO_CONTENT,
        _ => NS_ERROR_FAILURE,
    }
}

#[no_mangle]
/// Turns ping logging on or off.
/// Returns an error if the logging failed to be configured.
pub unsafe extern "C" fn fog_set_log_pings(value: bool) -> nsresult {
    if api::set_log_pings(value) {
        return NS_OK;
    } else {
        return NS_ERROR_FAILURE;
    }
}
