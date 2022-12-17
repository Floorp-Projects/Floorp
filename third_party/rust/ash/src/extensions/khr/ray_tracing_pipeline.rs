use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct RayTracingPipeline {
    handle: vk::Device,
    fp: vk::KhrRayTracingPipelineFn,
}

impl RayTracingPipeline {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrRayTracingPipelineFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[inline]
    pub unsafe fn get_properties(
        instance: &Instance,
        pdevice: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceRayTracingPipelinePropertiesKHR {
        let mut props_rt = vk::PhysicalDeviceRayTracingPipelinePropertiesKHR::default();
        {
            let mut props = vk::PhysicalDeviceProperties2::builder().push_next(&mut props_rt);
            instance.get_physical_device_properties2(pdevice, &mut props);
        }
        props_rt
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdTraceRaysKHR.html>
    #[inline]
    pub unsafe fn cmd_trace_rays(
        &self,
        command_buffer: vk::CommandBuffer,
        raygen_shader_binding_tables: &vk::StridedDeviceAddressRegionKHR,
        miss_shader_binding_tables: &vk::StridedDeviceAddressRegionKHR,
        hit_shader_binding_tables: &vk::StridedDeviceAddressRegionKHR,
        callable_shader_binding_tables: &vk::StridedDeviceAddressRegionKHR,
        width: u32,
        height: u32,
        depth: u32,
    ) {
        (self.fp.cmd_trace_rays_khr)(
            command_buffer,
            raygen_shader_binding_tables,
            miss_shader_binding_tables,
            hit_shader_binding_tables,
            callable_shader_binding_tables,
            width,
            height,
            depth,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateRayTracingPipelinesKHR.html>
    #[inline]
    pub unsafe fn create_ray_tracing_pipelines(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        pipeline_cache: vk::PipelineCache,
        create_info: &[vk::RayTracingPipelineCreateInfoKHR],
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<Vec<vk::Pipeline>> {
        let mut pipelines = vec![mem::zeroed(); create_info.len()];
        (self.fp.create_ray_tracing_pipelines_khr)(
            self.handle,
            deferred_operation,
            pipeline_cache,
            create_info.len() as u32,
            create_info.as_ptr(),
            allocation_callbacks.as_raw_ptr(),
            pipelines.as_mut_ptr(),
        )
        .result_with_success(pipelines)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetRayTracingShaderGroupHandlesKHR.html>
    #[inline]
    pub unsafe fn get_ray_tracing_shader_group_handles(
        &self,
        pipeline: vk::Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
    ) -> VkResult<Vec<u8>> {
        let mut data = Vec::<u8>::with_capacity(data_size);
        (self.fp.get_ray_tracing_shader_group_handles_khr)(
            self.handle,
            pipeline,
            first_group,
            group_count,
            data_size,
            data.as_mut_ptr().cast(),
        )
        .result()?;
        data.set_len(data_size);
        Ok(data)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetRayTracingCaptureReplayShaderGroupHandlesKHR.html>
    #[inline]
    pub unsafe fn get_ray_tracing_capture_replay_shader_group_handles(
        &self,
        pipeline: vk::Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
    ) -> VkResult<Vec<u8>> {
        let mut data = Vec::<u8>::with_capacity(data_size);
        (self
            .fp
            .get_ray_tracing_capture_replay_shader_group_handles_khr)(
            self.handle,
            pipeline,
            first_group,
            group_count,
            data_size,
            data.as_mut_ptr().cast(),
        )
        .result()?;
        data.set_len(data_size);
        Ok(data)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdTraceRaysIndirectKHR.html>
    ///
    /// `indirect_device_address` is a buffer device address which is a pointer to a [`vk::TraceRaysIndirectCommandKHR`] structure containing the trace ray parameters.
    #[inline]
    pub unsafe fn cmd_trace_rays_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        raygen_shader_binding_table: &[vk::StridedDeviceAddressRegionKHR],
        miss_shader_binding_table: &[vk::StridedDeviceAddressRegionKHR],
        hit_shader_binding_table: &[vk::StridedDeviceAddressRegionKHR],
        callable_shader_binding_table: &[vk::StridedDeviceAddressRegionKHR],
        indirect_device_address: vk::DeviceAddress,
    ) {
        (self.fp.cmd_trace_rays_indirect_khr)(
            command_buffer,
            raygen_shader_binding_table.as_ptr(),
            miss_shader_binding_table.as_ptr(),
            hit_shader_binding_table.as_ptr(),
            callable_shader_binding_table.as_ptr(),
            indirect_device_address,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetRayTracingShaderGroupStackSizeKHR.html>
    #[inline]
    pub unsafe fn get_ray_tracing_shader_group_stack_size(
        &self,
        pipeline: vk::Pipeline,
        group: u32,
        group_shader: vk::ShaderGroupShaderKHR,
    ) -> vk::DeviceSize {
        (self.fp.get_ray_tracing_shader_group_stack_size_khr)(
            self.handle,
            pipeline,
            group,
            group_shader,
        )
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdSetRayTracingPipelineStackSizeKHR.html>
    #[inline]
    pub unsafe fn cmd_set_ray_tracing_pipeline_stack_size(
        &self,
        command_buffer: vk::CommandBuffer,
        pipeline_stack_size: u32,
    ) {
        (self.fp.cmd_set_ray_tracing_pipeline_stack_size_khr)(command_buffer, pipeline_stack_size);
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrRayTracingPipelineFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrRayTracingPipelineFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
