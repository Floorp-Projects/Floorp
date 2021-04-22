#![allow(dead_code)]
use crate::version::{DeviceV1_0, InstanceV1_0};
use crate::vk;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct MeshShader {
    mesh_shader_fn: vk::NvMeshShaderFn,
}

impl MeshShader {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(instance: &I, device: &D) -> MeshShader {
        let mesh_shader_fn = vk::NvMeshShaderFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        MeshShader { mesh_shader_fn }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMeshTasksNV.html>"]
    pub unsafe fn cmd_draw_mesh_tasks(
        &self,
        command_buffer: vk::CommandBuffer,
        task_count: u32,
        first_task: u32,
    ) {
        self.mesh_shader_fn
            .cmd_draw_mesh_tasks_nv(command_buffer, task_count, first_task);
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMeshTasksIndirectNV.html>"]
    pub unsafe fn cmd_draw_mesh_tasks_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        self.mesh_shader_fn.cmd_draw_mesh_tasks_indirect_nv(
            command_buffer,
            buffer,
            offset,
            draw_count,
            stride,
        );
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMeshTasksIndirectCountNV.html>"]
    pub unsafe fn cmd_draw_mesh_tasks_indirect_count(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        count_buffer: vk::Buffer,
        count_buffer_offset: vk::DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        self.mesh_shader_fn.cmd_draw_mesh_tasks_indirect_count_nv(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        );
    }
    pub fn name() -> &'static CStr {
        vk::NvMeshShaderFn::name()
    }

    pub fn fp(&self) -> &vk::NvMeshShaderFn {
        &self.mesh_shader_fn
    }
}
