use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct ExternalSemaphoreFd {
    handle: vk::Device,
    external_semaphore_fd_fn: vk::KhrExternalSemaphoreFdFn,
}

impl ExternalSemaphoreFd {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let external_semaphore_fd_fn = vk::KhrExternalSemaphoreFdFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            external_semaphore_fd_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrExternalSemaphoreFdFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkImportSemaphoreFdKHR.html>"]
    pub unsafe fn import_semaphore_fd(
        &self,
        import_info: &vk::ImportSemaphoreFdInfoKHR,
    ) -> VkResult<()> {
        self.external_semaphore_fd_fn
            .import_semaphore_fd_khr(self.handle, import_info)
            .result()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreFdKHR.html>"]
    pub unsafe fn get_semaphore_fd(&self, get_info: &vk::SemaphoreGetFdInfoKHR) -> VkResult<i32> {
        let mut fd = -1;

        self.external_semaphore_fd_fn
            .get_semaphore_fd_khr(self.handle, get_info, &mut fd)
            .result_with_success(fd)
    }

    pub fn fp(&self) -> &vk::KhrExternalSemaphoreFdFn {
        &self.external_semaphore_fd_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
