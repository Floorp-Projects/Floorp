// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use cubeb_core::{Context, DeviceCollectionRef, DeviceId, DeviceRef, DeviceType, Result, Stream,
                 StreamParams, StreamParamsRef};
use ffi;
use std::ffi::CStr;
use std::os::raw::c_void;

pub trait ContextOps {
    fn init(context_name: Option<&CStr>) -> Result<Context>;
    fn backend_id(&mut self) -> &CStr;
    fn max_channel_count(&mut self) -> Result<u32>;
    fn min_latency(&mut self, params: StreamParams) -> Result<u32>;
    fn preferred_sample_rate(&mut self) -> Result<u32>;
    fn enumerate_devices(
        &mut self,
        devtype: DeviceType,
        collection: &DeviceCollectionRef,
    ) -> Result<()>;
    fn device_collection_destroy(&mut self, collection: &mut DeviceCollectionRef) -> Result<()>;
    #[cfg_attr(feature = "cargo-clippy", allow(too_many_arguments))]
    fn stream_init(
        &mut self,
        stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&StreamParamsRef>,
        output_device: DeviceId,
        output_stream_params: Option<&StreamParamsRef>,
        latency_frames: u32,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Stream>;
    fn register_device_collection_changed(
        &mut self,
        devtype: DeviceType,
        cb: ffi::cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> Result<()>;
}

pub trait StreamOps {
    fn start(&mut self) -> Result<()>;
    fn stop(&mut self) -> Result<()>;
    fn reset_default_device(&mut self) -> Result<()>;
    fn position(&mut self) -> Result<u64>;
    fn latency(&mut self) -> Result<u32>;
    fn input_latency(&mut self) -> Result<u32>;
    fn set_volume(&mut self, volume: f32) -> Result<()>;
    fn current_device(&mut self) -> Result<&DeviceRef>;
    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()>;
    fn register_device_changed_callback(
        &mut self,
        device_changed_callback: ffi::cubeb_device_changed_callback,
    ) -> Result<()>;
}
