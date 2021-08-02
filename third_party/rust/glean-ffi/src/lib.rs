// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![deny(broken_intra_doc_links)]

use std::convert::TryFrom;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::panic::UnwindSafe;
use std::path::PathBuf;

use ffi_support::{define_string_destructor, ConcurrentHandleMap, FfiStr, IntoFfi};

#[cfg(all(not(target_os = "android"), not(target_os = "ios")))]
use once_cell::sync::OnceCell;

pub use glean_core::metrics::MemoryUnit;
pub use glean_core::metrics::TimeUnit;
pub use glean_core::upload::ffi_upload_result::*;
use glean_core::Glean;
pub use glean_core::Lifetime;

mod macros;

mod boolean;
pub mod byte_buffer;
mod counter;
mod custom_distribution;
mod datetime;
mod event;
mod ffi_string_ext;
mod from_raw;
mod handlemap_ext;
mod jwe;
mod labeled;
mod memory_distribution;
pub mod ping_type;
mod quantity;
mod string;
mod string_list;
mod timespan;
mod timing_distribution;
pub mod upload;
mod uuid;

#[cfg(all(not(target_os = "android"), not(target_os = "ios")))]
mod fd_logger;

#[cfg(unix)]
#[macro_use]
mod weak;

use ffi_string_ext::FallibleToString;
use from_raw::*;
use handlemap_ext::HandleMapExtension;
use ping_type::PING_TYPES;
use upload::FfiPingUploadTask;

/// Execute the callback with a reference to the Glean singleton, returning a `Result`.
///
/// The callback returns a `Result<T, E>` while:
///
/// - Catching panics, and logging them.
/// - Converting `T` to a C-compatible type using [`IntoFfi`].
/// - Logging `E` and returning a default value.
pub(crate) fn with_glean<R, F>(callback: F) -> R::Value
where
    F: UnwindSafe + FnOnce(&Glean) -> Result<R, glean_core::Error>,
    R: IntoFfi,
{
    let mut error = ffi_support::ExternError::success();
    let res =
        ffi_support::abort_on_panic::call_with_result(
            &mut error,
            || match glean_core::global_glean() {
                Some(glean) => {
                    let glean = glean.lock().unwrap();
                    callback(&glean)
                }
                None => Err(glean_core::Error::not_initialized()),
            },
        );
    handlemap_ext::log_if_error(error);
    res
}

/// Execute the callback with a mutable reference to the Glean singleton, returning a `Result`.
///
/// The callback returns a `Result<T, E>` while:
///
/// - Catching panics, and logging them.
/// - Converting `T` to a C-compatible type using [`IntoFfi`].
/// - Logging `E` and returning a default value.
pub(crate) fn with_glean_mut<R, F>(callback: F) -> R::Value
where
    F: UnwindSafe + FnOnce(&mut Glean) -> Result<R, glean_core::Error>,
    R: IntoFfi,
{
    let mut error = ffi_support::ExternError::success();
    let res =
        ffi_support::abort_on_panic::call_with_result(
            &mut error,
            || match glean_core::global_glean() {
                Some(glean) => {
                    let mut glean = glean.lock().unwrap();
                    callback(&mut glean)
                }
                None => Err(glean_core::Error::not_initialized()),
            },
        );
    handlemap_ext::log_if_error(error);
    res
}

/// Execute the callback with a reference to the Glean singleton, returning a value.
///
/// The callback returns a value while:
///
/// - Catching panics, and logging them.
/// - Converting the returned value to a C-compatible type using [`IntoFfi`].
pub(crate) fn with_glean_value<R, F>(callback: F) -> R::Value
where
    F: UnwindSafe + FnOnce(&Glean) -> R,
    R: IntoFfi,
{
    with_glean(|glean| Ok(callback(glean)))
}

