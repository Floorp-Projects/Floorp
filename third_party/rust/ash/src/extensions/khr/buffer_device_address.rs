use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct BufferDeviceAddress {
    handle: vk::Device,
    fns: vk::KhrBufferDeviceAddressFn,
}

impl BufferDeviceAddress {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fns = vk::KhrBufferDeviceAddressFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            fns,
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferDeviceAddressKHR.html>"]
    pub unsafe fn get_buffer_device_address(
        &self,
        info: &vk::BufferDeviceAddressInfoKHR,
    ) -> vk::DeviceAddress {
        self.fns.get_buffer_device_address_khr(self.handle, info)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferOpaqueCaptureAddressKHR.html>"]
    pub unsafe fn get_buffer_opaque_capture_address(
        &self,
        info: &vk::BufferDeviceAddressInfoKHR,
    ) -> u64 {
        self.fns
            .get_buffer_opaque_capture_address_khr(self.handle, info)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceMemoryOpaqueCaptureAddressKHR.html>"]
    pub unsafe fn get_device_memory_opaque_capture_address(
        &self,
        info: &vk::DeviceMemoryOpaqueCaptureAddressInfoKHR,
    ) -> u64 {
        self.fns
            .get_device_memory_opaque_capture_address_khr(self.handle, info)
    }

    pub fn name() -> &'static CStr {
        vk::KhrBufferDeviceAddressFn::name()
    }

    pub fn fp(&self) -> &vk::KhrBufferDeviceAddressFn {
        &self.fns
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
