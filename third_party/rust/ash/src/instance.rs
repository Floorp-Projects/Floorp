#[cfg(doc)]
use super::Entry;
use crate::device::Device;
use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use std::mem;
use std::os::raw::c_char;
use std::ptr;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkInstance.html>
#[derive(Clone)]
pub struct Instance {
    pub(crate) handle: vk::Instance,

    pub(crate) instance_fn_1_0: vk::InstanceFnV1_0,
    pub(crate) instance_fn_1_1: vk::InstanceFnV1_1,
    pub(crate) instance_fn_1_2: vk::InstanceFnV1_2,
    pub(crate) instance_fn_1_3: vk::InstanceFnV1_3,
}

impl Instance {
    pub unsafe fn load(static_fn: &vk::StaticFn, instance: vk::Instance) -> Self {
        let load_fn = |name: &std::ffi::CStr| {
            mem::transmute((static_fn.get_instance_proc_addr)(instance, name.as_ptr()))
        };

        Self {
            handle: instance,

            instance_fn_1_0: vk::InstanceFnV1_0::load(load_fn),
            instance_fn_1_1: vk::InstanceFnV1_1::load(load_fn),
            instance_fn_1_2: vk::InstanceFnV1_2::load(load_fn),
            instance_fn_1_3: vk::InstanceFnV1_3::load(load_fn),
        }
    }

    #[inline]
    pub fn handle(&self) -> vk::Instance {
        self.handle
    }
}

/// Vulkan core 1.3
#[allow(non_camel_case_types)]
impl Instance {
    #[inline]
    pub fn fp_v1_3(&self) -> &vk::InstanceFnV1_3 {
        &self.instance_fn_1_3
    }

    /// Retrieve the number of elements to pass to [`get_physical_device_tool_properties()`][Self::get_physical_device_tool_properties()]
    #[inline]
    pub unsafe fn get_physical_device_tool_properties_len(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> VkResult<usize> {
        let mut count = 0;
        (self.instance_fn_1_3.get_physical_device_tool_properties)(
            physical_device,
            &mut count,
            ptr::null_mut(),
        )
        .result_with_success(count as usize)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceToolProperties.html>
    ///
    /// Call [`get_physical_device_tool_properties_len()`][Self::get_physical_device_tool_properties_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_physical_device_tool_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        out: &mut [vk::PhysicalDeviceToolProperties],
    ) -> VkResult<()> {
        let mut count = out.len() as u32;
        (self.instance_fn_1_3.get_physical_device_tool_properties)(
            physical_device,
            &mut count,
            out.as_mut_ptr(),
        )
        .result()?;
        assert_eq!(count as usize, out.len());
        Ok(())
    }
}

/// Vulkan core 1.2
#[allow(non_camel_case_types)]
impl Instance {
    #[inline]
    pub fn fp_v1_2(&self) -> &vk::InstanceFnV1_2 {
        &self.instance_fn_1_2
    }
}

/// Vulkan core 1.1
#[allow(non_camel_case_types)]
impl Instance {
    #[inline]
    pub fn fp_v1_1(&self) -> &vk::InstanceFnV1_1 {
        &self.instance_fn_1_1
    }

