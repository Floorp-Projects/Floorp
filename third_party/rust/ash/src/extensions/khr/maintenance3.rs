use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Maintenance3 {
    handle: vk::Device,
    fns: vk::KhrMaintenance3Fn,
}

impl Maintenance3 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fns = vk::KhrMaintenance3Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            fns,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrMaintenance3Fn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDescriptorSetLayoutSupportKHR.html>"]
    pub unsafe fn get_descriptor_set_layout_support(
        &self,
        create_info: &vk::DescriptorSetLayoutCreateInfo,
        out: &mut vk::DescriptorSetLayoutSupportKHR,
    ) {
        self.fns
            .get_descriptor_set_layout_support_khr(self.handle, create_info, out);
    }

    pub fn fp(&self) -> &vk::KhrMaintenance3Fn {
        &self.fns
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
