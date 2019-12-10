use super::auto_release::*;
use super::utils::cubeb_sample_size;
use cubeb_backend::{ffi, ChannelLayout, SampleFormat};
use std::mem;
use std::os::raw::{c_int, c_void};
use std::ptr;

#[derive(Debug)]
pub struct Mixer {
    mixer: AutoRelease<ffi::cubeb_mixer>,
    format: SampleFormat,
    in_channels: u32,
    in_layout: ChannelLayout,
    out_channels: u32,
    out_layout: ChannelLayout,
    // Only accessed from callback thread.
    buffer: Vec<u8>,
}

impl Mixer {
    pub fn new(
        format: SampleFormat,
        in_channels: u32,
        in_layout: ChannelLayout,
        out_channels: u32,
        out_layout: ChannelLayout,
    ) -> Self {
        let raw_mixer = unsafe {
            ffi::cubeb_mixer_create(
                format.into(),
                in_channels,
                in_layout.into(),
                out_channels,
                out_layout.into(),
            )
        };
        assert!(!raw_mixer.is_null(), "Failed to create mixer");
        Self {
            mixer: AutoRelease::new(raw_mixer, ffi::cubeb_mixer_destroy),
            format,
            in_channels,
            in_layout,
            out_channels,
            out_layout,
            buffer: Vec::new(),
        }
    }

    pub fn update_buffer_size(&mut self, frames: usize) -> bool {
        let size_needed = frames * self.in_channels as usize * cubeb_sample_size(self.format);
        let elements_needed = size_needed / mem::size_of::<u8>();
        if self.buffer.len() < elements_needed {
            self.buffer.resize(elements_needed, 0);
            true
        } else {
            false
        }
    }

    pub fn get_buffer_mut_ptr(&mut self) -> *mut u8 {
        self.buffer.as_mut_ptr()
    }

    fn get_buffer_info(&self) -> (*const u8, usize) {
        (
            self.buffer.as_ptr(),
            self.buffer.len() * mem::size_of::<u8>(),
        )
    }

    // `update_buffer_size` must be called before this.
    pub fn mix(
        &mut self,
        frames: usize,
        dest_buffer: *mut c_void,
        dest_buffer_size: usize,
    ) -> c_int {
        let size_needed = frames * self.in_channels as usize * cubeb_sample_size(self.format);
        let (src_buffer_ptr, src_buffer_size) = self.get_buffer_info();
        assert!(src_buffer_size >= size_needed);
        unsafe {
            ffi::cubeb_mixer_mix(
                self.mixer.as_mut(),
                frames,
                src_buffer_ptr as *const c_void,
                src_buffer_size,
                dest_buffer,
                dest_buffer_size,
            )
        }
    }

    fn destroy(&mut self) {
        if !self.mixer.as_ptr().is_null() {
            self.mixer.reset(ptr::null_mut());
        }
    }
}

impl Drop for Mixer {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl Default for Mixer {
    fn default() -> Self {
        Self {
            mixer: AutoRelease::new(ptr::null_mut(), ffi::cubeb_mixer_destroy),
            format: SampleFormat::Float32NE,
            in_channels: 0,
            in_layout: ChannelLayout::UNDEFINED,
            out_channels: 0,
            out_layout: ChannelLayout::UNDEFINED,
            buffer: Vec::default(),
        }
    }
}
