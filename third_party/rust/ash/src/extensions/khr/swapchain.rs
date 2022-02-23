use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Swapchain {
    handle: vk::Device,
    fp: vk::KhrSwapchainFn,
}

impl Swapchain {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrSwapchainFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySwapchainKHR.html>"]
    pub unsafe fn destroy_swapchain(
        &self,
        swapchain: vk::SwapchainKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.fp
            .destroy_swapchain_khr(self.handle, swapchain, allocation_callbacks.as_raw_ptr());
    }

    /// On success, returns the next image's index and whether the swapchain is suboptimal for the surface.
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireNextImageKHR.html>"]
    pub unsafe fn acquire_next_image(
        &self,
        swapchain: vk::SwapchainKHR,
        timeout: u64,
        semaphore: vk::Semaphore,
        fence: vk::Fence,
    ) -> VkResult<(u32, bool)> {
        let mut index = 0;
        let err_code = self.fp.acquire_next_image_khr(
            self.handle,
            swapchain,
            timeout,
            semaphore,
            fence,
            &mut index,
        );
        match err_code {
            vk::Result::SUCCESS => Ok((index, false)),
            vk::Result::SUBOPTIMAL_KHR => Ok((index, true)),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSwapchainKHR.html>"]
    pub unsafe fn create_swapchain(
        &self,
        create_info: &vk::SwapchainCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SwapchainKHR> {
        let mut swapchain = mem::zeroed();
        self.fp
            .create_swapchain_khr(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut swapchain,
            )
            .result_with_success(swapchain)
    }

    /// On success, returns whether the swapchain is suboptimal for the surface.
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueuePresentKHR.html>"]
    pub unsafe fn queue_present(
        &self,
        queue: vk::Queue,
        present_info: &vk::PresentInfoKHR,
    ) -> VkResult<bool> {
        let err_code = self.fp.queue_present_khr(queue, present_info);
        match err_code {
            vk::Result::SUCCESS => Ok(false),
            vk::Result::SUBOPTIMAL_KHR => Ok(true),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSwapchainImagesKHR.html>"]
    pub unsafe fn get_swapchain_images(
        &self,
        swapchain: vk::SwapchainKHR,
    ) -> VkResult<Vec<vk::Image>> {
        read_into_uninitialized_vector(|count, data| {
            self.fp
                .get_swapchain_images_khr(self.handle, swapchain, count, data)
        })
    }

    pub fn name() -> &'static CStr {
        vk::KhrSwapchainFn::name()
    }

    pub fn fp(&self) -> &vk::KhrSwapchainFn {
        &self.fp
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
