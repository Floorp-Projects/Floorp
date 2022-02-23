use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct TimelineSemaphore {
    handle: vk::Device,
    fp: vk::KhrTimelineSemaphoreFn,
}

impl TimelineSemaphore {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrTimelineSemaphoreFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreCounterValue.html>"]
    pub unsafe fn get_semaphore_counter_value(&self, semaphore: vk::Semaphore) -> VkResult<u64> {
        let mut value = 0;
        self.fp
            .get_semaphore_counter_value_khr(self.handle, semaphore, &mut value)
            .result_with_success(value)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitSemaphores.html>"]
    pub unsafe fn wait_semaphores(
        &self,
        wait_info: &vk::SemaphoreWaitInfo,
        timeout: u64,
    ) -> VkResult<()> {
        self.fp
            .wait_semaphores_khr(self.handle, wait_info, timeout)
            .result()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSignalSemaphore.html>"]
    pub unsafe fn signal_semaphore(&self, signal_info: &vk::SemaphoreSignalInfo) -> VkResult<()> {
        self.fp
            .signal_semaphore_khr(self.handle, signal_info)
            .result()
    }

    pub fn name() -> &'static CStr {
        vk::KhrTimelineSemaphoreFn::name()
    }

    pub fn fp(&self) -> &vk::KhrTimelineSemaphoreFn {
        &self.fp
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
