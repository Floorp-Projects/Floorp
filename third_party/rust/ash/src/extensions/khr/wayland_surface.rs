use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{EntryCustom, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct WaylandSurface {
    handle: vk::Instance,
    wayland_surface_fn: vk::KhrWaylandSurfaceFn,
}

impl WaylandSurface {
    pub fn new<L>(entry: &EntryCustom<L>, instance: &Instance) -> Self {
        let surface_fn = vk::KhrWaylandSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self {
            handle: instance.handle(),
            wayland_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrWaylandSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateWaylandSurfaceKHR.html>"]
    pub unsafe fn create_wayland_surface(
        &self,
        create_info: &vk::WaylandSurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        self.wayland_surface_fn
            .create_wayland_surface_khr(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut surface,
            )
            .result_with_success(surface)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceWaylandPresentationSupportKHR.html>"]
    pub unsafe fn get_physical_device_wayland_presentation_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_index: u32,
        wl_display: &mut vk::wl_display,
    ) -> bool {
        let b = self
            .wayland_surface_fn
            .get_physical_device_wayland_presentation_support_khr(
                physical_device,
                queue_family_index,
                wl_display,
            );

        b > 0
    }

    pub fn fp(&self) -> &vk::KhrWaylandSurfaceFn {
        &self.wayland_surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
