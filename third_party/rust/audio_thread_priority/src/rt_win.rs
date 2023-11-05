#[cfg(feature = "windows")]
mod os {
    pub use windows::Win32::Foundation::GetLastError;
    pub use windows::Win32::Foundation::HANDLE;
    pub use windows::Win32::Foundation::PSTR;
    pub use windows::Win32::System::Threading::{
        AvRevertMmThreadCharacteristics, AvSetMmThreadCharacteristicsA,
    };

    pub fn ok(rv: windows::Win32::Foundation::BOOL) -> bool {
        rv.as_bool()
    }

    pub fn invalid_handle(handle: HANDLE) -> bool {
        handle.is_invalid()
    }
}
#[cfg(feature = "winapi")]
mod os {
    pub use winapi::shared::ntdef::HANDLE;
    pub use winapi::um::avrt::{AvRevertMmThreadCharacteristics, AvSetMmThreadCharacteristicsA};
    pub use winapi::um::errhandlingapi::GetLastError;

    pub fn ok(rv: winapi::shared::minwindef::BOOL) -> bool {
        rv != 0
    }

    #[allow(non_snake_case)]
    pub fn PSTR(ptr: *const u8) -> *const i8 {
        ptr as _
    }

    pub fn invalid_handle(handle: HANDLE) -> bool {
        handle.is_null()
    }
}

use crate::AudioThreadPriorityError;

use log::info;

#[derive(Debug)]
pub struct RtPriorityHandleInternal {
    mmcss_task_index: u32,
    task_handle: os::HANDLE,
}

impl RtPriorityHandleInternal {
    pub fn new(mmcss_task_index: u32, task_handle: os::HANDLE) -> RtPriorityHandleInternal {
        RtPriorityHandleInternal {
            mmcss_task_index,
            task_handle,
        }
    }
}

pub fn demote_current_thread_from_real_time_internal(
    rt_priority_handle: RtPriorityHandleInternal,
) -> Result<(), AudioThreadPriorityError> {
    let rv = unsafe { os::AvRevertMmThreadCharacteristics(rt_priority_handle.task_handle) };
    if !os::ok(rv) {
        return Err(AudioThreadPriorityError::new(&format!(
            "Unable to restore the thread priority ({:?})",
            unsafe { os::GetLastError() }
        )));
    }

    info!(
        "task {} priority restored.",
        rt_priority_handle.mmcss_task_index
    );

    Ok(())
}

pub fn promote_current_thread_to_real_time_internal(
    _audio_buffer_frames: u32,
    _audio_samplerate_hz: u32,
) -> Result<RtPriorityHandleInternal, AudioThreadPriorityError> {
    let mut task_index = 0u32;

    let handle =
        unsafe { os::AvSetMmThreadCharacteristicsA(os::PSTR("Audio\0".as_ptr()), &mut task_index) };
    let handle = RtPriorityHandleInternal::new(task_index, handle);

    if os::invalid_handle(handle.task_handle) {
        return Err(AudioThreadPriorityError::new(&format!(
            "Unable to restore the thread priority ({:?})",
            unsafe { os::GetLastError() }
        )));
    }

    info!(
        "task {} bumped to real time priority.",
        handle.mmcss_task_index
    );

    Ok(handle)
}