    /// Retrieve the number of elements to pass to [`enumerate_physical_device_groups()`][Self::enumerate_physical_device_groups()]
    #[inline]
    pub unsafe fn enumerate_physical_device_groups_len(&self) -> VkResult<usize> {
        let mut group_count = 0;
        (self.instance_fn_1_1.enumerate_physical_device_groups)(
            self.handle(),
            &mut group_count,
            ptr::null_mut(),
        )
        .result_with_success(group_count as usize)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumeratePhysicalDeviceGroups.html>
    ///
    /// Call [`enumerate_physical_device_groups_len()`][Self::enumerate_physical_device_groups_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn enumerate_physical_device_groups(
        &self,
        out: &mut [vk::PhysicalDeviceGroupProperties],
    ) -> VkResult<()> {
        let mut count = out.len() as u32;
        (self.instance_fn_1_1.enumerate_physical_device_groups)(
            self.handle(),
            &mut count,
            out.as_mut_ptr(),
        )
        .result()?;
        assert_eq!(count as usize, out.len());
        Ok(())
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceFeatures2.html>
    #[inline]
    pub unsafe fn get_physical_device_features2(
        &self,
        physical_device: vk::PhysicalDevice,
        features: &mut vk::PhysicalDeviceFeatures2,
    ) {
        (self.instance_fn_1_1.get_physical_device_features2)(physical_device, features);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceProperties2.html>
    #[inline]
    pub unsafe fn get_physical_device_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        prop: &mut vk::PhysicalDeviceProperties2,
    ) {
        (self.instance_fn_1_1.get_physical_device_properties2)(physical_device, prop);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceFormatProperties2.html>
    #[inline]
    pub unsafe fn get_physical_device_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
        out: &mut vk::FormatProperties2,
    ) {
        (self.instance_fn_1_1.get_physical_device_format_properties2)(physical_device, format, out);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties2.html>
    #[inline]
    pub unsafe fn get_physical_device_image_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceImageFormatInfo2,
        image_format_prop: &mut vk::ImageFormatProperties2,
    ) -> VkResult<()> {
        (self
            .instance_fn_1_1
            .get_physical_device_image_format_properties2)(
            physical_device,
            format_info,
            image_format_prop,
        )
        .result()
    }

    /// Retrieve the number of elements to pass to [`get_physical_device_queue_family_properties2()`][Self::get_physical_device_queue_family_properties2()]
    #[inline]
    pub unsafe fn get_physical_device_queue_family_properties2_len(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> usize {
        let mut queue_count = 0;
        (self
            .instance_fn_1_1
            .get_physical_device_queue_family_properties2)(
            physical_device,
            &mut queue_count,
            ptr::null_mut(),
        );
        queue_count as usize
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties2.html>
    ///
    /// Call [`get_physical_device_queue_family_properties2_len()`][Self::get_physical_device_queue_family_properties2_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_physical_device_queue_family_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        out: &mut [vk::QueueFamilyProperties2],
    ) {
        let mut count = out.len() as u32;
        (self
            .instance_fn_1_1
            .get_physical_device_queue_family_properties2)(
            physical_device,
            &mut count,
            out.as_mut_ptr(),
        );
        assert_eq!(count as usize, out.len());
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceMemoryProperties2.html>
    #[inline]
    pub unsafe fn get_physical_device_memory_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        out: &mut vk::PhysicalDeviceMemoryProperties2,
    ) {
        (self.instance_fn_1_1.get_physical_device_memory_properties2)(physical_device, out);
    }

    /// Retrieve the number of elements to pass to [`get_physical_device_sparse_image_format_properties2()`][Self::get_physical_device_sparse_image_format_properties2()]
    #[inline]
    pub unsafe fn get_physical_device_sparse_image_format_properties2_len(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceSparseImageFormatInfo2,
    ) -> usize {
        let mut format_count = 0;
        (self
            .instance_fn_1_1
            .get_physical_device_sparse_image_format_properties2)(
            physical_device,
            format_info,
            &mut format_count,
            ptr::null_mut(),
        );
        format_count as usize
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceSparseImageFormatProperties2.html>
    ///
    /// Call [`get_physical_device_sparse_image_format_properties2_len()`][Self::get_physical_device_sparse_image_format_properties2_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_physical_device_sparse_image_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceSparseImageFormatInfo2,
        out: &mut [vk::SparseImageFormatProperties2],
    ) {
        let mut count = out.len() as u32;
        (self
            .instance_fn_1_1
            .get_physical_device_sparse_image_format_properties2)(
            physical_device,
            format_info,
            &mut count,
            out.as_mut_ptr(),
        );
        assert_eq!(count as usize, out.len());
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceExternalBufferProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_external_buffer_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        external_buffer_info: &vk::PhysicalDeviceExternalBufferInfo,
        out: &mut vk::ExternalBufferProperties,
    ) {
        (self
            .instance_fn_1_1
            .get_physical_device_external_buffer_properties)(
            physical_device,
            external_buffer_info,
            out,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceExternalFenceProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_external_fence_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        external_fence_info: &vk::PhysicalDeviceExternalFenceInfo,
        out: &mut vk::ExternalFenceProperties,
    ) {
        (self
            .instance_fn_1_1
            .get_physical_device_external_fence_properties)(
            physical_device,
            external_fence_info,
            out,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceExternalSemaphoreProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_external_semaphore_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        external_semaphore_info: &vk::PhysicalDeviceExternalSemaphoreInfo,
        out: &mut vk::ExternalSemaphoreProperties,
    ) {
        (self
            .instance_fn_1_1
            .get_physical_device_external_semaphore_properties)(
            physical_device,
            external_semaphore_info,
            out,
        );
    }
}

/// Vulkan core 1.0
#[allow(non_camel_case_types)]
impl Instance {
    #[inline]
    pub fn fp_v1_0(&self) -> &vk::InstanceFnV1_0 {
        &self.instance_fn_1_0
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateDevice.html>
    ///
    /// # Safety
    ///
    /// There is a [parent/child relation] between [`Instance`] and the resulting [`Device`].  The
    /// application must not [destroy][Instance::destroy_instance()] the parent [`Instance`] object
    /// before first [destroying][Device::destroy_device()] the returned [`Device`] child object.
    /// [`Device`] does _not_ implement [drop][drop()] semantics and can only be destroyed via
    /// [`destroy_device()`][Device::destroy_device()].
    ///
    /// See the [`Entry::create_instance()`] documentation for more destruction ordering rules on
    /// [`Instance`].
    ///
    /// [parent/child relation]: https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#fundamentals-objectmodel-lifetime
    #[inline]
    pub unsafe fn create_device(
        &self,
        physical_device: vk::PhysicalDevice,
        create_info: &vk::DeviceCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<Device> {
        let mut device = mem::zeroed();
        (self.instance_fn_1_0.create_device)(
            physical_device,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut device,
        )
        .result()?;
        Ok(Device::load(&self.instance_fn_1_0, device))
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceProcAddr.html>
    #[inline]
    pub unsafe fn get_device_proc_addr(
        &self,
        device: vk::Device,
        p_name: *const c_char,
    ) -> vk::PFN_vkVoidFunction {
        (self.instance_fn_1_0.get_device_proc_addr)(device, p_name)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDestroyInstance.html>
    #[inline]
    pub unsafe fn destroy_instance(&self, allocation_callbacks: Option<&vk::AllocationCallbacks>) {
        (self.instance_fn_1_0.destroy_instance)(self.handle(), allocation_callbacks.as_raw_ptr());
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceFormatProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_format_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
    ) -> vk::FormatProperties {
        let mut format_prop = mem::zeroed();
        (self.instance_fn_1_0.get_physical_device_format_properties)(
            physical_device,
            format,
            &mut format_prop,
        );
        format_prop
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_image_format_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
        typ: vk::ImageType,
        tiling: vk::ImageTiling,
        usage: vk::ImageUsageFlags,
        flags: vk::ImageCreateFlags,
    ) -> VkResult<vk::ImageFormatProperties> {
        let mut image_format_prop = mem::zeroed();
        (self
            .instance_fn_1_0
            .get_physical_device_image_format_properties)(
            physical_device,
            format,
            typ,
            tiling,
            usage,
            flags,
            &mut image_format_prop,
        )
        .result_with_success(image_format_prop)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceMemoryProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_memory_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceMemoryProperties {
        let mut memory_prop = mem::zeroed();
        (self.instance_fn_1_0.get_physical_device_memory_properties)(
            physical_device,
            &mut memory_prop,
        );
        memory_prop
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceProperties {
        let mut prop = mem::zeroed();
        (self.instance_fn_1_0.get_physical_device_properties)(physical_device, &mut prop);
        prop
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_queue_family_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> Vec<vk::QueueFamilyProperties> {
        read_into_uninitialized_vector(|count, data| {
            (self
                .instance_fn_1_0
                .get_physical_device_queue_family_properties)(
                physical_device, count, data
            );
            vk::Result::SUCCESS
        })
        // The closure always returns SUCCESS
        .unwrap()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceFeatures.html>
    #[inline]
    pub unsafe fn get_physical_device_features(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceFeatures {
        let mut prop = mem::zeroed();
        (self.instance_fn_1_0.get_physical_device_features)(physical_device, &mut prop);
        prop
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumeratePhysicalDevices.html>
    #[inline]
    pub unsafe fn enumerate_physical_devices(&self) -> VkResult<Vec<vk::PhysicalDevice>> {
        read_into_uninitialized_vector(|count, data| {
            (self.instance_fn_1_0.enumerate_physical_devices)(self.handle(), count, data)
        })
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumerateDeviceExtensionProperties.html>
    #[inline]
    pub unsafe fn enumerate_device_extension_properties(
        &self,
        device: vk::PhysicalDevice,
    ) -> VkResult<Vec<vk::ExtensionProperties>> {
        read_into_uninitialized_vector(|count, data| {
            (self.instance_fn_1_0.enumerate_device_extension_properties)(
                device,
                ptr::null(),
                count,
                data,
            )
        })
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumerateDeviceLayerProperties.html>
    #[inline]
    pub unsafe fn enumerate_device_layer_properties(
        &self,
        device: vk::PhysicalDevice,
    ) -> VkResult<Vec<vk::LayerProperties>> {
        read_into_uninitialized_vector(|count, data| {
            (self.instance_fn_1_0.enumerate_device_layer_properties)(device, count, data)
        })
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceSparseImageFormatProperties.html>
    #[inline]
    pub unsafe fn get_physical_device_sparse_image_format_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
        typ: vk::ImageType,
        samples: vk::SampleCountFlags,
        usage: vk::ImageUsageFlags,
        tiling: vk::ImageTiling,
    ) -> Vec<vk::SparseImageFormatProperties> {
        read_into_uninitialized_vector(|count, data| {
            (self
                .instance_fn_1_0
                .get_physical_device_sparse_image_format_properties)(
                physical_device,
                format,
                typ,
                samples,
                usage,
                tiling,
                count,
                data,
            );
            vk::Result::SUCCESS
        })
        // The closure always returns SUCCESS
        .unwrap()
    }
}
