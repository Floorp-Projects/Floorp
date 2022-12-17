use crate::prelude::*;
use crate::vk;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct GetSurfaceCapabilities2 {
    fp: vk::KhrGetSurfaceCapabilities2Fn,
}

impl GetSurfaceCapabilities2 {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let fp = vk::KhrGetSurfaceCapabilities2Fn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceSurfaceCapabilities2KHR.html>
    #[inline]
    pub unsafe fn get_physical_device_surface_capabilities2(
        &self,
        physical_device: vk::PhysicalDevice,
        surface_info: &vk::PhysicalDeviceSurfaceInfo2KHR,
    ) -> VkResult<vk::SurfaceCapabilities2KHR> {
        let mut surface_capabilities = Default::default();
        (self.fp.get_physical_device_surface_capabilities2_khr)(
            physical_device,
            surface_info,
            &mut surface_capabilities,
        )
        .result_with_success(surface_capabilities)
    }

    /// Retrieve the number of elements to pass to [`get_physical_device_surface_formats2()`][Self::get_physical_device_surface_formats2()]
    #[inline]
    pub unsafe fn get_physical_device_surface_formats2_len(
        &self,
        physical_device: vk::PhysicalDevice,
        surface_info: &vk::PhysicalDeviceSurfaceInfo2KHR,
    ) -> VkResult<usize> {
        let mut count = 0;
        let err_code = (self.fp.get_physical_device_surface_formats2_khr)(
            physical_device,
            surface_info,
            &mut count,
            std::ptr::null_mut(),
        );
        err_code.result_with_success(count as usize)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceSurfaceFormats2KHR.html>
    ///
    /// Call [`get_physical_device_surface_formats2_len()`][Self::get_physical_device_surface_formats2_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_physical_device_surface_formats2(
        &self,
        physical_device: vk::PhysicalDevice,
        surface_info: &vk::PhysicalDeviceSurfaceInfo2KHR,
        out: &mut [vk::SurfaceFormat2KHR],
    ) -> VkResult<()> {
        let mut count = out.len() as u32;
        let err_code = (self.fp.get_physical_device_surface_formats2_khr)(
            physical_device,
            surface_info,
            &mut count,
            out.as_mut_ptr(),
        );
        assert_eq!(count as usize, out.len());
        err_code.result()
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrGetSurfaceCapabilities2Fn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrGetSurfaceCapabilities2Fn {
        &self.fp
    }
}
