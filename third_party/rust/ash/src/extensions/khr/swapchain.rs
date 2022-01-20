use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Swapchain {
    handle: vk::Device,
    swapchain_fn: vk::KhrSwapchainFn,
}

impl Swapchain {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let swapchain_fn = vk::KhrSwapchainFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            swapchain_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrSwapchainFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySwapchainKHR.html>"]
    pub unsafe fn destroy_swapchain(
        &self,
        swapchain: vk::SwapchainKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.swapchain_fn.destroy_swapchain_khr(
            self.handle,
            swapchain,
            allocation_callbacks.as_raw_ptr(),
        );
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
        let err_code = self.swapchain_fn.acquire_next_image_khr(
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
        self.swapchain_fn
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
        create_info: &vk::PresentInfoKHR,
    ) -> VkResult<bool> {
        let err_code = self.swapchain_fn.queue_present_khr(queue, create_info);
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
            self.swapchain_fn
                .get_swapchain_images_khr(self.handle, swapchain, count, data)
        })
    }

    pub fn fp(&self) -> &vk::KhrSwapchainFn {
        &self.swapchain_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
