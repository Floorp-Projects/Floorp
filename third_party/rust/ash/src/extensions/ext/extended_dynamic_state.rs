use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;
use std::ptr;

#[derive(Clone)]
pub struct ExtendedDynamicState {
    handle: vk::Instance,
    extended_dynamic_state_fn: vk::ExtExtendedDynamicStateFn,
}

impl ExtendedDynamicState {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let extended_dynamic_state_fn = vk::ExtExtendedDynamicStateFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: instance.handle(),
            extended_dynamic_state_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::ExtExtendedDynamicStateFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetCullModeEXT.html>"]
    pub unsafe fn cmd_set_cull_mode(
        &self,
        command_buffer: vk::CommandBuffer,
        cull_mode: vk::CullModeFlags,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_cull_mode_ext(command_buffer, cull_mode)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetFrontFaceEXT.html>"]
    pub unsafe fn cmd_set_front_face(
        &self,
        command_buffer: vk::CommandBuffer,
        front_face: vk::FrontFace,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_front_face_ext(command_buffer, front_face)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetPrimitiveTopologyEXT.html>"]
    pub unsafe fn cmd_set_primitive_topology(
        &self,
        command_buffer: vk::CommandBuffer,
        primitive_topology: vk::PrimitiveTopology,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_primitive_topology_ext(command_buffer, primitive_topology)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetViewportWithCountEXT.html>"]
    pub unsafe fn cmd_set_viewport_with_count(
        &self,
        command_buffer: vk::CommandBuffer,
        viewports: &[vk::Viewport],
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_viewport_with_count_ext(
                command_buffer,
                viewports.len() as u32,
                viewports.as_ptr(),
            )
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetScissorWithCountEXT.html>"]
    pub unsafe fn cmd_set_scissor_with_count(
        &self,
        command_buffer: vk::CommandBuffer,
        scissors: &[vk::Rect2D],
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_scissor_with_count_ext(
                command_buffer,
                scissors.len() as u32,
                scissors.as_ptr(),
            )
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindVertexBuffers2EXT.html>"]
    pub unsafe fn cmd_bind_vertex_buffers2(
        &self,
        command_buffer: vk::CommandBuffer,
        first_binding: u32,
        buffers: &[vk::Buffer],
        offsets: &[vk::DeviceSize],
        sizes: Option<&[vk::DeviceSize]>,
        strides: Option<&[vk::DeviceSize]>,
    ) {
        assert_eq!(offsets.len(), buffers.len());
        let p_sizes = if let Some(sizes) = sizes {
            assert_eq!(sizes.len(), buffers.len());
            sizes.as_ptr()
        } else {
            ptr::null()
        };
        let p_strides = if let Some(strides) = strides {
            assert_eq!(strides.len(), buffers.len());
            strides.as_ptr()
        } else {
            ptr::null()
        };
        self.extended_dynamic_state_fn.cmd_bind_vertex_buffers2_ext(
            command_buffer,
            first_binding,
            buffers.len() as u32,
            buffers.as_ptr(),
            offsets.as_ptr(),
            p_sizes,
            p_strides,
        )
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthTestEnableEXT.html>"]
    pub unsafe fn cmd_set_depth_test_enable(
        &self,
        command_buffer: vk::CommandBuffer,
        depth_test_enable: bool,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_depth_test_enable_ext(command_buffer, depth_test_enable.into())
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthWriteEnableEXT.html>"]
    pub unsafe fn cmd_set_depth_write_enable(
        &self,
        command_buffer: vk::CommandBuffer,
        depth_write_enable: bool,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_depth_write_enable_ext(command_buffer, depth_write_enable.into())
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthCompareOpEXT.html>"]
    pub unsafe fn cmd_set_depth_compare_op(
        &self,
        command_buffer: vk::CommandBuffer,
        depth_compare_op: vk::CompareOp,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_depth_compare_op_ext(command_buffer, depth_compare_op)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBoundsTestEnableEXT.html>"]
    pub unsafe fn cmd_set_depth_bounds_test_enable(
        &self,
        command_buffer: vk::CommandBuffer,
        depth_bounds_test_enable: bool,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_depth_bounds_test_enable_ext(command_buffer, depth_bounds_test_enable.into())
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilTestEnableEXT.html>"]
    pub unsafe fn cmd_set_stencil_test_enable(
        &self,
        command_buffer: vk::CommandBuffer,
        stencil_test_enable: bool,
    ) {
        self.extended_dynamic_state_fn
            .cmd_set_stencil_test_enable_ext(command_buffer, stencil_test_enable.into())
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilOpEXT.html>"]
    pub unsafe fn cmd_set_stencil_op(
        &self,
        command_buffer: vk::CommandBuffer,
        face_mask: vk::StencilFaceFlags,
        fail_op: vk::StencilOp,
        pass_op: vk::StencilOp,
        depth_fail_op: vk::StencilOp,
        compare_op: vk::CompareOp,
    ) {
        self.extended_dynamic_state_fn.cmd_set_stencil_op_ext(
            command_buffer,
            face_mask,
            fail_op,
            pass_op,
            depth_fail_op,
            compare_op,
        )
    }

    pub fn fp(&self) -> &vk::ExtExtendedDynamicStateFn {
        &self.extended_dynamic_state_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
