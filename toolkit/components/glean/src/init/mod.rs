// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::env;
use std::ffi::CString;
use std::ops::DerefMut;
use std::path::PathBuf;

#[cfg(target_os = "android")]
use nserror::NS_ERROR_NOT_IMPLEMENTED;
use nserror::{nsresult, NS_ERROR_FAILURE};
use nsstring::{nsACString, nsCString, nsString};
#[cfg(not(target_os = "android"))]
use xpcom::interfaces::mozIViaduct;
use xpcom::interfaces::{nsIFile, nsIXULAppInfo};
use xpcom::XpCom;

use glean::{ClientInfoMetrics, Configuration};

#[cfg(not(target_os = "android"))]
mod upload_pref;
#[cfg(not(target_os = "android"))]
mod user_activity;
#[cfg(not(target_os = "android"))]
mod viaduct_uploader;

#[cfg(not(target_os = "android"))]
use upload_pref::UploadPrefObserver;
#[cfg(not(target_os = "android"))]
use user_activity::UserActivityObserver;
#[cfg(not(target_os = "android"))]
use viaduct_uploader::ViaductUploader;

/// Project FOG's entry point.
///
/// This assembles client information and the Glean configuration and then initializes the global
/// Glean instance.
#[cfg(not(target_os = "android"))]
#[no_mangle]
pub extern "C" fn fog_init(
    data_path_override: &nsACString,
    app_id_override: &nsACString,
) -> nsresult {
    let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
    let recording_enabled = static_prefs::pref!("telemetry.fog.test.localhost_port") < 0;
    let uploader = Some(Box::new(ViaductUploader) as Box<dyn glean::net::PingUploader>);

    fog_init_internal(
        data_path_override,
        app_id_override,
        upload_enabled || recording_enabled,
        uploader,
    )
    .into()
}

/// Project FOG's entry point on Android.
///
/// This assembles client information and the Glean configuration and then initializes the global
/// Glean instance.
/// It always enables upload and set no uploader.
/// This should only be called in test scenarios.
/// In normal use Glean should be initialized and controlled by the Glean Kotlin SDK.
#[cfg(target_os = "android")]
#[no_mangle]
pub extern "C" fn fog_init(
    data_path_override: &nsACString,
    app_id_override: &nsACString,
) -> nsresult {
    // On Android always enable Glean upload.
    let upload_enabled = true;
    // Don't set up an uploader.
    let uploader = None;

    fog_init_internal(
        data_path_override,
        app_id_override,
        upload_enabled,
        uploader,
    )
    .into()
}

fn fog_init_internal(
    data_path_override: &nsACString,
    app_id_override: &nsACString,
    upload_enabled: bool,
    uploader: Option<Box<dyn glean::net::PingUploader>>,
) -> Result<(), nsresult> {
    fog::metrics::fog::initialization.start();

    log::debug!("Initializing FOG.");

    setup_observers()?;

    let (mut conf, client_info) = build_configuration(data_path_override, app_id_override)?;

    conf.upload_enabled = upload_enabled;
    conf.uploader = uploader;

    setup_viaduct();

    // If we're operating in automation without any specific source tags to set,
    // set the tag "automation" so any pings that escape don't clutter the tables.
    // See https://mozilla.github.io/glean/book/user/debugging/index.html#enabling-debugging-features-through-environment-variables
    if env::var("MOZ_AUTOMATION").is_ok() && env::var("GLEAN_SOURCE_TAGS").is_err() {
        log::info!("In automation, setting 'automation' source tag.");
        glean::set_source_tags(vec!["automation".to_string()]);
        log::info!("In automation, disabling MPS to avoid 4am issues.");
        conf.use_core_mps = false;
    }

    log::debug!("Configuration: {:#?}", conf);

    // Register all custom pings before we initialize.
    fog::pings::register_pings();

    glean::initialize(conf, client_info);

    fog::metrics::fog::initialization.stop();

    Ok(())
}