/// Execute the callback with a mutable reference to the Glean singleton, returning a value.
///
/// The callback returns a value while:
///
/// - Catching panics, and logging them.
/// - Converting the returned value to a C-compatible type using [`IntoFfi`].
pub(crate) fn with_glean_value_mut<R, F>(callback: F) -> R::Value
where
    F: UnwindSafe + FnOnce(&mut Glean) -> R,
    R: IntoFfi,
{
    with_glean_mut(|glean| Ok(callback(glean)))
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
            log::trace!("Android logging should be hooked up!")
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

        let logger = oslog::OsLogger::new("org.mozilla.glean").level_filter(level);

        match logger.init() {
            Ok(_) => log::trace!("os_log should be hooked up!"),
            // Please note that this is only expected to fail during unit tests,
            // where the logger might have already been initialized by a previous
            // test. So it's fine to print with the "logger".
            Err(_) => log::warn!("os_log was already initialized"),
        };
    }

    // Make sure logging does something on non Android platforms as well. Use
    // the RUST_LOG environment variable to set the desired log level, e.g.
    // setting RUST_LOG=debug sets the log level to debug.
    #[cfg(all(not(target_os = "android"), not(target_os = "ios")))]
    {
        match env_logger::try_init() {
            Ok(_) => log::trace!("stdout logging should be hooked up!"),
            // Please note that this is only expected to fail during unit tests,
            // where the logger might have already been initialized by a previous
            // test. So it's fine to print with the "logger".
            Err(_) => log::warn!("stdout logging was already initialized"),
        };
    }
}

#[cfg(all(not(target_os = "android"), not(target_os = "ios")))]
static FD_LOGGER: OnceCell<fd_logger::FdLogger> = OnceCell::new();

/// Initialize the logging system to send JSON messages to a file descriptor
/// (Unix) or file handle (Windows).
///
/// Not available on Android and iOS.
///
/// `fd` is a writable file descriptor (on Unix) or file handle (on Windows).
///
/// # Safety
/// Unsafe because the fd u64 passed in will be interpreted as either a file
/// descriptor (Unix) or file handle (Windows) without any checking.
#[cfg(all(not(target_os = "android"), not(target_os = "ios")))]
#[no_mangle]
pub unsafe extern "C" fn glean_enable_logging_to_fd(fd: u64) {
    // Set up logging to a file descriptor/handle. For this usage, the
    // language binding should setup a pipe and pass in the descriptor to
    // the writing side of the pipe as the `fd` parameter. Log messages are
    // written as JSON to the file descriptor.
    if FD_LOGGER.set(fd_logger::FdLogger::new(fd)).is_ok() {
        // Set the level so everything goes through to the language
        // binding side where it will be filtered by the language
        // binding's logging system.
        if log::set_logger(FD_LOGGER.get().unwrap()).is_ok() {
            log::set_max_level(log::LevelFilter::Debug);
        }
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
    pub language_binding_name: FfiStr<'a>,
    pub upload_enabled: u8,
    pub max_events: Option<&'a i32>,
    pub delay_ping_lifetime_io: u8,
}

/// Convert the FFI-compatible configuration object into the proper Rust configuration object.
impl TryFrom<&FfiConfiguration<'_>> for glean_core::Configuration {
    type Error = glean_core::Error;

    fn try_from(cfg: &FfiConfiguration) -> Result<Self, Self::Error> {
        let data_path = cfg.data_dir.to_string_fallible()?;
        let data_path = PathBuf::from(data_path);
        let application_id = cfg.package_name.to_string_fallible()?;
        let language_binding_name = cfg.language_binding_name.to_string_fallible()?;
        let upload_enabled = cfg.upload_enabled != 0;
        let max_events = cfg.max_events.filter(|&&i| i >= 0).map(|m| *m as usize);
        let delay_ping_lifetime_io = cfg.delay_ping_lifetime_io != 0;
        let app_build = "unknown".to_string();
        let use_core_mps = false;

        Ok(Self {
            upload_enabled,
            data_path,
            application_id,
            language_binding_name,
            max_events,
            delay_ping_lifetime_io,
            app_build,
            use_core_mps,
        })
    }
}

/// # Safety
///
/// A valid and non-null configuration object is required for this function.
#[no_mangle]
pub unsafe extern "C" fn glean_initialize(cfg: *const FfiConfiguration) -> u8 {
    assert!(!cfg.is_null());

    handlemap_ext::handle_result(|| {
        // We can create a reference to the FfiConfiguration struct:
        // 1. We did a null check
        // 2. We're not holding on to it beyond this function
        //    and we copy out all data when needed.
        let glean_cfg = glean_core::Configuration::try_from(&*cfg)?;
        let glean = Glean::new(glean_cfg)?;
        glean_core::setup_glean(glean)?;
        log::info!("Glean initialized");
        Ok(true)
    })
}

