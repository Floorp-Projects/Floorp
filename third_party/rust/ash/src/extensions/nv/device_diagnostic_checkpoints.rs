use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;
use std::os::raw::c_void;

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

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetCheckpointNV.html>"]
    pub unsafe fn cmd_set_checkpoint(
        &self,
        command_buffer: vk::CommandBuffer,
        p_checkpoint_marker: *const c_void,
    ) {
        self.fp
            .cmd_set_checkpoint_nv(command_buffer, p_checkpoint_marker);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetQueueCheckpointDataNV.html>"]
    pub unsafe fn get_queue_checkpoint_data(&self, queue: vk::Queue) -> Vec<vk::CheckpointDataNV> {
        let mut checkpoint_data_count: u32 = 0;
        self.fp.get_queue_checkpoint_data_nv(
            queue,
            &mut checkpoint_data_count,
            std::ptr::null_mut(),
        );
        let mut checkpoint_data: Vec<vk::CheckpointDataNV> =
            vec![vk::CheckpointDataNV::default(); checkpoint_data_count as _];
        self.fp.get_queue_checkpoint_data_nv(
            queue,
            &mut checkpoint_data_count,
            checkpoint_data.as_mut_ptr(),
        );
        checkpoint_data
    }

    pub fn name() -> &'static CStr {
        vk::NvDeviceDiagnosticCheckpointsFn::name()
    }

    pub fn fp(&self) -> &vk::NvDeviceDiagnosticCheckpointsFn {
        &self.fp
    }
}
