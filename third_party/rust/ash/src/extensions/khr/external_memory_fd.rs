use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct ExternalMemoryFd {
    handle: vk::Device,
    fp: vk::KhrExternalMemoryFdFn,
}

impl ExternalMemoryFd {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrExternalMemoryFdFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetMemoryFdKHR.html>
    #[inline]
    pub unsafe fn get_memory_fd(&self, get_fd_info: &vk::MemoryGetFdInfoKHR) -> VkResult<i32> {
        let mut fd = -1;
        (self.fp.get_memory_fd_khr)(self.handle, get_fd_info, &mut fd).result_with_success(fd)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetMemoryFdPropertiesKHR.html>
    #[inline]
    pub unsafe fn get_memory_fd_properties(
        &self,
        handle_type: vk::ExternalMemoryHandleTypeFlags,
        fd: i32,
    ) -> VkResult<vk::MemoryFdPropertiesKHR> {
        let mut memory_fd_properties = Default::default();
        (self.fp.get_memory_fd_properties_khr)(
            self.handle,
            handle_type,
            fd,
            &mut memory_fd_properties,
        )
        .result_with_success(memory_fd_properties)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrExternalMemoryFdFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrExternalMemoryFdFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
