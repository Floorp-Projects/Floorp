use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct MetalSurface {
    handle: vk::Instance,
    fp: vk::ExtMetalSurfaceFn,
}

impl MetalSurface {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::ExtMetalSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateMetalSurfaceEXT.html>"]
    pub unsafe fn create_metal_surface(
        &self,
        create_info: &vk::MetalSurfaceCreateInfoEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        self.fp
            .create_metal_surface_ext(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut surface,
            )
            .result_with_success(surface)
    }

    pub fn name() -> &'static CStr {
        vk::ExtMetalSurfaceFn::name()
    }

    pub fn fp(&self) -> &vk::ExtMetalSurfaceFn {
        &self.fp
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
