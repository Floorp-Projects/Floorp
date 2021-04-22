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

// Skip everything on Android.
//
// FOG is disabled on Android until we enable it in GeckoView.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1670261.
//
// This also ensures no one will actually be able to use the Rust API,
// because that will fail the build.
#![cfg(not(target_os = "android"))]

// No one is currently using the Glean SDK, so let's export it, so we know it gets
// compiled.
pub extern crate fog;

#[macro_use]
extern crate cstr;
#[macro_use]
extern crate xpcom;

use std::env;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::path::PathBuf;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsACString, nsCStr, nsCString, nsString};
use xpcom::interfaces::{
    mozIViaduct, nsIFile, nsIObserver, nsIPrefBranch, nsISupports, nsIXULAppInfo,
};
use xpcom::{RefPtr, XpCom};

use glean::{ClientInfoMetrics, Configuration};

mod user_activity;
mod viaduct_uploader;

use crate::user_activity::UserActivityObserver;
use crate::viaduct_uploader::ViaductUploader;

/// Project FOG's entry point.
///
/// This assembles client information and the Glean configuration and then initializes the global
/// Glean instance.
#[no_mangle]
pub unsafe extern "C" fn fog_init(
    data_path_override: &nsACString,
    app_id_override: &nsACString,
) -> nsresult {
    fog::metrics::fog::initialization.start();

    log::debug!("Initializing FOG.");

    let data_path_str = if data_path_override.is_empty() {
        match get_data_path() {
            Ok(dp) => dp,
            Err(e) => return e,
        }
    } else {
        data_path_override.to_utf8().to_string()
    };
    let data_path = PathBuf::from(&data_path_str);

    let (app_build, app_display_version, channel) = match get_app_info() {
        Ok(ai) => ai,
        Err(e) => return e,
    };

    let client_info = ClientInfoMetrics {
        app_build,
        app_display_version,
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

    if let Err(e) = UserActivityObserver::begin_observing() {
        log::error!(
            "Could not observe user activity. Abandoning FOG init due to {:?}",
            e
        );
        return e;
    }

    const SERVER: &str = "https://incoming.telemetry.mozilla.org";
    let localhost_port = static_prefs::pref!("telemetry.fog.test.localhost_port");
    let server = if localhost_port > 0 {
        format!("http://localhost:{}", localhost_port)
    } else {
        String::from(SERVER)
    };

    let application_id = if app_id_override.is_empty() {
        "firefox.desktop".to_string()
    } else {
        app_id_override.to_utf8().to_string()
    };

    let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
    let configuration = Configuration {
        upload_enabled,
        data_path,
        application_id,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some(channel),
        server_endpoint: Some(server),
        uploader: Some(Box::new(crate::ViaductUploader) as Box<dyn glean::net::PingUploader>),
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

    // If we're operating in automation without any specific source tags to set,
    // set the tag "automation" so any pings that escape don't clutter the tables.
    // See https://mozilla.github.io/glean/book/user/debugging/index.html#enabling-debugging-features-through-environment-variables
    // IMPORTANT: Call this before glean::initialize until bug 1706729 is sorted.
    if env::var("MOZ_AUTOMATION").is_ok() && env::var("GLEAN_SOURCE_TAGS").is_err() {
        glean::set_source_tags(vec!["automation".to_string()]);
    }

    glean::initialize(configuration, client_info);

    // Register all custom pings before we initialize.
    fog::pings::register_pings();

    fog::metrics::fog::initialization.stop();

    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn fog_shutdown() {
    glean::shutdown();
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
        glean::set_upload_enabled(upload_enabled);
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
    let res = fog::ipc::replay_from_buf(slice);
    if res.is_err() {
        log::warn!("Unable to replay ipc buffer. This will result in data loss.");
        fog::metrics::fog_ipc::replay_failures.add(1);
    }
}

#[no_mangle]
/// Sets the debug tag for pings assembled in the future.
/// Returns an error result if the provided value is not a valid tag.
pub unsafe extern "C" fn fog_set_debug_view_tag(value: &nsACString) -> nsresult {
    let result = glean::set_debug_view_tag(&value.to_string());
    if result {
        return NS_OK;
    } else {
        return NS_ERROR_FAILURE;
    }
}

#[no_mangle]
/// Submits a ping by name.
pub unsafe extern "C" fn fog_submit_ping(ping_name: &nsACString) -> nsresult {
    glean::submit_ping_by_name(&ping_name.to_string(), None);
    NS_OK
}

#[no_mangle]
/// Turns ping logging on or off.
/// Returns an error if the logging failed to be configured.
pub unsafe extern "C" fn fog_set_log_pings(value: bool) -> nsresult {
    glean::set_log_pings(value);
    NS_OK
}
