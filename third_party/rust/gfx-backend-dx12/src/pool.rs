use std::{fmt, sync::Arc};

use parking_lot::Mutex;
use winapi::shared::winerror;

use crate::{command::CommandBuffer, Backend, Shared};
use hal::{command, pool};

const REUSE_COUNT: usize = 64;

pub type CommandAllocatorIndex = thunderdome::Index;

struct AllocatorEntry {
    raw: native::CommandAllocator,
    active_lists: usize,
    total_lists: usize,
}

#[derive(Default)]
struct CommandManager {
    allocators: thunderdome::Arena<AllocatorEntry>,
    free_allocators: Vec<CommandAllocatorIndex>,
    lists: Vec<native::GraphicsCommandList>,
}

impl CommandManager {
    fn acquire(&mut self, index: CommandAllocatorIndex) -> native::CommandAllocator {
        let entry = &mut self.allocators[index];
        assert!(entry.total_lists < REUSE_COUNT);
        entry.active_lists += 1;
        entry.total_lists += 1;
        return entry.raw;
    }

    fn release_allocator(&mut self, index: CommandAllocatorIndex) {
        let entry = &mut self.allocators[index];
        if entry.total_lists >= REUSE_COUNT {
            debug!("Cooling command allocator {:?}", index);
        } else {
            self.free_allocators.push(index);
        }
    }

    fn release_list(&mut self, list: native::GraphicsCommandList, index: CommandAllocatorIndex) {
        //pre-condition: list must be closed
        let entry = &mut self.allocators[index];
        entry.active_lists -= 1;
        if entry.active_lists == 0 && entry.total_lists >= REUSE_COUNT {
            debug!("Re-warming command allocator {:?}", index);
            entry.raw.reset();
            entry.total_lists = 0;
            self.free_allocators.push(index);
        }
        self.lists.push(list);
    }
}

pub struct PoolShared {
    device: native::Device,
    list_type: native::CmdListType,
    manager: Mutex<CommandManager>,
}

impl fmt::Debug for PoolShared {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        // TODO: print out as struct
        fmt.write_str("PoolShared")
    }
}

impl PoolShared {
    pub fn acquire(&self) -> (CommandAllocatorIndex, native::GraphicsCommandList) {
        let mut man_guard = self.manager.lock();
        let allocator_index = match man_guard.free_allocators.pop() {
            Some(index) => index,
            None => {
                let (raw, hr) = self.device.create_command_allocator(self.list_type);
                assert_eq!(
                    winerror::S_OK,
                    hr,
                    "error on command allocator creation: {:x}",
                    hr
                );
                man_guard.allocators.insert(AllocatorEntry {
                    raw,
                    active_lists: 0,
                    total_lists: 0,
                })
            }
        };
        let raw = man_guard.acquire(allocator_index);

        let list = match man_guard.lists.pop() {
            Some(list) => {
                list.reset(raw, native::PipelineState::null());
                list
            }
            None => {
                let (command_list, hr) = self.device.create_graphics_command_list(
                    self.list_type,
                    raw,
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
        (allocator_index, list)
    }

    pub fn release_allocator(&self, allocator_index: CommandAllocatorIndex) {
        self.manager.lock().release_allocator(allocator_index);
    }

    pub fn release_list(
        &self,
        list: native::GraphicsCommandList,
        allocator_index: CommandAllocatorIndex,
    ) {
        self.manager.lock().release_list(list, allocator_index);
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
            manager: Mutex::default(),
        });
        CommandPool {
            shared: Arc::clone(shared),
            pool_shared,
        }
    }
}

impl pool::CommandPool<Backend> for CommandPool {
    unsafe fn reset(&mut self, _release_resources: bool) {
        //TODO: reset all the allocators?
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
        I: Iterator<Item = CommandBuffer>,
    {
        let mut man_guard = self.pool_shared.manager.lock();
        for cbuf in cbufs {
            if let Some((index, list)) = cbuf.destroy() {
                man_guard.release_allocator(index);
                if let Some(list) = list {
                    man_guard.release_list(list, index);
                }
            }
        }
    }
}
