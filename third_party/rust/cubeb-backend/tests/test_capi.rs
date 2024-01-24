// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

#![cfg_attr(feature = "cargo-clippy", allow(clippy::float_cmp))]

#[macro_use]
extern crate cubeb_backend;

use cubeb_backend::{
    ffi, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceRef, DeviceType,
    InputProcessingParams, Ops, Result, Stream, StreamOps, StreamParams, StreamParamsRef,
};
use std::ffi::CStr;
use std::os::raw::c_void;
use std::ptr;

pub const OPS: Ops = capi_new!(TestContext, TestStream);

struct TestContext {
    #[allow(dead_code)]
    pub ops: *const Ops,
}

impl ContextOps for TestContext {
    fn init(_context_name: Option<&CStr>) -> Result<Context> {
        let ctx = Box::new(TestContext {
            ops: &OPS as *const _,
        });
        Ok(unsafe { Context::from_ptr(Box::into_raw(ctx) as *mut _) })
    }

    fn backend_id(&mut self) -> &'static CStr {
        unsafe { CStr::from_ptr(b"remote\0".as_ptr() as *const _) }
    }
    fn max_channel_count(&mut self) -> Result<u32> {
        Ok(0u32)
    }
    fn min_latency(&mut self, _params: StreamParams) -> Result<u32> {
        Ok(0u32)
    }
    fn preferred_sample_rate(&mut self) -> Result<u32> {
        Ok(0u32)
    }
    fn supported_input_processing_params(&mut self) -> Result<InputProcessingParams> {
        Ok(InputProcessingParams::NONE)
    }
    fn enumerate_devices(
        &mut self,
        _devtype: DeviceType,
        collection: &DeviceCollectionRef,
    ) -> Result<()> {
        let coll = unsafe { &mut *collection.as_ptr() };
        coll.device = 0xDEAD_BEEF as *mut _;
        coll.count = usize::max_value();
        Ok(())
    }
    fn device_collection_destroy(&mut self, collection: &mut DeviceCollectionRef) -> Result<()> {
        let coll = unsafe { &mut *collection.as_ptr() };
        assert_eq!(coll.device, 0xDEAD_BEEF as *mut _);
        assert_eq!(coll.count, usize::max_value());
        coll.device = ptr::null_mut();
        coll.count = 0;
        Ok(())
    }
    fn stream_init(
        &mut self,
        _stream_name: Option<&CStr>,
        _input_device: DeviceId,
        _input_stream_params: Option<&StreamParamsRef>,
        _output_device: DeviceId,
        _output_stream_params: Option<&StreamParamsRef>,
        _latency_frame: u32,
        _data_callback: ffi::cubeb_data_callback,
        _state_callback: ffi::cubeb_state_callback,
        _user_ptr: *mut c_void,
    ) -> Result<Stream> {
        Ok(unsafe { Stream::from_ptr(0xDEAD_BEEF as *mut _) })
    }
    fn register_device_collection_changed(
        &mut self,
        _dev_type: DeviceType,
        _collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
        _user_ptr: *mut c_void,
    ) -> Result<()> {
        Ok(())
    }
}

struct TestStream {}

impl StreamOps for TestStream {
    fn start(&mut self) -> Result<()> {
        Ok(())
    }
    fn stop(&mut self) -> Result<()> {
        Ok(())
    }
    fn position(&mut self) -> Result<u64> {
        Ok(0u64)
    }
    fn latency(&mut self) -> Result<u32> {
        Ok(0u32)
    }
    fn input_latency(&mut self) -> Result<u32> {
        Ok(0u32)
    }
    fn set_volume(&mut self, volume: f32) -> Result<()> {
        assert_eq!(volume, 0.5);
        Ok(())
    }
    fn set_name(&mut self, name: &CStr) -> Result<()> {
        assert_eq!(name, CStr::from_bytes_with_nul(b"test\0").unwrap());
        Ok(())
    }
    fn current_device(&mut self) -> Result<&DeviceRef> {
        Ok(unsafe { DeviceRef::from_ptr(0xDEAD_BEEF as *mut _) })
    }
    fn set_input_mute(&mut self, mute: bool) -> Result<()> {
        assert_eq!(mute, true);
        Ok(())
    }
    fn set_input_processing_params(&mut self, params: InputProcessingParams) -> Result<()> {
        assert_eq!(params, InputProcessingParams::ECHO_CANCELLATION);
        Ok(())
    }
    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()> {
        assert_eq!(device.as_ptr(), 0xDEAD_BEEF as *mut _);
        Ok(())
    }
    fn register_device_changed_callback(
        &mut self,
        _: ffi::cubeb_device_changed_callback,
    ) -> Result<()> {
        Ok(())
    }
}

