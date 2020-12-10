#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::{vk, RawPtr};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DebugUtils {
    handle: vk::Instance,
    debug_utils_fn: vk::ExtDebugUtilsFn,
}

impl DebugUtils {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> DebugUtils {
        let debug_utils_fn = vk::ExtDebugUtilsFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        DebugUtils {
            handle: instance.handle(),
            debug_utils_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::ExtDebugUtilsFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetDebugUtilsObjectNameEXT.html>"]
    pub unsafe fn debug_utils_set_object_name(
        &self,
        device: vk::Device,
        name_info: &vk::DebugUtilsObjectNameInfoEXT,
    ) -> VkResult<()> {
        let err_code = self
            .debug_utils_fn
            .set_debug_utils_object_name_ext(device, name_info);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetDebugUtilsObjectTagEXT.html>"]
    pub unsafe fn debug_utils_set_object_tag(
        &self,
        device: vk::Device,
        tag_info: &vk::DebugUtilsObjectTagInfoEXT,
    ) -> VkResult<()> {
        let err_code = self
            .debug_utils_fn
            .set_debug_utils_object_tag_ext(device, tag_info);
        match err_code {
            vk::Result::SUCCESS => Ok(()),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginDebugUtilsLabelEXT.html>"]
    pub unsafe fn cmd_begin_debug_utils_label(
        &self,
        command_buffer: vk::CommandBuffer,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        self.debug_utils_fn
            .cmd_begin_debug_utils_label_ext(command_buffer, label);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndDebugUtilsLabelEXT.html>"]
    pub unsafe fn cmd_end_debug_utils_label(&self, command_buffer: vk::CommandBuffer) {
        self.debug_utils_fn
            .cmd_end_debug_utils_label_ext(command_buffer);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdInsertDebugUtilsLabelEXT.html>"]
    pub unsafe fn cmd_insert_debug_utils_label(
        &self,
        command_buffer: vk::CommandBuffer,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        self.debug_utils_fn
            .cmd_insert_debug_utils_label_ext(command_buffer, label);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueBeginDebugUtilsLabelEXT.html>"]
    pub unsafe fn queue_begin_debug_utils_label(
        &self,
        queue: vk::Queue,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        self.debug_utils_fn
            .queue_begin_debug_utils_label_ext(queue, label);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueEndDebugUtilsLabelEXT.html>"]
    pub unsafe fn queue_end_debug_utils_label(&self, queue: vk::Queue) {
        self.debug_utils_fn.queue_end_debug_utils_label_ext(queue);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueInsertDebugUtilsLabelEXT.html>"]
    pub unsafe fn queue_insert_debug_utils_label(
        &self,
        queue: vk::Queue,
        label: &vk::DebugUtilsLabelEXT,
    ) {
        self.debug_utils_fn
            .queue_insert_debug_utils_label_ext(queue, label);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDebugUtilsMessengerEXT.html>"]
    pub unsafe fn create_debug_utils_messenger(
        &self,
        create_info: &vk::DebugUtilsMessengerCreateInfoEXT,
        allocator: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DebugUtilsMessengerEXT> {
        let mut messenger = mem::zeroed();
        let err_code = self.debug_utils_fn.create_debug_utils_messenger_ext(
            self.handle,
            create_info,
            allocator.as_raw_ptr(),
            &mut messenger,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(messenger),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDebugUtilsMessengerEXT.html>"]
    pub unsafe fn destroy_debug_utils_messenger(
        &self,
        messenger: vk::DebugUtilsMessengerEXT,
        allocator: Option<&vk::AllocationCallbacks>,
    ) {
        self.debug_utils_fn.destroy_debug_utils_messenger_ext(
            self.handle,
            messenger,
            allocator.as_raw_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSubmitDebugUtilsMessageEXT.html>"]
    pub unsafe fn submit_debug_utils_message(
        &self,
        instance: vk::Instance,
        message_severity: vk::DebugUtilsMessageSeverityFlagsEXT,
        message_types: vk::DebugUtilsMessageTypeFlagsEXT,
        callback_data: &vk::DebugUtilsMessengerCallbackDataEXT,
    ) {
        self.debug_utils_fn.submit_debug_utils_message_ext(
            instance,
            message_severity,
            message_types,
            callback_data,
        );
    }

    pub fn fp(&self) -> &vk::ExtDebugUtilsFn {
        &self.debug_utils_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
