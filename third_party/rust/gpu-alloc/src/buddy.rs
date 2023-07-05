use {
    crate::{
        align_up, error::AllocationError, heap::Heap, slab::Slab, unreachable_unchecked,
        util::try_arc_unwrap, MemoryBounds,
    },
    alloc::{sync::Arc, vec::Vec},
    core::{convert::TryFrom as _, mem::replace, ptr::NonNull},
    gpu_alloc_types::{AllocationFlags, DeviceMapError, MemoryDevice, MemoryPropertyFlags},
};

#[derive(Debug)]
pub(crate) struct BuddyBlock<M> {
    pub memory: Arc<M>,
    pub ptr: Option<NonNull<u8>>,
    pub offset: u64,
    pub size: u64,
    pub chunk: usize,
    pub index: usize,
}

unsafe impl<M> Sync for BuddyBlock<M> where M: Sync {}
unsafe impl<M> Send for BuddyBlock<M> where M: Send {}

#[derive(Clone, Copy, Debug)]
enum PairState {
    Exhausted,
    Ready {
        ready: Side,
        next: usize,
        prev: usize,
    },
}

impl PairState {
    unsafe fn replace_next(&mut self, value: usize) -> usize {
        match self {
            PairState::Exhausted => unreachable_unchecked(),
            PairState::Ready { next, .. } => replace(next, value),
        }
    }

