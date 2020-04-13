#![allow(dead_code)]
use crate::device::Device;
use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use std::mem;
use std::os::raw::c_char;
use std::ptr;

#[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkInstance.html>"]
#[derive(Clone)]
pub struct Instance {
    handle: vk::Instance,
    instance_fn_1_0: vk::InstanceFnV1_0,
    instance_fn_1_1: vk::InstanceFnV1_1,
    instance_fn_1_2: vk::InstanceFnV1_2,
}
impl Instance {
    pub unsafe fn load(static_fn: &vk::StaticFn, instance: vk::Instance) -> Self {
        let instance_fn_1_0 = vk::InstanceFnV1_0::load(|name| {
            mem::transmute(static_fn.get_instance_proc_addr(instance, name.as_ptr()))
        });
        let instance_fn_1_1 = vk::InstanceFnV1_1::load(|name| {
            mem::transmute(static_fn.get_instance_proc_addr(instance, name.as_ptr()))
        });
        let instance_fn_1_2 = vk::InstanceFnV1_2::load(|name| {
            mem::transmute(static_fn.get_instance_proc_addr(instance, name.as_ptr()))
        });

        Instance {
            handle: instance,
            instance_fn_1_0,
            instance_fn_1_1,
            instance_fn_1_2,
        }
    }
}

impl InstanceV1_0 for Instance {
    type Device = Device;
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDevice.html>"]
    ///
    /// # Safety
    /// In order for the created `Device` to be valid for the duration of its
    /// usage, the `Instance` this was called on must be dropped later than the
    /// resulting `Device`.
    unsafe fn create_device(
        &self,
        physical_device: vk::PhysicalDevice,
        create_info: &vk::DeviceCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> Result<Self::Device, vk::Result> {
        let mut device: vk::Device = mem::zeroed();
        let err_code = self.fp_v1_0().create_device(
            physical_device,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut device,
        );
        if err_code != vk::Result::SUCCESS {
            return Err(err_code);
        }
        Ok(Device::load(&self.instance_fn_1_0, device))
    }
    fn handle(&self) -> vk::Instance {
        self.handle
    }

    fn fp_v1_0(&self) -> &vk::InstanceFnV1_0 {
        &self.instance_fn_1_0
    }
}

impl InstanceV1_1 for Instance {
    fn fp_v1_1(&self) -> &vk::InstanceFnV1_1 {
        &self.instance_fn_1_1
    }
}

impl InstanceV1_2 for Instance {
    fn fp_v1_2(&self) -> &vk::InstanceFnV1_2 {
        &self.instance_fn_1_2
    }
}

#[allow(non_camel_case_types)]
pub trait InstanceV1_2: InstanceV1_1 {
    fn fp_v1_2(&self) -> &vk::InstanceFnV1_2;
}

#[allow(non_camel_case_types)]
pub trait InstanceV1_1: InstanceV1_0 {
    fn fp_v1_1(&self) -> &vk::InstanceFnV1_1;

