use std::{fmt, sync::Arc};

use winapi::shared::winerror::SUCCEEDED;

use crate::{command::CommandBuffer, Backend, Shared};
use hal::{command, pool};

#[derive(Debug)]
pub enum CommandPoolAllocator {
    Shared(native::CommandAllocator),
    Individual(Vec<native::CommandAllocator>),
}

pub struct CommandPool {
    pub(crate) allocator: CommandPoolAllocator,
    pub(crate) device: native::Device,
    pub(crate) list_type: native::CmdListType,
    pub(crate) shared: Arc<Shared>,
    pub(crate) create_flags: pool::CommandPoolCreateFlags,
}

impl fmt::Debug for CommandPool {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("CommandPool")
    }
}

impl CommandPool {
    fn create_command_list(&mut self) -> (native::GraphicsCommandList, native::CommandAllocator) {
        let command_allocator = match self.allocator {
            CommandPoolAllocator::Shared(ref allocator) => allocator.clone(),
            CommandPoolAllocator::Individual(ref mut allocators) => {
                let (command_allocator, hr) = self.device.create_command_allocator(self.list_type);

                // TODO: error handling
                if !SUCCEEDED(hr) {
                    error!("error on command allocator creation: {:x}", hr);
                }

                allocators.push(command_allocator);
                command_allocator
            }
        };

        // allocate command lists
        let (command_list, hr) = self.device.create_graphics_command_list(
            self.list_type,
            command_allocator,
            native::PipelineState::null(),
            0,
        );

        if !SUCCEEDED(hr) {
            error!("error on command list creation: {:x}", hr);
        }

        // Close command list as they are initiated as recording.
        // But only one command list can be recording for each allocator
        let _hr = command_list.close();

        (command_list, command_allocator)
    }

    pub(crate) fn destroy(self) {
        match self.allocator {
            CommandPoolAllocator::Shared(ref allocator) => unsafe {
                allocator.destroy();
            },
            CommandPoolAllocator::Individual(ref allocators) => {
                for allocator in allocators.iter() {
                    unsafe {
                        allocator.destroy();
                    }
                }
            }
        }
    }
}

unsafe impl Send for CommandPool {}
unsafe impl Sync for CommandPool {}

impl pool::CommandPool<Backend> for CommandPool {
    unsafe fn reset(&mut self, _release_resources: bool) {
        match self.allocator {
            CommandPoolAllocator::Shared(ref allocator) => {
                allocator.Reset();
            }
            CommandPoolAllocator::Individual(ref mut allocators) => {
                for allocator in allocators.iter_mut() {
                    allocator.Reset();
                }
            }
        }
    }

    unsafe fn allocate_one(&mut self, level: command::Level) -> CommandBuffer {
        // TODO: Implement secondary buffers
        assert_eq!(level, command::Level::Primary);
        let (command_list, command_allocator) = self.create_command_list();
        CommandBuffer::new(
            command_list,
            command_allocator,
            self.shared.clone(),
            self.create_flags,
        )
    }

    unsafe fn free<I>(&mut self, cbufs: I)
    where
        I: IntoIterator<Item = CommandBuffer>,
    {
        for mut cbuf in cbufs {
            cbuf.destroy();
        }
    }
}
