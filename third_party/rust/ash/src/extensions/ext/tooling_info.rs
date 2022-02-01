use crate::prelude::*;
use crate::vk;
use crate::{EntryCustom, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct ToolingInfo {
    handle: vk::Instance,
    tooling_info_fn: vk::ExtToolingInfoFn,
}

impl ToolingInfo {
    pub fn new<L>(entry: &EntryCustom<L>, instance: &Instance) -> Self {
        let tooling_info_fn = vk::ExtToolingInfoFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self {
            handle: instance.handle(),
            tooling_info_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::ExtToolingInfoFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceToolPropertiesEXT.html>"]
    pub unsafe fn get_physical_device_tool_properties(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> VkResult<Vec<vk::PhysicalDeviceToolPropertiesEXT>> {
        read_into_defaulted_vector(|count, data| {
            self.tooling_info_fn
                .get_physical_device_tool_properties_ext(physical_device, count, data)
        })
    }

    pub fn fp(&self) -> &vk::ExtToolingInfoFn {
        &self.tooling_info_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
