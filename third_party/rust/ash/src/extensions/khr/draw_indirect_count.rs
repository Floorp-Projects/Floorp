use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DrawIndirectCount {
    fp: vk::KhrDrawIndirectCountFn,
}

impl DrawIndirectCount {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fp = vk::KhrDrawIndirectCountFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkCmdDrawIndexedIndirectCountKHR.html>"]
    pub unsafe fn cmd_draw_indexed_indirect_count(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        count_buffer: vk::Buffer,
        count_buffer_offset: vk::DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        self.fp.cmd_draw_indexed_indirect_count_khr(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkCmdDrawIndirectCountKHR.html>"]
    pub unsafe fn cmd_draw_indirect_count(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        count_buffer: vk::Buffer,
        count_buffer_offset: vk::DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        self.fp.cmd_draw_indexed_indirect_count_khr(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        );
    }

    pub fn name() -> &'static CStr {
        vk::KhrDrawIndirectCountFn::name()
    }

    pub fn fp(&self) -> &vk::KhrDrawIndirectCountFn {
        &self.fp
    }
}