#[test]
fn test_ops_context_init() {
    let mut c: *mut ffi::cubeb = ptr::null_mut();
    assert_eq!(
        unsafe { OPS.init.unwrap()(&mut c, ptr::null()) },
        ffi::CUBEB_OK
    );
    unsafe { OPS.destroy.unwrap()(c) }
}

#[test]
fn test_ops_context_max_channel_count() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut max_channel_count = u32::max_value();
    assert_eq!(
        unsafe { OPS.get_max_channel_count.unwrap()(c, &mut max_channel_count) },
        ffi::CUBEB_OK
    );
    assert_eq!(max_channel_count, 0);
}

#[test]
fn test_ops_context_min_latency() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let params: ffi::cubeb_stream_params = unsafe { ::std::mem::zeroed() };
    let mut latency = u32::max_value();
    assert_eq!(
        unsafe { OPS.get_min_latency.unwrap()(c, params, &mut latency) },
        ffi::CUBEB_OK
    );
    assert_eq!(latency, 0);
}

#[test]
fn test_ops_context_preferred_sample_rate() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut rate = u32::max_value();
    assert_eq!(
        unsafe { OPS.get_preferred_sample_rate.unwrap()(c, &mut rate) },
        ffi::CUBEB_OK
    );
    assert_eq!(rate, 0);
}

#[test]
fn test_ops_context_supported_input_processing_params() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut params: ffi::cubeb_input_processing_params = InputProcessingParams::all().bits();
    assert_eq!(
        unsafe { OPS.get_supported_input_processing_params.unwrap()(c, &mut params) },
        ffi::CUBEB_OK
    );
    assert_eq!(params, ffi::CUBEB_INPUT_PROCESSING_PARAM_NONE);
}

#[test]
fn test_ops_context_enumerate_devices() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut coll = ffi::cubeb_device_collection {
        device: ptr::null_mut(),
        count: 0,
    };
    assert_eq!(
        unsafe { OPS.enumerate_devices.unwrap()(c, 0, &mut coll) },
        ffi::CUBEB_OK
    );
    assert_eq!(coll.device, 0xDEAD_BEEF as *mut _);
    assert_eq!(coll.count, usize::max_value())
}

#[test]
fn test_ops_context_device_collection_destroy() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut coll = ffi::cubeb_device_collection {
        device: 0xDEAD_BEEF as *mut _,
        count: usize::max_value(),
    };
    assert_eq!(
        unsafe { OPS.device_collection_destroy.unwrap()(c, &mut coll) },
        ffi::CUBEB_OK
    );
    assert_eq!(coll.device, ptr::null_mut());
    assert_eq!(coll.count, 0);
}

// stream_init: Some($crate::capi::capi_stream_init::<$ctx>),
// stream_destroy: Some($crate::capi::capi_stream_destroy::<$stm>),
// stream_start: Some($crate::capi::capi_stream_start::<$stm>),
// stream_stop: Some($crate::capi::capi_stream_stop::<$stm>),
// stream_get_position: Some($crate::capi::capi_stream_get_position::<$stm>),

#[test]
fn test_ops_stream_latency() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    let mut latency = u32::max_value();
    assert_eq!(
        unsafe { OPS.stream_get_latency.unwrap()(s, &mut latency) },
        ffi::CUBEB_OK
    );
    assert_eq!(latency, 0);
}

#[test]
fn test_ops_stream_set_volume() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    unsafe {
        OPS.stream_set_volume.unwrap()(s, 0.5);
    }
}

#[test]
fn test_ops_stream_set_name() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    unsafe {
        OPS.stream_set_name.unwrap()(s, CStr::from_bytes_with_nul(b"test\0").unwrap().as_ptr());
    }
}

#[test]
fn test_ops_stream_current_device() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    let mut device: *mut ffi::cubeb_device = ptr::null_mut();
    assert_eq!(
        unsafe { OPS.stream_get_current_device.unwrap()(s, &mut device) },
        ffi::CUBEB_OK
    );
    assert_eq!(device, 0xDEAD_BEEF as *mut _);
}

#[test]
fn test_ops_stream_set_input_mute() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    assert_eq!(
        unsafe { OPS.stream_set_input_mute.unwrap()(s, 1) },
        ffi::CUBEB_OK
    );
}

#[test]
fn test_ops_stream_set_input_processing_params() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    assert_eq!(
        unsafe {
            OPS.stream_set_input_processing_params.unwrap()(
                s,
                ffi::CUBEB_INPUT_PROCESSING_PARAM_ECHO_CANCELLATION,
            )
        },
        ffi::CUBEB_OK
    );
}

#[test]
fn test_ops_stream_device_destroy() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    unsafe {
        OPS.stream_device_destroy.unwrap()(s, 0xDEAD_BEEF as *mut _);
    }
}
