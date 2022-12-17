use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_mesh_shader.html>
#[derive(Clone)]
pub struct MeshShader {
    fp: vk::ExtMeshShaderFn,
}

impl MeshShader {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fp = vk::ExtMeshShaderFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    /// <https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdDrawMeshTasksEXT.html>
    #[inline]
    pub unsafe fn cmd_draw_mesh_tasks(
        &self,
        command_buffer: vk::CommandBuffer,
        group_count_x: u32,
        group_count_y: u32,
        group_count_z: u32,
    ) {
        (self.fp.cmd_draw_mesh_tasks_ext)(
            command_buffer,
            group_count_x,
            group_count_y,
            group_count_z,
        );
    }

    /// <https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdDrawMeshTasksIndirectEXT.html>
    ///
    /// `buffer` contains `draw_count` [`vk::DrawMeshTasksIndirectCommandEXT`] structures starting at `offset` in bytes, holding the draw parameters.
    #[inline]
    pub unsafe fn cmd_draw_mesh_tasks_indirect(
        &self,
        command_buffer: vk::CommandBuffer,
        buffer: vk::Buffer,
        offset: vk::DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        (self.fp.cmd_draw_mesh_tasks_indirect_ext)(
            command_buffer,
            buffer,
            offset,
            draw_count,
            stride,
        );
    }

    /// <https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdDrawMeshTasksIndirectCountEXT.html>
    ///
    /// `buffer` contains a maximum of `max_draw_count` [`vk::DrawMeshTasksIndirectCommandEXT`] structures starting at `offset` in bytes, holding the draw parameters.
    /// `count_buffer` is the buffer containing the draw count, starting at `count_buffer_offset` in bytes.
    /// The actual number of executed draw calls is the minimum of the count specified in `count_buffer` and `max_draw_count`.
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
        (self.fp.cmd_draw_mesh_tasks_indirect_count_ext)(
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
        vk::ExtMeshShaderFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtMeshShaderFn {
        &self.fp
    }
}