    unsafe fn enumerate_physical_device_groups_len(&self) -> usize {
        let mut group_count = mem::zeroed();
        self.fp_v1_1().enumerate_physical_device_groups(
            self.handle(),
            &mut group_count,
            ptr::null_mut(),
        );
        group_count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumeratePhysicalDeviceGroups.html>"]
    fn enumerate_physical_device_groups(
        &self,
        out: &mut [vk::PhysicalDeviceGroupProperties],
    ) -> VkResult<()> {
        unsafe {
            let mut group_count = out.len() as u32;
            let err_code = self.fp_v1_1().enumerate_physical_device_groups(
                self.handle(),
                &mut group_count,
                out.as_mut_ptr(),
            );
            if err_code == vk::Result::SUCCESS {
                Ok(())
            } else {
                Err(err_code)
            }
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFeatures2.html>"]
    unsafe fn get_physical_device_features2(
        &self,
        physical_device: vk::PhysicalDevice,
        features: &mut vk::PhysicalDeviceFeatures2,
    ) {
        self.fp_v1_1()
            .get_physical_device_features2(physical_device, features);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceProperties2.html>"]
    unsafe fn get_physical_device_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        prop: &mut vk::PhysicalDeviceProperties2,
    ) {
        self.fp_v1_1()
            .get_physical_device_properties2(physical_device, prop);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFormatProperties2.html>"]
    unsafe fn get_physical_device_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
        out: &mut vk::FormatProperties2,
    ) {
        self.fp_v1_1()
            .get_physical_device_format_properties2(physical_device, format, out);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties2.html>"]
    unsafe fn get_physical_device_image_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceImageFormatInfo2,
        image_format_prop: &mut vk::ImageFormatProperties2,
    ) -> VkResult<()> {
        let err_code = self.fp_v1_1().get_physical_device_image_format_properties2(
            physical_device,
            format_info,
            image_format_prop,
        );
        if err_code == vk::Result::SUCCESS {
            Ok(())
        } else {
            Err(err_code)
        }
    }

    unsafe fn get_physical_device_queue_family_properties2_len(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> usize {
        let mut queue_count = 0;
        self.fp_v1_1().get_physical_device_queue_family_properties2(
            physical_device,
            &mut queue_count,
            ptr::null_mut(),
        );
        queue_count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties2.html>"]
    unsafe fn get_physical_device_queue_family_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_props: &mut [vk::QueueFamilyProperties2],
    ) {
        let mut queue_count = queue_family_props.len() as u32;
        self.fp_v1_1().get_physical_device_queue_family_properties2(
            physical_device,
            &mut queue_count,
            queue_family_props.as_mut_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceMemoryProperties2.html>"]
    unsafe fn get_physical_device_memory_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        out: &mut vk::PhysicalDeviceMemoryProperties2,
    ) {
        self.fp_v1_1()
            .get_physical_device_memory_properties2(physical_device, out);
    }

    unsafe fn get_physical_device_sparse_image_format_properties2_len(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceSparseImageFormatInfo2,
    ) -> usize {
        let mut format_count = 0;
        self.fp_v1_1()
            .get_physical_device_sparse_image_format_properties2(
                physical_device,
                format_info,
                &mut format_count,
                ptr::null_mut(),
            );
        format_count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSparseImageFormatProperties2.html>"]
    unsafe fn get_physical_device_sparse_image_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceSparseImageFormatInfo2,
        out: &mut [vk::SparseImageFormatProperties2],
    ) {
        let mut format_count = out.len() as u32;
        self.fp_v1_1()
            .get_physical_device_sparse_image_format_properties2(
                physical_device,
                format_info,
                &mut format_count,
                out.as_mut_ptr(),
            );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalBufferProperties.html>"]
    unsafe fn get_physical_device_external_buffer_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        external_buffer_info: &vk::PhysicalDeviceExternalBufferInfo,
        out: &mut vk::ExternalBufferProperties,
    ) {
        self.fp_v1_1()
            .get_physical_device_external_buffer_properties(
                physical_device,
                external_buffer_info,
                out,
            );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalFenceProperties.html>"]
    unsafe fn get_physical_device_external_fence_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        external_fence_info: &vk::PhysicalDeviceExternalFenceInfo,
        out: &mut vk::ExternalFenceProperties,
    ) {
        self.fp_v1_1()
            .get_physical_device_external_fence_properties(
                physical_device,
                external_fence_info,
                out,
            );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalSemaphoreProperties.html>"]
    unsafe fn get_physical_device_external_semaphore_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        external_semaphore_info: &vk::PhysicalDeviceExternalSemaphoreInfo,
        out: &mut vk::ExternalSemaphoreProperties,
    ) {
        self.fp_v1_1()
            .get_physical_device_external_semaphore_properties(
                physical_device,
                external_semaphore_info,
                out,
            );
    }
}

#[allow(non_camel_case_types)]
pub trait InstanceV1_0 {
    type Device;
    fn handle(&self) -> vk::Instance;
    fn fp_v1_0(&self) -> &vk::InstanceFnV1_0;
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDevice.html>"]
    ///
    /// # Safety
    /// In order for the created `Device` to be valid for the duration of its
    /// usage, the `Instance` this was called on must be dropped later than the
    /// resulting `Device`.
    unsafe fn create_device(
        &self,
        physical_device: vk::PhysicalDevice,
        create_info: &vk::DeviceCreateInfo,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> Result<Self::Device, vk::Result>;

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceProcAddr.html>"]
    unsafe fn get_device_proc_addr(
        &self,
        device: vk::Device,
        p_name: *const c_char,
    ) -> vk::PFN_vkVoidFunction {
        self.fp_v1_0().get_device_proc_addr(device, p_name)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyInstance.html>"]
    unsafe fn destroy_instance(&self, allocation_callbacks: Option<&vk::AllocationCallbacks>) {
        self.fp_v1_0()
            .destroy_instance(self.handle(), allocation_callbacks.as_raw_ptr());
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFormatProperties.html>"]
    unsafe fn get_physical_device_format_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
    ) -> vk::FormatProperties {
        let mut format_prop = mem::zeroed();
        self.fp_v1_0().get_physical_device_format_properties(
            physical_device,
            format,
            &mut format_prop,
        );
        format_prop
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties.html>"]
    unsafe fn get_physical_device_image_format_properties(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
        typ: vk::ImageType,
        tiling: vk::ImageTiling,
        usage: vk::ImageUsageFlags,
        flags: vk::ImageCreateFlags,
    ) -> VkResult<vk::ImageFormatProperties> {
        let mut image_format_prop = mem::zeroed();
        let err_code = self.fp_v1_0().get_physical_device_image_format_properties(
            physical_device,
            format,
            typ,
            tiling,
            usage,
            flags,
            &mut image_format_prop,
        );
        if err_code == vk::Result::SUCCESS {
            Ok(image_format_prop)
        } else {
            Err(err_code)
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceMemoryProperties.html>"]
    unsafe fn get_physical_device_memory_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceMemoryProperties {
        let mut memory_prop = mem::zeroed();
        self.fp_v1_0()
            .get_physical_device_memory_properties(physical_device, &mut memory_prop);
        memory_prop
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceProperties.html>"]
    unsafe fn get_physical_device_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceProperties {
        let mut prop = mem::zeroed();
        self.fp_v1_0()
            .get_physical_device_properties(physical_device, &mut prop);
        prop
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties.html>"]
    unsafe fn get_physical_device_queue_family_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> Vec<vk::QueueFamilyProperties> {
        let mut queue_count = 0;
        self.fp_v1_0().get_physical_device_queue_family_properties(
            physical_device,
            &mut queue_count,
            ptr::null_mut(),
        );
        let mut queue_families_vec = Vec::with_capacity(queue_count as usize);
        self.fp_v1_0().get_physical_device_queue_family_properties(
            physical_device,
            &mut queue_count,
            queue_families_vec.as_mut_ptr(),
        );
        queue_families_vec.set_len(queue_count as usize);
        queue_families_vec
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFeatures.html>"]
    unsafe fn get_physical_device_features(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceFeatures {
        let mut prop = mem::zeroed();
        self.fp_v1_0()
            .get_physical_device_features(physical_device, &mut prop);
        prop
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumeratePhysicalDevices.html>"]
    unsafe fn enumerate_physical_devices(&self) -> VkResult<Vec<vk::PhysicalDevice>> {
        let mut num = mem::zeroed();
        self.fp_v1_0()
            .enumerate_physical_devices(self.handle(), &mut num, ptr::null_mut());
        let mut physical_devices = Vec::<vk::PhysicalDevice>::with_capacity(num as usize);
        let err_code = self.fp_v1_0().enumerate_physical_devices(
            self.handle(),
            &mut num,
            physical_devices.as_mut_ptr(),
        );
        physical_devices.set_len(num as usize);
        match err_code {
            vk::Result::SUCCESS => Ok(physical_devices),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateDeviceExtensionProperties.html>"]
    unsafe fn enumerate_device_extension_properties(
        &self,
        device: vk::PhysicalDevice,
    ) -> Result<Vec<vk::ExtensionProperties>, vk::Result> {
        let mut num = 0;
        self.fp_v1_0().enumerate_device_extension_properties(
            device,
            ptr::null(),
            &mut num,
            ptr::null_mut(),
        );
        let mut data = Vec::with_capacity(num as usize);
        let err_code = self.fp_v1_0().enumerate_device_extension_properties(
            device,
            ptr::null(),
            &mut num,
            data.as_mut_ptr(),
        );
        data.set_len(num as usize);
        match err_code {
            vk::Result::SUCCESS => Ok(data),
            _ => Err(err_code),
        }
    }
}
