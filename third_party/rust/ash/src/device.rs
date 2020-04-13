#![allow(clippy::trivially_copy_pass_by_ref)]
use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use std::mem;
use std::os::raw::c_void;
use std::ptr;

#[allow(non_camel_case_types)]
pub trait DeviceV1_2: DeviceV1_1 {
    fn fp_v1_2(&self) -> &vk::DeviceFnV1_2;

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirectCount.html>"]
    unsafe fn cmd_draw_indirect_count(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        count_buffer: vk::Buffer,
        count_buffer_offset: vk::DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        self.fp_v1_2().cmd_draw_indirect_count(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexedIndirectCount.html>"]
    unsafe fn cmd_draw_indexed_indirect_count(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        count_buffer: vk::Buffer,
        count_buffer_offset: vk::DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        self.fp_v1_2().cmd_draw_indexed_indirect_count(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRenderPass2.html>"]
    unsafe fn create_render_pass2(
        &self,
        create_info: &vk::RenderPassCreateInfo2,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::RenderPass> {
        let mut renderpass = mem::zeroed();
        let err_code = self.fp_v1_2().create_render_pass2(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut renderpass,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(renderpass),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginRenderPass2.html>"]
    unsafe fn cmd_begin_render_pass2(
        &self,
        command_buffer: vk::CommandBuffer,
        render_pass_begin_info: &vk::RenderPassBeginInfo,
        subpass_begin_info: &vk::SubpassBeginInfo,
    ) {
        self.fp_v1_2().cmd_begin_render_pass2(
            command_buffer,
            render_pass_begin_info,
            subpass_begin_info,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdNextSubpass2.html>"]
    unsafe fn cmd_next_subpass2(
        &self,
        command_buffer: vk::CommandBuffer,
        subpass_begin_info: &vk::SubpassBeginInfo,
        subpass_end_info: &vk::SubpassEndInfo,
    ) {
        self.fp_v1_2()
            .cmd_next_subpass2(command_buffer, subpass_begin_info, subpass_end_info);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndRenderPass2.html>"]
    unsafe fn cmd_end_render_pass2(
        &self,
        command_buffer: vk::CommandBuffer,
        subpass_end_info: &vk::SubpassEndInfo,
    ) {
        self.fp_v1_2()
            .cmd_end_render_pass2(command_buffer, subpass_end_info);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetQueryPool.html>"]
    unsafe fn reset_query_pool(
        &self,
        device: vk::Device,
        query_pool: vk::QueryPool,
        first_query: u32,
        query_count: u32,
    ) {
        self.fp_v1_2()
            .reset_query_pool(device, query_pool, first_query, query_count);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreCounterValue.html>"]
    unsafe fn get_semaphore_counter_value(
        &self,
        device: vk::Device,
        semaphore: vk::Semaphore,
    ) -> VkResult<u64> {
        let mut value = 0;
        let err_code = self
            .fp_v1_2()
            .get_semaphore_counter_value(device, semaphore, &mut value);
        match err_code {
            vk::Result::SUCCESS => Ok(value),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitSemaphores.html>"]
    unsafe fn wait_semaphores(
        &self,
        device: vk::Device,
        wait_info: &vk::SemaphoreWaitInfo,
        timeout: u64,
    ) -> VkResult<()> {
        let err_code = self.fp_v1_2().wait_semaphores(device, wait_info, timeout);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSignalSemaphore.html>"]
    unsafe fn signal_semaphore(
        &self,
        device: vk::Device,
        signal_info: &vk::SemaphoreSignalInfo,
    ) -> VkResult<()> {
        let err_code = self.fp_v1_2().signal_semaphore(device, signal_info);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferDeviceAddress.html>"]
    unsafe fn get_buffer_device_address(
        &self,
        device: vk::Device,
        info: &vk::BufferDeviceAddressInfo,
    ) -> vk::DeviceAddress {
        self.fp_v1_2().get_buffer_device_address(device, info)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferOpaqueCaptureAddress.html>"]
    unsafe fn get_buffer_opaque_capture_address(
        &self,
        device: vk::Device,
        info: &vk::BufferDeviceAddressInfo,
    ) -> u64 {
        self.fp_v1_2()
            .get_buffer_opaque_capture_address(device, info)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceMemoryOpaqueCaptureAddress.html>"]
    unsafe fn get_device_memory_opaque_capture_address(
        &self,
        device: vk::Device,
        info: &vk::DeviceMemoryOpaqueCaptureAddressInfo,
    ) -> u64 {
        self.fp_v1_2()
            .get_device_memory_opaque_capture_address(device, info)
    }
}

#[allow(non_camel_case_types)]
pub trait DeviceV1_1: DeviceV1_0 {
    fn fp_v1_1(&self) -> &vk::DeviceFnV1_1;

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindBufferMemory2.html>"]
    unsafe fn bind_buffer_memory2(&self, bind_infos: &[vk::BindBufferMemoryInfo]) -> VkResult<()> {
        let err_code = self.fp_v1_1().bind_buffer_memory2(
            self.handle(),
            bind_infos.len() as _,
            bind_infos.as_ptr(),
        );
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindImageMemory2.html>"]
    unsafe fn bind_image_memory2(&self, bind_infos: &[vk::BindImageMemoryInfo]) -> VkResult<()> {
        let err_code = self.fp_v1_1().bind_image_memory2(
            self.handle(),
            bind_infos.len() as _,
            bind_infos.as_ptr(),
        );
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupPeerMemoryFeatures.html>"]
    unsafe fn get_device_group_peer_memory_features(
        &self,
        heap_index: u32,
        local_device_index: u32,
        remote_device_index: u32,
    ) -> vk::PeerMemoryFeatureFlags {
        let mut peer_memory_features = mem::zeroed();
        self.fp_v1_1().get_device_group_peer_memory_features(
            self.handle(),
            heap_index,
            local_device_index,
            remote_device_index,
            &mut peer_memory_features,
        );
        peer_memory_features
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDeviceMask.html>"]
    unsafe fn cmd_set_device_mask(&self, command_buffer: vk::CommandBuffer, device_mask: u32) {
        self.fp_v1_1()
            .cmd_set_device_mask(command_buffer, device_mask);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDispatchBase.html>"]
    unsafe fn cmd_dispatch_base(
        &self,
        command_buffer: vk::CommandBuffer,
        base_group_x: u32,
        base_group_y: u32,
        base_group_z: u32,
        group_count_x: u32,
        group_count_y: u32,
        group_count_z: u32,
    ) {
        self.fp_v1_1().cmd_dispatch_base(
            command_buffer,
            base_group_x,
            base_group_y,
            base_group_z,
            group_count_x,
            group_count_y,
            group_count_z,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageMemoryRequirements2.html>"]
    unsafe fn get_image_memory_requirements2(
        &self,
        info: &vk::ImageMemoryRequirementsInfo2,
        out: &mut vk::MemoryRequirements2,
    ) {
        self.fp_v1_1()
            .get_image_memory_requirements2(self.handle(), info, out);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferMemoryRequirements2.html>"]
    unsafe fn get_buffer_memory_requirements2(
        &self,
        info: &vk::BufferMemoryRequirementsInfo2,
        out: &mut vk::MemoryRequirements2,
    ) {
        self.fp_v1_1()
            .get_buffer_memory_requirements2(self.handle(), info, out);
    }

    unsafe fn get_image_sparse_memory_requirements2_len(
        &self,
        info: &vk::ImageSparseMemoryRequirementsInfo2,
    ) -> usize {
        let mut count = mem::zeroed();
        self.fp_v1_1().get_image_sparse_memory_requirements2(
            self.handle(),
            info,
            &mut count,
            ptr::null_mut(),
        );
        count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageSparseMemoryRequirements2.html>"]
    unsafe fn get_image_sparse_memory_requirements2(
        &self,
        info: &vk::ImageSparseMemoryRequirementsInfo2,
        out: &mut [vk::SparseImageMemoryRequirements2],
    ) {
        let mut count = out.len() as u32;
        self.fp_v1_1().get_image_sparse_memory_requirements2(
            self.handle(),
            info,
            &mut count,
            out.as_mut_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkTrimCommandPool.html>"]
    unsafe fn trim_command_pool(
        &self,
        command_pool: vk::CommandPool,
        flags: vk::CommandPoolTrimFlags,
    ) {
        self.fp_v1_1()
            .trim_command_pool(self.handle(), command_pool, flags);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSamplerYcbcrConversion.html>"]
    unsafe fn create_sampler_ycbcr_conversion(
        &self,
        create_info: &vk::SamplerYcbcrConversionCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SamplerYcbcrConversion> {
        let mut ycbcr_conversion = mem::zeroed();
        let err_code = self.fp_v1_1().create_sampler_ycbcr_conversion(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut ycbcr_conversion,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(ycbcr_conversion),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySamplerYcbcrConversion.html>"]
    unsafe fn destroy_sampler_ycbcr_conversion(
        &self,
        ycbcr_conversion: vk::SamplerYcbcrConversion,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_1().destroy_sampler_ycbcr_conversion(
            self.handle(),
            ycbcr_conversion,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDescriptorUpdateTemplate.html>"]
    unsafe fn create_descriptor_update_template(
        &self,
        create_info: &vk::DescriptorUpdateTemplateCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DescriptorUpdateTemplate> {
        let mut descriptor_update_template = mem::zeroed();
        let err_code = self.fp_v1_1().create_descriptor_update_template(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut descriptor_update_template,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(descriptor_update_template),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDescriptorUpdateTemplate.html>"]
    unsafe fn destroy_descriptor_update_template(
        &self,
        descriptor_update_template: vk::DescriptorUpdateTemplate,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_1().destroy_descriptor_update_template(
            self.handle(),
            descriptor_update_template,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUpdateDescriptorSetWithTemplate.html>"]
    unsafe fn update_descriptor_set_with_template(
        &self,
        descriptor_set: vk::DescriptorSet,
        descriptor_update_template: vk::DescriptorUpdateTemplate,
        data: *const c_void,
    ) {
        self.fp_v1_1().update_descriptor_set_with_template(
            self.handle(),
            descriptor_set,
            descriptor_update_template,
            data,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDescriptorSetLayoutSupport.html>"]
    unsafe fn get_descriptor_set_layout_support(
        &self,
        create_info: &vk::DescriptorSetLayoutCreateInfo,
        out: &mut vk::DescriptorSetLayoutSupport,
    ) {
        self.fp_v1_1()
            .get_descriptor_set_layout_support(self.handle(), create_info, out);
    }
}

#[allow(non_camel_case_types)]
pub trait DeviceV1_0 {
    fn handle(&self) -> vk::Device;
    fn fp_v1_0(&self) -> &vk::DeviceFnV1_0;
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDevice.html>"]
    unsafe fn destroy_device(&self, allocation_callbacks: Option<&vk::AllocationCallbacks>) {
        self.fp_v1_0()
            .destroy_device(self.handle(), allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySampler.html>"]
    unsafe fn destroy_sampler(
        &self,
        sampler: vk::Sampler,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_sampler(self.handle(), sampler, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFreeMemory.html>"]
    unsafe fn free_memory(
        &self,
        memory: vk::DeviceMemory,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .free_memory(self.handle(), memory, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFreeCommandBuffers.html>"]
    unsafe fn free_command_buffers(
        &self,
        command_pool: vk::CommandPool,
        command_buffers: &[vk::CommandBuffer],
    ) {
        self.fp_v1_0().free_command_buffers(
            self.handle(),
            command_pool,
            command_buffers.len() as u32,
            command_buffers.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateEvent.html>"]
    unsafe fn create_event(
        &self,
        create_info: &vk::EventCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::Event> {
        let mut event = mem::zeroed();
        let err_code = self.fp_v1_0().create_event(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut event,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(event),
            _ => Err(err_code),
        }
    }

    /// Returns true if the event was set, and false if the event was reset, otherwise it will
    /// return the error code.
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetEventStatus.html>"]
    unsafe fn get_event_status(&self, event: vk::Event) -> VkResult<bool> {
        let err_code = self.fp_v1_0().get_event_status(self.handle(), event);
        match err_code {
            vk::Result::EVENT_SET => Ok(true),
            vk::Result::EVENT_RESET => Ok(false),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetEvent.html>"]
    unsafe fn set_event(&self, event: vk::Event) -> VkResult<()> {
        let err_code = self.fp_v1_0().set_event(self.handle(), event);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetEvent.html>"]
    unsafe fn reset_event(&self, event: vk::Event) -> VkResult<()> {
        let err_code = self.fp_v1_0().reset_event(self.handle(), event);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetEvent.html>"]
    unsafe fn cmd_set_event(
        &self,
        command_buffer: vk::CommandBuffer,
        event: vk::Event,
        stage_mask: vk::PipelineStageFlags,
    ) {
        self.fp_v1_0()
            .cmd_set_event(command_buffer, event, stage_mask);
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResetEvent.html>"]
    unsafe fn cmd_reset_event(
        &self,
        command_buffer: vk::CommandBuffer,
        event: vk::Event,
        stage_mask: vk::PipelineStageFlags,
    ) {
        self.fp_v1_0()
            .cmd_reset_event(command_buffer, event, stage_mask);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWaitEvents.html>"]
    unsafe fn cmd_wait_events(
        &self,
        command_buffer: vk::CommandBuffer,
        events: &[vk::Event],
        src_stage_mask: vk::PipelineStageFlags,
        dst_stage_mask: vk::PipelineStageFlags,
        memory_barriers: &[vk::MemoryBarrier],
        buffer_memory_barriers: &[vk::BufferMemoryBarrier],
        image_memory_barriers: &[vk::ImageMemoryBarrier],
    ) {
        self.fp_v1_0().cmd_wait_events(
            command_buffer,
            events.len() as _,
            events.as_ptr(),
            src_stage_mask,
            dst_stage_mask,
            memory_barriers.len() as _,
            memory_barriers.as_ptr(),
            buffer_memory_barriers.len() as _,
            buffer_memory_barriers.as_ptr(),
            image_memory_barriers.len() as _,
            image_memory_barriers.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyFence.html>"]
    unsafe fn destroy_fence(
        &self,
        fence: vk::Fence,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_fence(self.handle(), fence, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyEvent.html>"]
    unsafe fn destroy_event(
        &self,
        event: vk::Event,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_event(self.handle(), event, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyImage.html>"]
    unsafe fn destroy_image(
        &self,
        image: vk::Image,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_image(self.handle(), image, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyCommandPool.html>"]
    unsafe fn destroy_command_pool(
        &self,
        pool: vk::CommandPool,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_command_pool(self.handle(), pool, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyImageView.html>"]
    unsafe fn destroy_image_view(
        &self,
        image_view: vk::ImageView,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_image_view(
            self.handle(),
            image_view,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyRenderPass.html>"]
    unsafe fn destroy_render_pass(
        &self,
        renderpass: vk::RenderPass,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_render_pass(
            self.handle(),
            renderpass,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyFramebuffer.html>"]
    unsafe fn destroy_framebuffer(
        &self,
        framebuffer: vk::Framebuffer,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_framebuffer(
            self.handle(),
            framebuffer,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyPipelineLayout.html>"]
    unsafe fn destroy_pipeline_layout(
        &self,
        pipeline_layout: vk::PipelineLayout,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_pipeline_layout(
            self.handle(),
            pipeline_layout,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyPipelineCache.html>"]
    unsafe fn destroy_pipeline_cache(
        &self,
        pipeline_cache: vk::PipelineCache,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_pipeline_cache(
            self.handle(),
            pipeline_cache,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyBuffer.html>"]
    unsafe fn destroy_buffer(
        &self,
        buffer: vk::Buffer,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_buffer(self.handle(), buffer, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyShaderModule.html>"]
    unsafe fn destroy_shader_module(
        &self,
        shader: vk::ShaderModule,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_shader_module(
            self.handle(),
            shader,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyPipeline.html>"]
    unsafe fn destroy_pipeline(
        &self,
        pipeline: vk::Pipeline,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_pipeline(self.handle(), pipeline, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySemaphore.html>"]
    unsafe fn destroy_semaphore(
        &self,
        semaphore: vk::Semaphore,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_semaphore(
            self.handle(),
            semaphore,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDescriptorPool.html>"]
    unsafe fn destroy_descriptor_pool(
        &self,
        pool: vk::DescriptorPool,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_descriptor_pool(
            self.handle(),
            pool,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyQueryPool.html>"]
    unsafe fn destroy_query_pool(
        &self,
        pool: vk::QueryPool,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0()
            .destroy_query_pool(self.handle(), pool, allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDescriptorSetLayout.html>"]
    unsafe fn destroy_descriptor_set_layout(
        &self,
        layout: vk::DescriptorSetLayout,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_descriptor_set_layout(
            self.handle(),
            layout,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFreeDescriptorSets.html>"]
    unsafe fn free_descriptor_sets(
        &self,
        pool: vk::DescriptorPool,
        descriptor_sets: &[vk::DescriptorSet],
    ) {
        self.fp_v1_0().free_descriptor_sets(
            self.handle(),
            pool,
            descriptor_sets.len() as u32,
            descriptor_sets.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUpdateDescriptorSets.html>"]
    unsafe fn update_descriptor_sets(
        &self,
        descriptor_writes: &[vk::WriteDescriptorSet],
        descriptor_copies: &[vk::CopyDescriptorSet],
    ) {
        self.fp_v1_0().update_descriptor_sets(
            self.handle(),
            descriptor_writes.len() as u32,
            descriptor_writes.as_ptr(),
            descriptor_copies.len() as u32,
            descriptor_copies.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSampler.html>"]
    unsafe fn create_sampler(
        &self,
        create_info: &vk::SamplerCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::Sampler> {
        let mut sampler = mem::zeroed();
        let err_code = self.fp_v1_0().create_sampler(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut sampler,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(sampler),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBlitImage.html>"]
    unsafe fn cmd_blit_image(
        &self,
        command_buffer: vk::CommandBuffer,
        src_image: vk::Image,
        src_image_layout: vk::ImageLayout,
        dst_image: vk::Image,
        dst_image_layout: vk::ImageLayout,
        regions: &[vk::ImageBlit],
        filter: vk::Filter,
    ) {
        self.fp_v1_0().cmd_blit_image(
            command_buffer,
            src_image,
            src_image_layout,
            dst_image,
            dst_image_layout,
            regions.len() as _,
            regions.as_ptr(),
            filter,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResolveImage.html>"]
    unsafe fn cmd_resolve_image(
        &self,
        command_buffer: vk::CommandBuffer,
        src_image: vk::Image,
        src_image_layout: vk::ImageLayout,
        dst_image: vk::Image,
        dst_image_layout: vk::ImageLayout,
        regions: &[vk::ImageResolve],
    ) {
        self.fp_v1_0().cmd_resolve_image(
            command_buffer,
            src_image,
            src_image_layout,
            dst_image,
            dst_image_layout,
            regions.len() as u32,
            regions.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdFillBuffer.html>"]
    unsafe fn cmd_fill_buffer(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        size: vk::DeviceSize,
        data: u32,
    ) {
        self.fp_v1_0()
            .cmd_fill_buffer(command_buffer, buffer, offset, size, data);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdUpdateBuffer.html>"]
    unsafe fn cmd_update_buffer(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        data: &[u8],
    ) {
        self.fp_v1_0().cmd_update_buffer(
            command_buffer,
            buffer,
            offset,
            data.len() as u64,
            data.as_ptr() as _,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyBuffer.html>"]
    unsafe fn cmd_copy_buffer(
        &self,
        command_buffer: vk::CommandBuffer,
        src_buffer: vk::Buffer,
        dst_buffer: vk::Buffer,
        regions: &[vk::BufferCopy],
    ) {
        self.fp_v1_0().cmd_copy_buffer(
            command_buffer,
            src_buffer,
            dst_buffer,
            regions.len() as u32,
            regions.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyImageToBuffer.html>"]
    unsafe fn cmd_copy_image_to_buffer(
        &self,
        command_buffer: vk::CommandBuffer,
        src_image: vk::Image,
        src_image_layout: vk::ImageLayout,
        dst_buffer: vk::Buffer,
        regions: &[vk::BufferImageCopy],
    ) {
        self.fp_v1_0().cmd_copy_image_to_buffer(
            command_buffer,
            src_image,
            src_image_layout,
            dst_buffer,
            regions.len() as u32,
            regions.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyBufferToImage.html>"]
    unsafe fn cmd_copy_buffer_to_image(
        &self,
        command_buffer: vk::CommandBuffer,
        src_buffer: vk::Buffer,
        dst_image: vk::Image,
        dst_image_layout: vk::ImageLayout,
        regions: &[vk::BufferImageCopy],
    ) {
        self.fp_v1_0().cmd_copy_buffer_to_image(
            command_buffer,
            src_buffer,
            dst_image,
            dst_image_layout,
            regions.len() as u32,
            regions.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyImage.html>"]
    unsafe fn cmd_copy_image(
        &self,
        command_buffer: vk::CommandBuffer,
        src_image: vk::Image,
        src_image_layout: vk::ImageLayout,
        dst_image: vk::Image,
        dst_image_layout: vk::ImageLayout,
        regions: &[vk::ImageCopy],
    ) {
        self.fp_v1_0().cmd_copy_image(
            command_buffer,
            src_image,
            src_image_layout,
            dst_image,
            dst_image_layout,
            regions.len() as u32,
            regions.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAllocateDescriptorSets.html>"]
    unsafe fn allocate_descriptor_sets(
        &self,
        create_info: &vk::DescriptorSetAllocateInfo,
    ) -> VkResult<Vec<vk::DescriptorSet>> {
        let mut desc_set = Vec::with_capacity(create_info.descriptor_set_count as usize);
        let err_code = self.fp_v1_0().allocate_descriptor_sets(
            self.handle(),
            create_info,
            desc_set.as_mut_ptr(),
        );

        desc_set.set_len(create_info.descriptor_set_count as usize);
        match err_code {
            vk::Result::SUCCESS => Ok(desc_set),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDescriptorSetLayout.html>"]
    unsafe fn create_descriptor_set_layout(
        &self,
        create_info: &vk::DescriptorSetLayoutCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DescriptorSetLayout> {
        let mut layout = mem::zeroed();
        let err_code = self.fp_v1_0().create_descriptor_set_layout(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut layout,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(layout),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDeviceWaitIdle.html>"]
    unsafe fn device_wait_idle(&self) -> VkResult<()> {
        let err_code = self.fp_v1_0().device_wait_idle(self.handle());
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDescriptorPool.html>"]
    unsafe fn create_descriptor_pool(
        &self,
        create_info: &vk::DescriptorPoolCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DescriptorPool> {
        let mut pool = mem::zeroed();
        let err_code = self.fp_v1_0().create_descriptor_pool(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut pool,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(pool),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetDescriptorPool.html>"]
    unsafe fn reset_descriptor_pool(
        &self,
        pool: vk::DescriptorPool,
        flags: vk::DescriptorPoolResetFlags,
    ) -> VkResult<()> {
        let err_code = self
            .fp_v1_0()
            .reset_descriptor_pool(self.handle(), pool, flags);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetCommandPool.html>"]
    unsafe fn reset_command_pool(
        &self,
        command_pool: vk::CommandPool,
        flags: vk::CommandPoolResetFlags,
    ) -> VkResult<()> {
        let err_code = self
            .fp_v1_0()
            .reset_command_pool(self.handle(), command_pool, flags);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetCommandBuffer.html>"]
    unsafe fn reset_command_buffer(
        &self,
        command_buffer: vk::CommandBuffer,
        flags: vk::CommandBufferResetFlags,
    ) -> VkResult<()> {
        let err_code = self.fp_v1_0().reset_command_buffer(command_buffer, flags);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetFences.html>"]
    unsafe fn reset_fences(&self, fences: &[vk::Fence]) -> VkResult<()> {
        let err_code =
            self.fp_v1_0()
                .reset_fences(self.handle(), fences.len() as u32, fences.as_ptr());
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindIndexBuffer.html>"]
    unsafe fn cmd_bind_index_buffer(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        index_type: vk::IndexType,
    ) {
        self.fp_v1_0()
            .cmd_bind_index_buffer(command_buffer, buffer, offset, index_type);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdClearColorImage.html>"]
    unsafe fn cmd_clear_color_image(
        &self,
        command_buffer: vk::CommandBuffer,
        image: vk::Image,
        image_layout: vk::ImageLayout,
        clear_color_value: &vk::ClearColorValue,
        ranges: &[vk::ImageSubresourceRange],
    ) {
        self.fp_v1_0().cmd_clear_color_image(
            command_buffer,
            image,
            image_layout,
            clear_color_value,
            ranges.len() as u32,
            ranges.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdClearDepthStencilImage.html>"]
    unsafe fn cmd_clear_depth_stencil_image(
        &self,
        command_buffer: vk::CommandBuffer,
        image: vk::Image,
        image_layout: vk::ImageLayout,
        clear_depth_stencil_value: &vk::ClearDepthStencilValue,
        ranges: &[vk::ImageSubresourceRange],
    ) {
        self.fp_v1_0().cmd_clear_depth_stencil_image(
            command_buffer,
            image,
            image_layout,
            clear_depth_stencil_value,
            ranges.len() as u32,
            ranges.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdClearAttachments.html>"]
    unsafe fn cmd_clear_attachments(
        &self,
        command_buffer: vk::CommandBuffer,
        attachments: &[vk::ClearAttachment],
        rects: &[vk::ClearRect],
    ) {
        self.fp_v1_0().cmd_clear_attachments(
            command_buffer,
            attachments.len() as u32,
            attachments.as_ptr(),
            rects.len() as u32,
            rects.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexed.html>"]
    unsafe fn cmd_draw_indexed(
        &self,
        command_buffer: vk::CommandBuffer,
        index_count: u32,
        instance_count: u32,
        first_index: u32,
        vertex_offset: i32,
        first_instance: u32,
    ) {
        self.fp_v1_0().cmd_draw_indexed(
            command_buffer,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexedIndirect.html>"]
    unsafe fn cmd_draw_indexed_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        self.fp_v1_0().cmd_draw_indexed_indirect(
            command_buffer,
            buffer,
            offset,
            draw_count,
            stride,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdExecuteCommands.html>"]
    unsafe fn cmd_execute_commands(
        &self,
        primary_command_buffer: vk::CommandBuffer,
        secondary_command_buffers: &[vk::CommandBuffer],
    ) {
        self.fp_v1_0().cmd_execute_commands(
            primary_command_buffer,
            secondary_command_buffers.len() as u32,
            secondary_command_buffers.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindDescriptorSets.html>"]
    unsafe fn cmd_bind_descriptor_sets(
        &self,
        command_buffer: vk::CommandBuffer,
        pipeline_bind_point: vk::PipelineBindPoint,
        layout: vk::PipelineLayout,
        first_set: u32,
        descriptor_sets: &[vk::DescriptorSet],
        dynamic_offsets: &[u32],
    ) {
        self.fp_v1_0().cmd_bind_descriptor_sets(
            command_buffer,
            pipeline_bind_point,
            layout,
            first_set,
            descriptor_sets.len() as u32,
            descriptor_sets.as_ptr(),
            dynamic_offsets.len() as u32,
            dynamic_offsets.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyQueryPoolResults.html>"]
    unsafe fn cmd_copy_query_pool_results(
        &self,
        command_buffer: vk::CommandBuffer,
        query_pool: vk::QueryPool,
        first_query: u32,
        query_count: u32,
        dst_buffer: vk::Buffer,
        dst_offset: vk::DeviceSize,
        stride: vk::DeviceSize,
        flags: vk::QueryResultFlags,
    ) {
        self.fp_v1_0().cmd_copy_query_pool_results(
            command_buffer,
            query_pool,
            first_query,
            query_count,
            dst_buffer,
            dst_offset,
            stride,
            flags,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushConstants.html>"]
    unsafe fn cmd_push_constants(
        &self,
        command_buffer: vk::CommandBuffer,
        layout: vk::PipelineLayout,
        stage_flags: vk::ShaderStageFlags,
        offset: u32,
        constants: &[u8],
    ) {
        self.fp_v1_0().cmd_push_constants(
            command_buffer,
            layout,
            stage_flags,
            offset,
            constants.len() as _,
            constants.as_ptr() as _,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginRenderPass.html>"]
    unsafe fn cmd_begin_render_pass(
        &self,
        command_buffer: vk::CommandBuffer,
        create_info: &vk::RenderPassBeginInfo,
        contents: vk::SubpassContents,
    ) {
        self.fp_v1_0()
            .cmd_begin_render_pass(command_buffer, create_info, contents);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdNextSubpass.html>"]
    unsafe fn cmd_next_subpass(
        &self,
        command_buffer: vk::CommandBuffer,
        contents: vk::SubpassContents,
    ) {
        self.fp_v1_0().cmd_next_subpass(command_buffer, contents);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindPipeline.html>"]
    unsafe fn cmd_bind_pipeline(
        &self,
        command_buffer: vk::CommandBuffer,
        pipeline_bind_point: vk::PipelineBindPoint,
        pipeline: vk::Pipeline,
    ) {
        self.fp_v1_0()
            .cmd_bind_pipeline(command_buffer, pipeline_bind_point, pipeline);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetScissor.html>"]
    unsafe fn cmd_set_scissor(
        &self,
        command_buffer: vk::CommandBuffer,
        first_scissor: u32,
        scissors: &[vk::Rect2D],
    ) {
        self.fp_v1_0().cmd_set_scissor(
            command_buffer,
            first_scissor,
            scissors.len() as u32,
            scissors.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetLineWidth.html>"]
    unsafe fn cmd_set_line_width(&self, command_buffer: vk::CommandBuffer, line_width: f32) {
        self.fp_v1_0()
            .cmd_set_line_width(command_buffer, line_width);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindVertexBuffers.html>"]
    unsafe fn cmd_bind_vertex_buffers(
        &self,
        command_buffer: vk::CommandBuffer,
        first_binding: u32,
        buffers: &[vk::Buffer],
        offsets: &[vk::DeviceSize],
    ) {
        debug_assert_eq!(buffers.len(), offsets.len());
        self.fp_v1_0().cmd_bind_vertex_buffers(
            command_buffer,
            first_binding,
            buffers.len() as u32,
            buffers.as_ptr(),
            offsets.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndRenderPass.html>"]
    unsafe fn cmd_end_render_pass(&self, command_buffer: vk::CommandBuffer) {
        self.fp_v1_0().cmd_end_render_pass(command_buffer);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDraw.html>"]
    unsafe fn cmd_draw(
        &self,
        command_buffer: vk::CommandBuffer,
        vertex_count: u32,
        instance_count: u32,
        first_vertex: u32,
        first_instance: u32,
    ) {
        self.fp_v1_0().cmd_draw(
            command_buffer,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirect.html>"]
    unsafe fn cmd_draw_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        self.fp_v1_0()
            .cmd_draw_indirect(command_buffer, buffer, offset, draw_count, stride);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDispatch.html>"]
    unsafe fn cmd_dispatch(
        &self,
        command_buffer: vk::CommandBuffer,
        group_count_x: u32,
        group_count_y: u32,
        group_count_z: u32,
    ) {
        self.fp_v1_0()
            .cmd_dispatch(command_buffer, group_count_x, group_count_y, group_count_z);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDispatchIndirect.html>"]
    unsafe fn cmd_dispatch_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
    ) {
        self.fp_v1_0()
            .cmd_dispatch_indirect(command_buffer, buffer, offset);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetViewport.html>"]
    unsafe fn cmd_set_viewport(
        &self,
        command_buffer: vk::CommandBuffer,
        first_viewport: u32,
        viewports: &[vk::Viewport],
    ) {
        self.fp_v1_0().cmd_set_viewport(
            command_buffer,
            first_viewport,
            viewports.len() as u32,
            viewports.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBias.html>"]
    unsafe fn cmd_set_depth_bias(
        &self,
        command_buffer: vk::CommandBuffer,
        constant_factor: f32,
        clamp: f32,
        slope_factor: f32,
    ) {
        self.fp_v1_0()
            .cmd_set_depth_bias(command_buffer, constant_factor, clamp, slope_factor);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetBlendConstants.html>"]
    unsafe fn cmd_set_blend_constants(
        &self,
        command_buffer: vk::CommandBuffer,
        blend_constants: &[f32; 4],
    ) {
        self.fp_v1_0()
            .cmd_set_blend_constants(command_buffer, blend_constants);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBounds.html>"]
    unsafe fn cmd_set_depth_bounds(
        &self,
        command_buffer: vk::CommandBuffer,
        min_depth_bounds: f32,
        max_depth_bounds: f32,
    ) {
        self.fp_v1_0()
            .cmd_set_depth_bounds(command_buffer, min_depth_bounds, max_depth_bounds);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilCompareMask.html>"]
    unsafe fn cmd_set_stencil_compare_mask(
        &self,
        command_buffer: vk::CommandBuffer,
        face_mask: vk::StencilFaceFlags,
        compare_mask: u32,
    ) {
        self.fp_v1_0()
            .cmd_set_stencil_compare_mask(command_buffer, face_mask, compare_mask);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilWriteMask.html>"]
    unsafe fn cmd_set_stencil_write_mask(
        &self,
        command_buffer: vk::CommandBuffer,
        face_mask: vk::StencilFaceFlags,
        write_mask: u32,
    ) {
        self.fp_v1_0()
            .cmd_set_stencil_write_mask(command_buffer, face_mask, write_mask);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilReference.html>"]
    unsafe fn cmd_set_stencil_reference(
        &self,
        command_buffer: vk::CommandBuffer,
        face_mask: vk::StencilFaceFlags,
        reference: u32,
    ) {
        self.fp_v1_0()
            .cmd_set_stencil_reference(command_buffer, face_mask, reference);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetQueryPoolResults.html>"]
    unsafe fn get_query_pool_results<T>(
        &self,
        query_pool: vk::QueryPool,
        first_query: u32,
        query_count: u32,
        data: &mut [T],
        flags: vk::QueryResultFlags,
    ) -> VkResult<()> {
        let data_length = query_count as usize;
        assert!(
            mem::size_of::<T>() <= mem::size_of::<u64>(),
            "T can not be bigger than an u64"
        );
        assert!(
            data_length <= data.len(),
            "query_count was higher than the length of the slice"
        );
        let data_size = mem::size_of::<T>() * data_length;
        let err_code = self.fp_v1_0().get_query_pool_results(
            self.handle(),
            query_pool,
            first_query,
            query_count,
            data_size,
            data.as_mut_ptr() as *mut _,
            mem::size_of::<T>() as _,
            flags,
        );

        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginQuery.html>"]
    unsafe fn cmd_begin_query(
        &self,
        command_buffer: vk::CommandBuffer,
        query_pool: vk::QueryPool,
        query: u32,
        flags: vk::QueryControlFlags,
    ) {
        self.fp_v1_0()
            .cmd_begin_query(command_buffer, query_pool, query, flags);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndQuery.html>"]
    unsafe fn cmd_end_query(
        &self,
        command_buffer: vk::CommandBuffer,
        query_pool: vk::QueryPool,
        query: u32,
    ) {
        self.fp_v1_0()
            .cmd_end_query(command_buffer, query_pool, query);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResetQueryPool.html>"]
    unsafe fn cmd_reset_query_pool(
        &self,
        command_buffer: vk::CommandBuffer,
        pool: vk::QueryPool,
        first_query: u32,
        query_count: u32,
    ) {
        self.fp_v1_0()
            .cmd_reset_query_pool(command_buffer, pool, first_query, query_count);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteTimestamp.html>"]
    unsafe fn cmd_write_timestamp(
        &self,
        command_buffer: vk::CommandBuffer,
        pipeline_stage: vk::PipelineStageFlags,
        query_pool: vk::QueryPool,
        query: u32,
    ) {
        self.fp_v1_0()
            .cmd_write_timestamp(command_buffer, pipeline_stage, query_pool, query);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSemaphore.html>"]
    unsafe fn create_semaphore(
        &self,
        create_info: &vk::SemaphoreCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::Semaphore> {
        let mut semaphore = mem::zeroed();
        let err_code = self.fp_v1_0().create_semaphore(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut semaphore,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(semaphore),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateGraphicsPipelines.html>"]
    unsafe fn create_graphics_pipelines(
        &self,
        pipeline_cache: vk::PipelineCache,
        create_infos: &[vk::GraphicsPipelineCreateInfo],
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> Result<Vec<vk::Pipeline>, (Vec<vk::Pipeline>, vk::Result)> {
        let mut pipelines = Vec::with_capacity(create_infos.len());
        let err_code = self.fp_v1_0().create_graphics_pipelines(
            self.handle(),
            pipeline_cache,
            create_infos.len() as u32,
            create_infos.as_ptr(),
            allocation_callbacks.as_raw_ptr(),
            pipelines.as_mut_ptr(),
        );
        pipelines.set_len(create_infos.len());
        match err_code {
            vk::Result::SUCCESS => Ok(pipelines),
            _ => Err((pipelines, err_code)),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateComputePipelines.html>"]
    unsafe fn create_compute_pipelines(
        &self,
        pipeline_cache: vk::PipelineCache,
        create_infos: &[vk::ComputePipelineCreateInfo],
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> Result<Vec<vk::Pipeline>, (Vec<vk::Pipeline>, vk::Result)> {
        let mut pipelines = Vec::with_capacity(create_infos.len());
        let err_code = self.fp_v1_0().create_compute_pipelines(
            self.handle(),
            pipeline_cache,
            create_infos.len() as u32,
            create_infos.as_ptr(),
            allocation_callbacks.as_raw_ptr(),
            pipelines.as_mut_ptr(),
        );
        pipelines.set_len(create_infos.len());
        match err_code {
            vk::Result::SUCCESS => Ok(pipelines),
            _ => Err((pipelines, err_code)),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateBuffer.html>"]
    unsafe fn create_buffer(
        &self,
        create_info: &vk::BufferCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::Buffer> {
        let mut buffer = mem::zeroed();
        let err_code = self.fp_v1_0().create_buffer(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut buffer,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(buffer),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreatePipelineLayout.html>"]
    unsafe fn create_pipeline_layout(
        &self,
        create_info: &vk::PipelineLayoutCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::PipelineLayout> {
        let mut pipeline_layout = mem::zeroed();
        let err_code = self.fp_v1_0().create_pipeline_layout(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut pipeline_layout,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(pipeline_layout),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreatePipelineCache.html>"]
    unsafe fn create_pipeline_cache(
        &self,
        create_info: &vk::PipelineCacheCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::PipelineCache> {
        let mut pipeline_cache = mem::zeroed();
        let err_code = self.fp_v1_0().create_pipeline_cache(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut pipeline_cache,
        );

        match err_code {
            vk::Result::SUCCESS => Ok(pipeline_cache),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineCacheData.html>"]
    unsafe fn get_pipeline_cache_data(
        &self,
        pipeline_cache: vk::PipelineCache,
    ) -> VkResult<Vec<u8>> {
        let mut data_size: usize = 0;
        let err_code = self.fp_v1_0().get_pipeline_cache_data(
            self.handle(),
            pipeline_cache,
            &mut data_size,
            ptr::null_mut(),
        );
        if err_code != vk::Result::SUCCESS {
            return Err(err_code);
        };
        let mut data: Vec<u8> = Vec::with_capacity(data_size);
        let err_code = self.fp_v1_0().get_pipeline_cache_data(
            self.handle(),
            pipeline_cache,
            &mut data_size,
            data.as_mut_ptr() as _,
        );
        data.set_len(data_size);
        match err_code {
            vk::Result::SUCCESS => Ok(data),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkMapMemory.html>"]
    unsafe fn map_memory(
        &self,
        memory: vk::DeviceMemory,
        offset: vk::DeviceSize,
        size: vk::DeviceSize,
        flags: vk::MemoryMapFlags,
    ) -> VkResult<*mut c_void> {
        let mut data: *mut c_void = ptr::null_mut();
        let err_code =
            self.fp_v1_0()
                .map_memory(self.handle(), memory, offset, size, flags, &mut data);
        match err_code {
            vk::Result::SUCCESS => Ok(data),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUnmapMemory.html>"]
    unsafe fn unmap_memory(&self, memory: vk::DeviceMemory) {
        self.fp_v1_0().unmap_memory(self.handle(), memory);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkInvalidateMappedMemoryRanges.html>"]
    unsafe fn invalidate_mapped_memory_ranges(
        &self,
        ranges: &[vk::MappedMemoryRange],
    ) -> VkResult<()> {
        let err_code = self.fp_v1_0().invalidate_mapped_memory_ranges(
            self.handle(),
            ranges.len() as u32,
            ranges.as_ptr(),
        );
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFlushMappedMemoryRanges.html>"]
    unsafe fn flush_mapped_memory_ranges(&self, ranges: &[vk::MappedMemoryRange]) -> VkResult<()> {
        let err_code = self.fp_v1_0().flush_mapped_memory_ranges(
            self.handle(),
            ranges.len() as u32,
            ranges.as_ptr(),
        );
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateFramebuffer.html>"]
    unsafe fn create_framebuffer(
        &self,
        create_info: &vk::FramebufferCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::Framebuffer> {
        let mut framebuffer = mem::zeroed();
        let err_code = self.fp_v1_0().create_framebuffer(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut framebuffer,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(framebuffer),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceQueue.html>"]
    unsafe fn get_device_queue(&self, queue_family_index: u32, queue_index: u32) -> vk::Queue {
        let mut queue = mem::zeroed();
        self.fp_v1_0()
            .get_device_queue(self.handle(), queue_family_index, queue_index, &mut queue);
        queue
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPipelineBarrier.html>"]
    unsafe fn cmd_pipeline_barrier(
        &self,
        command_buffer: vk::CommandBuffer,
        src_stage_mask: vk::PipelineStageFlags,
        dst_stage_mask: vk::PipelineStageFlags,
        dependency_flags: vk::DependencyFlags,
        memory_barriers: &[vk::MemoryBarrier],
        buffer_memory_barriers: &[vk::BufferMemoryBarrier],
        image_memory_barriers: &[vk::ImageMemoryBarrier],
    ) {
        self.fp_v1_0().cmd_pipeline_barrier(
            command_buffer,
            src_stage_mask,
            dst_stage_mask,
            dependency_flags,
            memory_barriers.len() as u32,
            memory_barriers.as_ptr(),
            buffer_memory_barriers.len() as u32,
            buffer_memory_barriers.as_ptr(),
            image_memory_barriers.len() as u32,
            image_memory_barriers.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRenderPass.html>"]
    unsafe fn create_render_pass(
        &self,
        create_info: &vk::RenderPassCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::RenderPass> {
        let mut renderpass = mem::zeroed();
        let err_code = self.fp_v1_0().create_render_pass(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut renderpass,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(renderpass),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBeginCommandBuffer.html>"]
    unsafe fn begin_command_buffer(
        &self,
        command_buffer: vk::CommandBuffer,
        begin_info: &vk::CommandBufferBeginInfo,
    ) -> VkResult<()> {
        let err_code = self
            .fp_v1_0()
            .begin_command_buffer(command_buffer, begin_info);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEndCommandBuffer.html>"]
    unsafe fn end_command_buffer(&self, command_buffer: vk::CommandBuffer) -> VkResult<()> {
        let err_code = self.fp_v1_0().end_command_buffer(command_buffer);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitForFences.html>"]
    unsafe fn wait_for_fences(
        &self,
        fences: &[vk::Fence],
        wait_all: bool,
        timeout: u64,
    ) -> VkResult<()> {
        let err_code = self.fp_v1_0().wait_for_fences(
            self.handle(),
            fences.len() as u32,
            fences.as_ptr(),
            wait_all as u32,
            timeout,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetFenceStatus.html>"]
    unsafe fn get_fence_status(&self, fence: vk::Fence) -> VkResult<bool> {
        let err_code = self.fp_v1_0().get_fence_status(self.handle(), fence);
        match err_code {
            vk::Result::SUCCESS => Ok(true),
            vk::Result::NOT_READY => Ok(false),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueWaitIdle.html>"]
    unsafe fn queue_wait_idle(&self, queue: vk::Queue) -> VkResult<()> {
        let err_code = self.fp_v1_0().queue_wait_idle(queue);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueSubmit.html>"]
    unsafe fn queue_submit(
        &self,
        queue: vk::Queue,
        submits: &[vk::SubmitInfo],
        fence: vk::Fence,
    ) -> VkResult<()> {
        let err_code =
            self.fp_v1_0()
                .queue_submit(queue, submits.len() as u32, submits.as_ptr(), fence);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateBufferView.html>"]
    unsafe fn create_buffer_view(
        &self,
        create_info: &vk::BufferViewCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::BufferView> {
        let mut buffer_view = mem::zeroed();
        let err_code = self.fp_v1_0().create_buffer_view(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut buffer_view,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(buffer_view),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyBufferView.html>"]
    unsafe fn destroy_buffer_view(
        &self,
        buffer_view: vk::BufferView,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp_v1_0().destroy_buffer_view(
            self.handle(),
            buffer_view,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateImageView.html>"]
    unsafe fn create_image_view(
        &self,
        create_info: &vk::ImageViewCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::ImageView> {
        let mut image_view = mem::zeroed();
        let err_code = self.fp_v1_0().create_image_view(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut image_view,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(image_view),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAllocateCommandBuffers.html>"]
    unsafe fn allocate_command_buffers(
        &self,
        create_info: &vk::CommandBufferAllocateInfo,
    ) -> VkResult<Vec<vk::CommandBuffer>> {
        let mut buffers = Vec::with_capacity(create_info.command_buffer_count as usize);
        let err_code = self.fp_v1_0().allocate_command_buffers(
            self.handle(),
            create_info,
            buffers.as_mut_ptr(),
        );
        buffers.set_len(create_info.command_buffer_count as usize);
        match err_code {
            vk::Result::SUCCESS => Ok(buffers),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateCommandPool.html>"]
    unsafe fn create_command_pool(
        &self,
        create_info: &vk::CommandPoolCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::CommandPool> {
        let mut pool = mem::zeroed();
        let err_code = self.fp_v1_0().create_command_pool(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut pool,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(pool),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateQueryPool.html>"]
    unsafe fn create_query_pool(
        &self,
        create_info: &vk::QueryPoolCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::QueryPool> {
        let mut pool = mem::zeroed();
        let err_code = self.fp_v1_0().create_query_pool(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut pool,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(pool),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateImage.html>"]
    unsafe fn create_image(
        &self,
        create_info: &vk::ImageCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::Image> {
        let mut image = mem::zeroed();
        let err_code = self.fp_v1_0().create_image(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut image,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(image),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageSubresourceLayout.html>"]
    unsafe fn get_image_subresource_layout(
        &self,
        image: vk::Image,
        subresource: vk::ImageSubresource,
    ) -> vk::SubresourceLayout {
        let mut layout = mem::zeroed();
        self.fp_v1_0().get_image_subresource_layout(
            self.handle(),
            image,
            &subresource,
            &mut layout,
        );
        layout
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageMemoryRequirements.html>"]
    unsafe fn get_image_memory_requirements(&self, image: vk::Image) -> vk::MemoryRequirements {
        let mut mem_req = mem::zeroed();
        self.fp_v1_0()
            .get_image_memory_requirements(self.handle(), image, &mut mem_req);
        mem_req
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferMemoryRequirements.html>"]
    unsafe fn get_buffer_memory_requirements(&self, buffer: vk::Buffer) -> vk::MemoryRequirements {
        let mut mem_req = mem::zeroed();
        self.fp_v1_0()
            .get_buffer_memory_requirements(self.handle(), buffer, &mut mem_req);
        mem_req
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAllocateMemory.html>"]
    unsafe fn allocate_memory(
        &self,
        create_info: &vk::MemoryAllocateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DeviceMemory> {
        let mut memory = mem::zeroed();
        let err_code = self.fp_v1_0().allocate_memory(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut memory,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(memory),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateShaderModule.html>"]
    unsafe fn create_shader_module(
        &self,
        create_info: &vk::ShaderModuleCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::ShaderModule> {
        let mut shader = mem::zeroed();
        let err_code = self.fp_v1_0().create_shader_module(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut shader,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(shader),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateFence.html>"]
    unsafe fn create_fence(
        &self,
        create_info: &vk::FenceCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::Fence> {
        let mut fence = mem::zeroed();
        let err_code = self.fp_v1_0().create_fence(
            self.handle(),
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut fence,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(fence),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindBufferMemory.html>"]
    unsafe fn bind_buffer_memory(
        &self,
        buffer: vk::Buffer,
        device_memory: vk::DeviceMemory,
        offset: vk::DeviceSize,
    ) -> VkResult<()> {
        let err_code =
            self.fp_v1_0()
                .bind_buffer_memory(self.handle(), buffer, device_memory, offset);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindImageMemory.html>"]
    unsafe fn bind_image_memory(
        &self,
        image: vk::Image,
        device_memory: vk::DeviceMemory,
        offset: vk::DeviceSize,
    ) -> VkResult<()> {
        let err_code =
            self.fp_v1_0()
                .bind_image_memory(self.handle(), image, device_memory, offset);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }
}

#[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDevice.html>"]
#[derive(Clone)]
pub struct Device {
    handle: vk::Device,
    device_fn_1_0: vk::DeviceFnV1_0,
    device_fn_1_1: vk::DeviceFnV1_1,
    device_fn_1_2: vk::DeviceFnV1_2,
}
impl Device {
    pub unsafe fn load(instance_fn: &vk::InstanceFnV1_0, device: vk::Device) -> Self {
        let device_fn_1_0 = vk::DeviceFnV1_0::load(|name| {
            mem::transmute(instance_fn.get_device_proc_addr(device, name.as_ptr()))
        });
        let device_fn_1_1 = vk::DeviceFnV1_1::load(|name| {
            mem::transmute(instance_fn.get_device_proc_addr(device, name.as_ptr()))
        });
        let device_fn_1_2 = vk::DeviceFnV1_2::load(|name| {
            mem::transmute(instance_fn.get_device_proc_addr(device, name.as_ptr()))
        });
        Device {
            handle: device,
            device_fn_1_0,
            device_fn_1_1,
            device_fn_1_2,
        }
    }
}

impl DeviceV1_0 for Device {
    fn handle(&self) -> vk::Device {
        self.handle
    }

    fn fp_v1_0(&self) -> &vk::DeviceFnV1_0 {
        &self.device_fn_1_0
    }
}

impl DeviceV1_1 for Device {
    fn fp_v1_1(&self) -> &vk::DeviceFnV1_1 {
        &self.device_fn_1_1
    }
}

impl DeviceV1_2 for Device {
    fn fp_v1_2(&self) -> &vk::DeviceFnV1_2 {
        &self.device_fn_1_2
    }
}

impl Device {
    pub fn handle(&self) -> vk::Device {
        self.handle
    }
}
