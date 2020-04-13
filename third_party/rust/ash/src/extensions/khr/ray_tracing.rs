#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{DeviceV1_0, InstanceV1_0, InstanceV1_1};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct RayTracing {
    handle: vk::Device,
    ray_tracing_fn: vk::KhrRayTracingFn,
}

impl RayTracing {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(instance: &I, device: &D) -> RayTracing {
        let ray_tracing_fn = vk::KhrRayTracingFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        RayTracing {
            handle: device.handle(),
            ray_tracing_fn,
        }
    }

    pub unsafe fn get_properties<I: InstanceV1_1>(
        instance: &I,
        pdevice: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceRayTracingPropertiesKHR {
        let mut props_rt = vk::PhysicalDeviceRayTracingPropertiesKHR::default();
        {
            let mut props = vk::PhysicalDeviceProperties2::builder().push_next(&mut props_rt);
            instance.get_physical_device_properties2(pdevice, &mut props);
        }
        props_rt
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateAccelerationStructureKHR.html>"]
    pub unsafe fn create_acceleration_structure(
        &self,
        create_info: &vk::AccelerationStructureCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::AccelerationStructureKHR> {
        let mut accel_struct = mem::zeroed();
        let err_code = self.ray_tracing_fn.create_acceleration_structure_khr(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut accel_struct,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(accel_struct),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyAccelerationStructureKHR.html>"]
    pub unsafe fn destroy_acceleration_structure(
        &self,
        accel_struct: vk::AccelerationStructureKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.ray_tracing_fn.destroy_acceleration_structure_khr(
            self.handle,
            accel_struct,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureMemoryRequirementsKHR.html>"]
    pub unsafe fn get_acceleration_structure_memory_requirements(
        &self,
        info: &vk::AccelerationStructureMemoryRequirementsInfoKHR,
    ) -> vk::MemoryRequirements2KHR {
        let mut requirements = mem::zeroed();
        self.ray_tracing_fn
            .get_acceleration_structure_memory_requirements_khr(
                self.handle,
                info,
                &mut requirements,
            );
        requirements
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindAccelerationStructureMemoryKHR.html>"]
    pub unsafe fn bind_acceleration_structure_memory(
        &self,
        bind_info: &[vk::BindAccelerationStructureMemoryInfoKHR],
    ) -> VkResult<()> {
        let err_code = self.ray_tracing_fn.bind_acceleration_structure_memory_khr(
            self.handle,
            bind_info.len() as u32,
            bind_info.as_ptr(),
        );
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBuildAccelerationStructureKHR.html>"]
    pub unsafe fn cmd_build_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        infos: &[vk::AccelerationStructureBuildGeometryInfoKHR],
        offset_infos: &[&[vk::AccelerationStructureBuildOffsetInfoKHR]],
    ) {
        let offset_info_ptr = offset_infos
            .iter()
            .map(|slice| slice.as_ptr())
            .collect::<Vec<*const vk::AccelerationStructureBuildOffsetInfoKHR>>();

        self.ray_tracing_fn.cmd_build_acceleration_structure_khr(
            command_buffer,
            infos.len() as u32,
            infos.as_ptr(),
            offset_info_ptr.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyAccelerationStructureKHR.html>"]
    pub unsafe fn cmd_copy_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyAccelerationStructureInfoKHR,
    ) {
        self.ray_tracing_fn
            .cmd_copy_acceleration_structure_khr(command_buffer, info);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdTraceRaysKHR.html>"]
    pub unsafe fn cmd_trace_rays(
        &self,
        command_buffer: vk::CommandBuffer,
        raygen_shader_binding_tables: &[vk::StridedBufferRegionKHR],
        miss_shader_binding_tables: &[vk::StridedBufferRegionKHR],
        hit_shader_binding_tables: &[vk::StridedBufferRegionKHR],
        callable_shader_binding_tables: &[vk::StridedBufferRegionKHR],
        width: u32,
        height: u32,
        depth: u32,
    ) {
        self.ray_tracing_fn.cmd_trace_rays_khr(
            command_buffer,
            raygen_shader_binding_tables.as_ptr(),
            miss_shader_binding_tables.as_ptr(),
            hit_shader_binding_tables.as_ptr(),
            callable_shader_binding_tables.as_ptr(),
            width,
            height,
            depth,
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRayTracingPipelinesKHR.html>"]
    pub unsafe fn create_ray_tracing_pipelines(
        &self,
        pipeline_cache: vk::PipelineCache,
        create_info: &[vk::RayTracingPipelineCreateInfoKHR],
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<Vec<vk::Pipeline>> {
        let mut pipelines = vec![mem::zeroed(); create_info.len()];
        let err_code = self.ray_tracing_fn.create_ray_tracing_pipelines_khr(
            self.handle,
            pipeline_cache,
            create_info.len() as u32,
            create_info.as_ptr(),
            allocation_callbacks.as_raw_ptr(),
            pipelines.as_mut_ptr(),
        );
        match err_code {
            vk::Result::SUCCESS => Ok(pipelines),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRayTracingShaderGroupHandlesKHR.html>"]
    pub unsafe fn get_ray_tracing_shader_group_handles(
        &self,
        pipeline: vk::Pipeline,
        first_group: u32,
        group_count: u32,
        data: &mut [u8],
    ) -> VkResult<()> {
        let err_code = self
            .ray_tracing_fn
            .get_ray_tracing_shader_group_handles_khr(
                self.handle,
                pipeline,
                first_group,
                group_count,
                data.len(),
                data.as_mut_ptr() as *mut std::ffi::c_void,
            );
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureHandleKHR.html>"]
    pub unsafe fn get_acceleration_structure_device_address(
        &self,
        info: &vk::AccelerationStructureDeviceAddressInfoKHR,
    ) -> vk::DeviceAddress {
        self.ray_tracing_fn
            .get_acceleration_structure_device_address_khr(self.handle, info)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteAccelerationStructuresPropertiesKHR.html>"]
    pub unsafe fn cmd_write_acceleration_structures_properties(
        &self,
        command_buffer: vk::CommandBuffer,
        structures: &[vk::AccelerationStructureKHR],
        query_type: vk::QueryType,
        query_pool: vk::QueryPool,
        first_query: u32,
    ) {
        self.ray_tracing_fn
            .cmd_write_acceleration_structures_properties_khr(
                command_buffer,
                structures.len() as u32,
                structures.as_ptr(),
                query_type,
                query_pool,
                first_query,
            );
    }

    pub unsafe fn cmd_build_acceleration_structure_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::AccelerationStructureBuildGeometryInfoKHR,
        indirect_buffer: vk::Buffer,
        indirect_offset: vk::DeviceSize,
        indirect_stride: u32,
    ) {
        self.ray_tracing_fn
            .cmd_build_acceleration_structure_indirect_khr(
                command_buffer,
                info,
                indirect_buffer,
                indirect_offset,
                indirect_stride,
            );
    }

    pub unsafe fn copy_acceleration_structure_to_memory(
        &self,
        device: vk::Device,
        info: &vk::CopyAccelerationStructureToMemoryInfoKHR,
    ) -> VkResult<()> {
        let err_code = self
            .ray_tracing_fn
            .copy_acceleration_structure_to_memory_khr(device, info);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    pub unsafe fn copy_memory_to_acceleration_structure(
        &self,
        device: vk::Device,
        info: &vk::CopyMemoryToAccelerationStructureInfoKHR,
    ) -> VkResult<()> {
        let err_code = self
            .ray_tracing_fn
            .copy_memory_to_acceleration_structure_khr(device, info);

        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    pub unsafe fn cmd_copy_acceleration_structure_to_memory(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyAccelerationStructureToMemoryInfoKHR,
    ) {
        self.ray_tracing_fn
            .cmd_copy_acceleration_structure_to_memory_khr(command_buffer, info);
    }

    pub unsafe fn cmd_copy_memory_to_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyMemoryToAccelerationStructureInfoKHR,
    ) {
        self.ray_tracing_fn
            .cmd_copy_memory_to_acceleration_structure_khr(command_buffer, info);
    }

    pub unsafe fn get_ray_tracing_capture_replay_shader_group_handles(
        &self,
        device: vk::Device,
        pipeline: vk::Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
    ) -> VkResult<Vec<u8>> {
        let mut data: Vec<u8> = Vec::with_capacity(data_size);

        let err_code = self
            .ray_tracing_fn
            .get_ray_tracing_capture_replay_shader_group_handles_khr(
                device,
                pipeline,
                first_group,
                group_count,
                data_size,
                data.as_mut_ptr() as *mut _,
            );

        match err_code {
            vk::Result::SUCCESS => Ok(data),
            _ => Err(err_code),
        }
    }

    pub unsafe fn cmd_trace_rays_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        raygen_shader_binding_table: &[vk::StridedBufferRegionKHR],
        miss_shader_binding_table: &[vk::StridedBufferRegionKHR],
        hit_shader_binding_table: &[vk::StridedBufferRegionKHR],
        callable_shader_binding_table: &[vk::StridedBufferRegionKHR],
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
    ) {
        self.ray_tracing_fn.cmd_trace_rays_indirect_khr(
            command_buffer,
            raygen_shader_binding_table.as_ptr(),
            miss_shader_binding_table.as_ptr(),
            hit_shader_binding_table.as_ptr(),
            callable_shader_binding_table.as_ptr(),
            buffer,
            offset,
        );
    }

    pub unsafe fn get_device_acceleration_structure_compatibility(
        &self,
        device: vk::Device,
        version: &vk::AccelerationStructureVersionKHR,
    ) -> VkResult<()> {
        let err_code = self
            .ray_tracing_fn
            .get_device_acceleration_structure_compatibility_khr(device, version);

        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrRayTracingFn::name()
    }

    pub fn fp(&self) -> &vk::KhrRayTracingFn {
        &self.ray_tracing_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
