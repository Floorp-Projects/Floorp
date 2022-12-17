use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DebugReport {
    handle: vk::Instance,
    fp: vk::ExtDebugReportFn,
}

impl DebugReport {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::ExtDebugReportFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDestroyDebugReportCallbackEXT.html>
    #[inline]
    pub unsafe fn destroy_debug_report_callback(
        &self,
        debug: vk::DebugReportCallbackEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        (self.fp.destroy_debug_report_callback_ext)(
            self.handle,
            debug,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateDebugReportCallbackEXT.html>
    #[inline]
    pub unsafe fn create_debug_report_callback(
        &self,
        create_info: &vk::DebugReportCallbackCreateInfoEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DebugReportCallbackEXT> {
        let mut debug_cb = mem::zeroed();
        (self.fp.create_debug_report_callback_ext)(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut debug_cb,
        )
        .result_with_success(debug_cb)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtDebugReportFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtDebugReportFn {
        &self.fp
    }

    #[inline]
    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
