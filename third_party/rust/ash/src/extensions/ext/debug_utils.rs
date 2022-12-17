use crate::prelude::*;
use crate::{vk, RawPtr};
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DebugUtils {
    handle: vk::Instance,
    fp: vk::ExtDebugUtilsFn,
}

impl DebugUtils {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::ExtDebugUtilsFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkSetDebugUtilsObjectNameEXT.html>
    #[deprecated = "Backwards-compatible alias containing a typo, use `set_debug_utils_object_name()` instead"]
    #[inline]
    pub unsafe fn debug_utils_set_object_name(
        &self,
        device: vk::Device,
        name_info: &vk::DebugUtilsObjectNameInfoEXT,
    ) -> VkResult<()> {
        self.set_debug_utils_object_name(device, name_info)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkSetDebugUtilsObjectNameEXT.html>
    #[inline]
    pub unsafe fn set_debug_utils_object_name(
        &self,
        device: vk::Device,
        name_info: &vk::DebugUtilsObjectNameInfoEXT,
    ) -> VkResult<()> {
        (self.fp.set_debug_utils_object_name_ext)(device, name_info).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkSetDebugUtilsObjectTagEXT.html>
    #[deprecated = "Backwards-compatible alias containing a typo, use `set_debug_utils_object_tag()` instead"]
    #[inline]
    pub unsafe fn debug_utils_set_object_tag(
        &self,
        device: vk::Device,
        tag_info: &vk::DebugUtilsObjectTagInfoEXT,
    ) -> VkResult<()> {
        self.set_debug_utils_object_tag(device, tag_info)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkSetDebugUtilsObjectTagEXT.html>
    #[inline]
    pub unsafe fn set_debug_utils_object_tag(
        &self,
        device: vk::Device,
        tag_info: &vk::DebugUtilsObjectTagInfoEXT,
    ) -> VkResult<()> {
        (self.fp.set_debug_utils_object_tag_ext)(device, tag_info).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdBeginDebugUtilsLabelEXT.html>
    #[inline]
    pub unsafe fn cmd_begin_debug_utils_label(
        &self,
        command_buffer: vk::CommandBuffer,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        (self.fp.cmd_begin_debug_utils_label_ext)(command_buffer, label);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdEndDebugUtilsLabelEXT.html>
    #[inline]
    pub unsafe fn cmd_end_debug_utils_label(&self, command_buffer: vk::CommandBuffer) {
        (self.fp.cmd_end_debug_utils_label_ext)(command_buffer);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdInsertDebugUtilsLabelEXT.html>
    #[inline]
    pub unsafe fn cmd_insert_debug_utils_label(
        &self,
        command_buffer: vk::CommandBuffer,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        (self.fp.cmd_insert_debug_utils_label_ext)(command_buffer, label);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkQueueBeginDebugUtilsLabelEXT.html>
    #[inline]
    pub unsafe fn queue_begin_debug_utils_label(
        &self,
        queue: vk::Queue,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        (self.fp.queue_begin_debug_utils_label_ext)(queue, label);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkQueueEndDebugUtilsLabelEXT.html>
    #[inline]
    pub unsafe fn queue_end_debug_utils_label(&self, queue: vk::Queue) {
        (self.fp.queue_end_debug_utils_label_ext)(queue);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkQueueInsertDebugUtilsLabelEXT.html>
    #[inline]
    pub unsafe fn queue_insert_debug_utils_label(
        &self,
        queue: vk::Queue,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        (self.fp.queue_insert_debug_utils_label_ext)(queue, label);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateDebugUtilsMessengerEXT.html>
    #[inline]
    pub unsafe fn create_debug_utils_messenger(
        &self,
        create_info: &vk::DebugUtilsMessengerCreateInfoEXT,
        allocator: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DebugUtilsMessengerEXT> {
        let mut messenger = mem::zeroed();
        (self.fp.create_debug_utils_messenger_ext)(
            self.handle,
            create_info,
            allocator.as_raw_ptr(),
            &mut messenger,
        )
        .result_with_success(messenger)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDestroyDebugUtilsMessengerEXT.html>
    #[inline]
    pub unsafe fn destroy_debug_utils_messenger(
        &self,
        messenger: vk::DebugUtilsMessengerEXT,
        allocator: Option<&vk::AllocationCallbacks>,
    ) {
        (self.fp.destroy_debug_utils_messenger_ext)(self.handle, messenger, allocator.as_raw_ptr());
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkSubmitDebugUtilsMessageEXT.html>
    #[inline]
    pub unsafe fn submit_debug_utils_message(
        &self,
        message_severity: vk::DebugUtilsMessageSeverityFlagsEXT,
        message_types: vk::DebugUtilsMessageTypeFlagsEXT,
        callback_data: &vk::DebugUtilsMessengerCallbackDataEXT,
    ) {
        (self.fp.submit_debug_utils_message_ext)(
            self.handle,
            message_severity,
            message_types,
            callback_data,
        );
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtDebugUtilsFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtDebugUtilsFn {
        &self.fp
    }

    #[inline]
    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
