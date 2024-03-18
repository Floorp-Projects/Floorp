use self::avrt_lib::AvRtLibrary;
use crate::AudioThreadPriorityError;
use log::info;
use std::sync::OnceLock;
use windows_sys::{
    w,
    Win32::Foundation::{HANDLE, WIN32_ERROR},
};

#[derive(Debug)]
pub struct RtPriorityHandleInternal {
    mmcss_task_index: u32,
    task_handle: HANDLE,
}

impl RtPriorityHandleInternal {
    fn new(mmcss_task_index: u32, task_handle: HANDLE) -> RtPriorityHandleInternal {
        RtPriorityHandleInternal {
            mmcss_task_index,
            task_handle,
        }
    }
}

fn avrt() -> Result<&'static AvRtLibrary, AudioThreadPriorityError> {
    static AV_RT_LIBRARY: OnceLock<Result<AvRtLibrary, WIN32_ERROR>> = OnceLock::new();
    AV_RT_LIBRARY
        .get_or_init(AvRtLibrary::try_new)
        .as_ref()
        .map_err(|win32_error| {
            AudioThreadPriorityError::new(&format!("Unable to load avrt.dll ({win32_error})"))
        })
}

pub fn promote_current_thread_to_real_time_internal(
    _audio_buffer_frames: u32,
    _audio_samplerate_hz: u32,
) -> Result<RtPriorityHandleInternal, AudioThreadPriorityError> {
    avrt()?
        .set_mm_thread_characteristics(w!("Audio"))
        .map(|(mmcss_task_index, task_handle)| {
            info!("task {mmcss_task_index} bumped to real time priority.");
            RtPriorityHandleInternal::new(mmcss_task_index, task_handle)
        })
        .map_err(|win32_error| {
            AudioThreadPriorityError::new(&format!(
                "Unable to bump the thread priority ({win32_error})"
            ))
        })
}

pub fn demote_current_thread_from_real_time_internal(
    rt_priority_handle: RtPriorityHandleInternal,
) -> Result<(), AudioThreadPriorityError> {
    let RtPriorityHandleInternal {
        mmcss_task_index,
        task_handle,
    } = rt_priority_handle;
    avrt()?
        .revert_mm_thread_characteristics(task_handle)
        .map(|_| {
            info!("task {mmcss_task_index} priority restored.");
        })
        .map_err(|win32_error| {
            AudioThreadPriorityError::new(&format!(
                "Unable to restore the thread priority for task {mmcss_task_index} ({win32_error})"
            ))
        })
}

mod avrt_lib {
    use super::win32_utils::{win32_error_if, OwnedLibrary};
    use std::sync::Once;
    use windows_sys::{
        core::PCWSTR,
        s, w,
        Win32::Foundation::{BOOL, FALSE, HANDLE, WIN32_ERROR},
    };

    type AvSetMmThreadCharacteristicsWFn = unsafe extern "system" fn(PCWSTR, *mut u32) -> HANDLE;
    type AvRevertMmThreadCharacteristicsFn = unsafe extern "system" fn(HANDLE) -> BOOL;

    #[derive(Debug)]
    pub(super) struct AvRtLibrary {
        // This field is never read because only used for its Drop behavior
        #[allow(dead_code)]
        module: OwnedLibrary,

        av_set_mm_thread_characteristics_w: AvSetMmThreadCharacteristicsWFn,
        av_revert_mm_thread_characteristics: AvRevertMmThreadCharacteristicsFn,
    }

    impl AvRtLibrary {
        pub(super) fn try_new() -> Result<Self, WIN32_ERROR> {
            let module = OwnedLibrary::try_new(w!("avrt.dll"))?;
            let av_set_mm_thread_characteristics_w = unsafe {
                std::mem::transmute::<_, AvSetMmThreadCharacteristicsWFn>(
                    module.get_proc(s!("AvSetMmThreadCharacteristicsW"))?,
                )
            };
            let av_revert_mm_thread_characteristics = unsafe {
                std::mem::transmute::<_, AvRevertMmThreadCharacteristicsFn>(
                    module.get_proc(s!("AvRevertMmThreadCharacteristics"))?,
                )
            };
            Ok(Self {
                module,
                av_set_mm_thread_characteristics_w,
                av_revert_mm_thread_characteristics,
            })
        }

