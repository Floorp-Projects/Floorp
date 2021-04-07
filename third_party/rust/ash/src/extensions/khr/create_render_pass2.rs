#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{DeviceV1_0, InstanceV1_0};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct CreateRenderPass2 {
    handle: vk::Device,
    khr_create_renderpass2_fn: vk::KhrCreateRenderpass2Fn,
}

impl CreateRenderPass2 {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(instance: &I, device: &D) -> CreateRenderPass2 {
        let khr_create_renderpass2_fn = vk::KhrCreateRenderpass2Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        CreateRenderPass2 {
            handle: device.handle(),
            khr_create_renderpass2_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrCreateRenderpass2Fn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRenderPass2.html>"]
    pub unsafe fn create_render_pass2(
        &self,
        create_info: &vk::RenderPassCreateInfo2,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::RenderPass> {
        let mut renderpass = mem::zeroed();
        self.khr_create_renderpass2_fn
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
        self.khr_create_renderpass2_fn.cmd_begin_render_pass2_khr(
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
        self.khr_create_renderpass2_fn.cmd_next_subpass2_khr(
            command_buffer,
            subpass_begin_info,
            subpass_end_info,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndRenderPass2.html>"]
    pub unsafe fn cmd_end_render_pass2(
        &self,
        command_buffer: vk::CommandBuffer,
        subpass_end_info: &vk::SubpassEndInfo,
    ) {
        self.khr_create_renderpass2_fn
            .cmd_end_render_pass2_khr(command_buffer, subpass_end_info);
    }

    pub fn fp(&self) -> &vk::KhrCreateRenderpass2Fn {
        &self.khr_create_renderpass2_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
