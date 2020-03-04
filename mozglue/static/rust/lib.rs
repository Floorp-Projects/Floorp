/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use arrayvec::{Array, ArrayString};
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
struct ArrayCString<A: Array<Item = u8> + Copy> {
    inner: ArrayString<A>,
}

impl<S: AsRef<str>, A: Array<Item = u8> + Copy> From<S> for ArrayCString<A> {
    /// Contrary to ArrayString::from, truncates at the closest unicode
    /// character boundary.
    /// ```
    /// assert_eq!(ArrayCString::<[_; 4]>::from("éà"),
    ///            ArrayCString::<[_; 4]>::from("é"));
    /// assert_eq!(&*ArrayCString::<[_; 4]>::from("éà"), "é\0");
    /// ```
    fn from(s: S) -> Self {
        let s = s.as_ref();
        let len = cmp::min(s.len(), A::CAPACITY - 1);
        let mut result = Self {
            inner: ArrayString::from(str_truncate_valid(s, len)).unwrap(),
        };
        result.inner.push('\0');
        result
    }
}

impl<A: Array<Item = u8> + Copy> Deref for ArrayCString<A> {
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
    // and RustMozCrash wants one).
    let message = ArrayCString::<[_; 512]>::from(message);
    let filename = ArrayCString::<[_; 512]>::from(filename);
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
pub extern "C" fn install_rust_panic_hook() {
    panic::set_hook(Box::new(panic_hook));
}
