/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! This provides a way to direct rust logging into the gecko logger.

#[macro_use]
extern crate lazy_static;

use app_services_logger::{AppServicesLogger, LOGGERS_BY_TARGET};
use log::Log;
use log::{Level, LevelFilter};
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::os::raw::c_int;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::RwLock;
use std::{cmp, env};

extern "C" {
    fn ExternMozLog(tag: *const c_char, prio: c_int, text: *const c_char);
    fn gfx_critical_note(msg: *const c_char);
    #[cfg(target_os = "android")]
    fn __android_log_write(prio: c_int, tag: *const c_char, text: *const c_char) -> c_int;
}

lazy_static! {
    // This could be a proper static once [1] is fixed or parking_lot's const fn
    // support is not nightly-only.
    //
    // [1]: https://github.com/rust-lang/rust/issues/73714
    static ref LOG_MODULE_MAP: RwLock<HashMap<String, (LevelFilter, bool)>> = RwLock::new(HashMap::new());
}

/// This tells us whether LOG_MODULE_MAP is possibly non-empty.
static LOGGING_ACTIVE: AtomicBool = AtomicBool::new(false);

/// This function searches for the module's name in the hashmap. If that is not
/// found, it proceeds to search for the parent modules.
/// It returns a tuple containing the matched string, if the matched module
/// was a pattern match, and the level we found in the hashmap.
/// If none is found, it will return the module's name and LevelFilter::Off
fn get_level_for_module<'a>(
    map: &HashMap<String, (LevelFilter, bool)>,
    key: &'a str,
) -> (&'a str, bool, LevelFilter) {
    if let Some((level, is_pattern_match)) = map.get(key) {
        return (key, *is_pattern_match, level.clone());
    }

    let mut mod_name = &key[..];
    while let Some(pos) = mod_name.rfind("::") {
        mod_name = &mod_name[..pos];
        if let Some((level, is_pattern_match)) = map.get(mod_name) {
            return (mod_name, *is_pattern_match, level.clone());
        }
    }

    return (key, false, LevelFilter::Off);
}

/// This function takes a record to maybe log to Gecko.
/// It returns true if the record was handled by Gecko's logging, and false
/// otherwise.
pub fn log_to_gecko(record: &log::Record) -> bool {
    if !LOGGING_ACTIVE.load(Ordering::Relaxed) {
        return false;
    }

    let key = match record.module_path() {
        Some(key) => key,
        None => return false,
    };

    let (mod_name, is_pattern_match, level) = {
        let map = LOG_MODULE_MAP.read().unwrap();
        get_level_for_module(&map, &key)
    };

    if level == LevelFilter::Off {
        return false;
    }

    if level < record.metadata().level() {
        return false;
    }

    // Map the log::Level to mozilla::LogLevel.
    let moz_log_level = match record.metadata().level() {
        Level::Error => 1, // Error
        Level::Warn => 2,  // Warning
        Level::Info => 3,  // Info
        Level::Debug => 4, // Debug
        Level::Trace => 5, // Verbose
    };

    // If it was a pattern match, we need to append ::* to the matched string.
    let (tag, msg) = if is_pattern_match {
        (
            CString::new(format!("{}::*", mod_name)).unwrap(),
            CString::new(format!("[{}] {}", key, record.args())).unwrap(),
        )
    } else {
        (
            CString::new(key).unwrap(),
            CString::new(format!("{}", record.args())).unwrap(),
        )
    };

    unsafe {
        ExternMozLog(tag.as_ptr(), moz_log_level, msg.as_ptr());
    }

    return true;
}

