use crate::prelude::*;
use crate::vk;
use crate::{Device, Entry, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_full_screen_exclusive.html>
#[derive(Clone)]
pub struct FullScreenExclusive {
    handle: vk::Device,
    fp: vk::ExtFullScreenExclusiveFn,
}

impl FullScreenExclusive {
    /// # Warning
    /// [`Instance`] functions cannot be loaded from a [`Device`] and will always panic when called:
    /// - [`Self::get_physical_device_surface_present_modes2()`]
    ///
    /// Load this struct using an [`Instance`] instead via [`Self::new_from_instance()`] if the
    /// above [`Instance`] function is called. This will be solved in the next breaking `ash`
    /// release: <https://github.com/ash-rs/ash/issues/727>.
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::ExtFullScreenExclusiveFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// Loads all functions on the [`Instance`] instead of [`Device`]. This incurs an extra
    /// dispatch table for [`Device`] functions but also allows the [`Instance`] function to be
    /// loaded instead of always panicking. See also [`Self::new()`] for more details.
    ///
    /// It is okay to pass [`vk::Device::null()`] when this struct is only used to call the
    /// [`Instance`] function.
    pub fn new_from_instance(entry: &Entry, instance: &Instance, device: vk::Device) -> Self {
        let fp = vk::ExtFullScreenExclusiveFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self { handle: device, fp }
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
    ///
    /// # Warning
    ///
    /// Function will always panic unless this struct is loaded via [`Self::new_from_instance()`].
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
