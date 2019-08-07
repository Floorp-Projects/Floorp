// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#![cfg_attr(feature = "oom_with_hook", feature(alloc_error_hook))]

extern crate geckoservo;

extern crate kvstore;
extern crate mp4parse_capi;
extern crate nsstring;
extern crate nserror;
extern crate xpcom;
extern crate netwerk_helper;
extern crate prefs_parser;
extern crate static_prefs;
#[cfg(feature = "gecko_profiler")]
extern crate profiler_helper;
extern crate mozurl;
#[cfg(feature = "quantum_render")]
extern crate webrender_bindings;
#[cfg(feature = "cubeb_coreaudio_rust")]
extern crate cubeb_coreaudio;
#[cfg(feature = "cubeb_pulse_rust")]
extern crate cubeb_pulse;
extern crate encoding_c;
extern crate encoding_glue;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_client;
#[cfg(feature = "cubeb-remoting")]
extern crate audioipc_server;
extern crate env_logger;
extern crate authenticator;
extern crate gkrust_utils;
extern crate log;
#[cfg(feature = "new_cert_storage")]
extern crate cert_storage;
extern crate cosec;
extern crate rsdparsa_capi;
#[cfg(feature = "new_xulstore")]
extern crate xulstore;
#[cfg(feature = "spidermonkey_rust")]
extern crate jsrust_shared;
#[cfg(feature = "bitsdownload")]
extern crate bitsdownload;
extern crate storage;
#[cfg(feature = "moz_places")]
extern crate bookmark_sync;
extern crate shift_or_euc_c;

extern crate arrayvec;

extern crate audio_thread_priority;

use std::boxed::Box;
use std::env;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::os::raw::c_int;
#[cfg(target_os = "android")]
use log::Level;
#[cfg(not(target_os = "android"))]
use log::Log;
use std::cmp;
use std::panic;
use std::ops::Deref;
use arrayvec::{Array, ArrayString};

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

extern "C" {
    // We can't use MOZ_Crash directly because it may be weakly linked
    // to libxul, and rust can't handle that.
    fn GeckoCrash(filename: *const c_char, line: c_int, reason: *const c_char) -> !;
}

/// Truncate a string at the closest unicode character boundary
/// ```
/// assert_eq!(str_truncate_valid("éà", 3), "é");
/// assert_eq!(str_truncate_valid("éà", 4), "éè");
/// ```
fn str_truncate_valid(s: &str, mut mid: usize) -> &str {
    loop {
        if let Some(res) = s.get(..mid) {
            return res;
        }
        mid -= 1;
    }
}

/// Similar to ArrayString, but with terminating nul character.
#[derive(Debug, PartialEq)]
struct ArrayCString<A: Array<Item = u8>> {
    inner: ArrayString<A>,
}

impl<S: AsRef<str>, A: Array<Item = u8>> From<S> for ArrayCString<A> {
    /// Contrary to ArrayString::from, truncates at the closest unicode
    /// character boundary.
    /// ```
    /// assert_eq!(ArrayCString::<[_; 4]>::from("éà"),
    ///            ArrayCString::<[_; 4]>::from("é"));
    /// assert_eq!(&*ArrayCString::<[_; 4]>::from("éà"), "é\0");
    /// ```
    fn from(s: S) -> Self {
        let s = s.as_ref();
        let len = cmp::min(s.len(), A::capacity() - 1);
        let mut result = Self {
            inner: ArrayString::from(str_truncate_valid(s, len)).unwrap(),
        };
        result.inner.push('\0');
        result
    }
}

impl<A: Array<Item = u8>> Deref for ArrayCString<A> {
    type Target = str;

    fn deref(&self) -> &str {
        self.inner.as_str()
    }
}

fn panic_hook(info: &panic::PanicInfo) {
    // Try to handle &str/String payloads, which should handle 99% of cases.
    let payload = info.payload();
    let message = if let Some(s) = payload.downcast_ref::<&str>() {
        s
    } else if let Some(s) = payload.downcast_ref::<String>() {
        s.as_str()
    } else {
        // Not the most helpful thing, but seems unlikely to happen
        // in practice.
        "Unhandled rust panic payload!"
    };
    let (filename, line) = if let Some(loc) = info.location() {
        (loc.file(), loc.line())
    } else {
        ("unknown.rs", 0)
    };
    // Copy the message and filename to the stack in order to safely add
    // a terminating nul character (since rust strings don't come with one
    // and GeckoCrash wants one).
    let message = ArrayCString::<[_; 512]>::from(message);
    let filename = ArrayCString::<[_; 512]>::from(filename);
    unsafe {
        GeckoCrash(filename.as_ptr() as *const c_char, line as c_int,
                   message.as_ptr() as *const c_char);
    }
}

/// Configure a panic hook to redirect rust panics to Gecko's MOZ_Crash.
#[no_mangle]
pub extern "C" fn install_rust_panic_hook() {
    panic::set_hook(Box::new(panic_hook));
}

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
