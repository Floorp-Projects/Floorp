use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct RayTracing {
    handle: vk::Device,
    fp: vk::NvRayTracingFn,
}

impl RayTracing {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::NvRayTracingFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[inline]
    pub unsafe fn get_properties(
        instance: &Instance,
        pdevice: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceRayTracingPropertiesNV {
        let mut props_rt = vk::PhysicalDeviceRayTracingPropertiesNV::default();
        {
            let mut props = vk::PhysicalDeviceProperties2::builder().push_next(&mut props_rt);
            instance.get_physical_device_properties2(pdevice, &mut props);
        }
        props_rt
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateAccelerationStructureNV.html>
    #[inline]
    pub unsafe fn create_acceleration_structure(
        &self,
        create_info: &vk::AccelerationStructureCreateInfoNV,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::AccelerationStructureNV> {
        let mut accel_struct = mem::zeroed();
        (self.fp.create_acceleration_structure_nv)(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut accel_struct,
        )
        .result_with_success(accel_struct)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDestroyAccelerationStructureNV.html>
    #[inline]
    pub unsafe fn destroy_acceleration_structure(
        &self,
        accel_struct: vk::AccelerationStructureNV,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        (self.fp.destroy_acceleration_structure_nv)(
            self.handle,
            accel_struct,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetAccelerationStructureMemoryRequirementsNV.html>
    #[inline]
    pub unsafe fn get_acceleration_structure_memory_requirements(
        &self,
        info: &vk::AccelerationStructureMemoryRequirementsInfoNV,
    ) -> vk::MemoryRequirements2KHR {
        let mut requirements = mem::zeroed();
        (self.fp.get_acceleration_structure_memory_requirements_nv)(
            self.handle,
            info,
            &mut requirements,
        );
        requirements
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkBindAccelerationStructureMemoryNV.html>
    #[inline]
    pub unsafe fn bind_acceleration_structure_memory(
        &self,
        bind_info: &[vk::BindAccelerationStructureMemoryInfoNV],
    ) -> VkResult<()> {
        (self.fp.bind_acceleration_structure_memory_nv)(
            self.handle,
            bind_info.len() as u32,
            bind_info.as_ptr(),
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdBuildAccelerationStructureNV.html>
    #[inline]
    pub unsafe fn cmd_build_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::AccelerationStructureInfoNV,
        instance_data: vk::Buffer,
        instance_offset: vk::DeviceSize,
        update: bool,
        dst: vk::AccelerationStructureNV,
        src: vk::AccelerationStructureNV,
        scratch: vk::Buffer,
        scratch_offset: vk::DeviceSize,
    ) {
        (self.fp.cmd_build_acceleration_structure_nv)(
            command_buffer,
            info,
            instance_data,
            instance_offset,
            if update { vk::TRUE } else { vk::FALSE },
            dst,
            src,
            scratch,
            scratch_offset,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyAccelerationStructureNV.html>
    #[inline]
    pub unsafe fn cmd_copy_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        dst: vk::AccelerationStructureNV,
        src: vk::AccelerationStructureNV,
        mode: vk::CopyAccelerationStructureModeNV,
    ) {
        (self.fp.cmd_copy_acceleration_structure_nv)(command_buffer, dst, src, mode);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdTraceRaysNV.html>
    #[inline]
    pub unsafe fn cmd_trace_rays(
        &self,
        command_buffer: vk::CommandBuffer,
        raygen_shader_binding_table_buffer: vk::Buffer,
        raygen_shader_binding_offset: vk::DeviceSize,
        miss_shader_binding_table_buffer: vk::Buffer,
        miss_shader_binding_offset: vk::DeviceSize,
        miss_shader_binding_stride: vk::DeviceSize,
        hit_shader_binding_table_buffer: vk::Buffer,
        hit_shader_binding_offset: vk::DeviceSize,
        hit_shader_binding_stride: vk::DeviceSize,
        callable_shader_binding_table_buffer: vk::Buffer,
        callable_shader_binding_offset: vk::DeviceSize,
        callable_shader_binding_stride: vk::DeviceSize,
        width: u32,
        height: u32,
        depth: u32,
    ) {
        (self.fp.cmd_trace_rays_nv)(
            command_buffer,
            raygen_shader_binding_table_buffer,
            raygen_shader_binding_offset,
            miss_shader_binding_table_buffer,
            miss_shader_binding_offset,
            miss_shader_binding_stride,
            hit_shader_binding_table_buffer,
            hit_shader_binding_offset,
            hit_shader_binding_stride,
            callable_shader_binding_table_buffer,
            callable_shader_binding_offset,
            callable_shader_binding_stride,
            width,
            height,
            depth,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateRayTracingPipelinesNV.html>
    #[inline]
    pub unsafe fn create_ray_tracing_pipelines(
        &self,
        pipeline_cache: vk::PipelineCache,
        create_info: &[vk::RayTracingPipelineCreateInfoNV],
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<Vec<vk::Pipeline>> {
        let mut pipelines = vec![mem::zeroed(); create_info.len()];
        (self.fp.create_ray_tracing_pipelines_nv)(
            self.handle,
            pipeline_cache,
            create_info.len() as u32,
            create_info.as_ptr(),
            allocation_callbacks.as_raw_ptr(),
            pipelines.as_mut_ptr(),
        )
        .result_with_success(pipelines)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetRayTracingShaderGroupHandlesNV.html>
    #[inline]
    pub unsafe fn get_ray_tracing_shader_group_handles(
        &self,
        pipeline: vk::Pipeline,
        first_group: u32,
        group_count: u32,
        data: &mut [u8],
    ) -> VkResult<()> {
        (self.fp.get_ray_tracing_shader_group_handles_nv)(
            self.handle,
            pipeline,
            first_group,
            group_count,
            data.len(),
            data.as_mut_ptr().cast(),
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetAccelerationStructureHandleNV.html>
    #[inline]
    pub unsafe fn get_acceleration_structure_handle(
        &self,
        accel_struct: vk::AccelerationStructureNV,
    ) -> VkResult<u64> {
        let mut handle: u64 = 0;
        let handle_ptr: *mut u64 = &mut handle;
        (self.fp.get_acceleration_structure_handle_nv)(
            self.handle,
            accel_struct,
            std::mem::size_of::<u64>(),
            handle_ptr.cast(),
        )
        .result_with_success(handle)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdWriteAccelerationStructuresPropertiesNV.html>
    #[inline]
    pub unsafe fn cmd_write_acceleration_structures_properties(
        &self,
        command_buffer: vk::CommandBuffer,
        structures: &[vk::AccelerationStructureNV],
        query_type: vk::QueryType,
        query_pool: vk::QueryPool,
        first_query: u32,
    ) {
        (self.fp.cmd_write_acceleration_structures_properties_nv)(
            command_buffer,
            structures.len() as u32,
            structures.as_ptr(),
            query_type,
            query_pool,
            first_query,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCompileDeferredNV.html>
    #[inline]
    pub unsafe fn compile_deferred(&self, pipeline: vk::Pipeline, shader: u32) -> VkResult<()> {
        (self.fp.compile_deferred_nv)(self.handle, pipeline, shader).result()
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::NvRayTracingFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::NvRayTracingFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
