use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct ExternalMemoryFd {
    handle: vk::Device,
    external_memory_fd_fn: vk::KhrExternalMemoryFdFn,
}

impl ExternalMemoryFd {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let external_memory_fd_fn = vk::KhrExternalMemoryFdFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            external_memory_fd_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrExternalMemoryFdFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryFdKHR.html>"]
    pub unsafe fn get_memory_fd(&self, create_info: &vk::MemoryGetFdInfoKHR) -> VkResult<i32> {
        let mut fd = -1;

        self.external_memory_fd_fn
            .get_memory_fd_khr(self.handle, create_info, &mut fd)
            .result_with_success(fd)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryFdPropertiesKHR.html>"]
    pub unsafe fn get_memory_fd_properties_khr(
        &self,
        handle_type: vk::ExternalMemoryHandleTypeFlags,
        fd: i32,
    ) -> VkResult<vk::MemoryFdPropertiesKHR> {
        let mut memory_fd_properties = Default::default();
        self.external_memory_fd_fn
            .get_memory_fd_properties_khr(self.handle, handle_type, fd, &mut memory_fd_properties)
            .result_with_success(memory_fd_properties)
    }

    pub fn fp(&self) -> &vk::KhrExternalMemoryFdFn {
        &self.external_memory_fd_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
