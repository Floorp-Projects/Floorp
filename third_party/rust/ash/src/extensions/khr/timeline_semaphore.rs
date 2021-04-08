#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::vk;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct TimelineSemaphore {
    handle: vk::Instance,
    timeline_semaphore_fn: vk::KhrTimelineSemaphoreFn,
}

impl TimelineSemaphore {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> TimelineSemaphore {
        let timeline_semaphore_fn = vk::KhrTimelineSemaphoreFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });

        TimelineSemaphore {
            handle: instance.handle(),
            timeline_semaphore_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrTimelineSemaphoreFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreCounterValue.html>"]
    pub unsafe fn get_semaphore_counter_value(
        &self,
        device: vk::Device,
        semaphore: vk::Semaphore,
    ) -> VkResult<u64> {
        let mut value = 0;
        self.timeline_semaphore_fn
            .get_semaphore_counter_value_khr(device, semaphore, &mut value)
            .result_with_success(value)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitSemaphores.html>"]
    pub unsafe fn wait_semaphores(
        &self,
        device: vk::Device,
        wait_info: &vk::SemaphoreWaitInfo,
        timeout: u64,
    ) -> VkResult<()> {
        self.timeline_semaphore_fn
            .wait_semaphores_khr(device, wait_info, timeout)
            .into()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSignalSemaphore.html>"]
    pub unsafe fn signal_semaphore(
        &self,
        device: vk::Device,
        signal_info: &vk::SemaphoreSignalInfo,
    ) -> VkResult<()> {
        self.timeline_semaphore_fn
            .signal_semaphore_khr(device, signal_info)
            .into()
    }

    pub fn fp(&self) -> &vk::KhrTimelineSemaphoreFn {
        &self.timeline_semaphore_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
