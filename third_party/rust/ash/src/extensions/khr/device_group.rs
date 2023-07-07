#[cfg(doc)]
use super::Swapchain;
use crate::prelude::*;
use crate::vk;
use crate::{Device, Entry, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_device_group.html>
#[derive(Clone)]
pub struct DeviceGroup {
    handle: vk::Device,
    fp: vk::KhrDeviceGroupFn,
}

impl DeviceGroup {
    /// # Warning
    /// [`Instance`] functions cannot be loaded from a [`Device`] and will always panic when called:
    /// - [`Self::get_physical_device_present_rectangles()`]
    ///
    /// Load this struct using an [`Instance`] instead via [`Self::new_from_instance()`] if the
    /// above [`Instance`] function is called. This will be solved in the next breaking `ash`
    /// release: <https://github.com/ash-rs/ash/issues/727>.
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrDeviceGroupFn::load(|name| unsafe {
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
        let fp = vk::KhrDeviceGroupFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self { handle: device, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceGroupPeerMemoryFeaturesKHR.html>
    #[inline]
    pub unsafe fn get_device_group_peer_memory_features(
        &self,
        heap_index: u32,
        local_device_index: u32,
        remote_device_index: u32,
    ) -> vk::PeerMemoryFeatureFlags {
        let mut peer_memory_features = mem::zeroed();
        (self.fp.get_device_group_peer_memory_features_khr)(
            self.handle,
            heap_index,
            local_device_index,
            remote_device_index,
            &mut peer_memory_features,
        );
        peer_memory_features
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdSetDeviceMaskKHR.html>
    #[inline]
    pub unsafe fn cmd_set_device_mask(&self, command_buffer: vk::CommandBuffer, device_mask: u32) {
        (self.fp.cmd_set_device_mask_khr)(command_buffer, device_mask)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdDispatchBaseKHR.html>
    #[inline]
    pub unsafe fn cmd_dispatch_base(
        &self,
        command_buffer: vk::CommandBuffer,
        base_group: (u32, u32, u32),
        group_count: (u32, u32, u32),
    ) {
        (self.fp.cmd_dispatch_base_khr)(
            command_buffer,
            base_group.0,
            base_group.1,
            base_group.2,
            group_count.0,
            group_count.1,
            group_count.2,
        )
    }

    /// Requires [`VK_KHR_surface`] to be enabled.
    ///
    /// Also available as [`Swapchain::get_device_group_present_capabilities()`] since [Vulkan 1.1].
    ///
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceGroupPresentCapabilitiesKHR.html>
    ///
    /// [Vulkan 1.1]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_VERSION_1_1.html
    /// [`VK_KHR_surface`]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_surface.html
    #[inline]
    pub unsafe fn get_device_group_present_capabilities(
        &self,
        device_group_present_capabilities: &mut vk::DeviceGroupPresentCapabilitiesKHR,
    ) -> VkResult<()> {
        (self.fp.get_device_group_present_capabilities_khr)(
            self.handle,
            device_group_present_capabilities,
        )
        .result()
    }

    /// Requires [`VK_KHR_surface`] to be enabled.
    ///
    /// Also available as [`Swapchain::get_device_group_surface_present_modes()`] since [Vulkan 1.1].
    ///
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceGroupSurfacePresentModesKHR.html>
    ///
    /// [Vulkan 1.1]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_VERSION_1_1.html
    /// [`VK_KHR_surface`]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_surface.html
    #[inline]
    pub unsafe fn get_device_group_surface_present_modes(
        &self,
        surface: vk::SurfaceKHR,
    ) -> VkResult<vk::DeviceGroupPresentModeFlagsKHR> {
        let mut modes = mem::zeroed();
        (self.fp.get_device_group_surface_present_modes_khr)(self.handle, surface, &mut modes)
            .result_with_success(modes)
    }

    /// Requires [`VK_KHR_surface`] to be enabled.
    ///
    /// Also available as [`Swapchain::get_physical_device_present_rectangles()`] since [Vulkan 1.1].
    ///
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDevicePresentRectanglesKHR.html>
    ///
    /// [Vulkan 1.1]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_VERSION_1_1.html
    /// [`VK_KHR_surface`]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_surface.html
    ///
    /// # Warning
    ///
    /// Function will always panic unless this struct is loaded via [`Self::new_from_instance()`].
    #[inline]
    pub unsafe fn get_physical_device_present_rectangles(
        &self,
        physical_device: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
    ) -> VkResult<Vec<vk::Rect2D>> {
        read_into_uninitialized_vector(|count, data| {
            (self.fp.get_physical_device_present_rectangles_khr)(
                physical_device,
                surface,
                count,
                data,
            )
        })
    }

    /// On success, returns the next image's index and whether the swapchain is suboptimal for the surface.
    ///
    /// Requires [`VK_KHR_swapchain`] to be enabled.
    ///
    /// Also available as [`Swapchain::acquire_next_image2()`] since [Vulkan 1.1].
    ///
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkAcquireNextImage2KHR.html>
    ///
    /// [Vulkan 1.1]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_VERSION_1_1.html
    /// [`VK_KHR_swapchain`]: https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_swapchain.html
    #[inline]
    pub unsafe fn acquire_next_image2(
        &self,
        acquire_info: &vk::AcquireNextImageInfoKHR,
    ) -> VkResult<(u32, bool)> {
        let mut index = 0;
        let err_code = (self.fp.acquire_next_image2_khr)(self.handle, acquire_info, &mut index);
        match err_code {
            vk::Result::SUCCESS => Ok((index, false)),
            vk::Result::SUBOPTIMAL_KHR => Ok((index, true)),
            _ => Err(err_code),
        }
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrDeviceGroupFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrDeviceGroupFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
