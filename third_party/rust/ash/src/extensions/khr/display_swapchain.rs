use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DisplaySwapchain {
    handle: vk::Device,
    fp: vk::KhrDisplaySwapchainFn,
}

impl DisplaySwapchain {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrDisplaySwapchainFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSharedSwapchainsKHR.html>"]
    pub unsafe fn create_shared_swapchains(
        &self,
        create_infos: &[vk::SwapchainCreateInfoKHR],
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<Vec<vk::SwapchainKHR>> {
        let mut swapchains = Vec::with_capacity(create_infos.len());
        let err_code = self.fp.create_shared_swapchains_khr(
            self.handle,
            create_infos.len() as u32,
            create_infos.as_ptr(),
            allocation_callbacks.as_raw_ptr(),
            swapchains.as_mut_ptr(),
        );
        swapchains.set_len(create_infos.len());
        err_code.result_with_success(swapchains)
    }

    pub fn name() -> &'static CStr {
        vk::KhrDisplaySwapchainFn::name()
    }

    pub fn fp(&self) -> &vk::KhrDisplaySwapchainFn {
        &self.fp
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
