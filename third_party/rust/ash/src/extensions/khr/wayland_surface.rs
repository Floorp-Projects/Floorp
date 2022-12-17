use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct WaylandSurface {
    handle: vk::Instance,
    fp: vk::KhrWaylandSurfaceFn,
}

impl WaylandSurface {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::KhrWaylandSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateWaylandSurfaceKHR.html>
    #[inline]
    pub unsafe fn create_wayland_surface(
        &self,
        create_info: &vk::WaylandSurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        (self.fp.create_wayland_surface_khr)(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut surface,
        )
        .result_with_success(surface)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceWaylandPresentationSupportKHR.html>
    #[inline]
    pub unsafe fn get_physical_device_wayland_presentation_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_index: u32,
        wl_display: &mut vk::wl_display,
    ) -> bool {
        let b = (self.fp.get_physical_device_wayland_presentation_support_khr)(
            physical_device,
            queue_family_index,
            wl_display,
        );

        b > 0
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrWaylandSurfaceFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrWaylandSurfaceFn {
        &self.fp
    }

    #[inline]
    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
