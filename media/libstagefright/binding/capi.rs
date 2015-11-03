//! C API for mp4parse module.
//!
//! Parses ISO Base Media Format aka video/mp4 streams.
//!
//! # Examples
//!
//! ```rust
//! extern crate mp4parse;
//!
//! // Minimal valid mp4 containing no tracks.
//! let data = b"\0\0\0\x0cftypmp42";
//!
//! let context = mp4parse::mp4parse_new();
//! unsafe {
//!     let rv = mp4parse::mp4parse_read(context, data.as_ptr(), data.len());
//!     assert_eq!(0, rv);
//!     mp4parse::mp4parse_free(context);
//! }
//! ```

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std;
use std::io::Cursor;

// Symbols we need from our rust api.
use MediaContext;
use read_box;
use Error;

/// Allocate an opaque rust-side parser context.
#[no_mangle]
pub extern "C" fn mp4parse_new() -> *mut MediaContext {
    let context = Box::new(MediaContext::new());
    Box::into_raw(context)
}

/// Free a rust-side parser context.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_free(context: *mut MediaContext) {
    assert!(!context.is_null());
    let _ = Box::from_raw(context);
}

/// Feed a buffer through `read_box()` with the given rust-side
/// parser context, returning the number of detected tracks.
///
/// This is safe to call with NULL arguments but will crash
/// if given invalid pointers, as is usual for C.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_read(context: *mut MediaContext, buffer: *const u8, size: usize) -> i32 {
    // Validate arguments from C.
    if context.is_null() {
        return -1;
    }
    if buffer.is_null() || size < 8 {
        return -1;
    }

    let mut context: &mut MediaContext = &mut *context;

    // Wrap the buffer we've been give in a slice.
    let b = std::slice::from_raw_parts(buffer, size);
    let mut c = Cursor::new(b);

    // Parse in a subthread to catch any panics.
    let task = std::thread::spawn(move || {
        loop {
            match read_box(&mut c, &mut context) {
                Ok(_) => {},
                Err(Error::UnexpectedEOF) => { break },
                Err(e) => { panic!(e); },
            }
        }
        // Make sure the track count fits in an i32 so we can use
        // negative values for failure.
        assert!(context.tracks.len() < std::i32::MAX as usize);
        context.tracks.len() as i32
    });
    task.join().unwrap_or(-1)
}

#[test]
fn new_context() {
    let context = mp4parse_new();
    assert!(!context.is_null());
    unsafe { mp4parse_free(context); }
}

#[test]
#[should_panic(expected = "assertion failed")]
fn free_null_context() {
    unsafe { mp4parse_free(std::ptr::null_mut()); }
}

#[test]
fn arg_validation() {
    let null_buffer = std::ptr::null();
    let null_context = std::ptr::null_mut();

    let context = mp4parse_new();
    assert!(!context.is_null());

    let buffer = vec![0u8; 8];

    unsafe {
        assert_eq!(-1, mp4parse_read(null_context, null_buffer, 0));
        assert_eq!(-1, mp4parse_read(context, null_buffer, 0));
    }

    for size in 0..buffer.len() {
        println!("testing buffer length {}", size);
        unsafe {
            assert_eq!(-1, mp4parse_read(context, buffer.as_ptr(), size));
        }
    }

    unsafe { mp4parse_free(context); }
}
