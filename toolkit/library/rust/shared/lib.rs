// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

extern crate geckoservo;

extern crate app_services_logger;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_client;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_server;
extern crate authenticator;
#[cfg(feature = "bitsdownload")]
extern crate bitsdownload;
#[cfg(feature = "moz_places")]
extern crate bookmark_sync;
extern crate cascade_bloom_filter;
extern crate cert_storage;
extern crate chardetng_c;
extern crate cosec;
#[cfg(feature = "cubeb_coreaudio_rust")]
extern crate cubeb_coreaudio;
#[cfg(feature = "cubeb_pulse_rust")]
extern crate cubeb_pulse;
extern crate encoding_glue;
extern crate fog_control;
extern crate gecko_profiler;
extern crate gkrust_utils;
extern crate http_sfv;
extern crate jsrust_shared;
extern crate kvstore;
extern crate mapped_hyph;
extern crate mozurl;
extern crate mp4parse_capi;
extern crate netwerk_helper;
extern crate nserror;
extern crate nsstring;
extern crate prefs_parser;
extern crate processtools;
#[cfg(feature = "gecko_profiler")]
extern crate profiler_helper;
extern crate rsdparsa_capi;
extern crate static_prefs;
extern crate storage;
#[cfg(feature = "quantum_render")]
extern crate webrender_bindings;
extern crate xpcom;
#[cfg(feature = "new_xulstore")]
extern crate xulstore;

extern crate audio_thread_priority;

#[cfg(not(target_os = "android"))]
extern crate webext_storage_bridge;

#[cfg(feature = "webrtc")]
extern crate mdns_service;
extern crate neqo_glue;
#[cfg(feature = "webgpu")]
extern crate wgpu_bindings;

extern crate qcms;

extern crate unic_langid;
extern crate unic_langid_ffi;

extern crate fluent_langneg;
extern crate fluent_langneg_ffi;

extern crate fluent;
extern crate fluent_ffi;

extern crate fluent_fallback;
extern crate l10nregistry_ffi;
extern crate localization_ffi;

#[cfg(not(target_os = "android"))]
extern crate viaduct;

extern crate gecko_logger;

#[cfg(feature = "oxidized_breakpad")]
extern crate rust_minidump_writer_linux;

extern crate log;
use log::info;

use std::{ffi::CStr, os::raw::c_char};

use gecko_logger::GeckoLogger;

#[no_mangle]
pub extern "C" fn GkRust_Init() {
    // Initialize logging.
    let _ = GeckoLogger::init();
}

#[no_mangle]
pub extern "C" fn GkRust_Shutdown() {}

/// Used to implement `nsIDebug2::RustPanic` for testing purposes.
#[no_mangle]
pub extern "C" fn intentional_panic(message: *const c_char) {
    panic!("{}", unsafe { CStr::from_ptr(message) }.to_string_lossy());
}

/// Used to implement `nsIDebug2::rustLog` for testing purposes.
#[no_mangle]
pub extern "C" fn debug_log(target: *const c_char, message: *const c_char) {
    unsafe {
        // NOTE: The `info!` log macro is used here because we have the `release_max_level_info` feature set.
        info!(target: CStr::from_ptr(target).to_str().unwrap(), "{}", CStr::from_ptr(message).to_str().unwrap());
    }
}
