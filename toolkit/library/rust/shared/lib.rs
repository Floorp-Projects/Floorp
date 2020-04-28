// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#![cfg_attr(feature = "oom_with_hook", feature(alloc_error_hook))]

extern crate geckoservo;

#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_client;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_server;
extern crate authenticator;
#[cfg(feature = "bitsdownload")]
extern crate bitsdownload;
#[cfg(feature = "moz_places")]
extern crate bookmark_sync;
#[cfg(feature = "new_cert_storage")]
extern crate cert_storage;
extern crate chardetng_c;
extern crate cosec;
#[cfg(feature = "cubeb_coreaudio_rust")]
extern crate cubeb_coreaudio;
#[cfg(feature = "cubeb_pulse_rust")]
extern crate cubeb_pulse;
extern crate encoding_glue;
extern crate env_logger;
#[cfg(feature = "glean")]
extern crate fog;
extern crate gkrust_utils;
extern crate jsrust_shared;
extern crate kvstore;
extern crate log;
extern crate mapped_hyph;
extern crate mozurl;
extern crate mp4parse_capi;
extern crate netwerk_helper;
extern crate nserror;
extern crate nsstring;
extern crate prefs_parser;
#[cfg(feature = "gecko_profiler")]
extern crate profiler_helper;
extern crate rsdparsa_capi;
extern crate shift_or_euc_c;
extern crate static_prefs;
extern crate storage;
#[cfg(feature = "quantum_render")]
extern crate webrender_bindings;
extern crate xpcom;
#[cfg(feature = "new_xulstore")]
extern crate xulstore;

extern crate audio_thread_priority;

#[cfg(feature = "new_webext_storage")]
extern crate webext_storage_bridge;

#[cfg(feature = "webrtc")]
extern crate mdns_service;
extern crate neqo_glue;
#[cfg(feature = "webgpu")]
extern crate wgpu_bindings;

#[cfg(feature = "wasm_library_sandboxing")]
extern crate rlbox_lucet_sandbox;

extern crate unic_langid;
extern crate unic_langid_ffi;

extern crate fluent_langneg;
extern crate fluent_langneg_ffi;

extern crate fluent;
extern crate fluent_ffi;

extern crate rusqlite;

#[cfg(feature = "services_sync")]
extern crate golden_gate;

#[cfg(feature = "remote")]
extern crate remote;

#[cfg(target_os = "android")]
use log::Level;
#[cfg(not(target_os = "android"))]
use log::Log;
use std::boxed::Box;
use std::env;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
#[cfg(target_os = "android")]
use std::os::raw::c_int;

extern "C" {
    fn gfx_critical_note(msg: *const c_char);
    #[cfg(target_os = "android")]
    fn __android_log_write(prio: c_int, tag: *const c_char, text: *const c_char) -> c_int;
}

struct GeckoLogger {
    logger: env_logger::Logger,
}

impl GeckoLogger {
    fn new() -> GeckoLogger {
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

    fn init() -> Result<(), log::SetLoggerError> {
        let gecko_logger = Self::new();

        log::set_max_level(gecko_logger.logger.filter());
        log::set_boxed_logger(Box::new(gecko_logger))
    }

    fn should_log_to_gfx_critical_note(record: &log::Record) -> bool {
        if record.level() == log::Level::Error && record.target().contains("webrender") {
            true
        } else {
            false
        }
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
        self.logger.log(record);
    }

    #[cfg(target_os = "android")]
    fn log_out(&self, record: &log::Record) {
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
        self.logger.enabled(metadata)
    }

    fn log(&self, record: &log::Record) {
        // Forward log to gfxCriticalNote, if the log should be in gfx crash log.
        self.maybe_log_to_gfx_critical_note(record);
        self.log_out(record);
    }

    fn flush(&self) {}
}

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

#[cfg(feature = "oom_with_hook")]
mod oom_hook {
    use std::alloc::{set_alloc_error_hook, Layout};

    extern "C" {
        fn GeckoHandleOOM(size: usize) -> !;
    }

    pub fn hook(layout: Layout) {
        unsafe {
            GeckoHandleOOM(layout.size());
        }
    }

    pub fn install() {
        set_alloc_error_hook(hook);
    }
}

#[no_mangle]
pub extern "C" fn install_rust_oom_hook() {
    #[cfg(feature = "oom_with_hook")]
    oom_hook::install();
}

#[cfg(feature = "moz_memory")]
mod moz_memory {
    use std::alloc::{GlobalAlloc, Layout};
    use std::os::raw::c_void;

    extern "C" {
        fn malloc(size: usize) -> *mut c_void;

        fn free(ptr: *mut c_void);

        fn calloc(nmemb: usize, size: usize) -> *mut c_void;

        fn realloc(ptr: *mut c_void, size: usize) -> *mut c_void;

        #[cfg(windows)]
        fn _aligned_malloc(size: usize, align: usize) -> *mut c_void;

        #[cfg(not(windows))]
        fn memalign(align: usize, size: usize) -> *mut c_void;
    }

    #[cfg(windows)]
    unsafe fn memalign(align: usize, size: usize) -> *mut c_void {
        _aligned_malloc(size, align)
    }

    pub struct GeckoAlloc;

    #[inline(always)]
    fn need_memalign(layout: Layout) -> bool {
        // mozjemalloc guarantees a minimum alignment of 16 for all sizes, except
        // for size classes below 16 (4 and 8).
        layout.align() > layout.size() || layout.align() > 16
    }

    unsafe impl GlobalAlloc for GeckoAlloc {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
            if need_memalign(layout) {
                memalign(layout.align(), layout.size()) as *mut u8
            } else {
                malloc(layout.size()) as *mut u8
            }
        }

        unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
            free(ptr as *mut c_void)
        }

        unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
            if need_memalign(layout) {
                let ptr = self.alloc(layout);
                if !ptr.is_null() {
                    std::ptr::write_bytes(ptr, 0, layout.size());
                }
                ptr
            } else {
                calloc(1, layout.size()) as *mut u8
            }
        }

        unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
            let new_layout = Layout::from_size_align_unchecked(new_size, layout.align());
            if need_memalign(new_layout) {
                let new_ptr = self.alloc(new_layout);
                if !new_ptr.is_null() {
                    let size = std::cmp::min(layout.size(), new_size);
                    std::ptr::copy_nonoverlapping(ptr, new_ptr, size);
                    self.dealloc(ptr, layout);
                }
                new_ptr
            } else {
                realloc(ptr as *mut c_void, new_size) as *mut u8
            }
        }
    }
}

#[cfg(feature = "moz_memory")]
#[global_allocator]
static A: moz_memory::GeckoAlloc = moz_memory::GeckoAlloc;
