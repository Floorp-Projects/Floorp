use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{EntryCustom, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Surface {
    handle: vk::Instance,
    surface_fn: vk::KhrSurfaceFn,
}

impl Surface {
    pub fn new<L>(entry: &EntryCustom<L>, instance: &Instance) -> Self {
        let surface_fn = vk::KhrSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self {
            handle: instance.handle(),
            surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceSupportKHR.html>"]
    pub unsafe fn get_physical_device_surface_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_index: u32,
        surface: vk::SurfaceKHR,
    ) -> VkResult<bool> {
        let mut b = 0;
        self.surface_fn
            .get_physical_device_surface_support_khr(
                physical_device,
                queue_family_index,
                surface,
                &mut b,
            )
            .result_with_success(b > 0)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfacePresentModesKHR.html>"]
    pub unsafe fn get_physical_device_surface_present_modes(
        &self,
        physical_device: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> VkResult<Vec<vk::PresentModeKHR>> {
        read_into_uninitialized_vector(|count, data| {
            self.surface_fn
                .get_physical_device_surface_present_modes_khr(
                    physical_device,
                    surface,
                    count,
                    data,
                )
        })
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceCapabilitiesKHR.html>"]
    pub unsafe fn get_physical_device_surface_capabilities(
        &self,
        physical_device: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> VkResult<vk::SurfaceCapabilitiesKHR> {
        let mut surface_capabilities = mem::zeroed();
        self.surface_fn
            .get_physical_device_surface_capabilities_khr(
                physical_device,
                surface,
                &mut surface_capabilities,
            )
            .result_with_success(surface_capabilities)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceFormatsKHR.html>"]
    pub unsafe fn get_physical_device_surface_formats(
        &self,
        physical_device: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> VkResult<Vec<vk::SurfaceFormatKHR>> {
        read_into_uninitialized_vector(|count, data| {
            self.surface_fn.get_physical_device_surface_formats_khr(
                physical_device,
                surface,
                count,
                data,
            )
        })
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySurfaceKHR.html>"]
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

    pub fn fp(&self) -> &vk::KhrSurfaceFn {
        &self.surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
