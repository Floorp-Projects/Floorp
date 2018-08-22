// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#![cfg_attr(feature = "oom_with_global_alloc",
            feature(global_allocator, alloc, alloc_system, allocator_api))]
#![cfg_attr(feature = "oom_with_hook", feature(alloc_error_hook))]

#[cfg(feature="servo")]
extern crate geckoservo;

extern crate mp4parse_capi;
extern crate nsstring;
extern crate nserror;
extern crate xpcom;
extern crate netwerk_helper;
extern crate prefs_parser;
extern crate mozurl;
#[cfg(feature = "quantum_render")]
extern crate webrender_bindings;
#[cfg(feature = "cubeb_pulse_rust")]
extern crate cubeb_pulse;
extern crate encoding_c;
extern crate encoding_glue;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_client;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_server;
extern crate env_logger;
extern crate u2fhid;
extern crate log;
extern crate cosec;
extern crate rsdparsa_capi;

use std::boxed::Box;
use std::env;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
#[cfg(target_os = "android")]
use std::os::raw::c_int;
#[cfg(target_os = "android")]
use log::Level;
#[cfg(not(target_os = "android"))]
use log::Log;
use std::panic;

extern "C" {
    fn gfx_critical_note(msg: *const c_char);
    #[cfg(target_os = "android")]
    fn __android_log_write(prio: c_int, tag: *const c_char, text: *const c_char) -> c_int;
}

struct GeckoLogger {
    logger: env_logger::Logger
}

impl GeckoLogger {
    fn new() -> GeckoLogger {
        let mut builder = env_logger::Builder::new();
        let default_level = if cfg!(debug_assertions) { "warn" } else { "error" };
        let logger = match env::var("RUST_LOG") {
            Ok(v) => builder.parse(&v).build(),
            _ => builder.parse(default_level).build(),
        };

        GeckoLogger {
            logger
        }
    }

    fn init() -> Result<(), log::SetLoggerError> {
        let gecko_logger = Self::new();

        log::set_max_level(gecko_logger.logger.filter());
        log::set_boxed_logger(Box::new(gecko_logger))
    }

    fn should_log_to_gfx_critical_note(record: &log::Record) -> bool {
        if record.level() == log::Level::Error &&
           record.target().contains("webrender") {
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
            Level::Error => 6 /* ERROR */,
            Level::Warn => 5 /* WARN */,
            Level::Info => 4 /* INFO */,
            Level::Debug => 3 /* DEBUG */,
            Level::Trace => 2 /* VERBOSE */,
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

    fn flush(&self) { }
}

#[no_mangle]
pub extern "C" fn GkRust_Init() {
    // Initialize logging.
    let _ = GeckoLogger::init();
}

#[no_mangle]
pub extern "C" fn GkRust_Shutdown() {
}

/// Used to implement `nsIDebug2::RustPanic` for testing purposes.
#[no_mangle]
pub extern "C" fn intentional_panic(message: *const c_char) {
    panic!("{}", unsafe { CStr::from_ptr(message) }.to_string_lossy());
}

/// Contains the panic message, if set.
static mut PANIC_REASON: Option<*const str> = None;

/// Configure a panic hook to capture panic messages for crash reports.
///
/// We don't store this in `gMozCrashReason` because:
/// a) Rust strings aren't null-terminated, so we'd have to allocate
///    memory to get a null-terminated string
/// b) The panic=abort handler is going to call `abort()` on non-Windows,
///    which is `mozalloc_abort` for us, which will use `MOZ_CRASH` and
///    overwrite `gMozCrashReason` with an unhelpful string.
#[no_mangle]
pub extern "C" fn install_rust_panic_hook() {
    let default_hook = panic::take_hook();
    panic::set_hook(Box::new(move |info| {
        // Try to handle &str/String payloads, which should handle 99% of cases.
        let payload = info.payload();
        // We'll hold a raw *const str here, but it will be OK because
        // Rust is going to abort the process before the payload could be
        // deallocated.
        if let Some(s) = payload.downcast_ref::<&str>() {
            unsafe { PANIC_REASON = Some(*s as *const str); }
        } else if let Some(s) = payload.downcast_ref::<String>() {
            unsafe { PANIC_REASON = Some(s.as_str() as *const str); }
        } else {
            // Not the most helpful thing, but seems unlikely to happen
            // in practice.
            println!("Unhandled panic payload!");
        }
        // Fall through to the default hook so we still print the reason and
        // backtrace to the console.
        default_hook(info);
    }));
}

#[no_mangle]
pub extern "C" fn get_rust_panic_reason(reason: *mut *const c_char, length: *mut usize) -> bool {
    unsafe {
        if let Some(s) = PANIC_REASON {
            *reason = s as *const c_char;
            *length = (*s).len();
            true
        } else {
            false
        }
    }
}

// Wrap the rust system allocator to override the OOM handler, redirecting
// to Gecko's, which interacts with the crash reporter.
// This relies on unstable APIs that have not changed between 1.24 and 1.27.
// In 1.27, the API changed, so we'll need to adapt to those changes before
// we can ship with 1.27. As of writing, there might still be further changes
// to those APIs before 1.27 is released, so we wait for those.
#[cfg(feature = "oom_with_global_alloc")]
mod global_alloc {
    extern crate alloc;
    extern crate alloc_system;

    use self::alloc::allocator::{Alloc, AllocErr, Layout};
    use self::alloc_system::System;

    pub struct GeckoHeap;

    extern "C" {
        fn GeckoHandleOOM(size: usize) -> !;
    }

    unsafe impl<'a> Alloc for &'a GeckoHeap {
        unsafe fn alloc(&mut self, layout: Layout) -> Result<*mut u8, AllocErr> {
            System.alloc(layout)
        }

        unsafe fn dealloc(&mut self, ptr: *mut u8, layout: Layout) {
            System.dealloc(ptr, layout)
        }

        fn oom(&mut self, e: AllocErr) -> ! {
            match e {
                AllocErr::Exhausted { request } => unsafe { GeckoHandleOOM(request.size()) },
                _ => System.oom(e),
            }
        }

        unsafe fn realloc(
            &mut self,
            ptr: *mut u8,
            layout: Layout,
            new_layout: Layout,
        ) -> Result<*mut u8, AllocErr> {
            System.realloc(ptr, layout, new_layout)
        }

        unsafe fn alloc_zeroed(&mut self, layout: Layout) -> Result<*mut u8, AllocErr> {
            System.alloc_zeroed(layout)
        }
    }

}

#[cfg(feature = "oom_with_global_alloc")]
#[global_allocator]
static HEAP: global_alloc::GeckoHeap = global_alloc::GeckoHeap;

#[cfg(feature = "oom_with_hook")]
mod oom_hook {
    use std::alloc::{Layout, set_alloc_error_hook};

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
