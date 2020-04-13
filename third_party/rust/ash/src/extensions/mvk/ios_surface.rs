#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct IOSSurface {
    handle: vk::Instance,
    ios_surface_fn: vk::MvkIosSurfaceFn,
}

impl IOSSurface {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> IOSSurface {
        let surface_fn = vk::MvkIosSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        IOSSurface {
            handle: instance.handle(),
            ios_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::MvkIosSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateIOSSurfaceMVK.html>"]
    pub unsafe fn create_ios_surface_mvk(
        &self,
        create_info: &vk::IOSSurfaceCreateInfoMVK,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        let err_code = self.ios_surface_fn.create_ios_surface_mvk(
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

    pub fn fp(&self) -> &vk::MvkIosSurfaceFn {
        &self.ios_surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
