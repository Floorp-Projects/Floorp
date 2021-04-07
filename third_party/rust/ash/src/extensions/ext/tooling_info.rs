#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::vk;
use std::ffi::CStr;
use std::mem;
use std::ptr;

#[derive(Clone)]
pub struct ToolingInfo {
    handle: vk::Instance,
    tooling_info_fn: vk::ExtToolingInfoFn,
}

impl ToolingInfo {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> ToolingInfo {
        let tooling_info_fn = vk::ExtToolingInfoFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        ToolingInfo {
            handle: instance.handle(),
            tooling_info_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::ExtToolingInfoFn::name()
    }

    #[doc = "https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceToolPropertiesEXT.html"]
    pub unsafe fn get_physical_device_tool_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> VkResult<Vec<vk::PhysicalDeviceToolPropertiesEXT>> {
        let mut count = 0;
        self.tooling_info_fn
            .get_physical_device_tool_properties_ext(physical_device, &mut count, ptr::null_mut())
            .result()?;
        let mut v = Vec::with_capacity(count as usize);
        let err_code = self
            .tooling_info_fn
            .get_physical_device_tool_properties_ext(physical_device, &mut count, v.as_mut_ptr());
        v.set_len(count as usize);
        err_code.result_with_success(v)
    }

    pub fn fp(&self) -> &vk::ExtToolingInfoFn {
        &self.tooling_info_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
