// Copyright 2020 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use std::path::Path;

/// https://developer.apple.com/documentation/metal/mtlcapturedestination?language=objc
#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLCaptureDestination {
    DeveloperTools = 1,
    GpuTraceDocument = 2,
}

/// https://developer.apple.com/documentation/metal/mtlcapturedescriptor
pub enum MTLCaptureDescriptor {}

foreign_obj_type! {
    type CType = MTLCaptureDescriptor;
    pub struct CaptureDescriptor;
    pub struct CaptureDescriptorRef;
}

impl CaptureDescriptor {
    pub fn new() -> Self {
        unsafe {
            let class = class!(MTLCaptureDescriptor);
            msg_send![class, new]
        }
    }
}

impl CaptureDescriptorRef {
    /// https://developer.apple.com/documentation/metal/mtlcapturedescriptor/3237248-captureobject
    pub fn set_capture_device(&self, device: &DeviceRef) {
        unsafe { msg_send![self, setCaptureObject: device] }
    }

    /// https://developer.apple.com/documentation/metal/mtlcapturedescriptor/3237248-captureobject
    pub fn set_capture_scope(&self, scope: &CaptureScopeRef) {
        unsafe { msg_send![self, setCaptureObject: scope] }
    }

    /// https://developer.apple.com/documentation/metal/mtlcapturedescriptor/3237248-captureobject
    pub fn set_capture_command_queue(&self, command_queue: &CommandQueueRef) {
        unsafe { msg_send![self, setCaptureObject: command_queue] }
    }

    /// https://developer.apple.com/documentation/metal/mtlcapturedescriptor/3237250-outputurl
    pub fn output_url(&self) -> &Path {
        let output_url = unsafe { msg_send![self, outputURL] };
        let output_url = nsstring_as_str(output_url);

        Path::new(output_url)
    }

    /// https://developer.apple.com/documentation/metal/mtlcapturedescriptor/3237250-outputurl
    pub fn set_output_url<P: AsRef<Path>>(&self, output_url: P) {
        let output_url = nsstring_from_str(output_url.as_ref().to_str().unwrap());

        unsafe { msg_send![self, setOutputURL: output_url] }
    }

    /// https://developer.apple.com/documentation/metal/mtlcapturedescriptor?language=objc
    pub fn destination(&self) -> MTLCaptureDestination {
        unsafe { msg_send![self, destination] }
    }

    /// https://developer.apple.com/documentation/metal/mtlcapturedescriptor?language=objc
    pub fn set_destination(&self, destination: MTLCaptureDestination) {
        unsafe { msg_send![self, setDestination: destination] }
    }
}
