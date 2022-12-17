use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;
use std::ptr;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_external_memory_win32.html>
#[derive(Clone)]
pub struct ExternalMemoryWin32 {
    handle: vk::Device,
    fp: vk::KhrExternalMemoryWin32Fn,
}

impl ExternalMemoryWin32 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrExternalMemoryWin32Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetMemoryWin32HandleKHR.html>
    #[inline]
    pub unsafe fn get_memory_win32_handle(
        &self,
        create_info: &vk::MemoryGetWin32HandleInfoKHR,
    ) -> VkResult<vk::HANDLE> {
        let mut handle = ptr::null_mut();
        (self.fp.get_memory_win32_handle_khr)(self.handle, create_info, &mut handle)
            .result_with_success(handle)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetMemoryWin32HandlePropertiesKHR.html>
    #[inline]
    pub unsafe fn get_memory_win32_handle_properties(
        &self,
        handle_type: vk::ExternalMemoryHandleTypeFlags,
        handle: vk::HANDLE,
    ) -> VkResult<vk::MemoryWin32HandlePropertiesKHR> {
        let mut memory_win32_handle_properties = Default::default();
        (self.fp.get_memory_win32_handle_properties_khr)(
            self.handle,
            handle_type,
            handle,
            &mut memory_win32_handle_properties,
        )
        .result_with_success(memory_win32_handle_properties)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrExternalMemoryWin32Fn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrExternalMemoryWin32Fn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
