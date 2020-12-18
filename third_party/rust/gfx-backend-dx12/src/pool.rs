use std::{fmt, sync::Arc};

use parking_lot::Mutex;
use winapi::shared::winerror;

use crate::{command::CommandBuffer, Backend, Shared};
use hal::{command, pool};

pub struct PoolShared {
    device: native::Device,
    list_type: native::CmdListType,
    allocators: Mutex<Vec<native::CommandAllocator>>,
    lists: Mutex<Vec<native::GraphicsCommandList>>,
}

impl fmt::Debug for PoolShared {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        // TODO: print out as struct
        fmt.write_str("PoolShared")
    }
}

impl PoolShared {
    pub fn acquire(&self) -> (native::CommandAllocator, native::GraphicsCommandList) {
        let allocator = match self.allocators.lock().pop() {
            Some(allocator) => allocator,
            None => {
                let (allocator, hr) = self.device.create_command_allocator(self.list_type);
                assert_eq!(
                    winerror::S_OK,
                    hr,
                    "error on command allocator creation: {:x}",
                    hr
                );
                allocator
            }
        };
        let list = match self.lists.lock().pop() {
            Some(list) => {
                list.reset(allocator, native::PipelineState::null());
                list
            }
            None => {
                let (command_list, hr) = self.device.create_graphics_command_list(
                    self.list_type,
                    allocator,
                    native::PipelineState::null(),
                    0,
                );
                assert_eq!(
                    hr,
                    winerror::S_OK,
                    "error on command list creation: {:x}",
                    hr
                );
                command_list
            }
        };
        (allocator, list)
    }

    pub fn release_allocator(&self, allocator: native::CommandAllocator) {
        self.allocators.lock().push(allocator);
    }

    pub fn release_list(&self, list: native::GraphicsCommandList) {
        //pre-condition: list must be closed
        self.lists.lock().push(list);
    }
}

#[derive(Debug)]
pub struct CommandPool {
    shared: Arc<Shared>,
    pool_shared: Arc<PoolShared>,
}

unsafe impl Send for CommandPool {}
unsafe impl Sync for CommandPool {}

impl CommandPool {
    pub(crate) fn new(
        device: native::Device,
        list_type: native::CmdListType,
        shared: &Arc<Shared>,
        _create_flags: pool::CommandPoolCreateFlags,
    ) -> Self {
        let pool_shared = Arc::new(PoolShared {
            device,
            list_type,
            allocators: Mutex::default(),
            lists: Mutex::default(),
        });
        CommandPool {
            shared: Arc::clone(shared),
            pool_shared,
        }
    }
}

impl pool::CommandPool<Backend> for CommandPool {
    unsafe fn reset(&mut self, _release_resources: bool) {
        //do nothing. The allocated command buffers would not know
        // that this happened, but they should be ready to
        // process `begin` as if they are in `Initial` state.
    }

    unsafe fn allocate_one(&mut self, level: command::Level) -> CommandBuffer {
        // TODO: Implement secondary buffers
        assert_eq!(level, command::Level::Primary);
        CommandBuffer::new(&self.shared, &self.pool_shared)
    }

    unsafe fn free<I>(&mut self, cbufs: I)
    where
        I: IntoIterator<Item = CommandBuffer>,
    {
        let mut allocators = self.pool_shared.allocators.lock();
        let mut lists = self.pool_shared.lists.lock();
        for cbuf in cbufs {
            let (allocator, list) = cbuf.destroy();
            allocators.extend(allocator);
            lists.extend(list);
        }
    }
}
