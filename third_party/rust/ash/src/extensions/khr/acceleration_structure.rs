#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{DeviceV1_0, InstanceV1_0, InstanceV1_1};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct AccelerationStructure {
    handle: vk::Device,
    acceleration_structure_fn: vk::KhrAccelerationStructureFn,
}

impl AccelerationStructure {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(instance: &I, device: &D) -> Self {
        let acceleration_structure_fn = vk::KhrAccelerationStructureFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            acceleration_structure_fn,
        }
    }

    pub unsafe fn get_properties<I: InstanceV1_1>(
        instance: &I,
        pdevice: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceAccelerationStructurePropertiesKHR {
        let mut props_rt = vk::PhysicalDeviceAccelerationStructurePropertiesKHR::default();
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
        self.acceleration_structure_fn
            .create_acceleration_structure_khr(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut accel_struct,
            )
            .result_with_success(accel_struct)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyAccelerationStructureKHR.html>"]
    pub unsafe fn destroy_acceleration_structure(
        &self,
        accel_struct: vk::AccelerationStructureKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.acceleration_structure_fn
            .destroy_acceleration_structure_khr(
                self.handle,
                accel_struct,
                allocation_callbacks.as_raw_ptr(),
            );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBuildAccelerationStructuresKHR.html>"]
    pub unsafe fn cmd_build_acceleration_structures(
        &self,
        command_buffer: vk::CommandBuffer,
        infos: &[vk::AccelerationStructureBuildGeometryInfoKHR],
        build_range_infos: &[&[vk::AccelerationStructureBuildRangeInfoKHR]],
    ) {
        assert_eq!(infos.len(), build_range_infos.len());

        let build_range_infos = build_range_infos
            .iter()
            .zip(infos.iter())
            .map(|(range_info, info)| {
                assert_eq!(range_info.len(), info.geometry_count as usize);
                range_info.as_ptr()
            })
            .collect::<Vec<_>>();

        self.acceleration_structure_fn
            .cmd_build_acceleration_structures_khr(
                command_buffer,
                infos.len() as _,
                infos.as_ptr(),
                build_range_infos.as_ptr(),
            );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBuildAccelerationStructuresIndirectKHR.html>"]
    pub unsafe fn cmd_build_acceleration_structures_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        infos: &[vk::AccelerationStructureBuildGeometryInfoKHR],
        indirect_device_addresses: &[vk::DeviceAddress],
        indirect_strides: &[u32],
        max_primitive_counts: &[&[u32]],
    ) {
        assert_eq!(infos.len(), indirect_device_addresses.len());
        assert_eq!(infos.len(), indirect_strides.len());
        assert_eq!(infos.len(), max_primitive_counts.len());

        let max_primitive_counts = max_primitive_counts
            .iter()
            .zip(infos.iter())
            .map(|(cnt, info)| {
                assert_eq!(cnt.len(), info.geometry_count as usize);
                cnt.as_ptr()
            })
            .collect::<Vec<_>>();

        self.acceleration_structure_fn
            .cmd_build_acceleration_structures_indirect_khr(
                command_buffer,
                infos.len() as _,
                infos.as_ptr(),
                indirect_device_addresses.as_ptr(),
                indirect_strides.as_ptr(),
                max_primitive_counts.as_ptr(),
            );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBuildAccelerationStructuresKHR.html>"]
    pub unsafe fn build_acceleration_structures(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        infos: &[vk::AccelerationStructureBuildGeometryInfoKHR],
        build_range_infos: &[&[vk::AccelerationStructureBuildRangeInfoKHR]],
    ) -> VkResult<()> {
        assert_eq!(infos.len(), build_range_infos.len());

        let build_range_infos = build_range_infos
            .iter()
            .zip(infos.iter())
            .map(|(range_info, info)| {
                assert_eq!(range_info.len(), info.geometry_count as usize);
                range_info.as_ptr()
            })
            .collect::<Vec<_>>();

        self.acceleration_structure_fn
            .build_acceleration_structures_khr(
                self.handle,
                deferred_operation,
                infos.len() as _,
                infos.as_ptr(),
                build_range_infos.as_ptr(),
            )
            .into()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCopyAccelerationStructureKHR.html>"]
    pub unsafe fn copy_acceleration_structure(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        info: &vk::CopyAccelerationStructureInfoKHR,
    ) -> VkResult<()> {
        self.acceleration_structure_fn
            .copy_acceleration_structure_khr(self.handle, deferred_operation, info as *const _)
            .into()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCopyAccelerationStructureToMemoryKHR.html>"]
    pub unsafe fn copy_acceleration_structure_to_memory(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        info: &vk::CopyAccelerationStructureToMemoryInfoKHR,
    ) -> VkResult<()> {
        self.acceleration_structure_fn
            .copy_acceleration_structure_to_memory_khr(
                self.handle,
                deferred_operation,
                info as *const _,
            )
            .into()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCopyMemoryToAccelerationStructureKHR.html>"]
    pub unsafe fn copy_memory_to_acceleration_structure(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        info: &vk::CopyMemoryToAccelerationStructureInfoKHR,
    ) -> VkResult<()> {
        self.acceleration_structure_fn
            .copy_memory_to_acceleration_structure_khr(
                self.handle,
                deferred_operation,
                info as *const _,
            )
            .into()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWriteAccelerationStructuresPropertiesKHR.html>"]
    pub unsafe fn write_acceleration_structures_properties(
        &self,
        acceleration_structures: &[vk::AccelerationStructureKHR],
        query_type: vk::QueryType,
        data: &mut [u8],
        stride: usize,
    ) -> VkResult<()> {
        self.acceleration_structure_fn
            .write_acceleration_structures_properties_khr(
                self.handle,
                acceleration_structures.len() as _,
                acceleration_structures.as_ptr(),
                query_type,
                data.len(),
                data.as_mut_ptr() as *mut std::ffi::c_void,
                stride,
            )
            .into()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyAccelerationStructureKHR.html>"]
    pub unsafe fn cmd_copy_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyAccelerationStructureInfoKHR,
    ) {
        self.acceleration_structure_fn
            .cmd_copy_acceleration_structure_khr(command_buffer, info);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyAccelerationStructureToMemoryKHR.html>"]
    pub unsafe fn cmd_copy_acceleration_structure_to_memory(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyAccelerationStructureToMemoryInfoKHR,
    ) {
        self.acceleration_structure_fn
            .cmd_copy_acceleration_structure_to_memory_khr(command_buffer, info as *const _);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyMemoryToAccelerationStructureKHR.html>"]
    pub unsafe fn cmd_copy_memory_to_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyMemoryToAccelerationStructureInfoKHR,
    ) {
        self.acceleration_structure_fn
            .cmd_copy_memory_to_acceleration_structure_khr(command_buffer, info as *const _);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureHandleKHR.html>"]
    pub unsafe fn get_acceleration_structure_device_address(
        &self,
        info: &vk::AccelerationStructureDeviceAddressInfoKHR,
    ) -> vk::DeviceAddress {
        self.acceleration_structure_fn
            .get_acceleration_structure_device_address_khr(self.handle, info as *const _)
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
        self.acceleration_structure_fn
            .cmd_write_acceleration_structures_properties_khr(
                command_buffer,
                structures.len() as _,
                structures.as_ptr(),
                query_type,
                query_pool,
                first_query,
            );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceAccelerationStructureCompatibilityKHR.html>"]
    pub unsafe fn get_device_acceleration_structure_compatibility(
        &self,
        version: &vk::AccelerationStructureVersionInfoKHR,
    ) -> vk::AccelerationStructureCompatibilityKHR {
        let mut compatibility = vk::AccelerationStructureCompatibilityKHR::default();

        self.acceleration_structure_fn
            .get_device_acceleration_structure_compatibility_khr(
                self.handle,
                version,
                &mut compatibility as *mut _,
            );

        compatibility
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureBuildSizesKHR.html>"]
    pub unsafe fn get_acceleration_structure_build_sizes(
        &self,
        build_type: vk::AccelerationStructureBuildTypeKHR,
        build_info: &vk::AccelerationStructureBuildGeometryInfoKHR,
        max_primitive_counts: &[u32],
    ) -> vk::AccelerationStructureBuildSizesInfoKHR {
        assert_eq!(max_primitive_counts.len(), build_info.geometry_count as _);

        let mut size_info = vk::AccelerationStructureBuildSizesInfoKHR::default();

        self.acceleration_structure_fn
            .get_acceleration_structure_build_sizes_khr(
                self.handle,
                build_type,
                build_info as *const _,
                max_primitive_counts.as_ptr(),
                &mut size_info as *mut _,
            );

        size_info
    }

    pub fn name() -> &'static CStr {
        vk::KhrAccelerationStructureFn::name()
    }

    pub fn fp(&self) -> &vk::KhrAccelerationStructureFn {
        &self.acceleration_structure_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
