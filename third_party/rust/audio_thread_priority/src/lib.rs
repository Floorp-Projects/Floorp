/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#[warn(missing_docs)]
use cfg_if::cfg_if;
use std::error::Error;
use std::fmt;

#[derive(Debug)]
pub struct AudioThreadPriorityError {
    message: String,
    inner: Option<Box<dyn Error + 'static>>,
}

impl AudioThreadPriorityError {
    cfg_if! {
        if #[cfg(all(target_os = "linux", feature = "dbus"))] {
            fn new_with_inner(message: &str, inner: Box<dyn Error>) -> AudioThreadPriorityError {
                AudioThreadPriorityError {
                    message: message.into(),
                    inner: Some(inner),
                }
            }
        }
    }
    fn new(message: &str) -> AudioThreadPriorityError {
        AudioThreadPriorityError {
            message: message.into(),
            inner: None,
        }
    }
}

impl fmt::Display for AudioThreadPriorityError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut rv = write!(f, "AudioThreadPriorityError: {}", &self.message);
        if let Some(inner) = &self.inner {
            rv = write!(f, " ({})", inner);
        }
        rv
    }
}

impl Error for AudioThreadPriorityError {
    fn description(&self) -> &str {
        &self.message
    }

    fn source(&self) -> Option<&(dyn Error + 'static)> {
        self.inner.as_ref().map(|e| e.as_ref())
    }
}

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
    } else if #[cfg(all(target_os = "linux", feature = "dbus"))] {
        mod rt_linux;
        extern crate dbus;
        extern crate libc;
        use rt_linux::promote_current_thread_to_real_time_internal;
        use rt_linux::demote_current_thread_from_real_time_internal;
        use rt_linux::set_real_time_hard_limit_internal as set_real_time_hard_limit;
        use rt_linux::get_current_thread_info_internal;
        use rt_linux::promote_thread_to_real_time_internal;
        use rt_linux::demote_thread_from_real_time_internal;
        use rt_linux::RtPriorityThreadInfoInternal;
        use rt_linux::RtPriorityHandleInternal;
        #[no_mangle]
        /// Size of a RtPriorityThreadInfo or atp_thread_info struct, for use in FFI.
        pub static ATP_THREAD_INFO_SIZE: usize = std::mem::size_of::<RtPriorityThreadInfo>();
    } else {
        // blanket implementations for Android, Linux Desktop without dbus and others
        pub struct RtPriorityHandleInternal {}
        #[derive(Clone, Copy)]
        pub struct RtPriorityThreadInfoInternal {
            _dummy: u8
        }

        cfg_if! {
            if #[cfg(not(target_os = "linux"))] {
                pub type RtPriorityThreadInfo = RtPriorityThreadInfoInternal;
            }
        }

        impl RtPriorityThreadInfo {
            pub fn serialize(&self) -> [u8; 1] {
                [0]
            }
            pub fn deserialize(_: [u8; 1]) -> Self {
                RtPriorityThreadInfo{_dummy: 0}
            }
        }
        pub fn promote_current_thread_to_real_time_internal(_: u32, audio_samplerate_hz: u32) -> Result<RtPriorityHandle, AudioThreadPriorityError> {
            if audio_samplerate_hz == 0 {
                return Err(AudioThreadPriorityError{message: "sample rate is zero".to_string(), inner: None});
            }
            // no-op
            Ok(RtPriorityHandle{})
        }
        pub fn demote_current_thread_from_real_time_internal(_: RtPriorityHandle) -> Result<(), AudioThreadPriorityError> {
            // no-op
            Ok(())
        }
        pub fn set_real_time_hard_limit(
            _: u32,
            _: u32,
        ) -> Result<(), AudioThreadPriorityError> {
            Ok(())
        }
        pub fn get_current_thread_info_internal() -> Result<RtPriorityThreadInfo, AudioThreadPriorityError> {
            Ok(RtPriorityThreadInfo{_dummy: 0})
        }
        pub fn promote_thread_to_real_time_internal(
            _: RtPriorityThreadInfo,
            _: u32,
            audio_samplerate_hz: u32,
        ) -> Result<RtPriorityHandle, AudioThreadPriorityError> {
            if audio_samplerate_hz == 0 {
                return Err(AudioThreadPriorityError::new("sample rate is zero"));
            }
            return Ok(RtPriorityHandle{});
        }

        pub fn demote_thread_from_real_time_internal(_: RtPriorityThreadInfo) -> Result<(), AudioThreadPriorityError> {
            return Ok(());
        }
        #[no_mangle]
        /// Size of a RtPriorityThreadInfo or atp_thread_info struct, for use in FFI.
        pub static ATP_THREAD_INFO_SIZE: usize = std::mem::size_of::<RtPriorityThreadInfo>();
    }
}

