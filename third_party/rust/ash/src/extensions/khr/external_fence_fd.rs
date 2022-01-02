use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct ExternalFenceFd {
    handle: vk::Device,
    external_fence_fd_fn: vk::KhrExternalFenceFdFn,
}

impl ExternalFenceFd {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let external_fence_fd_fn = vk::KhrExternalFenceFdFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            external_fence_fd_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrExternalFenceFdFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkImportFenceFdKHR.html>"]
    pub unsafe fn import_fence_fd(&self, import_info: &vk::ImportFenceFdInfoKHR) -> VkResult<()> {
        self.external_fence_fd_fn
            .import_fence_fd_khr(self.handle, import_info)
            .result()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetFenceFdKHR.html>"]
    pub unsafe fn get_fence_fd(&self, get_info: &vk::FenceGetFdInfoKHR) -> VkResult<i32> {
        let mut fd = -1;

        self.external_fence_fd_fn
            .get_fence_fd_khr(self.handle, get_info, &mut fd)
            .result_with_success(fd)
    }

    pub fn fp(&self) -> &vk::KhrExternalFenceFdFn {
        &self.external_fence_fd_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
