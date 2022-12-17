use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct MeshShader {
    fp: vk::NvMeshShaderFn,
}

impl MeshShader {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fp = vk::NvMeshShaderFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdDrawMeshTasksNV.html>
    #[inline]
    pub unsafe fn cmd_draw_mesh_tasks(
        &self,
        command_buffer: vk::CommandBuffer,
        task_count: u32,
        first_task: u32,
    ) {
        (self.fp.cmd_draw_mesh_tasks_nv)(command_buffer, task_count, first_task);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdDrawMeshTasksIndirectNV.html>
    #[inline]
    pub unsafe fn cmd_draw_mesh_tasks_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        (self.fp.cmd_draw_mesh_tasks_indirect_nv)(
            command_buffer,
            buffer,
            offset,
            draw_count,
            stride,
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdDrawMeshTasksIndirectCountNV.html>
    #[inline]
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
        (self.fp.cmd_draw_mesh_tasks_indirect_count_nv)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        );
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::NvMeshShaderFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::NvMeshShaderFn {
        &self.fp
    }
}