#[no_mangle]
pub unsafe extern "C" fn set_rust_log_level(module: *const c_char, level: u8) {
    // Convert the Gecko level to a rust LevelFilter.
    let rust_level = match level {
        1 => LevelFilter::Error,
        2 => LevelFilter::Warn,
        3 => LevelFilter::Info,
        4 => LevelFilter::Debug,
        5 => LevelFilter::Trace,
        _ => LevelFilter::Off,
    };

    // This is the name of the rust module that we're trying to log in Gecko.
    let mut mod_name = CStr::from_ptr(module).to_string_lossy().into_owned();

    let is_pattern_match = mod_name.ends_with("::*");

    // If this is a pattern, remove the last "::*" from it so we can search it
    // in the map.
    if is_pattern_match {
        let len = mod_name.len() - 3;
        mod_name.truncate(len);
    }

    LOGGING_ACTIVE.store(true, Ordering::Relaxed);
    let mut map = LOG_MODULE_MAP.write().unwrap();
    map.insert(mod_name, (rust_level, is_pattern_match));

    // Figure out the max level of all the modules.
    let max = map
        .values()
        .map(|(lvl, _)| lvl)
        .max()
        .unwrap_or(&LevelFilter::Off);
    log::set_max_level(*max);
}

pub struct GeckoLogger {
    logger: env_logger::Logger,
}

impl GeckoLogger {
    pub fn new() -> GeckoLogger {
        let mut builder = env_logger::Builder::new();
        let default_level = if cfg!(debug_assertions) {
            "warn"
        } else {
            "error"
        };
        let logger = match env::var("RUST_LOG") {
            Ok(v) => builder.parse_filters(&v).build(),
            _ => builder.parse_filters(default_level).build(),
        };

        GeckoLogger { logger }
    }

    pub fn init() -> Result<(), log::SetLoggerError> {
        let gecko_logger = Self::new();

        // The max level may have already been set by gecko_logger. Don't
        // set it to a lower level.
        let level = cmp::max(log::max_level(), gecko_logger.logger.filter());
        log::set_max_level(level);
        log::set_boxed_logger(Box::new(gecko_logger))
    }

    fn should_log_to_app_services(target: &str) -> bool {
        return AppServicesLogger::is_app_services_logger_registered(target.into());
    }

    fn maybe_log_to_app_services(&self, record: &log::Record) {
        if Self::should_log_to_app_services(record.target()) {
            if let Some(l) = LOGGERS_BY_TARGET.read().unwrap().get(record.target()) {
                l.log(record);
            }
        }
    }

    fn should_log_to_gfx_critical_note(record: &log::Record) -> bool {
        record.level() == log::Level::Error && record.target().contains("webrender")
    }

    fn maybe_log_to_gfx_critical_note(&self, record: &log::Record) {
        if Self::should_log_to_gfx_critical_note(record) {
            let msg = CString::new(format!("{}", record.args())).unwrap();
            unsafe {
                gfx_critical_note(msg.as_ptr());
            }
        }
    }

    #[cfg(not(target_os = "android"))]
    fn log_out(&self, record: &log::Record) {
        // If the log wasn't handled by the gecko platform logger, just pass it
        // to the env_logger.
        if !log_to_gecko(record) {
            self.logger.log(record);
        }
    }

    #[cfg(target_os = "android")]
    fn log_out(&self, record: &log::Record) {
        if !self.logger.matches(record) {
            return;
        }

        let msg = CString::new(format!("{}", record.args())).unwrap();
        let tag = CString::new(record.module_path().unwrap()).unwrap();
        let prio = match record.metadata().level() {
            Level::Error => 6, /* ERROR */
            Level::Warn => 5,  /* WARN */
            Level::Info => 4,  /* INFO */
            Level::Debug => 3, /* DEBUG */
            Level::Trace => 2, /* VERBOSE */
        };
        // Output log directly to android log, since env_logger can output log
        // only to stderr or stdout.
        unsafe {
            __android_log_write(prio, tag.as_ptr(), msg.as_ptr());
        }
    }
}

impl log::Log for GeckoLogger {
    fn enabled(&self, metadata: &log::Metadata) -> bool {
        self.logger.enabled(metadata) || GeckoLogger::should_log_to_app_services(metadata.target())
    }

    fn log(&self, record: &log::Record) {
        // Forward log to gfxCriticalNote, if the log should be in gfx crash log.
        self.maybe_log_to_gfx_critical_note(record);
        self.maybe_log_to_app_services(record);
        self.log_out(record);
    }

    fn flush(&self) {}
}