fn build_configuration(
    data_path_override: &nsACString,
    app_id_override: &nsACString,
) -> Result<(Configuration, ClientInfoMetrics), nsresult> {
    let data_path_str = if data_path_override.is_empty() {
        get_data_path()?
    } else {
        data_path_override.to_utf8().to_string()
    };
    let data_path = PathBuf::from(&data_path_str);

    let (app_build, app_display_version, channel) = get_app_info()?;

    let client_info = ClientInfoMetrics {
        app_build,
        app_display_version,
        channel: Some(channel),
    };
    log::debug!("Client Info: {:#?}", client_info);

    const SERVER: &str = "https://incoming.telemetry.mozilla.org";
    let localhost_port = static_prefs::pref!("telemetry.fog.test.localhost_port");
    let server = if localhost_port > 0 {
        format!("http://localhost:{}", localhost_port)
    } else {
        String::from(SERVER)
    };

    // In the event that this isn't "firefox.desktop", we don't use core's MPS.
    let mut use_core_mps = false;
    let application_id = if app_id_override.is_empty() {
        use_core_mps = true;
        "firefox.desktop".to_string()
    } else {
        app_id_override.to_utf8().to_string()
    };

    let configuration = Configuration {
        upload_enabled: false,
        data_path,
        application_id,
        max_events: None,
        delay_ping_lifetime_io: true,
        server_endpoint: Some(server),
        uploader: None,
        use_core_mps,
    };

    Ok((configuration, client_info))
}

#[cfg(not(target_os = "android"))]
fn setup_observers() -> Result<(), nsresult> {
    if let Err(e) = UploadPrefObserver::begin_observing() {
        log::error!(
            "Could not observe data upload pref. Abandoning FOG init due to {:?}",
            e
        );
        return Err(e);
    }

    if let Err(e) = UserActivityObserver::begin_observing() {
        log::error!(
            "Could not observe user activity. Abandoning FOG init due to {:?}",
            e
        );
        return Err(e);
    }

    Ok(())
}

#[cfg(target_os = "android")]
fn setup_observers() -> Result<(), nsresult> {
    // No observers are set up on Android.
    Ok(())
}

/// Ensure Viaduct is initialized for networking unconditionally so we don't
/// need to check again if upload is later enabled.
///
/// Failing to initialize viaduct will log an error.
#[cfg(not(target_os = "android"))]
fn setup_viaduct() {
    // SAFETY: Everything here is self-contained.
    //
    // * We try to create an instance of an xpcom interface
    // * We bail out if that fails.
    // * We call a method on it without additional inputs.
    unsafe {
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
    }
}

#[cfg(target_os = "android")]
fn setup_viaduct() {
    // No viaduct is setup on Android.
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

/// **TEST-ONLY METHOD**
/// Resets FOG and the underlying Glean SDK, clearing stores.
#[cfg(not(target_os = "android"))]
#[no_mangle]
pub extern "C" fn fog_test_reset(
    data_path_override: &nsACString,
    app_id_override: &nsACString,
) -> nsresult {
    fog_test_reset_internal(data_path_override, app_id_override).into()
}

// Split out into its own function so I could use `?`
#[cfg(not(target_os = "android"))]
fn fog_test_reset_internal(
    data_path_override: &nsACString,
    app_id_override: &nsACString,
) -> Result<(), nsresult> {
    let (mut conf, client_info) = build_configuration(data_path_override, app_id_override)?;

    let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
    let recording_enabled = static_prefs::pref!("telemetry.fog.test.localhost_port") < 0;
    conf.upload_enabled = upload_enabled || recording_enabled;

    // Don't accidentally send "main" pings during tests.
    conf.use_core_mps = false;

    // I'd prefer to reuse the uploader, but it gets moved into Glean so we build anew.
    conf.uploader = Some(Box::new(ViaductUploader) as Box<dyn glean::net::PingUploader>);

    glean::test_reset_glean(conf, client_info, true);
    Ok(())
}

/// **TEST-ONLY METHOD**
/// Does nothing on Android. Returns NS_ERROR_NOT_IMPLEMENTED.
#[cfg(target_os = "android")]
#[no_mangle]
pub extern "C" fn fog_test_reset(
    _data_path_override: &nsACString,
    _app_id_override: &nsACString,
) -> nsresult {
    NS_ERROR_NOT_IMPLEMENTED
}
