#![allow(dead_code)]
use crate::version::{DeviceV1_0, InstanceV1_0};
use crate::vk;
use std::ffi::CStr;
use std::mem;
use std::os::raw::c_void;

#[derive(Clone)]
pub struct DeviceDiagnosticCheckpoints {
    device_diagnostic_checkpoints_fn: vk::NvDeviceDiagnosticCheckpointsFn,
}

impl DeviceDiagnosticCheckpoints {
    pub fn new<I: InstanceV1_0, D: DeviceV1_0>(
        instance: &I,
        device: &D,
    ) -> DeviceDiagnosticCheckpoints {
        let device_diagnostic_checkpoints_fn =
            vk::NvDeviceDiagnosticCheckpointsFn::load(|name| unsafe {
                mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
            });
        DeviceDiagnosticCheckpoints {
            device_diagnostic_checkpoints_fn,
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetCheckpointNV.html>"]
    pub unsafe fn cmd_set_checkpoint(
        &self,
        command_buffer: vk::CommandBuffer,
        p_checkpoint_marker: *const c_void,
    ) {
        self.device_diagnostic_checkpoints_fn
            .cmd_set_checkpoint_nv(command_buffer, p_checkpoint_marker);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetQueueCheckpointDataNV.html>"]
    pub unsafe fn get_queue_checkpoint_data(&self, queue: vk::Queue) -> Vec<vk::CheckpointDataNV> {
        let mut checkpoint_data_count: u32 = 0;
        self.device_diagnostic_checkpoints_fn
            .get_queue_checkpoint_data_nv(queue, &mut checkpoint_data_count, std::ptr::null_mut());
        let mut checkpoint_data: Vec<vk::CheckpointDataNV> =
            vec![vk::CheckpointDataNV::default(); checkpoint_data_count as _];
        self.device_diagnostic_checkpoints_fn
            .get_queue_checkpoint_data_nv(
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
        &self.device_diagnostic_checkpoints_fn
    }
}
