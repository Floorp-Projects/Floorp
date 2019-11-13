#![allow(dead_code)]
use prelude::*;
use std::ffi::CStr;
use std::mem;
use version::{EntryV1_0, InstanceV1_0};
use vk;
use RawPtr;

#[derive(Clone)]
pub struct XlibSurface {
    handle: vk::Instance,
    xlib_surface_fn: vk::KhrXlibSurfaceFn,
}

impl XlibSurface {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> XlibSurface {
        let surface_fn = vk::KhrXlibSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        XlibSurface {
            handle: instance.handle(),
            xlib_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrXlibSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkCreateXlibSurfaceKHR.html>"]
    pub unsafe fn create_xlib_surface(
        &self,
        create_info: &vk::XlibSurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::uninitialized();
        let err_code = self.xlib_surface_fn.create_xlib_surface_khr(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut surface,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(surface),
            _ => Err(err_code),
        }
    }
}
