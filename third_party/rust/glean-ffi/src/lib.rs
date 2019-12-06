// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::convert::TryFrom;
use std::os::raw::c_char;

use ffi_support::{define_string_destructor, ConcurrentHandleMap, FfiStr};
use lazy_static::lazy_static;

use glean_core::Glean;

mod macros;

mod boolean;
mod counter;
mod custom_distribution;
mod datetime;
mod event;
mod ffi_string_ext;
mod from_raw;
mod handlemap_ext;
mod labeled;
mod memory_distribution;
pub mod ping_type;
mod quantity;
mod string;
mod string_list;
mod timespan;
mod timing_distribution;
mod uuid;

use ffi_string_ext::FallibleToString;
use from_raw::*;
use handlemap_ext::HandleMapExtension;
use ping_type::PING_TYPES;

lazy_static! {
    static ref GLEAN: ConcurrentHandleMap<Glean> = ConcurrentHandleMap::new();
}

/// Initialize the logging system based on the target platform. This ensures
/// that logging is shown when executing the Glean SDK unit tests.
#[no_mangle]
pub extern "C" fn glean_enable_logging() {
    #[cfg(target_os = "android")]
    {
        let _ = std::panic::catch_unwind(|| {
            android_logger::init_once(
                android_logger::Config::default()
                    .with_min_level(log::Level::Debug)
                    .with_tag("libglean_ffi"),
            );
            log::debug!("Android logging should be hooked up!")
        });
    }

    // On iOS enable logging with a level filter.
    #[cfg(target_os = "ios")]
    {
        // Debug logging in debug mode.
        // (Note: `debug_assertions` is the next best thing to determine if this is a debug build)
        #[cfg(debug_assertions)]
        let level = log::LevelFilter::Debug;
        #[cfg(not(debug_assertions))]
        let level = log::LevelFilter::Info;

        let mut builder = env_logger::Builder::new();
        builder.filter(None, level);
        match builder.try_init() {
            Ok(_) => log::debug!("stdout logging should be hooked up!"),
            // Please note that this is only expected to fail during unit tests,
            // where the logger might have already been initialized by a previous
            // test. So it's fine to print with the "logger".
            Err(_) => log::debug!("stdout was already initialized"),
        };
    }

    // Make sure logging does something on non Android platforms as well. Use
    // the RUST_LOG environment variable to set the desired log level, e.g.
    // setting RUST_LOG=debug sets the log level to debug.
    #[cfg(all(not(target_os = "android"), not(target_os = "ios")))]
    {
        match env_logger::try_init() {
            Ok(_) => log::debug!("stdout logging should be hooked up!"),
            // Please note that this is only expected to fail during unit tests,
            // where the logger might have already been initialized by a previous
            // test. So it's fine to print with the "logger".
            Err(_) => log::debug!("stdout was already initialized"),
        };
    }
}

/// Configuration over FFI.
///
/// **CAUTION**: This must match _exactly_ the definition on the Kotlin side.
/// If this side is changed, the Kotlin side need to be changed, too.
#[repr(C)]
pub struct FfiConfiguration<'a> {
    pub data_dir: FfiStr<'a>,
    pub package_name: FfiStr<'a>,
    pub upload_enabled: u8,
    pub max_events: Option<&'a i32>,
    pub delay_ping_lifetime_io: u8,
}

/// Convert the FFI-compatible configuration object into the proper Rust configuration object.
impl TryFrom<&FfiConfiguration<'_>> for glean_core::Configuration {
    type Error = glean_core::Error;

    fn try_from(cfg: &FfiConfiguration) -> Result<Self, Self::Error> {
        let data_path = cfg.data_dir.to_string_fallible()?;
        let application_id = cfg.package_name.to_string_fallible()?;
        let upload_enabled = cfg.upload_enabled != 0;
        let max_events = cfg.max_events.filter(|&&i| i >= 0).map(|m| *m as usize);
        let delay_ping_lifetime_io = cfg.delay_ping_lifetime_io != 0;

        Ok(Self {
            upload_enabled,
            data_path,
            application_id,
            max_events,
            delay_ping_lifetime_io,
        })
    }
}

/// # Safety
///
/// A valid and non-null configuration object is required for this function.
#[no_mangle]
pub unsafe extern "C" fn glean_initialize(cfg: *const FfiConfiguration) -> u64 {
    assert!(!cfg.is_null());

    GLEAN.insert_with_log(|| {
        // We can create a reference to the FfiConfiguration struct:
        // 1. We did a null check
        // 2. We're not holding on to it beyond this function
        //    and we copy out all data when needed.
        let glean_cfg = glean_core::Configuration::try_from(&*cfg)?;
        let glean = Glean::new(glean_cfg)?;
        log::info!("Glean initialized");
        Ok(glean)
    })
}

