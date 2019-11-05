use ash::version::DeviceV1_0;
use ash::vk;
use smallvec::SmallVec;
use std::ptr;
use std::sync::Arc;

use crate::command::CommandBuffer;
use crate::conv;
use crate::{Backend, RawDevice};
use hal::{command, pool};

#[derive(Debug)]
pub struct RawCommandPool {
    pub(crate) raw: vk::CommandPool,
    pub(crate) device: Arc<RawDevice>,
}

impl pool::CommandPool<Backend> for RawCommandPool {
    unsafe fn reset(&mut self, release_resources: bool) {
        let flags = if release_resources {
            vk::CommandPoolResetFlags::RELEASE_RESOURCES
        } else {
            vk::CommandPoolResetFlags::empty()
        };

        assert_eq!(Ok(()), self.device.0.reset_command_pool(self.raw, flags));
    }

    unsafe fn allocate_vec(&mut self, num: usize, level: command::Level) -> SmallVec<[CommandBuffer; 1]> {
        let info = vk::CommandBufferAllocateInfo {
            s_type: vk::StructureType::COMMAND_BUFFER_ALLOCATE_INFO,
            p_next: ptr::null(),
            command_pool: self.raw,
            level: conv::map_command_buffer_level(level),
            command_buffer_count: num as u32,
        };

        let device = &self.device;
        let cbufs_raw = device.0
            .allocate_command_buffers(&info)
            .expect("Error on command buffer allocation");

        cbufs_raw
            .into_iter()
            .map(|buffer| CommandBuffer {
                raw: buffer,
                device: device.clone(),
            })
            .collect()
    }

    unsafe fn free<I>(&mut self, cbufs: I)
    where
        I: IntoIterator<Item = CommandBuffer>,
    {
        let buffers: SmallVec<[vk::CommandBuffer; 16]> =
            cbufs.into_iter().map(|buffer| buffer.raw).collect();
        self.device.0.free_command_buffers(self.raw, &buffers);
    }
}
