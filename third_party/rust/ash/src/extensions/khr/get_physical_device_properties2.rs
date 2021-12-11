use crate::prelude::*;
use crate::vk;
use crate::{EntryCustom, Instance};
use std::ffi::CStr;
use std::mem;
use std::ptr;

#[derive(Clone)]
pub struct GetPhysicalDeviceProperties2 {
    handle: vk::Instance,
    get_physical_device_properties2_fn: vk::KhrGetPhysicalDeviceProperties2Fn,
}

impl GetPhysicalDeviceProperties2 {
    pub fn new<L>(entry: &EntryCustom<L>, instance: &Instance) -> Self {
        let get_physical_device_properties2_fn =
            vk::KhrGetPhysicalDeviceProperties2Fn::load(|name| unsafe {
                mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
            });
        Self {
            handle: instance.handle(),
            get_physical_device_properties2_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrGetPhysicalDeviceProperties2Fn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFeatures2KHR.html>"]
    pub unsafe fn get_physical_device_features2(
        &self,
        physical_device: vk::PhysicalDevice,
        features: &mut vk::PhysicalDeviceFeatures2KHR,
    ) {
        self.get_physical_device_properties2_fn
            .get_physical_device_features2_khr(physical_device, features);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFormatProperties2KHR.html>"]
    pub unsafe fn get_physical_device_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format: vk::Format,
        format_properties: &mut vk::FormatProperties2KHR,
    ) {
        self.get_physical_device_properties2_fn
            .get_physical_device_format_properties2_khr(physical_device, format, format_properties);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties2KHR.html>"]
    pub unsafe fn get_physical_device_image_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        image_format_info: &vk::PhysicalDeviceImageFormatInfo2KHR,
        image_format_properties: &mut vk::ImageFormatProperties2KHR,
    ) -> VkResult<()> {
        self.get_physical_device_properties2_fn
            .get_physical_device_image_format_properties2_khr(
                physical_device,
                image_format_info,
                image_format_properties,
            )
            .result()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceMemoryProperties2KHR.html>"]
    pub unsafe fn get_physical_device_memory_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        memory_properties: &mut vk::PhysicalDeviceMemoryProperties2KHR,
    ) {
        self.get_physical_device_properties2_fn
            .get_physical_device_memory_properties2_khr(physical_device, memory_properties);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceProperties2KHR.html>"]
    pub unsafe fn get_physical_device_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        properties: &mut vk::PhysicalDeviceProperties2KHR,
    ) {
        self.get_physical_device_properties2_fn
            .get_physical_device_properties2_khr(physical_device, properties);
    }

    pub unsafe fn get_physical_device_queue_family_properties2_len(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> usize {
        let mut count = 0;
        self.get_physical_device_properties2_fn
            .get_physical_device_queue_family_properties2_khr(
                physical_device,
                &mut count,
                ptr::null_mut(),
            );
        count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties2KHR.html>"]
    pub unsafe fn get_physical_device_queue_family_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_properties: &mut [vk::QueueFamilyProperties2KHR],
    ) {
        let mut count = queue_family_properties.len() as u32;
        self.get_physical_device_properties2_fn
            .get_physical_device_queue_family_properties2_khr(
                physical_device,
                &mut count,
                queue_family_properties.as_mut_ptr(),
            );
    }

    pub unsafe fn get_physical_device_sparse_image_format_properties2_len(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceSparseImageFormatInfo2KHR,
    ) -> usize {
        let mut count = 0;
        self.get_physical_device_properties2_fn
            .get_physical_device_sparse_image_format_properties2_khr(
                physical_device,
                format_info,
                &mut count,
                ptr::null_mut(),
            );
        count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSparseImageFormatProperties2KHR.html>"]
    pub unsafe fn get_physical_device_sparse_image_format_properties2(
        &self,
        physical_device: vk::PhysicalDevice,
        format_info: &vk::PhysicalDeviceSparseImageFormatInfo2KHR,
        properties: &mut [vk::SparseImageFormatProperties2KHR],
    ) {
        let mut count = properties.len() as u32;
        self.get_physical_device_properties2_fn
            .get_physical_device_sparse_image_format_properties2_khr(
                physical_device,
                format_info,
                &mut count,
                properties.as_mut_ptr(),
            );
    }

    pub fn fp(&self) -> &vk::KhrGetPhysicalDeviceProperties2Fn {
        &self.get_physical_device_properties2_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
