use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct FullScreenExclusive {
    handle: vk::Device,
    fp: vk::ExtFullScreenExclusiveFn,
}

impl FullScreenExclusive {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::ExtFullScreenExclusiveFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkAcquireFullScreenExclusiveModeEXT.html>
    #[inline]
    pub unsafe fn acquire_full_screen_exclusive_mode(
        &self,
        swapchain: vk::SwapchainKHR,
    ) -> VkResult<()> {
        (self.fp.acquire_full_screen_exclusive_mode_ext)(self.handle, swapchain).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceSurfacePresentModes2EXT.html>
    #[inline]
    pub unsafe fn get_physical_device_surface_present_modes2(
        &self,
        physical_device: vk::PhysicalDevice,
        surface_info: &vk::PhysicalDeviceSurfaceInfo2KHR,
    ) -> VkResult<Vec<vk::PresentModeKHR>> {
        read_into_uninitialized_vector(|count, data| {
            (self.fp.get_physical_device_surface_present_modes2_ext)(
                physical_device,
                surface_info,
                count,
                data,
            )
        })
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkReleaseFullScreenExclusiveModeEXT.html>
    #[inline]
    pub unsafe fn release_full_screen_exclusive_mode(
        &self,
        swapchain: vk::SwapchainKHR,
    ) -> VkResult<()> {
        (self.fp.release_full_screen_exclusive_mode_ext)(self.handle, swapchain).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceGroupSurfacePresentModes2EXT.html>
    #[inline]
    pub unsafe fn get_device_group_surface_present_modes2(
        &self,
        surface_info: &vk::PhysicalDeviceSurfaceInfo2KHR,
    ) -> VkResult<vk::DeviceGroupPresentModeFlagsKHR> {
        let mut present_modes = mem::zeroed();
        (self.fp.get_device_group_surface_present_modes2_ext)(
            self.handle,
            surface_info,
            &mut present_modes,
        )
        .result_with_success(present_modes)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtFullScreenExclusiveFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtFullScreenExclusiveFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
