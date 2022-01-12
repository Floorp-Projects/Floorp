use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct CreateRenderPass2 {
    handle: vk::Device,
    fp: vk::KhrCreateRenderpass2Fn,
}

impl CreateRenderPass2 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrCreateRenderpass2Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRenderPass2.html>"]
    pub unsafe fn create_render_pass2(
        &self,
        create_info: &vk::RenderPassCreateInfo2,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::RenderPass> {
        let mut renderpass = mem::zeroed();
        self.fp
            .create_render_pass2_khr(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut renderpass,
            )
            .result_with_success(renderpass)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginRenderPass2.html>"]
    pub unsafe fn cmd_begin_render_pass2(
        &self,
        command_buffer: vk::CommandBuffer,
        render_pass_begin_info: &vk::RenderPassBeginInfo,
        subpass_begin_info: &vk::SubpassBeginInfo,
    ) {
        self.fp.cmd_begin_render_pass2_khr(
            command_buffer,
            render_pass_begin_info,
            subpass_begin_info,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdNextSubpass2.html>"]
    pub unsafe fn cmd_next_subpass2(
        &self,
        command_buffer: vk::CommandBuffer,
        subpass_begin_info: &vk::SubpassBeginInfo,
        subpass_end_info: &vk::SubpassEndInfo,
    ) {
        self.fp
            .cmd_next_subpass2_khr(command_buffer, subpass_begin_info, subpass_end_info);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndRenderPass2.html>"]
    pub unsafe fn cmd_end_render_pass2(
        &self,
        command_buffer: vk::CommandBuffer,
        subpass_end_info: &vk::SubpassEndInfo,
    ) {
        self.fp
            .cmd_end_render_pass2_khr(command_buffer, subpass_end_info);
    }

    pub fn name() -> &'static CStr {
        vk::KhrCreateRenderpass2Fn::name()
    }

    pub fn fp(&self) -> &vk::KhrCreateRenderpass2Fn {
        &self.fp
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