        pub(super) fn set_mm_thread_characteristics(
            &self,
            task_name: PCWSTR,
        ) -> Result<(u32, HANDLE), WIN32_ERROR> {
            // Ensure that the first call never runs in parallel with other calls. This
            // seems necessary to guarantee the success of these other calls. We saw them
            // fail with an error code of ERROR_PATH_NOT_FOUND in tests, presumably on a
            // machine where the MMCSS service was initially inactive.
            static FIRST_CALL: Once = Once::new();
            let mut first_call_result = None;
            FIRST_CALL.call_once(|| {
                first_call_result = Some(self.set_mm_thread_characteristics_internal(task_name))
            });
            first_call_result
                .unwrap_or_else(|| self.set_mm_thread_characteristics_internal(task_name))
        }

        fn set_mm_thread_characteristics_internal(
            &self,
            task_name: PCWSTR,
        ) -> Result<(u32, HANDLE), WIN32_ERROR> {
            let mut mmcss_task_index = 0u32;
            let task_handle = unsafe {
                (self.av_set_mm_thread_characteristics_w)(task_name, &mut mmcss_task_index)
            };
            win32_error_if(task_handle == 0)?;
            Ok((mmcss_task_index, task_handle))
        }

        pub(super) fn revert_mm_thread_characteristics(
            &self,
            handle: HANDLE,
        ) -> Result<(), WIN32_ERROR> {
            let rv = unsafe { (self.av_revert_mm_thread_characteristics)(handle) };
            win32_error_if(rv == FALSE)
        }
    }
}

mod win32_utils {
    use windows_sys::{
        core::{PCSTR, PCWSTR},
        Win32::{
            Foundation::{FreeLibrary, GetLastError, HMODULE, WIN32_ERROR},
            System::LibraryLoader::{GetProcAddress, LoadLibraryW},
        },
    };

    pub(super) fn win32_error_if(condition: bool) -> Result<(), WIN32_ERROR> {
        if condition {
            Err(unsafe { GetLastError() })
        } else {
            Ok(())
        }
    }

    #[derive(Debug)]
    pub(super) struct OwnedLibrary(HMODULE);

    impl OwnedLibrary {
        pub(super) fn try_new(lib_file_name: PCWSTR) -> Result<Self, WIN32_ERROR> {
            let module = unsafe { LoadLibraryW(lib_file_name) };
            win32_error_if(module == 0)?;
            Ok(Self(module))
        }

        fn raw(&self) -> HMODULE {
            self.0
        }

        /// SAFETY: The caller must transmute the value wrapped in a Ok(_) to the correct
        ///         function type, with the correct extern specifier.
        pub(super) unsafe fn get_proc(
            &self,
            proc_name: PCSTR,
        ) -> Result<unsafe extern "system" fn() -> isize, WIN32_ERROR> {
            let proc = unsafe { GetProcAddress(self.raw(), proc_name) };
            win32_error_if(proc.is_none())?;
            Ok(proc.unwrap())
        }
    }

    impl Drop for OwnedLibrary {
        fn drop(&mut self) {
            unsafe {
                FreeLibrary(self.raw());
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{
        avrt, demote_current_thread_from_real_time_internal,
        promote_current_thread_to_real_time_internal,
    };

    #[test]
    fn test_successful_avrt_library_load() {
        assert!(avrt().is_ok())
    }

    #[test]
    fn test_successful_api_use() {
        let handle = promote_current_thread_to_real_time_internal(512, 44100);
        println!("handle: {handle:?}");
        assert!(handle.is_ok());

        let result = demote_current_thread_from_real_time_internal(handle.unwrap());
        println!("result: {result:?}");
        assert!(result.is_ok());
    }
}
