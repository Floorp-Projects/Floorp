use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct MacOSSurface {
    handle: vk::Instance,
    fp: vk::MvkMacosSurfaceFn,
}

impl MacOSSurface {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::MvkMacosSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateMacOSSurfaceMVK.html>
    #[inline]
    pub unsafe fn create_mac_os_surface(
        &self,
        create_info: &vk::MacOSSurfaceCreateInfoMVK,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        (self.fp.create_mac_os_surface_mvk)(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut surface,
        )
        .result_with_success(surface)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::MvkMacosSurfaceFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::MvkMacosSurfaceFn {
        &self.fp
    }

    #[inline]
    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
