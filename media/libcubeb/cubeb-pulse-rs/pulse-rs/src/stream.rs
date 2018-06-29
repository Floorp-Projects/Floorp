// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ::*;
use context;
use ffi;
use operation;
use std::ffi::CStr;
use std::mem;
use std::os::raw::{c_int, c_void};
use std::ptr;
use util::*;

#[derive(Debug)]
pub struct Stream(*mut ffi::pa_stream);

impl Stream {
    pub fn new<'a, CM>(c: &Context, name: &::std::ffi::CStr, ss: &SampleSpec, map: CM) -> Option<Self>
        where CM: Into<Option<&'a ChannelMap>>
    {
        let ptr = unsafe {
            ffi::pa_stream_new(c.raw_mut(),
                               name.as_ptr(),
                               ss as *const _,
                               to_ptr(map.into()))
        };
        if ptr.is_null() {
            None
        } else {
            Some(Stream(ptr))
        }
    }

    #[doc(hidden)]
    pub fn raw_mut(&self) -> &mut ffi::pa_stream {
        unsafe { &mut *self.0 }
    }

    pub fn unref(self) {
        unsafe {
            ffi::pa_stream_unref(self.raw_mut());
        }
    }

    pub fn get_state(&self) -> StreamState {
        StreamState::try_from(unsafe {
            ffi::pa_stream_get_state(self.raw_mut())
        }).expect("pa_stream_get_state returned invalid StreamState")
    }

    pub fn get_context(&self) -> Option<Context> {
        let ptr = unsafe { ffi::pa_stream_get_context(self.raw_mut()) };
        if ptr.is_null() {
            return None;
        }

        let ctx = unsafe { context::from_raw_ptr(ptr) };
        Some(ctx)
    }

    pub fn get_index(&self) -> u32 {
        unsafe { ffi::pa_stream_get_index(self.raw_mut()) }
    }

