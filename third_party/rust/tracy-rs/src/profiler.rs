/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use minidl::Library;

// Must match `___tracy_source_location_data` struct in TracyC.h
#[repr(C)]
pub struct SourceLocation {
    pub name: *const u8,
    pub function: *const u8,
    pub file: *const u8,
    pub line: u32,
    pub color: u32,
}

// Must match `___tracy_c_zone_context` in TracyC.h
#[repr(C)]
#[derive(Copy, Clone)]
pub struct ZoneContext {
    id: u32,
    active: i32,
}

// Define a set of no-op implementation functions that are called if the tracy
// shared library is not loaded.
extern "C" fn unimpl_mark_frame(_: *const u8) {}
extern "C" fn unimpl_mark_frame_start(_: *const u8) {}
extern "C" fn unimpl_mark_frame_end(_: *const u8) {}
extern "C" fn unimpl_zone_begin(_: *const SourceLocation, _: i32) -> ZoneContext {
    ZoneContext {
        id: 0,
        active: 0,
    }
}
extern "C" fn unimpl_zone_end(_: ZoneContext) {}
extern "C" fn unimpl_zone_text(_: ZoneContext, _: *const u8, _: usize) {}

extern "C" fn unimpl_plot(_: *const u8, _: f64) {}

// Function pointers to the tracy API functions (if loaded), or the no-op functions above.
pub static mut MARK_FRAME: extern "C" fn(name: *const u8) = unimpl_mark_frame;
pub static mut MARK_FRAME_START: extern "C" fn(name: *const u8) = unimpl_mark_frame_start;
pub static mut MARK_FRAME_END: extern "C" fn(name: *const u8) = unimpl_mark_frame_end;
pub static mut EMIT_ZONE_BEGIN: extern "C" fn(srcloc: *const SourceLocation, active: i32) -> ZoneContext = unimpl_zone_begin;
pub static mut EMIT_ZONE_END: extern "C" fn(ctx: ZoneContext) = unimpl_zone_end;
pub static mut EMIT_ZONE_TEXT: extern "C" fn(ctx: ZoneContext, txt: *const u8, size: usize) = unimpl_zone_text;
pub static mut EMIT_PLOT: extern "C" fn(name: *const u8, val: f64) = unimpl_plot;

// Load the tracy library, and get function pointers. This is unsafe since:
// - It must not be called while other threads are calling the functions.
// - It doesn't ensure that the library is not unloaded during use.
pub unsafe fn load(path: &str) -> bool {
    match Library::load(path) {
        Ok(lib) => {
            // If lib loading succeeds, assume we can find all the symbols required.
            MARK_FRAME = lib.sym("___tracy_emit_frame_mark\0").unwrap();
            MARK_FRAME_START = lib.sym("___tracy_emit_frame_mark_start\0").unwrap();
            MARK_FRAME_END = lib.sym("___tracy_emit_frame_mark_end\0").unwrap();
            EMIT_ZONE_BEGIN = lib.sym("___tracy_emit_zone_begin\0").unwrap();
            EMIT_ZONE_END = lib.sym("___tracy_emit_zone_end\0").unwrap();
            EMIT_ZONE_TEXT = lib.sym("___tracy_emit_zone_text\0").unwrap();
            EMIT_PLOT = lib.sym("___tracy_emit_plot\0").unwrap();

            true
        }
        Err(..) => {
            println!("Failed to load the tracy profiling library!");

            false
        }
    }
}

/// A simple stack scope for tracing function execution time
pub struct ProfileScope {
    ctx: ZoneContext,
}

impl ProfileScope {
    pub fn new(callsite: &'static SourceLocation) -> Self {
        let ctx = unsafe { EMIT_ZONE_BEGIN(callsite, 1) };

        ProfileScope {
            ctx,
        }
    }

    pub fn text(&self, text: &[u8]) {
        unsafe { EMIT_ZONE_TEXT(self.ctx, text.as_ptr(), text.len()) };
    }
}

impl Drop for ProfileScope {
    fn drop(&mut self) {
        unsafe {
            EMIT_ZONE_END(self.ctx)
        }
    }
}

/// Define a profiling scope.
///
/// This macro records a Tracy 'zone' starting at the point at which the macro
/// occurs, and extending to the end of the block. More precisely, the zone ends
/// when a variable declared by this macro would be dropped, so early returns
/// exit the zone.
///
/// For example:
///
///     fn an_operation_that_may_well_take_a_long_time() {
///         profile_scope!("an_operation_that_may_well_take_a_long_time");
///         ...
///     }
///
/// The first argument to `profile_scope!` is the name of the zone, and must be
/// a string literal.
///
/// Tracy zones can also have associated 'text' values that vary from one
/// execution to the next - for example, a zone's text might record the name of
/// the file being processed. The text must be a `CStr` value. You can provide
/// zone text like this:
///
///     fn run_reftest(test_name: &str) {
///         profile_scope!("run_reftest", text: test_name);
///         ...
///     }
#[macro_export]
macro_rules! profile_scope {
    ($string:literal $(, text: $text:expr )? ) => {
        const CALLSITE: $crate::profiler::SourceLocation = $crate::profiler::SourceLocation {
            name: concat!($string, "\0").as_ptr(),
            function: concat!(module_path!(), "\0").as_ptr(),
            file: concat!(file!(), "\0").as_ptr(),
            line: line!(),
            color: 0xff0000ff,
        };

        let _profile_scope = $crate::profiler::ProfileScope::new(&CALLSITE);

        $(
            _profile_scope.text(str::as_bytes($text));
        )?
    }
}

/// Define the main frame marker, typically placed after swap_buffers or similar.
#[macro_export]
macro_rules! tracy_frame_marker {
    () => {
        unsafe {
            $crate::profiler::MARK_FRAME(std::ptr::null())
        }
    }
}

/// Define start of an explicit frame marker, typically used for sub-frames.
#[macro_export]
macro_rules! tracy_begin_frame {
    ($name:expr) => {
        unsafe {
            $crate::profiler::MARK_FRAME_START(concat!($name, "\0").as_ptr())
        }
    }
}

/// Define end of an explicit frame marker, typically used for sub-frames.
#[macro_export]
macro_rules! tracy_end_frame {
    ($name:expr) => {
        unsafe {
            $crate::profiler::MARK_FRAME_END(concat!($name, "\0").as_ptr())
        }
    }
}

#[macro_export]
macro_rules! tracy_plot {
    ($name:expr, $value:expr) => {
        unsafe {
            $crate::profiler::EMIT_PLOT(concat!($name, "\0").as_ptr(), $value)
        }
    }
}

// Provide a name for this thread to the profiler
pub fn register_thread_with_profiler(_: String) {
    // TODO(gw): Add support for passing this to the tracy api
}
