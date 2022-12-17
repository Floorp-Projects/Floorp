use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_copy_commands2.html>
#[derive(Clone)]
pub struct CopyCommands2 {
    fp: vk::KhrCopyCommands2Fn,
}

impl CopyCommands2 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fp = vk::KhrCopyCommands2Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyBuffer2KHR.html>
    #[inline]
    pub unsafe fn cmd_copy_buffer2(
        &self,
        command_buffer: vk::CommandBuffer,
        copy_buffer_info: &vk::CopyBufferInfo2KHR,
    ) {
        (self.fp.cmd_copy_buffer2_khr)(command_buffer, copy_buffer_info)
    }
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyImage2KHR.html>
    #[inline]
    pub unsafe fn cmd_copy_image2(
        &self,
        command_buffer: vk::CommandBuffer,
        copy_image_info: &vk::CopyImageInfo2KHR,
    ) {
        (self.fp.cmd_copy_image2_khr)(command_buffer, copy_image_info)
    }
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyBufferToImage2KHR.html>
    #[inline]
    pub unsafe fn cmd_copy_buffer_to_image2(
        &self,
        command_buffer: vk::CommandBuffer,
        copy_buffer_to_image_info: &vk::CopyBufferToImageInfo2KHR,
    ) {
        (self.fp.cmd_copy_buffer_to_image2_khr)(command_buffer, copy_buffer_to_image_info)
    }
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdCopyImageToBuffer2KHR.html>
    #[inline]
    pub unsafe fn cmd_copy_image_to_buffer2(
        &self,
        command_buffer: vk::CommandBuffer,
        copy_image_to_buffer_info: &vk::CopyImageToBufferInfo2KHR,
    ) {
        (self.fp.cmd_copy_image_to_buffer2_khr)(command_buffer, copy_image_to_buffer_info)
    }
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdBlitImage2KHR.html>
    #[inline]
    pub unsafe fn cmd_blit_image2(
        &self,
        command_buffer: vk::CommandBuffer,
        blit_image_info: &vk::BlitImageInfo2KHR,
    ) {
        (self.fp.cmd_blit_image2_khr)(command_buffer, blit_image_info)
    }
    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdResolveImage2KHR.html>
    #[inline]
    pub unsafe fn cmd_resolve_image2(
        &self,
        command_buffer: vk::CommandBuffer,
        resolve_image_info: &vk::ResolveImageInfo2KHR,
    ) {
        (self.fp.cmd_resolve_image2_khr)(command_buffer, resolve_image_info)
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrCopyCommands2Fn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrCopyCommands2Fn {
        &self.fp
    }
}
