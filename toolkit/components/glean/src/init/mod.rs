// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::env;
use std::ffi::CString;
use std::ops::DerefMut;
use std::path::PathBuf;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsACString, nsCString, nsString};
use xpcom::interfaces::{mozIViaduct, nsIFile, nsIXULAppInfo};
use xpcom::XpCom;

use glean::{ClientInfoMetrics, Configuration};

mod upload_pref;
mod user_activity;
mod viaduct_uploader;

use upload_pref::UploadPrefObserver;
use user_activity::UserActivityObserver;
use viaduct_uploader::ViaductUploader;

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

    if let Err(e) = UploadPrefObserver::begin_observing() {
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
        uploader: Some(Box::new(ViaductUploader) as Box<dyn glean::net::PingUploader>),
        use_core_mps: true,
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

    let pref_service = xpcom::services::get_PrefService().ok_or(NS_ERROR_FAILURE)?;
    let branch = xpcom::getter_addrefs(|p| {
        // Safe because:
        //  * `null` is explicitly allowed per documentation
        //  * `p` is a valid outparam guaranteed by `getter_addrefs`
        unsafe { pref_service.GetDefaultBranch(std::ptr::null(), p) }
    })?;
    let pref_name = CString::new("app.update.channel").map_err(|_| NS_ERROR_FAILURE)?;
    let mut channel = nsCString::new();
    // Safe because:
    //  * `branch` is non-null (otherwise `getter_addrefs` would've been `Err`
    //  * `pref_name` exists so a pointer to it is valid for the life of the function
    //  * `channel` exists so a pointer to it is valid, and it can be written to
    unsafe {
        if (*branch)
            .GetCharPref(pref_name.as_ptr(), channel.deref_mut() as *mut nsACString)
            .to_result()
            .is_err()
        {
            channel = "unknown".into();
        }
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
