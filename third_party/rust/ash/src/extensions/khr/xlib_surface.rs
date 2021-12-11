use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{EntryCustom, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct XlibSurface {
    handle: vk::Instance,
    xlib_surface_fn: vk::KhrXlibSurfaceFn,
}

impl XlibSurface {
    pub fn new<L>(entry: &EntryCustom<L>, instance: &Instance) -> Self {
        let surface_fn = vk::KhrXlibSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self {
            handle: instance.handle(),
            xlib_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrXlibSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateXlibSurfaceKHR.html>"]
    pub unsafe fn create_xlib_surface(
        &self,
        create_info: &vk::XlibSurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        self.xlib_surface_fn
            .create_xlib_surface_khr(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut surface,
            )
            .result_with_success(surface)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceXlibPresentationSupportKHR.html>"]
    pub unsafe fn get_physical_device_xlib_presentation_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_index: u32,
        display: &mut vk::Display,
        visual_id: vk::VisualID,
    ) -> bool {
        let b = self
            .xlib_surface_fn
            .get_physical_device_xlib_presentation_support_khr(
                physical_device,
                queue_family_index,
                display,
                visual_id,
            );

        b > 0
    }

    pub fn fp(&self) -> &vk::KhrXlibSurfaceFn {
        &self.xlib_surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