#[no_mangle]
pub extern "C" fn glean_on_ready_to_submit_pings() -> u8 {
    with_glean_value(|glean| glean.on_ready_to_submit_pings())
}

#[no_mangle]
pub extern "C" fn glean_is_upload_enabled() -> u8 {
    with_glean_value(|glean| glean.is_upload_enabled())
}

#[no_mangle]
pub extern "C" fn glean_set_upload_enabled(flag: u8) {
    with_glean_value_mut(|glean| glean.set_upload_enabled(flag != 0));
    // The return value of set_upload_enabled is an implementation detail
    // that isn't exposed over FFI.
}

#[no_mangle]
pub extern "C" fn glean_submit_ping_by_name(ping_name: FfiStr, reason: FfiStr) -> u8 {
    with_glean(|glean| {
        Ok(glean.submit_ping_by_name(&ping_name.to_string_fallible()?, reason.as_opt_str()))
    })
}

#[no_mangle]
pub extern "C" fn glean_ping_collect(ping_type_handle: u64, reason: FfiStr) -> *mut c_char {
    with_glean_value(|glean| {
        PING_TYPES.call_infallible(ping_type_handle, |ping_type| {
            let ping_maker = glean_core::ping::PingMaker::new();
            let data = ping_maker
                .collect_string(glean, ping_type, reason.as_opt_str())
                .unwrap_or_else(|| String::from(""));
            log::info!("Ping({}): {}", ping_type.name.as_str(), data);
            data
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_set_experiment_active(
    experiment_id: FfiStr,
    branch: FfiStr,
    extra_keys: RawStringArray,
    extra_values: RawStringArray,
    extra_len: i32,
) {
    with_glean(|glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        let branch = branch.to_string_fallible()?;
        let extra = from_raw_string_array_and_string_array(extra_keys, extra_values, extra_len)?;

        glean.set_experiment_active(experiment_id, branch, extra);
        Ok(())
    })
}

#[no_mangle]
pub extern "C" fn glean_set_experiment_inactive(experiment_id: FfiStr) {
    with_glean(|glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        glean.set_experiment_inactive(experiment_id);
        Ok(())
    })
}

#[no_mangle]
pub extern "C" fn glean_experiment_test_is_active(experiment_id: FfiStr) -> u8 {
    with_glean(|glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        Ok(glean.test_is_experiment_active(experiment_id))
    })
}

#[no_mangle]
pub extern "C" fn glean_experiment_test_get_data(experiment_id: FfiStr) -> *mut c_char {
    with_glean(|glean| {
        let experiment_id = experiment_id.to_string_fallible()?;
        Ok(glean.test_get_experiment_data_as_json(experiment_id))
    })
}

#[no_mangle]
pub extern "C" fn glean_clear_application_lifetime_metrics() {
    with_glean_value(|glean| glean.clear_application_lifetime_metrics());
}

/// Try to unblock the RLB dispatcher to start processing queued tasks.
///
/// **Note**: glean-core does not have its own dispatcher at the moment.
/// This tries to detect the RLB and, if loaded, instructs the RLB dispatcher to flush.
/// This allows the usage of both the RLB and other language bindings (e.g. Kotlin/Swift)
/// within the same application.
#[no_mangle]
pub extern "C" fn glean_flush_rlb_dispatcher() {
    #[cfg(unix)]
    #[allow(non_upper_case_globals)]
    {
        weak!(fn rlb_flush_dispatcher() -> ());

        if let Some(f) = rlb_flush_dispatcher.get() {
            // SAFETY:
            //
            // We did a dynamic lookup for this symbol.
            // This is only called if we found it.
            // We don't pass any data and don't read any return value, thus no data we directly
            // depend on will be corruptable.
            unsafe {
                f();
            }
        } else {
            log::info!("No RLB symbol found. Not trying to flush the RLB dispatcher.");
        }
    }
}

#[no_mangle]
pub extern "C" fn glean_set_dirty_flag(flag: u8) {
    with_glean_value_mut(|glean| glean.set_dirty_flag(flag != 0));
}

#[no_mangle]
pub extern "C" fn glean_is_dirty_flag_set() -> u8 {
    with_glean_value(|glean| glean.is_dirty_flag_set())
}

#[no_mangle]
pub extern "C" fn glean_handle_client_active() {
    with_glean_value_mut(|glean| glean.handle_client_active());
}

#[no_mangle]
pub extern "C" fn glean_handle_client_inactive() {
    with_glean_value_mut(|glean| glean.handle_client_inactive());
}

#[no_mangle]
pub extern "C" fn glean_test_clear_all_stores() {
    with_glean_value(|glean| glean.test_clear_all_stores())
}

#[no_mangle]
pub extern "C" fn glean_destroy_glean() {
    with_glean_value_mut(|glean| glean.destroy_db())
}

#[no_mangle]
pub extern "C" fn glean_is_first_run() -> u8 {
    with_glean_value(|glean| glean.is_first_run())
}

// Unfortunately, the way we use CFFI in Python ("out-of-line", "ABI mode") does not
// allow return values to be `union`s, so we need to use an output parameter instead of
// a return value to get the task. The output data will be consumed and freed by the
// `glean_process_ping_upload_response` below.
//
// Arguments:
//
// * `result`: the object the output task will be written to.
#[no_mangle]
pub extern "C" fn glean_get_upload_task(result: *mut FfiPingUploadTask) {
    with_glean_value(|glean| {
        let ffi_task = FfiPingUploadTask::from(glean.get_upload_task());
        unsafe {
            std::ptr::write(result, ffi_task);
        }
    });
}

/// Process and free a `FfiPingUploadTask`.
///
/// We need to pass the whole task instead of only the document id,
/// so that we can free the strings properly on Drop.
///
/// After return the `task` should not be used further by the caller.
///
/// # Safety
///
/// A valid and non-null upload task object is required for this function.
#[no_mangle]
pub unsafe extern "C" fn glean_process_ping_upload_response(
    task: *mut FfiPingUploadTask,
    status: u32,
) {
    // Safety:
    // * We null-check the passed task before dereferencing.
    // * We replace data behind the pointer with another valid variant.
    // * We gracefully handle invalid data in strings.
    if task.is_null() {
        return;
    }

    // Take out task and replace with valid value.
    // This value should never be read again on the FFI side,
    // but as it controls the memory, we put something valid in place, just in case.
    let task = std::ptr::replace(task, FfiPingUploadTask::Done);

    with_glean(|glean| {
        if let FfiPingUploadTask::Upload { document_id, .. } = task {
            assert!(!document_id.is_null());
            let document_id_str = CStr::from_ptr(document_id)
                .to_str()
                .map_err(|_| glean_core::Error::utf8_error())?;
            glean.process_ping_upload_response(document_id_str, status.into());
        };
        Ok(())
    });
}

/// # Safety
///
/// A valid and non-null configuration object is required for this function.
#[no_mangle]
pub unsafe extern "C" fn glean_initialize_for_subprocess(cfg: *const FfiConfiguration) -> u8 {
    assert!(!cfg.is_null());

    handlemap_ext::handle_result(|| {
        // We can create a reference to the FfiConfiguration struct:
        // 1. We did a null check
        // 2. We're not holding on to it beyond this function
        //    and we copy out all data when needed.
        let glean_cfg = glean_core::Configuration::try_from(&*cfg)?;
        let glean = Glean::new_for_subprocess(&glean_cfg, true)?;
        glean_core::setup_glean(glean)?;
        log::info!("Glean initialized for subprocess");
        Ok(true)
    })
}

#[no_mangle]
pub extern "C" fn glean_set_debug_view_tag(tag: FfiStr) -> u8 {
    with_glean_mut(|glean| {
        let tag = tag.to_string_fallible()?;
        Ok(glean.set_debug_view_tag(&tag))
    })
}

#[no_mangle]
pub extern "C" fn glean_set_log_pings(value: u8) {
    with_glean_mut(|glean| Ok(glean.set_log_pings(value != 0)));
}

#[no_mangle]
pub extern "C" fn glean_set_source_tags(raw_tags: RawStringArray, tags_count: i32) -> u8 {
    with_glean_mut(|glean| {
        let tags = from_raw_string_array(raw_tags, tags_count)?;
        Ok(glean.set_source_tags(tags))
    })
}

#[no_mangle]
pub extern "C" fn glean_get_timestamp_ms() -> u64 {
    glean_core::get_timestamp_ms()
}

define_string_destructor!(glean_str_free);
