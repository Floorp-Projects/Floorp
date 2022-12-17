use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_descriptor_buffer.html>
#[derive(Clone)]
pub struct DescriptorBuffer {
    handle: vk::Device,
    fp: vk::ExtDescriptorBufferFn,
}

impl DescriptorBuffer {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::ExtDescriptorBufferFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDescriptorSetLayoutSizeEXT.html>
    #[inline]
    pub unsafe fn get_descriptor_set_layout_size(
        &self,
        layout: vk::DescriptorSetLayout,
    ) -> vk::DeviceSize {
        let mut count = 0;
        (self.fp.get_descriptor_set_layout_size_ext)(self.handle, layout, &mut count);
        count
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDescriptorSetLayoutBindingOffsetEXT.html>
    #[inline]
    pub unsafe fn get_descriptor_set_layout_binding_offset(
        &self,
        layout: vk::DescriptorSetLayout,
        binding: u32,
    ) -> vk::DeviceSize {
        let mut offset = 0;
        (self.fp.get_descriptor_set_layout_binding_offset_ext)(
            self.handle,
            layout,
            binding,
            &mut offset,
        );
        offset
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDescriptorEXT.html>
    #[inline]
    pub unsafe fn get_descriptor(
        &self,
        descriptor_info: &vk::DescriptorGetInfoEXT,
        descriptor: &mut [u8],
    ) {
        (self.fp.get_descriptor_ext)(
            self.handle,
            descriptor_info,
            descriptor.len(),
            descriptor.as_mut_ptr().cast(),
        )
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorBuffersEXT.html>
    #[inline]
    pub unsafe fn cmd_bind_descriptor_buffers(
        &self,
        command_buffer: vk::CommandBuffer,
        binding_info: &[vk::DescriptorBufferBindingInfoEXT],
    ) {
        (self.fp.cmd_bind_descriptor_buffers_ext)(
            command_buffer,
            binding_info.len() as u32,
            binding_info.as_ptr(),
        )
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdSetDescriptorBufferOffsetsEXT.html>
    #[inline]
    pub unsafe fn cmd_set_descriptor_buffer_offsets(
        &self,
        command_buffer: vk::CommandBuffer,
        pipeline_bind_point: vk::PipelineBindPoint,
        layout: vk::PipelineLayout,
        first_set: u32,
        buffer_indices: &[u32],
        offsets: &[vk::DeviceSize],
    ) {
        assert_eq!(buffer_indices.len(), offsets.len());
        (self.fp.cmd_set_descriptor_buffer_offsets_ext)(
            command_buffer,
            pipeline_bind_point,
            layout,
            first_set,
            buffer_indices.len() as u32,
            buffer_indices.as_ptr(),
            offsets.as_ptr(),
        )
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorBufferEmbeddedSamplersEXT.html>
    #[inline]
    pub unsafe fn cmd_bind_descriptor_buffer_embedded_samplers(
        &self,
        command_buffer: vk::CommandBuffer,
        pipeline_bind_point: vk::PipelineBindPoint,
        layout: vk::PipelineLayout,
        set: u32,
    ) {
        (self.fp.cmd_bind_descriptor_buffer_embedded_samplers_ext)(
            command_buffer,
            pipeline_bind_point,
            layout,
            set,
        )
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetBufferOpaqueCaptureDescriptorDataEXT.html>
    #[inline]
    pub unsafe fn get_buffer_opaque_capture_descriptor_data(
        &self,
        info: &vk::BufferCaptureDescriptorDataInfoEXT,
        data: &mut [u8],
    ) -> VkResult<()> {
        (self.fp.get_buffer_opaque_capture_descriptor_data_ext)(
            self.handle,
            info,
            data.as_mut_ptr().cast(),
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetImageOpaqueCaptureDescriptorDataEXT.html>
    #[inline]
    pub unsafe fn get_image_opaque_capture_descriptor_data(
        &self,
        info: &vk::ImageCaptureDescriptorDataInfoEXT,
        data: &mut [u8],
    ) -> VkResult<()> {
        (self.fp.get_image_opaque_capture_descriptor_data_ext)(
            self.handle,
            info,
            data.as_mut_ptr().cast(),
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetImageViewOpaqueCaptureDescriptorDataEXT.html>
    #[inline]
    pub unsafe fn get_image_view_opaque_capture_descriptor_data(
        &self,
        info: &vk::ImageViewCaptureDescriptorDataInfoEXT,
        data: &mut [u8],
    ) -> VkResult<()> {
        (self.fp.get_image_view_opaque_capture_descriptor_data_ext)(
            self.handle,
            info,
            data.as_mut_ptr().cast(),
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetSamplerOpaqueCaptureDescriptorDataEXT.html>
    #[inline]
    pub unsafe fn get_sampler_opaque_capture_descriptor_data(
        &self,
        info: &vk::SamplerCaptureDescriptorDataInfoEXT,
        data: &mut [u8],
    ) -> VkResult<()> {
        (self.fp.get_sampler_opaque_capture_descriptor_data_ext)(
            self.handle,
            info,
            data.as_mut_ptr().cast(),
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT.html>
    #[inline]
    pub unsafe fn get_acceleration_structure_opaque_capture_descriptor_data(
        &self,
        info: &vk::AccelerationStructureCaptureDescriptorDataInfoEXT,
        data: &mut [u8],
    ) -> VkResult<()> {
        (self
            .fp
            .get_acceleration_structure_opaque_capture_descriptor_data_ext)(
            self.handle,
            info,
            data.as_mut_ptr().cast(),
        )
        .result()
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtDescriptorBufferFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtDescriptorBufferFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