    unsafe fn replace_prev(&mut self, value: usize) -> usize {
        match self {
            PairState::Exhausted => unreachable_unchecked(),
            PairState::Ready { prev, .. } => replace(prev, value),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Side {
    Left,
    Right,
}
use Side::*;

#[derive(Debug)]
struct PairEntry {
    state: PairState,
    chunk: usize,
    offset: u64,
    parent: Option<usize>,
}

struct SizeBlockEntry {
    chunk: usize,
    offset: u64,
    index: usize,
}

#[derive(Debug)]
struct Size {
    next_ready: usize,
    pairs: Slab<PairEntry>,
}
#[derive(Debug)]
enum Release {
    None,
    Parent(usize),
    Chunk(usize),
}

impl Size {
    fn new() -> Self {
        Size {
            pairs: Slab::new(),
            next_ready: 0,
        }
    }

    unsafe fn add_pair_and_acquire_left(
        &mut self,
        chunk: usize,
        offset: u64,
        parent: Option<usize>,
    ) -> SizeBlockEntry {
        if self.next_ready < self.pairs.len() {
            unreachable_unchecked()
        }

        let index = self.pairs.insert(PairEntry {
            state: PairState::Exhausted,
            chunk,
            offset,
            parent,
        });

        let entry = self.pairs.get_unchecked_mut(index);
        entry.state = PairState::Ready {
            next: index,
            prev: index,
            ready: Right, // Left is allocated.
        };
        self.next_ready = index;

        SizeBlockEntry {
            chunk,
            offset,
            index: index << 1,
        }
    }

    fn acquire(&mut self, size: u64) -> Option<SizeBlockEntry> {
        if self.next_ready >= self.pairs.len() {
            return None;
        }

        let ready = self.next_ready;

        let entry = unsafe { self.pairs.get_unchecked_mut(ready) };
        let chunk = entry.chunk;
        let offset = entry.offset;

        let bit = match entry.state {
            PairState::Exhausted => unsafe { unreachable_unchecked() },
            PairState::Ready { ready, next, prev } => {
                entry.state = PairState::Exhausted;

                if prev == self.next_ready {
                    // The only ready entry.
                    debug_assert_eq!(next, self.next_ready);
                    self.next_ready = self.pairs.len();
                } else {
                    let prev_entry = unsafe { self.pairs.get_unchecked_mut(prev) };
                    let prev_next = unsafe { prev_entry.state.replace_next(next) };
                    debug_assert_eq!(prev_next, self.next_ready);

                    let next_entry = unsafe { self.pairs.get_unchecked_mut(next) };
                    let next_prev = unsafe { next_entry.state.replace_prev(prev) };
                    debug_assert_eq!(next_prev, self.next_ready);

                    self.next_ready = next;
                }

                match ready {
                    Left => 0,
                    Right => 1,
                }
            }
        };

        Some(SizeBlockEntry {
            chunk,
            offset: offset + bit as u64 * size,
            index: (ready << 1) | bit as usize,
        })
    }

    fn release(&mut self, index: usize) -> Release {
        let side = match index & 1 {
            0 => Side::Left,
            1 => Side::Right,
            _ => unsafe { unreachable_unchecked() },
        };
        let entry_index = index >> 1;

        let len = self.pairs.len();

        let entry = self.pairs.get_mut(entry_index);

        let chunk = entry.chunk;
        let offset = entry.offset;
        let parent = entry.parent;

        match entry.state {
            PairState::Exhausted => {
                if self.next_ready == len {
                    entry.state = PairState::Ready {
                        ready: side,
                        next: entry_index,
                        prev: entry_index,
                    };
                    self.next_ready = entry_index;
                } else {
                    debug_assert!(self.next_ready < len);

                    let next = self.next_ready;
                    let next_entry = unsafe { self.pairs.get_unchecked_mut(next) };
                    let prev = unsafe { next_entry.state.replace_prev(entry_index) };

                    let prev_entry = unsafe { self.pairs.get_unchecked_mut(prev) };
                    let prev_next = unsafe { prev_entry.state.replace_next(entry_index) };
                    debug_assert_eq!(prev_next, next);

                    let entry = unsafe { self.pairs.get_unchecked_mut(entry_index) };
                    entry.state = PairState::Ready {
                        ready: side,
                        next,
                        prev,
                    };
                }
                Release::None
            }

            PairState::Ready { ready, .. } if ready == side => {
                panic!("Attempt to dealloate already free block")
            }

            PairState::Ready { next, prev, .. } => {
                unsafe {
                    self.pairs.remove_unchecked(entry_index);
                }

                if prev == entry_index {
                    debug_assert_eq!(next, entry_index);
                    self.next_ready = self.pairs.len();
                } else {
                    let prev_entry = unsafe { self.pairs.get_unchecked_mut(prev) };
                    let prev_next = unsafe { prev_entry.state.replace_next(next) };
                    debug_assert_eq!(prev_next, entry_index);

                    let next_entry = unsafe { self.pairs.get_unchecked_mut(next) };
                    let next_prev = unsafe { next_entry.state.replace_prev(prev) };
                    debug_assert_eq!(next_prev, entry_index);

                    self.next_ready = next;
                }

                match parent {
                    Some(parent) => Release::Parent(parent),
                    None => {
                        debug_assert_eq!(offset, 0);
                        Release::Chunk(chunk)
                    }
                }
            }
        }
    }
}

#[derive(Debug)]
struct Chunk<M> {
    memory: Arc<M>,
    ptr: Option<NonNull<u8>>,
    size: u64,
}

#[derive(Debug)]
pub(crate) struct BuddyAllocator<M> {
    minimal_size: u64,
    chunks: Slab<Chunk<M>>,
    sizes: Vec<Size>,
    memory_type: u32,
    props: MemoryPropertyFlags,
    atom_mask: u64,
}

unsafe impl<M> Sync for BuddyAllocator<M> where M: Sync {}
unsafe impl<M> Send for BuddyAllocator<M> where M: Send {}

impl<M> BuddyAllocator<M>
where
    M: MemoryBounds + 'static,
{
    pub fn new(
        minimal_size: u64,
        initial_dedicated_size: u64,
        memory_type: u32,
        props: MemoryPropertyFlags,
        atom_mask: u64,
    ) -> Self {
        assert!(
            minimal_size.is_power_of_two(),
            "Minimal allocation size of buddy allocator must be power of two"
        );
        assert!(
            initial_dedicated_size.is_power_of_two(),
            "Dedicated allocation size of buddy allocator must be power of two"
        );

        let initial_sizes = (initial_dedicated_size
            .trailing_zeros()
            .saturating_sub(minimal_size.trailing_zeros())) as usize;

        BuddyAllocator {
            minimal_size,
            chunks: Slab::new(),
            sizes: (0..initial_sizes).map(|_| Size::new()).collect(),
            memory_type,
            props,
            atom_mask: atom_mask | (minimal_size - 1),
        }
    }

    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn alloc(
        &mut self,
        device: &impl MemoryDevice<M>,
        size: u64,
        align_mask: u64,
        flags: AllocationFlags,
        heap: &mut Heap,
        allocations_remains: &mut u32,
    ) -> Result<BuddyBlock<M>, AllocationError> {
        let align_mask = align_mask | self.atom_mask;

        let size = align_up(size, align_mask)
            .and_then(|size| size.checked_next_power_of_two())
            .ok_or(AllocationError::OutOfDeviceMemory)?;

        let size = size.max(self.minimal_size);

        let size_index = size.trailing_zeros() - self.minimal_size.trailing_zeros();
        let size_index =
            usize::try_from(size_index).map_err(|_| AllocationError::OutOfDeviceMemory)?;

        while self.sizes.len() <= size_index {
            self.sizes.push(Size::new());
        }

        let host_visible = self.host_visible();

        let mut candidate_size_index = size_index;

        let (mut entry, entry_size_index) = loop {
            let sizes_len = self.sizes.len();

            let candidate_size_entry = &mut self.sizes[candidate_size_index];
            let candidate_size = self.minimal_size << candidate_size_index;

            if let Some(entry) = candidate_size_entry.acquire(candidate_size) {
                break (entry, candidate_size_index);
            }

            if sizes_len == candidate_size_index + 1 {
                // That's size of device allocation.
                if *allocations_remains == 0 {
                    return Err(AllocationError::TooManyObjects);
                }

                let chunk_size = self.minimal_size << (candidate_size_index + 1);
                let mut memory = device.allocate_memory(chunk_size, self.memory_type, flags)?;
                *allocations_remains -= 1;
                heap.alloc(chunk_size);

                let ptr = if host_visible {
                    match device.map_memory(&mut memory, 0, chunk_size) {
                        Ok(ptr) => Some(ptr),
                        Err(DeviceMapError::OutOfDeviceMemory) => {
                            return Err(AllocationError::OutOfDeviceMemory)
                        }
                        Err(DeviceMapError::MapFailed) | Err(DeviceMapError::OutOfHostMemory) => {
                            return Err(AllocationError::OutOfHostMemory)
                        }
                    }
                } else {
                    None
                };

                let chunk = self.chunks.insert(Chunk {
                    memory: Arc::new(memory),
                    ptr,
                    size: chunk_size,
                });

                let entry = candidate_size_entry.add_pair_and_acquire_left(chunk, 0, None);

                break (entry, candidate_size_index);
            }

            candidate_size_index += 1;
        };

        for size_index in (size_index..entry_size_index).rev() {
            let size_entry = &mut self.sizes[size_index];
            entry =
                size_entry.add_pair_and_acquire_left(entry.chunk, entry.offset, Some(entry.index));
        }

        let chunk_entry = self.chunks.get_unchecked(entry.chunk);

        debug_assert!(
            entry
                .offset
                .checked_add(size)
                .map_or(false, |end| end <= chunk_entry.size),
            "Offset + size is not in chunk bounds"
        );

        Ok(BuddyBlock {
            memory: chunk_entry.memory.clone(),
            ptr: chunk_entry
                .ptr
                .map(|ptr| NonNull::new_unchecked(ptr.as_ptr().add(entry.offset as usize))),
            offset: entry.offset,
            size,
            chunk: entry.chunk,
            index: entry.index,
        })
    }

    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn dealloc(
        &mut self,
        device: &impl MemoryDevice<M>,
        block: BuddyBlock<M>,
        heap: &mut Heap,
        allocations_remains: &mut u32,
    ) {
        debug_assert!(block.size.is_power_of_two());

        let size_index =
            (block.size.trailing_zeros() - self.minimal_size.trailing_zeros()) as usize;

        let mut release_index = block.index;
        let mut release_size_index = size_index;

        loop {
            match self.sizes[release_size_index].release(release_index) {
                Release::Parent(parent) => {
                    release_size_index += 1;
                    release_index = parent;
                }
                Release::Chunk(chunk) => {
                    debug_assert_eq!(chunk, block.chunk);
                    debug_assert_eq!(
                        self.chunks.get(chunk).size,
                        self.minimal_size << (release_size_index + 1)
                    );
                    let chunk = self.chunks.remove(chunk);
                    drop(block);

                    let memory = try_arc_unwrap(chunk.memory)
                        .expect("Memory shared after last block deallocated");

                    device.deallocate_memory(memory);
                    *allocations_remains += 1;
                    heap.dealloc(chunk.size);

                    return;
                }
                Release::None => return,
            }
        }
    }

    fn host_visible(&self) -> bool {
        self.props.contains(MemoryPropertyFlags::HOST_VISIBLE)
    }
}
