#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{DeviceV1_0, InstanceV1_0};
use crate::vk;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DebugMarker {
    debug_marker_fn: vk::ExtDebugMarkerFn,
}

impl DebugMarker {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(instance: &I, device: &D) -> DebugMarker {
        let debug_marker_fn = vk::ExtDebugMarkerFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        DebugMarker { debug_marker_fn }
    }

    pub fn name() -> &'static CStr {
        vk::ExtDebugMarkerFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDebugMarkerSetObjectNameEXT.html>"]
    pub unsafe fn debug_marker_set_object_name(
        &self,
        device: vk::Device,
        name_info: &vk::DebugMarkerObjectNameInfoEXT,
    ) -> VkResult<()> {
        let err_code = self
            .debug_marker_fn
            .debug_marker_set_object_name_ext(device, name_info);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDebugMarkerBeginEXT.html>"]
    pub unsafe fn cmd_debug_marker_begin(
        &self,
        command_buffer: vk::CommandBuffer,
        marker_info: &vk::DebugMarkerMarkerInfoEXT,
    ) {
        self.debug_marker_fn
            .cmd_debug_marker_begin_ext(command_buffer, marker_info);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDebugMarkerEndEXT.html>"]
    pub unsafe fn cmd_debug_marker_end(&self, command_buffer: vk::CommandBuffer) {
        self.debug_marker_fn
            .cmd_debug_marker_end_ext(command_buffer);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDebugMarkerInsertEXT.html>"]
    pub unsafe fn cmd_debug_marker_insert(
        &self,
        command_buffer: vk::CommandBuffer,
        marker_info: &vk::DebugMarkerMarkerInfoEXT,
    ) {
        self.debug_marker_fn
            .cmd_debug_marker_insert_ext(command_buffer, marker_info);
    }

    pub fn fp(&self) -> &vk::ExtDebugMarkerFn {
        &self.debug_marker_fn
    }
}
