#![allow(dead_code)]
use prelude::*;
use std::ffi::CStr;
use std::mem;
use std::ptr;
use version::{EntryV1_0, InstanceV1_0};
use vk;
use RawPtr;

#[derive(Clone)]
pub struct Surface {
    handle: vk::Instance,
    surface_fn: vk::KhrSurfaceFn,
}

impl Surface {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> Surface {
        let surface_fn = vk::KhrSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Surface {
            handle: instance.handle(),
            surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkGetPhysicalDeviceSurfaceSupportKHR.html>"]
    pub unsafe fn get_physical_device_surface_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_index: u32,
        surface: vk::SurfaceKHR,
    ) -> bool {
        let mut b = mem::uninitialized();
        self.surface_fn.get_physical_device_surface_support_khr(
            physical_device,
            queue_index,
            surface,
            &mut b,
        );
        b > 0
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkGetPhysicalDeviceSurfacePresentModesKHR.html>"]
    pub unsafe fn get_physical_device_surface_present_modes(
        &self,
        physical_device: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> VkResult<Vec<vk::PresentModeKHR>> {
        let mut count = 0;
        self.surface_fn
            .get_physical_device_surface_present_modes_khr(
                physical_device,
                surface,
                &mut count,
                ptr::null_mut(),
            );
        let mut v = Vec::with_capacity(count as usize);
        let err_code = self
            .surface_fn
            .get_physical_device_surface_present_modes_khr(
                physical_device,
                surface,
                &mut count,
                v.as_mut_ptr(),
            );
        v.set_len(count as usize);
        match err_code {
            vk::Result::SUCCESS => Ok(v),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkGetPhysicalDeviceSurfaceCapabilitiesKHR.html>"]
    pub unsafe fn get_physical_device_surface_capabilities(
        &self,
        physical_device: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> VkResult<vk::SurfaceCapabilitiesKHR> {
        let mut surface_capabilities = mem::uninitialized();
        let err_code = self
            .surface_fn
            .get_physical_device_surface_capabilities_khr(
                physical_device,
                surface,
                &mut surface_capabilities,
            );
        match err_code {
            vk::Result::SUCCESS => Ok(surface_capabilities),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkGetPhysicalDeviceSurfaceFormatsKHR.html>"]
    pub unsafe fn get_physical_device_surface_formats(
        &self,
        physical_device: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> VkResult<Vec<vk::SurfaceFormatKHR>> {
        let mut count = 0;
        self.surface_fn.get_physical_device_surface_formats_khr(
            physical_device,
            surface,
            &mut count,
            ptr::null_mut(),
        );
        let mut v = Vec::with_capacity(count as usize);
        let err_code = self.surface_fn.get_physical_device_surface_formats_khr(
            physical_device,
            surface,
            &mut count,
            v.as_mut_ptr(),
        );
        v.set_len(count as usize);
        match err_code {
            vk::Result::SUCCESS => Ok(v),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkDestroySurfaceKHR.html>"]
    pub unsafe fn destroy_surface(
        &self,
        surface: vk::SurfaceKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.surface_fn.destroy_surface_khr(
            self.handle,
            surface,
            allocation_callbacks.as_raw_ptr(),
        );
    }
}
