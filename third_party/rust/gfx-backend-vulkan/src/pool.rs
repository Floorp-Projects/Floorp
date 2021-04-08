use crate::{command::CommandBuffer, conv, Backend, RawDevice};
use hal::{command, pool};

use ash::{version::DeviceV1_0, vk};
use inplace_it::inplace_or_alloc_from_iter;

use std::sync::Arc;

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

        assert_eq!(Ok(()), self.device.raw.reset_command_pool(self.raw, flags));
    }

    unsafe fn allocate<E>(&mut self, num: usize, level: command::Level, list: &mut E)
    where
        E: Extend<CommandBuffer>,
    {
        let info = vk::CommandBufferAllocateInfo::builder()
            .command_pool(self.raw)
            .level(conv::map_command_buffer_level(level))
            .command_buffer_count(num as u32);

        let device = &self.device;

        list.extend(
            device
                .raw
                .allocate_command_buffers(&info)
                .expect("Error on command buffer allocation")
                .into_iter()
                .map(|buffer| CommandBuffer {
                    raw: buffer,
                    device: Arc::clone(device),
                }),
        );
    }

    unsafe fn free<I>(&mut self, command_buffers: I)
    where
        I: Iterator<Item = CommandBuffer>,
    {
        let cbufs_iter = command_buffers.map(|buffer| buffer.raw);
        inplace_or_alloc_from_iter(cbufs_iter, |cbufs| {
            if !cbufs.is_empty() {
                self.device.raw.free_command_buffers(self.raw, cbufs);
            }
        })
    }
}
