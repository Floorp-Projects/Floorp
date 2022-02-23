use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{EntryCustom, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct XcbSurface {
    handle: vk::Instance,
    xcb_surface_fn: vk::KhrXcbSurfaceFn,
}

impl XcbSurface {
    pub fn new<L>(entry: &EntryCustom<L>, instance: &Instance) -> Self {
        let surface_fn = vk::KhrXcbSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self {
            handle: instance.handle(),
            xcb_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrXcbSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateXcbSurfaceKHR.html>"]
    pub unsafe fn create_xcb_surface(
        &self,
        create_info: &vk::XcbSurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        self.xcb_surface_fn
            .create_xcb_surface_khr(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut surface,
            )
            .result_with_success(surface)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceXcbPresentationSupportKHR.html>"]
    pub unsafe fn get_physical_device_xcb_presentation_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_index: u32,
        connection: &mut vk::xcb_connection_t,
        visual_id: vk::xcb_visualid_t,
    ) -> bool {
        let b = self
            .xcb_surface_fn
            .get_physical_device_xcb_presentation_support_khr(
                physical_device,
                queue_family_index,
                connection,
                visual_id,
            );

        b > 0
    }

    pub fn fp(&self) -> &vk::KhrXcbSurfaceFn {
        &self.xcb_surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
