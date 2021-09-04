use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Maintenance1 {
    handle: vk::Device,
    fns: vk::KhrMaintenance1Fn,
}

impl Maintenance1 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fns = vk::KhrMaintenance1Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            fns,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrMaintenance1Fn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkTrimCommandPoolKHR.html>"]
    pub unsafe fn trim_command_pool(
        &self,
        command_pool: vk::CommandPool,
        flags: vk::CommandPoolTrimFlagsKHR,
    ) {
        self.fns
            .trim_command_pool_khr(self.handle, command_pool, flags);
    }

    pub fn fp(&self) -> &vk::KhrMaintenance1Fn {
        &self.fns
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
