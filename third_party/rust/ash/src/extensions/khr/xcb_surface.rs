use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct XcbSurface {
    handle: vk::Instance,
    fp: vk::KhrXcbSurfaceFn,
}

impl XcbSurface {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::KhrXcbSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateXcbSurfaceKHR.html>
    #[inline]
    pub unsafe fn create_xcb_surface(
        &self,
        create_info: &vk::XcbSurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        (self.fp.create_xcb_surface_khr)(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut surface,
        )
        .result_with_success(surface)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceXcbPresentationSupportKHR.html>
    #[inline]
    pub unsafe fn get_physical_device_xcb_presentation_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_index: u32,
        connection: &mut vk::xcb_connection_t,
        visual_id: vk::xcb_visualid_t,
    ) -> bool {
        let b = (self.fp.get_physical_device_xcb_presentation_support_khr)(
            physical_device,
            queue_family_index,
            connection,
            visual_id,
        );

        b > 0
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrXcbSurfaceFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrXcbSurfaceFn {
        &self.fp
    }

    #[inline]
    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