/// Opaque handle to a thread handle structure.
pub type RtPriorityHandle = RtPriorityHandleInternal;

cfg_if! {
    if #[cfg(target_os = "linux")] {
/// Opaque handle to a thread info.
///
/// This can be serialized to raw bytes to be sent via IPC.
///
/// This call is useful on Linux desktop only, when the process is sandboxed and
/// cannot promote itself directly.
pub type RtPriorityThreadInfo = RtPriorityThreadInfoInternal;


/// Get the calling thread's information, to be able to promote it to real-time from somewhere
/// else, later.
///
/// This call is useful on Linux desktop only, when the process is sandboxed and
/// cannot promote itself directly.
///
/// # Return value
///
/// Ok in case of success, with an opaque structure containing relevant info for the platform, Err
/// otherwise.
pub fn get_current_thread_info() -> Result<RtPriorityThreadInfo, AudioThreadPriorityError> {
    return get_current_thread_info_internal();
}

/// Return a byte buffer containing serialized information about a thread, to promote it to
/// real-time from elsewhere.
///
/// This call is useful on Linux desktop only, when the process is sandboxed and
/// cannot promote itself directly.
pub fn thread_info_serialize(
    thread_info: RtPriorityThreadInfo,
) -> [u8; std::mem::size_of::<RtPriorityThreadInfo>()] {
    return thread_info.serialize();
}

/// From a byte buffer, return a `RtPriorityThreadInfo`.
///
/// This call is useful on Linux desktop only, when the process is sandboxed and
/// cannot promote itself directly.
///
/// # Arguments
///
/// A byte buffer containing a serializezd `RtPriorityThreadInfo`.
pub fn thread_info_deserialize(
    bytes: [u8; std::mem::size_of::<RtPriorityThreadInfo>()],
) -> RtPriorityThreadInfo {
    return RtPriorityThreadInfoInternal::deserialize(bytes);
}

/// Get the calling threads' information, to promote it from another process or thread, with a C
/// API.
///
/// This is intended to call on the thread that will end up being promoted to real time priority,
/// but that cannot do it itself (probably because of sandboxing reasons).
///
/// After use, it MUST be freed by calling `atp_free_thread_info`.
///
/// # Return value
///
/// A pointer to a struct that can be serialized and deserialized, and that can be passed to
/// `atp_promote_thread_to_real_time`, even from another process.
#[no_mangle]
pub extern "C" fn atp_get_current_thread_info() -> *mut atp_thread_info {
    match get_current_thread_info() {
        Ok(thread_info) => Box::into_raw(Box::new(atp_thread_info(thread_info))),
        _ => std::ptr::null_mut(),
    }
}

/// Frees a thread info, with a c api.
///
/// # Arguments
///
/// thread_info: the `atp_thread_info` structure to free.
///
/// # Return value
///
/// 0 in case of success, 1 otherwise (if `thread_info` is NULL).
#[no_mangle]
pub extern "C" fn atp_free_thread_info(thread_info: *mut atp_thread_info) -> i32 {
    if thread_info.is_null() {
        return 1;
    }
    unsafe { Box::from_raw(thread_info) };
    0
}

/// Return a byte buffer containing serialized information about a thread, to promote it to
/// real-time from elsewhere, with a C API.
///
/// `bytes` MUST be `std::mem::size_of<RtPriorityThreadInfo>()` bytes long.
///
/// This is exposed in the C API as `ATP_THREAD_INFO_SIZE`.
///
/// This call is useful on Linux desktop only, when the process is sandboxed, cannot promote itself
/// directly, and the `atp_thread_info` struct must be passed via IPC.
#[no_mangle]
pub extern "C" fn atp_serialize_thread_info(
    thread_info: *mut atp_thread_info,
    bytes: *mut libc::c_void,
) {
    let thread_info = unsafe { &mut *thread_info };
    let source = thread_info.0.serialize();
    unsafe {
        std::ptr::copy(source.as_ptr(), bytes as *mut u8, source.len());
    }
}

/// From a byte buffer, return a `RtPriorityThreadInfo`, with a C API.
///
/// This call is useful on Linux desktop only, when the process is sandboxed and
/// cannot promote itself directly.
///
/// # Arguments
///
/// A byte buffer containing a serializezd `RtPriorityThreadInfo`.
#[no_mangle]
pub extern "C" fn atp_deserialize_thread_info(
    in_bytes: *mut u8,
) -> *mut atp_thread_info {
    let bytes = unsafe { *(in_bytes as *mut [u8; std::mem::size_of::<RtPriorityThreadInfoInternal>()]) };
    let thread_info = RtPriorityThreadInfoInternal::deserialize(bytes);
    return Box::into_raw(Box::new(atp_thread_info(thread_info)));
}

/// Promote a particular thread thread to real-time priority.
///
/// This call is useful on Linux desktop only, when the process is sandboxed and
/// cannot promote itself directly.
///
/// # Arguments
///
/// * `thread_info` - informations about the thread to promote, gathered using
/// `get_current_thread_info`.
/// * `audio_buffer_frames` - the exact or an upper limit on the number of frames that have to be
/// rendered each callback, or 0 for a sensible default value.
/// * `audio_samplerate_hz` - the sample-rate for this audio stream, in Hz.
///
/// # Return value
///
/// This function returns a `Result<RtPriorityHandle>`, which is an opaque struct to be passed to
/// `demote_current_thread_from_real_time` to revert to the previous thread priority.
pub fn promote_thread_to_real_time(
    thread_info: RtPriorityThreadInfo,
    audio_buffer_frames: u32,
    audio_samplerate_hz: u32,
) -> Result<RtPriorityHandle, AudioThreadPriorityError> {
    if audio_samplerate_hz == 0 {
        return Err(AudioThreadPriorityError::new("sample rate is zero"));
    }
    return promote_thread_to_real_time_internal(
        thread_info,
        audio_buffer_frames,
        audio_samplerate_hz,
    );
}

/// Demotes a thread from real-time priority.
///
/// # Arguments
///
/// * `thread_info` - An opaque struct returned from a successful call to
/// `get_current_thread_info`.
///
/// # Return value
///
/// `Ok` in case of success, `Err` otherwise.
pub fn demote_thread_from_real_time(thread_info: RtPriorityThreadInfo) -> Result<(), AudioThreadPriorityError> {
    return demote_thread_from_real_time_internal(thread_info);
}

/// Opaque info to a particular thread.
#[allow(non_camel_case_types)]
pub struct atp_thread_info(RtPriorityThreadInfo);

/// Promote a specific thread to real-time, with a C API.
///
/// This is useful when the thread to promote cannot make some system calls necessary to promote
/// it.
///
/// # Arguments
///
/// `thread_info` - the information of the thread to promote to real-time, gather from calling
/// `atp_get_current_thread_info` on the thread to promote.
/// * `audio_buffer_frames` - the exact or an upper limit on the number of frames that have to be
/// rendered each callback, or 0 for a sensible default value.
/// * `audio_samplerate_hz` - the sample-rate for this audio stream, in Hz.
///
/// # Return value
///
/// A pointer to an `atp_handle` in case of success, NULL otherwise.
#[no_mangle]
pub extern "C" fn atp_promote_thread_to_real_time(
    thread_info: *mut atp_thread_info,
    audio_buffer_frames: u32,
    audio_samplerate_hz: u32,
) -> *mut atp_handle {
    let thread_info = unsafe { &mut *thread_info };
    match promote_thread_to_real_time(thread_info.0, audio_buffer_frames, audio_samplerate_hz) {
        Ok(handle) => Box::into_raw(Box::new(atp_handle(handle))),
        _ => std::ptr::null_mut(),
    }
}

/// Demote a thread promoted to from real-time, with a C API.
///
/// # Arguments
///
/// `handle` -  an opaque struct received from a promoting function.
///
/// # Return value
///
/// 0 in case of success, non-zero otherwise.
#[no_mangle]
pub extern "C" fn atp_demote_thread_from_real_time(thread_info: *mut atp_thread_info) -> i32 {
    if thread_info.is_null() {
        return 1;
    }
    let thread_info = unsafe { (*thread_info).0 };

    match demote_thread_from_real_time(thread_info) {
        Ok(_) => 0,
        _ => 1,
    }
}

/// Set a real-time limit for the calling thread.
///
/// # Arguments
///
/// `audio_buffer_frames` - the number of frames the audio callback has to render each quantum. 0
/// picks a rather high default value.
/// `audio_samplerate_hz` - the sample-rate of the audio stream.
///
/// # Return value
///
/// 0 in case of success, 1 otherwise.
#[no_mangle]
pub extern "C" fn atp_set_real_time_limit(audio_buffer_frames: u32,
                                          audio_samplerate_hz: u32) -> i32 {
    let r = set_real_time_hard_limit(audio_buffer_frames, audio_samplerate_hz);
    if r.is_err() {
        return 1;
    }
    0
}

}
}

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
) -> Result<RtPriorityHandle, AudioThreadPriorityError> {
    if audio_samplerate_hz == 0 {
        return Err(AudioThreadPriorityError::new("sample rate is zero"));
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
pub fn demote_current_thread_from_real_time(
    handle: RtPriorityHandle,
) -> Result<(), AudioThreadPriorityError> {
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
                    assert!(true);
                }
                Err(e) => {
                    eprintln!("{}", e.description());
                    assert!(false);
                }
            }
        }
        {
            match promote_current_thread_to_real_time(512, 44100) {
                Ok(rt_prio_handle) => {
                    demote_current_thread_from_real_time(rt_prio_handle).unwrap();
                    assert!(true);
                }
                Err(e) => {
                    eprintln!("{}", e.description());
                    assert!(false);
                }
            }
        }
        {
            match promote_current_thread_to_real_time(512, 44100) {
                Ok(_) => {
                    assert!(true);
                }
                Err(e) => {
                    eprintln!("{}", e.description());
                    assert!(false);
                }
            }
            // automatically deallocated, but not demoted until the thread exits.
        }
    }
    cfg_if! {
        if #[cfg(target_os = "linux")] {
            use nix::unistd::*;
            use nix::sys::signal::*;

            #[test]
            fn test_linux_api() {
                {
                    let info = get_current_thread_info().unwrap();
                    match promote_thread_to_real_time(info, 512, 44100) {
                        Ok(_) => {
                            assert!(true);
                        }
                        Err(e) => {
                            eprintln!("{}", e);
                            assert!(false);
                        }
                    }
                }
                {
                    let info = get_current_thread_info().unwrap();
                    let bytes = info.serialize();
                    let info2 = RtPriorityThreadInfo::deserialize(bytes);
                    assert!(info == info2);
                }
                {
                    let info = get_current_thread_info().unwrap();
                    let bytes = thread_info_serialize(info);
                    let info2 = thread_info_deserialize(bytes);
                    assert!(info == info2);
                }
            }
            #[test]
            fn test_remote_promotion() {
                let (rd, wr) = pipe().unwrap();

                match fork().expect("fork failed") {
                    ForkResult::Parent{ child } => {
                        eprintln!("Parent PID: {}", getpid());
                        let mut bytes = [0 as u8; std::mem::size_of::<RtPriorityThreadInfo>()];
                        match read(rd, &mut bytes) {
                            Ok(_) => {
                                let info = RtPriorityThreadInfo::deserialize(bytes);
                                match promote_thread_to_real_time(info, 0, 44100) {
                                    Ok(_) => {
                                        eprintln!("thread promotion in the child from the parent succeeded");
                                        assert!(true);
                                    }
                                    Err(_) => {
                                        eprintln!("promotion Err");
                                        kill(child, SIGKILL).expect("Could not kill the child?");
                                        assert!(false);
                                    }
                                }
                            }
                            Err(e) => {
                                eprintln!("could not read from the pipe: {}", e);
                            }
                        }
                        kill(child, SIGKILL).expect("Could not kill the child?");
                    }
                    ForkResult::Child => {
                        let r = set_real_time_hard_limit(0, 44100);
                        if r.is_err() {
                            eprintln!("Could not set RT limit, the test will fail.");
                        }
                        eprintln!("Child pid: {}", getpid());
                        let info = get_current_thread_info().unwrap();
                        let bytes = info.serialize();
                        match write(wr, &bytes) {
                            Ok(_) => {
                                loop {
                                    std::thread::sleep(std::time::Duration::from_millis(1000));
                                    eprintln!("child sleeping, waiting to be promoted...");
                                }
                            }
                            Err(_) => {
                                eprintln!("write error on the pipe.");
                            }
                        }
                    }
                }
            }
        }
    }
}
