use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DebugMarker {
    handle: vk::Device,
    fp: vk::ExtDebugMarkerFn,
}

impl DebugMarker {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::ExtDebugMarkerFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDebugMarkerSetObjectNameEXT.html>
    #[inline]
    pub unsafe fn debug_marker_set_object_name(
        &self,
        name_info: &vk::DebugMarkerObjectNameInfoEXT,
    ) -> VkResult<()> {
        (self.fp.debug_marker_set_object_name_ext)(self.handle, name_info).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdDebugMarkerBeginEXT.html>
    #[inline]
    pub unsafe fn cmd_debug_marker_begin(
        &self,
        command_buffer: vk::CommandBuffer,
        marker_info: &vk::DebugMarkerMarkerInfoEXT,
    ) {
        (self.fp.cmd_debug_marker_begin_ext)(command_buffer, marker_info);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdDebugMarkerEndEXT.html>
    #[inline]
    pub unsafe fn cmd_debug_marker_end(&self, command_buffer: vk::CommandBuffer) {
        (self.fp.cmd_debug_marker_end_ext)(command_buffer);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdDebugMarkerInsertEXT.html>
    #[inline]
    pub unsafe fn cmd_debug_marker_insert(
        &self,
        command_buffer: vk::CommandBuffer,
        marker_info: &vk::DebugMarkerMarkerInfoEXT,
    ) {
        (self.fp.cmd_debug_marker_insert_ext)(command_buffer, marker_info);
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtDebugMarkerFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtDebugMarkerFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