    pub fn get_device_name<'a>(&'a self) -> Result<&'a CStr> {
        let r = unsafe { ffi::pa_stream_get_device_name(self.raw_mut()) };
        if r.is_null() {
            let err = if let Some(c) = self.get_context() {
                c.errno()
            } else {
                ffi::PA_ERR_UNKNOWN
            };
            return Err(ErrorCode::from_error_code(err));
        }
        Ok(unsafe { CStr::from_ptr(r) })
    }

    pub fn is_suspended(&self) -> Result<bool> {
        let r = unsafe { ffi::pa_stream_is_suspended(self.raw_mut()) };
        error_result!(r != 0, r)
    }

    pub fn is_corked(&self) -> Result<bool> {
        let r = unsafe { ffi::pa_stream_is_corked(self.raw_mut()) };
        error_result!(r != 0, r)
    }

    pub fn connect_playback<'a, D, A, V, S>(&self,
                                            dev: D,
                                            attr: A,
                                            flags: StreamFlags,
                                            volume: V,
                                            sync_stream: S)
                                            -> Result<()>
        where D: Into<Option<&'a CStr>>,
              A: Into<Option<&'a BufferAttr>>,
              V: Into<Option<&'a CVolume>>,
              S: Into<Option<&'a mut Stream>>
    {
        let r = unsafe {
            ffi::pa_stream_connect_playback(self.raw_mut(),
                                            str_to_ptr(dev.into()),
                                            to_ptr(attr.into()),
                                            flags.into(),
                                            to_ptr(volume.into()),
                                            map_to_mut_ptr(sync_stream.into(), |p| p.0))
        };
        error_result!((), r)
    }

    pub fn connect_record<'a, D, A>(&self, dev: D, attr: A, flags: StreamFlags) -> Result<()>
        where D: Into<Option<&'a CStr>>,
              A: Into<Option<&'a BufferAttr>>
    {
        let r = unsafe {
            ffi::pa_stream_connect_record(self.raw_mut(),
                                          str_to_ptr(dev.into()),
                                          to_ptr(attr.into()),
                                          flags.into())
        };
        error_result!((), r)
    }

    pub fn disconnect(&self) -> Result<()> {
        let r = unsafe { ffi::pa_stream_disconnect(self.raw_mut()) };
        error_result!((), r)
    }

    pub fn begin_write(&self, req_bytes: usize) -> Result<(*mut c_void, usize)> {
        let mut data: *mut c_void = ptr::null_mut();
        let mut nbytes = req_bytes;
        let r = unsafe { ffi::pa_stream_begin_write(self.raw_mut(), &mut data, &mut nbytes) };
        error_result!((data, nbytes), r)
    }

    pub fn cancel_write(&self) -> Result<()> {
        let r = unsafe { ffi::pa_stream_cancel_write(self.raw_mut()) };
        error_result!((), r)
    }

    pub fn write(&self, data: *const c_void, nbytes: usize, offset: i64, seek: SeekMode) -> Result<()> {
        let r = unsafe { ffi::pa_stream_write(self.raw_mut(), data, nbytes, None, offset, seek.into()) };
        error_result!((), r)
    }

    pub unsafe fn peek(&self, data: *mut *const c_void, length: *mut usize) -> Result<()> {
        let r = ffi::pa_stream_peek(self.raw_mut(), data, length);
        error_result!((), r)
    }

    pub fn drop(&self) -> Result<()> {
        let r = unsafe { ffi::pa_stream_drop(self.raw_mut()) };
        error_result!((), r)
    }

    pub fn writable_size(&self) -> Result<usize> {
        let r = unsafe { ffi::pa_stream_writable_size(self.raw_mut()) };
        if r == ::std::usize::MAX {
            let err = if let Some(c) = self.get_context() {
                c.errno()
            } else {
                ffi::PA_ERR_UNKNOWN
            };
            return Err(ErrorCode::from_error_code(err));
        }
        Ok(r)
    }

    pub fn readable_size(&self) -> Result<usize> {
        let r = unsafe { ffi::pa_stream_readable_size(self.raw_mut()) };
        if r == ::std::usize::MAX {
            let err = if let Some(c) = self.get_context() {
                c.errno()
            } else {
                ffi::PA_ERR_UNKNOWN
            };
            return Err(ErrorCode::from_error_code(err));
        }
        Ok(r)
    }

    pub fn update_timing_info<CB>(&self, _: CB, userdata: *mut c_void) -> Result<Operation>
        where CB: Fn(&Stream, i32, *mut c_void)
    {
        debug_assert_eq!(mem::size_of::<CB>(), 0);

        // See: A note about `wrapped` functions
        unsafe extern "C" fn wrapped<F>(s: *mut ffi::pa_stream, success: c_int, userdata: *mut c_void)
            where F: Fn(&Stream, i32, *mut c_void)
        {
            use std::mem::{forget, uninitialized};
            let mut stm = stream::from_raw_ptr(s);
            let result = uninitialized::<F>()(&mut stm, success, userdata);
            forget(stm);

            result
        }

        let r = unsafe { ffi::pa_stream_update_timing_info(self.raw_mut(), Some(wrapped::<CB>), userdata) };
        if r.is_null() {
            let err = if let Some(c) = self.get_context() {
                c.errno()
            } else {
                ffi::PA_ERR_UNKNOWN
            };
            return Err(ErrorCode::from_error_code(err));
        }
        Ok(unsafe { operation::from_raw_ptr(r) })
    }

    pub fn clear_state_callback(&self) {
        unsafe {
            ffi::pa_stream_set_state_callback(self.raw_mut(), None, ptr::null_mut());
        }
    }

    pub fn set_state_callback<CB>(&self, _: CB, userdata: *mut c_void)
        where CB: Fn(&Stream, *mut c_void)
    {
        debug_assert_eq!(mem::size_of::<CB>(), 0);

        // See: A note about `wrapped` functions
        unsafe extern "C" fn wrapped<F>(s: *mut ffi::pa_stream, userdata: *mut c_void)
            where F: Fn(&Stream, *mut c_void)
        {
            use std::mem::{forget, uninitialized};
            let mut stm = stream::from_raw_ptr(s);
            let result = uninitialized::<F>()(&mut stm, userdata);
            forget(stm);

            result
        }

        unsafe {
            ffi::pa_stream_set_state_callback(self.raw_mut(), Some(wrapped::<CB>), userdata);
        }
    }

    pub fn clear_write_callback(&self) {
        unsafe {
            ffi::pa_stream_set_write_callback(self.raw_mut(), None, ptr::null_mut());
        }
    }

    pub fn set_write_callback<CB>(&self, _: CB, userdata: *mut c_void)
        where CB: Fn(&Stream, usize, *mut c_void)
    {
        debug_assert_eq!(mem::size_of::<CB>(), 0);

        // See: A note about `wrapped` functions
        unsafe extern "C" fn wrapped<F>(s: *mut ffi::pa_stream, nbytes: usize, userdata: *mut c_void)
            where F: Fn(&Stream, usize, *mut c_void)
        {
            use std::mem::{forget, uninitialized};
            let mut stm = stream::from_raw_ptr(s);
            let result = uninitialized::<F>()(&mut stm, nbytes, userdata);
            forget(stm);

            result
        }

        unsafe {
            ffi::pa_stream_set_write_callback(self.raw_mut(), Some(wrapped::<CB>), userdata);
        }
    }

    pub fn clear_read_callback(&self) {
        unsafe {
            ffi::pa_stream_set_read_callback(self.raw_mut(), None, ptr::null_mut());
        }
    }

    pub fn set_read_callback<CB>(&self, _: CB, userdata: *mut c_void)
        where CB: Fn(&Stream, usize, *mut c_void)
    {
        debug_assert_eq!(mem::size_of::<CB>(), 0);

        // See: A note about `wrapped` functions
        unsafe extern "C" fn wrapped<F>(s: *mut ffi::pa_stream, nbytes: usize, userdata: *mut c_void)
            where F: Fn(&Stream, usize, *mut c_void)
        {
            use std::mem::{forget, uninitialized};
            let mut stm = stream::from_raw_ptr(s);
            let result = uninitialized::<F>()(&mut stm, nbytes, userdata);
            forget(stm);

            result
        }

        unsafe {
            ffi::pa_stream_set_read_callback(self.raw_mut(), Some(wrapped::<CB>), userdata);
        }
    }

    pub fn cork<CB>(&self, b: i32, _: CB, userdata: *mut c_void) -> Result<Operation>
        where CB: Fn(&Stream, i32, *mut c_void)
    {
        debug_assert_eq!(mem::size_of::<CB>(), 0);

        // See: A note about `wrapped` functions
        unsafe extern "C" fn wrapped<F>(s: *mut ffi::pa_stream, success: c_int, userdata: *mut c_void)
            where F: Fn(&Stream, i32, *mut c_void)
        {
            use std::mem::{forget, uninitialized};
            let mut stm = stream::from_raw_ptr(s);
            let result = uninitialized::<F>()(&mut stm, success, userdata);
            forget(stm);

            result
        }

        let r = unsafe { ffi::pa_stream_cork(self.raw_mut(), b, Some(wrapped::<CB>), userdata) };
        if r.is_null() {
            let err = if let Some(c) = self.get_context() {
                c.errno()
            } else {
                ffi::PA_ERR_UNKNOWN
            };
            return Err(ErrorCode::from_error_code(err));
        }
        Ok(unsafe { operation::from_raw_ptr(r) })
    }

    pub fn get_time(&self) -> Result<(USec)> {
        let mut usec: USec = 0;
        let r = unsafe { ffi::pa_stream_get_time(self.raw_mut(), &mut usec) };
        error_result!(usec, r)
    }

    pub fn get_latency(&self) -> Result<StreamLatency> {
        let mut usec: u64 = 0;
        let mut negative: i32 = 0;
        let r = unsafe { ffi::pa_stream_get_latency(self.raw_mut(), &mut usec, &mut negative) };
        error_result!(
            if negative == 0 {
                StreamLatency::Positive(usec)
            } else {
                StreamLatency::Negative(usec)
            },
            r
        )
    }

    pub fn get_sample_spec(&self) -> &SampleSpec {
        unsafe {
            let ptr = ffi::pa_stream_get_sample_spec(self.raw_mut());
            debug_assert!(!ptr.is_null());
            &*ptr
        }
    }

    pub fn get_channel_map(&self) -> &ChannelMap {
        unsafe {
            let ptr = ffi::pa_stream_get_channel_map(self.raw_mut());
            debug_assert!(!ptr.is_null());
            &*ptr
        }
    }

    pub fn get_buffer_attr(&self) -> &BufferAttr {
        unsafe {
            let ptr = ffi::pa_stream_get_buffer_attr(self.raw_mut());
            debug_assert!(!ptr.is_null());
            &*ptr
        }
    }
}

#[doc(hidden)]
pub unsafe fn from_raw_ptr(ptr: *mut ffi::pa_stream) -> Stream {
    Stream(ptr)
}
