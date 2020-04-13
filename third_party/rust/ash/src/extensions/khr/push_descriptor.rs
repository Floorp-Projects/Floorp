#![allow(dead_code)]
use crate::version::{DeviceV1_0, InstanceV1_0};
use crate::vk;
use std::ffi::c_void;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct PushDescriptor {
    handle: vk::Instance,
    push_descriptors_fn: vk::KhrPushDescriptorFn,
}

impl PushDescriptor {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(instance: &I, device: &D) -> PushDescriptor {
        let push_descriptors_fn = vk::KhrPushDescriptorFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });

        PushDescriptor {
            handle: instance.handle(),
            push_descriptors_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrPushDescriptorFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushDescriptorSetKHR.html>"]
    pub unsafe fn cmd_push_descriptor_set(
        &self,
        command_buffer: vk::CommandBuffer,
        pipeline_bind_point: vk::PipelineBindPoint,
        layout: vk::PipelineLayout,
        set: u32,
        descriptor_writes: &[vk::WriteDescriptorSet],
    ) {
        self.push_descriptors_fn.cmd_push_descriptor_set_khr(
            command_buffer,
            pipeline_bind_point,
            layout,
            set,
            descriptor_writes.len() as u32,
            descriptor_writes.as_ptr(),
        );
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushDescriptorSetWithTemplateKHR.html>"]
    pub unsafe fn cmd_push_descriptor_set_with_template(
        &self,
        command_buffer: vk::CommandBuffer,
        descriptor_update_template: vk::DescriptorUpdateTemplate,
        layout: vk::PipelineLayout,
        set: u32,
        p_data: *const c_void,
    ) {
        self.push_descriptors_fn
            .cmd_push_descriptor_set_with_template_khr(
                command_buffer,
                descriptor_update_template,
                layout,
                set,
                p_data,
            );
    }

    pub fn fp(&self) -> &vk::KhrPushDescriptorFn {
        &self.push_descriptors_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
