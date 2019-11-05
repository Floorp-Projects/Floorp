#![allow(dead_code)]
use prelude::*;
use std::ffi::CStr;
use std::mem;
use version::{EntryV1_0, InstanceV1_0};
use vk;
use RawPtr;

#[derive(Clone)]
pub struct AndroidSurface {
    handle: vk::Instance,
    android_surface_fn: vk::KhrAndroidSurfaceFn,
}

impl AndroidSurface {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> AndroidSurface {
        let surface_fn = vk::KhrAndroidSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        AndroidSurface {
            handle: instance.handle(),
            android_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrAndroidSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkCreateAndroidSurfaceKHR.html>"]
    pub unsafe fn create_android_surface(
        &self,
        create_info: &vk::AndroidSurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::uninitialized();
        let err_code = self.android_surface_fn.create_android_surface_khr(
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