/// # Safety
///
/// A valid and non-null configuration object is required for this function.
#[no_mangle]
pub unsafe extern "C" fn glean_initialize_migration(
    cfg: *const FfiConfiguration,
    seq_num_keys: RawStringArray,
    seq_num_values: RawIntArray,
    seq_num_len: i32,
) -> u64 {
    assert!(!cfg.is_null());

    GLEAN.insert_with_log(|| {
        // We can create a reference to the FfiConfiguration struct:
        // 1. We did a null check
        // 2. We're not holding on to it beyond this function
        //    and we copy out all data when needed.
        let glean_cfg = glean_core::Configuration::try_from(&*cfg)?;

        let seq_nums =
            from_raw_string_array_and_int_array(seq_num_keys, seq_num_values, seq_num_len);

        let glean = if let Ok(Some(seq_nums)) = seq_nums {
            Glean::with_sequence_numbers(glean_cfg, seq_nums)
        } else {
            Glean::new(glean_cfg)
        }?;

        log::info!("Glean initialized");
        Ok(glean)
    })
}

#[no_mangle]
pub extern "C" fn glean_on_ready_to_send_pings(glean_handle: u64) -> u8 {
    GLEAN.call_infallible(glean_handle, |glean| glean.on_ready_to_send_pings())
}

#[no_mangle]
pub extern "C" fn glean_is_upload_enabled(glean_handle: u64) -> u8 {
    GLEAN.call_infallible(glean_handle, |glean| glean.is_upload_enabled())
}

#[no_mangle]
pub extern "C" fn glean_set_upload_enabled(glean_handle: u64, flag: u8) {
    GLEAN.call_infallible_mut(glean_handle, |glean| glean.set_upload_enabled(flag != 0));
    // The return value of set_upload_enabled is an implementation detail
    // that isn't exposed over FFI.
}

#[no_mangle]
pub extern "C" fn glean_send_pings_by_name(
    glean_handle: u64,
    ping_names: RawStringArray,
    ping_names_len: i32,
) -> u8 {
    GLEAN.call_with_log(glean_handle, |glean| {
        let pings = from_raw_string_array(ping_names, ping_names_len)?;

        Ok(glean.send_pings_by_name(&pings))
    })
}

#[no_mangle]
pub extern "C" fn glean_ping_collect(glean_handle: u64, ping_type_handle: u64) -> *mut c_char {
    GLEAN.call_infallible(glean_handle, |glean| {
        PING_TYPES.call_infallible(ping_type_handle, |ping_type| {
            let ping_maker = glean_core::ping::PingMaker::new();
            let data = ping_maker
                .collect_string(glean, ping_type)
                .unwrap_or_else(|| String::from(""));
            log::info!("Ping({}): {}", ping_type.name.as_str(), data);
            data
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_set_experiment_active(
    glean_handle: u64,
    experiment_id: FfiStr,
    branch: FfiStr,
    extra_keys: RawStringArray,
    extra_values: RawStringArray,
    extra_len: i32,
) {
    GLEAN.call_with_log(glean_handle, |glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        let branch = branch.to_string_fallible()?;
        let extra = from_raw_string_array_and_string_array(extra_keys, extra_values, extra_len)?;

        glean.set_experiment_active(experiment_id, branch, extra);
        Ok(())
    })
}

#[no_mangle]
pub extern "C" fn glean_set_experiment_inactive(glean_handle: u64, experiment_id: FfiStr) {
    GLEAN.call_with_log(glean_handle, |glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        glean.set_experiment_inactive(experiment_id);
        Ok(())
    })
}

#[no_mangle]
pub extern "C" fn glean_experiment_test_is_active(glean_handle: u64, experiment_id: FfiStr) -> u8 {
    GLEAN.call_with_log(glean_handle, |glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        Ok(glean.test_is_experiment_active(experiment_id))
    })
}

#[no_mangle]
pub extern "C" fn glean_experiment_test_get_data(
    glean_handle: u64,
    experiment_id: FfiStr,
) -> *mut c_char {
    GLEAN.call_with_log(glean_handle, |glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        Ok(glean.test_get_experiment_data_as_json(experiment_id))
    })
}

#[no_mangle]
pub extern "C" fn glean_test_clear_all_stores(glean_handle: u64) {
    GLEAN.call_infallible(glean_handle, |glean| glean.test_clear_all_stores());
}

define_infallible_handle_map_deleter!(GLEAN, glean_destroy_glean);
define_string_destructor!(glean_str_free);
