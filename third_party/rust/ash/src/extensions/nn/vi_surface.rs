use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{EntryCustom, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct ViSurface {
    handle: vk::Instance,
    vi_surface_fn: vk::NnViSurfaceFn,
}

impl ViSurface {
    pub fn new<L>(entry: &EntryCustom<L>, instance: &Instance) -> Self {
        let surface_fn = vk::NnViSurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self {
            handle: instance.handle(),
            vi_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::NnViSurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateViSurfaceNN.html>"]
    pub unsafe fn create_vi_surface(
        &self,
        create_info: &vk::ViSurfaceCreateInfoNN,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        self.vi_surface_fn
            .create_vi_surface_nn(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut surface,
            )
            .result_with_success(surface)
    }

    pub fn fp(&self) -> &vk::NnViSurfaceFn {
        &self.vi_surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
