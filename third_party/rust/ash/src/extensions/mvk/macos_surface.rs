#![allow(dead_code)]
use prelude::*;
use std::ffi::CStr;
use std::mem;
use version::{EntryV1_0, InstanceV1_0};
use vk;
use RawPtr;

#[derive(Clone)]
pub struct MacOSSurface {
    handle: vk::Instance,
    macos_surface_fn: vk::MvkMacosSurfaceFn,
}

impl MacOSSurface {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> MacOSSurface {
        let surface_fn = vk::MvkMacosSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        MacOSSurface {
            handle: instance.handle(),
            macos_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::MvkMacosSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/vkCreateMacOSSurfaceMVK.html>"]
    pub unsafe fn create_mac_os_surface_mvk(
        &self,
        create_info: &vk::MacOSSurfaceCreateInfoMVK,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::uninitialized();
        let err_code = self.macos_surface_fn.create_mac_os_surface_mvk(
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
