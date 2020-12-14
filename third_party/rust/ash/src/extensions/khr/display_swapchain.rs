#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{DeviceV1_0, InstanceV1_0};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DisplaySwapchain {
    handle: vk::Device,
    swapchain_fn: vk::KhrDisplaySwapchainFn,
}

impl DisplaySwapchain {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(instance: &I, device: &D) -> DisplaySwapchain {
        let swapchain_fn = vk::KhrDisplaySwapchainFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        DisplaySwapchain {
            handle: device.handle(),
            swapchain_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrDisplaySwapchainFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSharedSwapchainsKHR.html>"]
    pub unsafe fn create_shared_swapchains(
        &self,
        create_infos: &[vk::SwapchainCreateInfoKHR],
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<Vec<vk::SwapchainKHR>> {
        let mut swapchains = Vec::with_capacity(create_infos.len());
        let err_code = self.swapchain_fn.create_shared_swapchains_khr(
            self.handle,
            create_infos.len() as u32,
            create_infos.as_ptr(),
            allocation_callbacks.as_raw_ptr(),
            swapchains.as_mut_ptr(),
        );
        swapchains.set_len(create_infos.len());
        match err_code {
            vk::Result::SUCCESS => Ok(swapchains),
            _ => Err(err_code),
        }
    }

    pub fn fp(&self) -> &vk::KhrDisplaySwapchainFn {
        &self.swapchain_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
