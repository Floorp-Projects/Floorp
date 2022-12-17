use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;
use std::os::raw::c_void;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_NV_device_diagnostic_checkpoints.html>
#[derive(Clone)]
pub struct DeviceDiagnosticCheckpoints {
    fp: vk::NvDeviceDiagnosticCheckpointsFn,
}

impl DeviceDiagnosticCheckpoints {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let fp = vk::NvDeviceDiagnosticCheckpointsFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdSetCheckpointNV.html>
    #[inline]
    pub unsafe fn cmd_set_checkpoint(
        &self,
        command_buffer: vk::CommandBuffer,
        p_checkpoint_marker: *const c_void,
    ) {
        (self.fp.cmd_set_checkpoint_nv)(command_buffer, p_checkpoint_marker);
    }

    /// Retrieve the number of elements to pass to [`get_queue_checkpoint_data()`][Self::get_queue_checkpoint_data()]
    #[inline]
    pub unsafe fn get_queue_checkpoint_data_len(&self, queue: vk::Queue) -> usize {
        let mut count = 0;
        (self.fp.get_queue_checkpoint_data_nv)(queue, &mut count, std::ptr::null_mut());
        count as usize
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetQueueCheckpointDataNV.html>
    ///
    /// Call [`get_queue_checkpoint_data_len()`][Self::get_queue_checkpoint_data_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_queue_checkpoint_data(
        &self,
        queue: vk::Queue,
        out: &mut [vk::CheckpointDataNV],
    ) {
        let mut count = out.len() as u32;
        (self.fp.get_queue_checkpoint_data_nv)(queue, &mut count, out.as_mut_ptr());
        assert_eq!(count as usize, out.len());
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::NvDeviceDiagnosticCheckpointsFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::NvDeviceDiagnosticCheckpointsFn {
        &self.fp
    }
}
