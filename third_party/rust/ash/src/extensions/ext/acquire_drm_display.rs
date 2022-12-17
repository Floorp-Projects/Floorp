use crate::prelude::*;
use crate::vk;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_acquire_drm_display.html>
#[derive(Clone)]
pub struct AcquireDrmDisplay {
    fp: vk::ExtAcquireDrmDisplayFn,
}

impl AcquireDrmDisplay {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::ExtAcquireDrmDisplayFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkAcquireDrmDisplayEXT.html>
    #[inline]
    pub unsafe fn acquire_drm_display(
        &self,
        physical_device: vk::PhysicalDevice,
        drm_fd: i32,
        display: vk::DisplayKHR,
    ) -> VkResult<()> {
        (self.fp.acquire_drm_display_ext)(physical_device, drm_fd, display).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDrmDisplayEXT.html>
    #[inline]
    pub unsafe fn get_drm_display(
        &self,
        physical_device: vk::PhysicalDevice,
        drm_fd: i32,
        connector_id: u32,
    ) -> VkResult<vk::DisplayKHR> {
        let mut display = mem::MaybeUninit::uninit();
        (self.fp.get_drm_display_ext)(physical_device, drm_fd, connector_id, display.as_mut_ptr())
            .assume_init_on_success(display)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtAcquireDrmDisplayFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtAcquireDrmDisplayFn {
        &self.fp
    }
}
