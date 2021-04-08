#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DebugReport {
    handle: vk::Instance,
    debug_report_fn: vk::ExtDebugReportFn,
}

impl DebugReport {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> DebugReport {
        let debug_report_fn = vk::ExtDebugReportFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        DebugReport {
            handle: instance.handle(),
            debug_report_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::ExtDebugReportFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDebugReportCallbackEXT.html>"]
    pub unsafe fn destroy_debug_report_callback(
        &self,
        debug: vk::DebugReportCallbackEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        self.debug_report_fn.destroy_debug_report_callback_ext(
            self.handle,
            debug,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDebugReportCallbackEXT.html>"]
    pub unsafe fn create_debug_report_callback(
        &self,
        create_info: &vk::DebugReportCallbackCreateInfoEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DebugReportCallbackEXT> {
        let mut debug_cb = mem::zeroed();
        self.debug_report_fn
            .create_debug_report_callback_ext(
                self.handle,
                create_info,
                allocation_callbacks.as_raw_ptr(),
                &mut debug_cb,
            )
            .result_with_success(debug_cb)
    }

    pub fn fp(&self) -> &vk::ExtDebugReportFn {
        &self.debug_report_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
