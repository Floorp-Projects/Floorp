// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#[cfg(feature="servo")]
extern crate geckoservo;

extern crate mp4parse_capi;
extern crate nsstring;
extern crate nserror;
extern crate rust_url_capi;
#[cfg(feature = "quantum_render")]
extern crate webrender_bindings;
#[cfg(feature = "cubeb_pulse_rust")]
extern crate cubeb_pulse;
extern crate encoding_c;
extern crate encoding_glue;

use std::boxed::Box;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::panic;



// This workaround is fixed in Rust 1.19. For details, see bug 1358151.
thread_local!(static UNUSED_THREAD_LOCAL: () = ());
#[no_mangle]
pub extern "C" fn rust_init_please_remove_this_after_updating_rust_1_19() {
    UNUSED_THREAD_LOCAL.with(|_| ());
}


/// Used to implement `nsIDebug2::RustPanic` for testing purposes.
#[no_mangle]
pub extern "C" fn intentional_panic(message: *const c_char) {
    panic!("{}", unsafe { CStr::from_ptr(message) }.to_string_lossy());
}

/// Contains the panic message, if set.
static mut PANIC_REASON: Option<(*const str, usize)> = None;

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
    panic::set_hook(Box::new(|info| {
        // Try to handle &str/String payloads, which should handle 99% of cases.
        let payload = info.payload();
        // We'll hold a raw *const str here, but it will be OK because
        // Rust is going to abort the process before the payload could be
        // deallocated.
        if let Some(s) = payload.downcast_ref::<&str>() {
            unsafe { PANIC_REASON = Some((*s as *const str, s.len())) }
        } else if let Some(s) = payload.downcast_ref::<String>() {
            unsafe { PANIC_REASON = Some((s.as_str() as *const str, s.len())) }
        } else {
            // Not the most helpful thing, but seems unlikely to happen
            // in practice.
            println!("Unhandled panic payload!");
        }
    }));
}

#[no_mangle]
pub extern "C" fn get_rust_panic_reason(reason: *mut *const c_char, length: *mut usize) -> bool {
    unsafe {
        match PANIC_REASON {
            Some((s, len)) => {
                *reason = s as *const c_char;
                *length = len;
                true
            }
            None => false,
        }
    }
}
