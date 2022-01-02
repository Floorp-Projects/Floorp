use super::auto_release::*;
use cubeb_backend::ffi;
use std::os::raw::{c_long, c_uint, c_void};
use std::ptr;

#[derive(Debug)]
pub struct Resampler(AutoRelease<ffi::cubeb_resampler>);

impl Resampler {
    pub fn new(
        stream: *mut ffi::cubeb_stream,
        mut input_params: Option<ffi::cubeb_stream_params>,
        mut output_params: Option<ffi::cubeb_stream_params>,
        target_rate: c_uint,
        data_callback: ffi::cubeb_data_callback,
        user_ptr: *mut c_void,
    ) -> Self {
        let raw_resampler = unsafe {
            let in_params = if input_params.is_some() {
                input_params.as_mut().unwrap() as *mut ffi::cubeb_stream_params
            } else {
                ptr::null_mut()
            };
            let out_params = if output_params.is_some() {
                output_params.as_mut().unwrap() as *mut ffi::cubeb_stream_params
            } else {
                ptr::null_mut()
            };
            ffi::cubeb_resampler_create(
                stream,
                in_params,
                out_params,
                target_rate,
                data_callback,
                user_ptr,
                ffi::CUBEB_RESAMPLER_QUALITY_DESKTOP,
            )
        };
        assert!(!raw_resampler.is_null(), "Failed to create resampler");
        let resampler = AutoRelease::new(raw_resampler, ffi::cubeb_resampler_destroy);
        Self(resampler)
    }

    pub fn fill(
        &mut self,
        input_buffer: *mut c_void,
        input_frame_count: *mut c_long,
        output_buffer: *mut c_void,
        output_frames_needed: c_long,
    ) -> c_long {
        unsafe {
            ffi::cubeb_resampler_fill(
                self.0.as_mut(),
                input_buffer,
                input_frame_count,
                output_buffer,
                output_frames_needed,
            )
        }
    }

    pub fn destroy(&mut self) {
        if !self.0.as_ptr().is_null() {
            self.0.reset(ptr::null_mut());
        }
    }
}

impl Drop for Resampler {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl Default for Resampler {
    fn default() -> Self {
        Self(AutoRelease::new(
            ptr::null_mut(),
            ffi::cubeb_resampler_destroy,
        ))
    }
}
