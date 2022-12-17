use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct PresentWait {
    handle: vk::Device,
    fp: vk::KhrPresentWaitFn,
}

impl PresentWait {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrPresentWaitFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkWaitForPresentKHR.html>
    #[inline]
    pub unsafe fn wait_for_present(
        &self,
        swapchain: vk::SwapchainKHR,
        present_id: u64,
        timeout: u64,
    ) -> VkResult<()> {
        (self.fp.wait_for_present_khr)(self.handle, swapchain, present_id, timeout).result()
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrPresentWaitFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrPresentWaitFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
