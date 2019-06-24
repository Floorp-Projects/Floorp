/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate cfg_if;
#[cfg(feature = "terminal-logging")]
extern crate simple_logger;
#[macro_use]
extern crate log;

cfg_if! {
    if #[cfg(target_os = "macos")] {
        mod rt_mach;
#[allow(unused, non_camel_case_types, non_snake_case, non_upper_case_globals)]
        mod mach_sys;
        extern crate mach;
        extern crate libc;
        use rt_mach::promote_current_thread_to_real_time_internal;
        use rt_mach::demote_current_thread_from_real_time_internal;
        use rt_mach::RtPriorityHandleInternal;
    } else if #[cfg(target_os = "windows")] {
        extern crate winapi;
        mod rt_win;
        use rt_win::promote_current_thread_to_real_time_internal;
        use rt_win::demote_current_thread_from_real_time_internal;
        use rt_win::RtPriorityHandleInternal;
    } else if #[cfg(target_os = "linux")] {
        mod rt_linux;
        extern crate dbus;
        extern crate libc;
        use rt_linux::promote_current_thread_to_real_time_internal;
        use rt_linux::demote_current_thread_from_real_time_internal;
        use rt_linux::RtPriorityHandleInternal;
    } else {
        pub struct RtPriorityHandleInternal {}
        pub fn promote_current_thread_to_real_time_internal(_: u32, audio_samplerate_hz: u32) -> Result<RtPriorityHandle, ()> {
            if audio_samplerate_hz == 0 {
                return Err(());
            }
            // no-op
            return Ok(RtPriorityHandle{});
        }
        pub fn demote_current_thread_from_real_time_internal(_: RtPriorityHandle) -> Result<(), ()> {
            // no-op
            return Ok(());
        }
    }
}

/// Opaque handle to a thread handle structure.
pub type RtPriorityHandle = RtPriorityHandleInternal;

/// Promote the calling thread thread to real-time priority.
///
/// # Arguments
///
/// * `audio_buffer_frames` - the exact or an upper limit on the number of frames that have to be
/// rendered each callback, or 0 for a sensible default value.
/// * `audio_samplerate_hz` - the sample-rate for this audio stream, in Hz.
///
/// # Return value
///
/// This function returns a `Result<RtPriorityHandle>`, which is an opaque struct to be passed to
/// `demote_current_thread_from_real_time` to revert to the previous thread priority.
pub fn promote_current_thread_to_real_time(
    audio_buffer_frames: u32,
    audio_samplerate_hz: u32,
) -> Result<RtPriorityHandle, ()> {
    if audio_samplerate_hz == 0 {
        return Err(());
    }
    return promote_current_thread_to_real_time_internal(audio_buffer_frames, audio_samplerate_hz);
}

/// Demotes the calling thread from real-time priority.
///
/// # Arguments
///
/// * `handle` - An opaque struct returned from a successful call to
/// `promote_current_thread_to_real_time`.
///
/// # Return value
///
/// `Ok` in scase of success, `Err` otherwise.
pub fn demote_current_thread_from_real_time(handle: RtPriorityHandle) -> Result<(), ()> {
    return demote_current_thread_from_real_time_internal(handle);
}

/// Opaque handle for the C API
#[allow(non_camel_case_types)]
pub struct atp_handle(RtPriorityHandle);

/// Promote the calling thread thread to real-time priority, with a C API.
///
/// # Arguments
///
/// * `audio_buffer_frames` - the exact or an upper limit on the number of frames that have to be
/// rendered each callback, or 0 for a sensible default value.
/// * `audio_samplerate_hz` - the sample-rate for this audio stream, in Hz.
///
/// # Return value
///
/// This function returns `NULL` in case of error: if it couldn't bump the thread, or if the
/// `audio_samplerate_hz` is zero. It returns an opaque handle, to be passed to
/// `atp_demote_current_thread_from_real_time` to demote the thread.
///
/// Additionaly, NULL can be returned in sandboxed processes on Linux, when DBUS cannot be used in
/// the process (for example because the socket to DBUS cannot be created). If this is the case,
/// it's necessary to get the information from the thread to promote and ask another process to
/// promote it (maybe via another privileged process).
#[no_mangle]
pub extern "C" fn atp_promote_current_thread_to_real_time(
    audio_buffer_frames: u32,
    audio_samplerate_hz: u32,
) -> *mut atp_handle {
    match promote_current_thread_to_real_time(audio_buffer_frames, audio_samplerate_hz) {
        Ok(handle) => Box::into_raw(Box::new(atp_handle(handle))),
        _ => std::ptr::null_mut(),
    }
}
/// Demotes the calling thread from real-time priority, with a C API.
///
/// # Arguments
///
/// * `atp_handle` - An opaque struct returned from a successful call to
/// `atp_promote_current_thread_to_real_time`.
///
/// # Return value
///
/// 0 in case of success, non-zero in case of error.
#[no_mangle]
pub extern "C" fn atp_demote_current_thread_from_real_time(handle: *mut atp_handle) -> i32 {
    assert!(!handle.is_null());
    let handle = unsafe { Box::from_raw(handle) };

    match demote_current_thread_from_real_time(handle.0) {
        Ok(_) => 0,
        _ => 1,
    }
}

/// Frees a handle, with a C API.
///
/// This is useful when it impractical to call `atp_demote_current_thread_from_real_time` on the
/// right thread. Access to the handle must be synchronized externaly, or the thread that was
/// promoted to real-time priority must have exited.
///
/// # Arguments
///
/// * `atp_handle` - An opaque struct returned from a successful call to
/// `atp_promote_current_thread_to_real_time`.
///
/// # Return value
///
/// 0 in case of success, non-zero in case of error.
#[no_mangle]
pub extern "C" fn atp_free_handle(handle: *mut atp_handle) -> i32 {
    if handle.is_null() {
        return 1;
    }
    let _handle = unsafe { Box::from_raw(handle) };
    0
}


#[cfg(test)]
mod tests {
    use super::*;
    #[cfg(feature = "terminal-logging")]
    use simple_logger;
#[test]
    fn it_works() {
#[cfg(feature = "terminal-logging")]
        simple_logger::init().unwrap();
        {
            assert!(promote_current_thread_to_real_time(0, 0).is_err());
        }
        {
            match promote_current_thread_to_real_time(0, 44100) {
                Ok(rt_prio_handle) => {
                    demote_current_thread_from_real_time(rt_prio_handle).unwrap();
                }
                Err(e) => {
                    panic!(e);
                }
            }
        }
        {
            match promote_current_thread_to_real_time(512, 44100) {
                Ok(rt_prio_handle) => {
                    demote_current_thread_from_real_time(rt_prio_handle).unwrap();
                }
                Err(e) => {
                    panic!(e);
                }
            }
        }
        {
            match promote_current_thread_to_real_time(512, 44100) {
                Ok(_) => { }
                Err(e) => {
                    panic!(e);
                }
            }
            // automatically deallocated, but not demoted until the thread exits.
        }
    }
}
