use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Synchronization2 {
    fp: vk::KhrSynchronization2Fn,
}

impl Synchronization2 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fp = vk::KhrSynchronization2Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdPipelineBarrier2KHR.html>
    #[inline]
    pub unsafe fn cmd_pipeline_barrier2(
        &self,
        command_buffer: vk::CommandBuffer,
        dependency_info: &vk::DependencyInfoKHR,
    ) {
        (self.fp.cmd_pipeline_barrier2_khr)(command_buffer, dependency_info)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdResetEvent2KHR.html>
    #[inline]
    pub unsafe fn cmd_reset_event2(
        &self,
        command_buffer: vk::CommandBuffer,
        event: vk::Event,
        stage_mask: vk::PipelineStageFlags2KHR,
    ) {
        (self.fp.cmd_reset_event2_khr)(command_buffer, event, stage_mask)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdSetEvent2KHR.html>
    #[inline]
    pub unsafe fn cmd_set_event2(
        &self,
        command_buffer: vk::CommandBuffer,
        event: vk::Event,
        dependency_info: &vk::DependencyInfoKHR,
    ) {
        (self.fp.cmd_set_event2_khr)(command_buffer, event, dependency_info)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdWaitEvents2KHR.html>
    #[inline]
    pub unsafe fn cmd_wait_events2(
        &self,
        command_buffer: vk::CommandBuffer,
        events: &[vk::Event],
        dependency_infos: &[vk::DependencyInfoKHR],
    ) {
        assert_eq!(events.len(), dependency_infos.len());
        (self.fp.cmd_wait_events2_khr)(
            command_buffer,
            events.len() as u32,
            events.as_ptr(),
            dependency_infos.as_ptr(),
        )
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdWriteTimestamp2KHR.html>
    #[inline]
    pub unsafe fn cmd_write_timestamp2(
        &self,
        command_buffer: vk::CommandBuffer,
        stage: vk::PipelineStageFlags2KHR,
        query_pool: vk::QueryPool,
        query: u32,
    ) {
        (self.fp.cmd_write_timestamp2_khr)(command_buffer, stage, query_pool, query)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkQueueSubmit2KHR.html>
    #[inline]
    pub unsafe fn queue_submit2(
        &self,
        queue: vk::Queue,
        submits: &[vk::SubmitInfo2KHR],
        fence: vk::Fence,
    ) -> VkResult<()> {
        (self.fp.queue_submit2_khr)(queue, submits.len() as u32, submits.as_ptr(), fence).result()
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrSynchronization2Fn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrSynchronization2Fn {
        &self.fp
    }
}
