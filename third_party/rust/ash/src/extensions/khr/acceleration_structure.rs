use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct AccelerationStructure {
    handle: vk::Device,
    fp: vk::KhrAccelerationStructureFn,
}

impl AccelerationStructure {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrAccelerationStructureFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[inline]
    pub unsafe fn get_properties(
        instance: &Instance,
        pdevice: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceAccelerationStructurePropertiesKHR {
        let mut props_rt = vk::PhysicalDeviceAccelerationStructurePropertiesKHR::default();
        {
            let mut props = vk::PhysicalDeviceProperties2::builder().push_next(&mut props_rt);
            instance.get_physical_device_properties2(pdevice, &mut props);
        }
        props_rt
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateAccelerationStructureKHR.html>
    #[inline]
    pub unsafe fn create_acceleration_structure(
        &self,
        create_info: &vk::AccelerationStructureCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::AccelerationStructureKHR> {
        let mut accel_struct = mem::zeroed();
        (self.fp.create_acceleration_structure_khr)(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut accel_struct,
        )
        .result_with_success(accel_struct)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDestroyAccelerationStructureKHR.html>
    #[inline]
    pub unsafe fn destroy_acceleration_structure(
        &self,
        accel_struct: vk::AccelerationStructureKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        (self.fp.destroy_acceleration_structure_khr)(
            self.handle,
            accel_struct,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdBuildAccelerationStructuresKHR.html>
    #[inline]
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

        (self.fp.cmd_build_acceleration_structures_khr)(
            command_buffer,
            infos.len() as _,
            infos.as_ptr(),
            build_range_infos.as_ptr(),
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdBuildAccelerationStructuresIndirectKHR.html>
    #[inline]
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

        (self.fp.cmd_build_acceleration_structures_indirect_khr)(
            command_buffer,
            infos.len() as _,
            infos.as_ptr(),
            indirect_device_addresses.as_ptr(),
            indirect_strides.as_ptr(),
            max_primitive_counts.as_ptr(),
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkBuildAccelerationStructuresKHR.html>
    #[inline]
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

        (self.fp.build_acceleration_structures_khr)(
            self.handle,
            deferred_operation,
            infos.len() as _,
            infos.as_ptr(),
            build_range_infos.as_ptr(),
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCopyAccelerationStructureKHR.html>
    #[inline]
    pub unsafe fn copy_acceleration_structure(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        info: &vk::CopyAccelerationStructureInfoKHR,
    ) -> VkResult<()> {
        (self.fp.copy_acceleration_structure_khr)(self.handle, deferred_operation, info).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCopyAccelerationStructureToMemoryKHR.html>
    #[inline]
    pub unsafe fn copy_acceleration_structure_to_memory(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        info: &vk::CopyAccelerationStructureToMemoryInfoKHR,
    ) -> VkResult<()> {
        (self.fp.copy_acceleration_structure_to_memory_khr)(self.handle, deferred_operation, info)
            .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCopyMemoryToAccelerationStructureKHR.html>
    #[inline]
    pub unsafe fn copy_memory_to_acceleration_structure(
        &self,
        deferred_operation: vk::DeferredOperationKHR,
        info: &vk::CopyMemoryToAccelerationStructureInfoKHR,
    ) -> VkResult<()> {
        (self.fp.copy_memory_to_acceleration_structure_khr)(self.handle, deferred_operation, info)
            .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkWriteAccelerationStructuresPropertiesKHR.html>
    #[inline]
    pub unsafe fn write_acceleration_structures_properties(
        &self,
        acceleration_structures: &[vk::AccelerationStructureKHR],
        query_type: vk::QueryType,
        data: &mut [u8],
        stride: usize,
    ) -> VkResult<()> {
        (self.fp.write_acceleration_structures_properties_khr)(
            self.handle,
            acceleration_structures.len() as _,
            acceleration_structures.as_ptr(),
            query_type,
            data.len(),
            data.as_mut_ptr().cast(),
            stride,
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyAccelerationStructureKHR.html>
    #[inline]
    pub unsafe fn cmd_copy_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyAccelerationStructureInfoKHR,
    ) {
        (self.fp.cmd_copy_acceleration_structure_khr)(command_buffer, info);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyAccelerationStructureToMemoryKHR.html>
    #[inline]
    pub unsafe fn cmd_copy_acceleration_structure_to_memory(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyAccelerationStructureToMemoryInfoKHR,
    ) {
        (self.fp.cmd_copy_acceleration_structure_to_memory_khr)(command_buffer, info);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyMemoryToAccelerationStructureKHR.html>
    #[inline]
    pub unsafe fn cmd_copy_memory_to_acceleration_structure(
        &self,
        command_buffer: vk::CommandBuffer,
        info: &vk::CopyMemoryToAccelerationStructureInfoKHR,
    ) {
        (self.fp.cmd_copy_memory_to_acceleration_structure_khr)(command_buffer, info);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetAccelerationStructureHandleKHR.html>
    #[inline]
    pub unsafe fn get_acceleration_structure_device_address(
        &self,
        info: &vk::AccelerationStructureDeviceAddressInfoKHR,
    ) -> vk::DeviceAddress {
        (self.fp.get_acceleration_structure_device_address_khr)(self.handle, info)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdWriteAccelerationStructuresPropertiesKHR.html>
    #[inline]
    pub unsafe fn cmd_write_acceleration_structures_properties(
        &self,
        command_buffer: vk::CommandBuffer,
        structures: &[vk::AccelerationStructureKHR],
        query_type: vk::QueryType,
        query_pool: vk::QueryPool,
        first_query: u32,
    ) {
        (self.fp.cmd_write_acceleration_structures_properties_khr)(
            command_buffer,
            structures.len() as _,
            structures.as_ptr(),
            query_type,
            query_pool,
            first_query,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceAccelerationStructureCompatibilityKHR.html>
    #[inline]
    pub unsafe fn get_device_acceleration_structure_compatibility(
        &self,
        version: &vk::AccelerationStructureVersionInfoKHR,
    ) -> vk::AccelerationStructureCompatibilityKHR {
        let mut compatibility = vk::AccelerationStructureCompatibilityKHR::default();

        (self.fp.get_device_acceleration_structure_compatibility_khr)(
            self.handle,
            version,
            &mut compatibility,
        );

        compatibility
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetAccelerationStructureBuildSizesKHR.html>
    #[inline]
    pub unsafe fn get_acceleration_structure_build_sizes(
        &self,
        build_type: vk::AccelerationStructureBuildTypeKHR,
        build_info: &vk::AccelerationStructureBuildGeometryInfoKHR,
        max_primitive_counts: &[u32],
    ) -> vk::AccelerationStructureBuildSizesInfoKHR {
        assert_eq!(max_primitive_counts.len(), build_info.geometry_count as _);

        let mut size_info = vk::AccelerationStructureBuildSizesInfoKHR::default();

        (self.fp.get_acceleration_structure_build_sizes_khr)(
            self.handle,
            build_type,
            build_info,
            max_primitive_counts.as_ptr(),
            &mut size_info,
        );

        size_info
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrAccelerationStructureFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrAccelerationStructureFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
