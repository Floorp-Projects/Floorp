use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct ExternalSemaphoreFd {
    handle: vk::Device,
    fp: vk::KhrExternalSemaphoreFdFn,
}

impl ExternalSemaphoreFd {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrExternalSemaphoreFdFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkImportSemaphoreFdKHR.html>
    #[inline]
    pub unsafe fn import_semaphore_fd(
        &self,
        import_info: &vk::ImportSemaphoreFdInfoKHR,
    ) -> VkResult<()> {
        (self.fp.import_semaphore_fd_khr)(self.handle, import_info).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetSemaphoreFdKHR.html>
    #[inline]
    pub unsafe fn get_semaphore_fd(&self, get_info: &vk::SemaphoreGetFdInfoKHR) -> VkResult<i32> {
        let mut fd = -1;
        (self.fp.get_semaphore_fd_khr)(self.handle, get_info, &mut fd).result_with_success(fd)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrExternalSemaphoreFdFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrExternalSemaphoreFdFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
