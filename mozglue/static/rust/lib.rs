/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![cfg_attr(feature = "oom_with_hook", feature(alloc_error_hook))]
#![cfg_attr(feature = "oom_with_alloc_error_panic", feature(panic_oom_payload))]

use arrayvec::ArrayString;
use std::cmp;
use std::ops::Deref;
use std::os::raw::c_char;
use std::os::raw::c_int;
use std::panic;

#[link(name = "wrappers")]
extern "C" {
    // We can't use MOZ_Crash directly because it may be weakly linked
    // and rust can't handle that.
    fn RustMozCrash(filename: *const c_char, line: c_int, reason: *const c_char) -> !;
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
struct ArrayCString<const CAP: usize> {
    inner: ArrayString<CAP>,
}

impl<S: AsRef<str>, const CAP: usize> From<S> for ArrayCString<CAP> {
    /// Contrary to ArrayString::from, truncates at the closest unicode
    /// character boundary.
    /// ```
    /// assert_eq!(ArrayCString::<4>::from("éà"),
    ///            ArrayCString::<4>::from("é"));
    /// assert_eq!(&*ArrayCString::<4>::from("éà"), "é\0");
    /// ```
    fn from(s: S) -> Self {
        let s = s.as_ref();
        let len = cmp::min(s.len(), CAP - 1);
        let mut result = Self {
            inner: ArrayString::from(str_truncate_valid(s, len)).unwrap(),
        };
        result.inner.push('\0');
        result
    }
}

impl<const CAP: usize> Deref for ArrayCString<CAP> {
    type Target = str;

    fn deref(&self) -> &str {
        self.inner.as_str()
    }
}

fn panic_hook(info: &panic::PanicInfo) {
    // Try to handle &str/String payloads, which should handle 99% of cases.
    let payload = info.payload();
    let message = if let Some(layout) = oom_hook::oom_layout(payload) {
        unsafe {
            oom_hook::RustHandleOOM(layout.size());
        }
    } else if let Some(s) = payload.downcast_ref::<&str>() {
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
    // and RustMozCrash wants one).
    let message = ArrayCString::<512>::from(message);
    let filename = ArrayCString::<512>::from(filename);
    unsafe {
        RustMozCrash(
            filename.as_ptr() as *const c_char,
            line as c_int,
            message.as_ptr() as *const c_char,
        );
    }
}

/// Configure a panic hook to redirect rust panics to MFBT's MOZ_Crash.
#[no_mangle]
pub extern "C" fn install_rust_hooks() {
    panic::set_hook(Box::new(panic_hook));
    #[cfg(feature = "oom_with_hook")]
    use std::alloc::set_alloc_error_hook;
    #[cfg(feature = "oom_with_hook")]
    set_alloc_error_hook(oom_hook::hook);
}

mod oom_hook {
    #[cfg(feature = "oom_with_alloc_error_panic")]
    use std::alloc::AllocErrorPanicPayload;
    use std::alloc::Layout;
    use std::any::Any;

    #[inline(always)]
    pub fn oom_layout(_payload: &dyn Any) -> Option<Layout> {
        #[cfg(feature = "oom_with_alloc_error_panic")]
        return _payload
            .downcast_ref::<AllocErrorPanicPayload>()
            .map(|p| p.layout());
        #[cfg(not(feature = "oom_with_alloc_error_panic"))]
        return None;
    }

    extern "C" {
        pub fn RustHandleOOM(size: usize) -> !;
    }

    #[cfg(feature = "oom_with_hook")]
    pub fn hook(layout: Layout) {
        unsafe {
            RustHandleOOM(layout.size());
        }
    }
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
