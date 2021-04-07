#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct MetalSurface {
    handle: vk::Instance,
    metal_surface_fn: vk::ExtMetalSurfaceFn,
}

impl MetalSurface {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> MetalSurface {
        let surface_fn = vk::ExtMetalSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        MetalSurface {
            handle: instance.handle(),
            metal_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::ExtMetalSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateMetalSurfaceEXT.html>"]
    pub unsafe fn create_metal_surface(
        &self,
        create_info: &vk::MetalSurfaceCreateInfoEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        self.metal_surface_fn
            .create_metal_surface_ext(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut surface,
            )
            .result_with_success(surface)
    }

    pub fn fp(&self) -> &vk::ExtMetalSurfaceFn {
        &self.metal_surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
