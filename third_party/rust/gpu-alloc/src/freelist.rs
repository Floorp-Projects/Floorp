use {
    crate::{
        align_down, align_up,
        error::AllocationError,
        heap::Heap,
        util::{arc_unwrap, is_arc_unique},
        MemoryBounds,
    },
    alloc::sync::Arc,
    core::{cmp::Ordering, ptr::NonNull},
    gpu_alloc_types::{AllocationFlags, DeviceMapError, MemoryDevice, MemoryPropertyFlags},
};

macro_rules! try_continue {
    ($e:expr) => {
        if let Some(v) = $e {
            v
        } else {
            continue;
        }
    };
}

#[derive(Debug)]
pub struct FreeListBlock<M> {
    pub memory: Arc<M>,
    pub ptr: Option<NonNull<u8>>,
    pub offset: u64,
    pub size: u64,
    pub chunk: u64,
}

unsafe impl<M> Sync for FreeListBlock<M> where M: Sync {}
unsafe impl<M> Send for FreeListBlock<M> where M: Send {}

impl<M> FreeListBlock<M> {
    fn cmp(&self, other: &Self) -> Ordering {
        debug_assert_eq!(
            Arc::ptr_eq(&self.memory, &other.memory),
            self.chunk == other.chunk
        );

        Ord::cmp(&self.chunk, &other.chunk).then(Ord::cmp(&self.offset, &other.offset))
    }
}

#[derive(Debug)]
pub(crate) struct FreeListAllocator<M> {
    freelist: Vec<FreeListBlock<M>>,
    total_free: u64,
    dealloc_threshold: u64,
    chunk_size: u64,
    memory_type: u32,
    props: MemoryPropertyFlags,
    atom_mask: u64,
    counter: u64,
}

impl<M> FreeListAllocator<M>
where
    M: MemoryBounds + 'static,
{
    pub fn new(
        chunk_size: u64,
        dealloc_threshold: u64,
        memory_type: u32,
        props: MemoryPropertyFlags,
        atom_mask: u64,
    ) -> Self {
        debug_assert_eq!(align_down(chunk_size, atom_mask), chunk_size);

        let chunk_size = min(chunk_size, isize::max_value());

        FreeListAllocator {
            freelist: Vec::new(),
            total_free: 0,
            chunk_size,
            dealloc_threshold,
            memory_type,
            props,
            atom_mask,
            counter: 0,
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
    ) -> Result<FreeListBlock<M>, AllocationError> {
        debug_assert!(
            self.chunk_size >= size,
            "GpuAllocator must not request allocations equal or greater to chunks size"
        );

        let size = align_up(size, self.atom_mask).expect(
            "Any value not greater than chunk size (which is aligned) has to fit for alignment",
        );

        let align_mask = align_mask | self.atom_mask;
        let host_visible = self.host_visible();

        match &mut *self.freelist {
            [] => {}
            [rest @ .., last] => {
                // Iterate in this order - last and then rest from begin to end.

                let iter = std::iter::once((rest.len(), last)).chain(rest.iter_mut().enumerate());

                for (index, region) in iter {
                    let offset = try_continue!(align_up(region.offset, align_mask));
                    let next_offset = try_continue!(offset.checked_add(size));

                    match Ord::cmp(&next_offset, &(offset + region.size)) {
                        Ordering::Equal => {
                            let region = self.freelist.remove(index);
                            // dbg!(&region, &self.freelist);

                            self.total_free -= size;
                            return Ok(region);
                        }
                        Ordering::Less => {
                            let block = FreeListBlock {
                                chunk: region.chunk,
                                memory: region.memory.clone(),
                                offset: region.offset,
                                size,
                                ptr: region.ptr,
                            };
                            region.offset = next_offset;
                            region.size -= size;
                            ptr_advance(&mut region.ptr, size);

                            // dbg!(&block, &self.freelist);
                            self.total_free -= size;
                            return Ok(block);
                        }
                        _ => {}
                    }
                }
            }
        }

        if *allocations_remains == 0 {
            dbg!(&self.freelist, size, align_mask);
            return Err(AllocationError::TooManyObjects);
        }

        let mut memory = device.allocate_memory(self.chunk_size, self.memory_type, flags)?;

        *allocations_remains -= 1;
        heap.alloc(self.chunk_size);

        let ptr = if host_visible {
            match device.map_memory(&mut memory, 0, self.chunk_size) {
                Ok(ptr) => Some(ptr),
                Err(DeviceMapError::MapFailed) => {
                    #[cfg(feature = "tracing")]
                    tracing::error!("Failed to map host-visible memory in linear allocator");
                    device.deallocate_memory(memory);
                    *allocations_remains += 1;
                    heap.dealloc(self.chunk_size);

                    return Err(AllocationError::OutOfHostMemory);
                }
                Err(DeviceMapError::OutOfDeviceMemory) => {
                    return Err(AllocationError::OutOfDeviceMemory);
                }
                Err(DeviceMapError::OutOfHostMemory) => {
                    return Err(AllocationError::OutOfHostMemory);
                }
            }
        } else {
            None
        };

        let memory = Arc::new(memory);

        let mut region = FreeListBlock {
            chunk: self.counter,
            ptr,
            memory,
            offset: 0,
            size: self.chunk_size,
        };
        self.counter += 1;

        if size < region.size {
            let block = FreeListBlock {
                chunk: region.chunk,
                memory: region.memory.clone(),
                offset: region.offset,
                size,
                ptr: region.ptr,
            };

            region.offset = size;
            region.size -= size;
            ptr_advance(&mut region.ptr, size);

            self.total_free += region.size;
            self.freelist.push(region);

            // dbg!(&block, &self.freelist);
            self.total_free -= size;
            Ok(block)
        } else {
            debug_assert_eq!(size, region.size);
            // dbg!(&region, &self.freelist);
            Ok(region)
        }
    }

    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn dealloc(
        &mut self,
        device: &impl MemoryDevice<M>,
        block: FreeListBlock<M>,
        heap: &mut Heap,
        allocations_remains: &mut u32,
    ) {
        debug_assert!(block.size < self.chunk_size);
        debug_assert_ne!(block.size, 0);
        let block_size = block.size;

        // dbg!(&block, &self.freelist);

        match self.freelist.binary_search_by(|b| b.cmp(&block)) {
            Ok(index) => {
                debug_assert_ne!(self.freelist[index].size, 0);
                panic!("Overlapping block found in free list: {:?}", self.freelist);
            }
            Err(index) => match &mut self.freelist[..=index] {
                [] => self.freelist.push(block),
                [next] => {
                    if next.chunk == block.chunk {
                        debug_assert!(Arc::ptr_eq(&next.memory, &block.memory));

                        assert!(
                            block.offset + block.size <= next.offset,
                            "Overlapping block found in free list. {:?}",
                            self.freelist
                        );

                        if block.offset + block.size == next.offset {
                            next.offset = block.offset;
                            next.size += block.size;
                            drop(block);
                            // dbg!(&self.freelist);
                            return;
                        }
                    }
                    self.freelist.insert(0, block);
                    // dbg!(&self.freelist);
                }
                [.., prev, next] => {
                    if next.chunk == block.chunk {
                        debug_assert!(Arc::ptr_eq(&next.memory, &block.memory));

                        assert!(
                            block.offset + block.size <= next.offset,
                            "Overlapping block found in free list. {:?}",
                            self.freelist
                        );

                        if block.offset + block.size == next.offset {
                            next.offset = block.offset;
                            next.size += block.size;
                            drop(block);

                            if prev.chunk == next.chunk && next.offset == prev.offset + prev.size {
                                assert!(
                                    prev.offset + prev.size <= next.offset,
                                    "Overlapping block found in free list. {:?}",
                                    self.freelist
                                );

                                prev.size += next.size;
                                self.freelist.remove(index);
                            }
                            // dbg!(&self.freelist);
                            return;
                        }
                    }
                    if prev.chunk == block.chunk {
                        debug_assert!(Arc::ptr_eq(&prev.memory, &block.memory));

                        assert!(
                            prev.offset + prev.size <= block.offset,
                            "Overlapping block found in free list. {:?}",
                            self.freelist
                        );

                        if prev.offset + prev.size == block.offset {
                            prev.size += block.size;
                            drop(block);
                            // dbg!(&self.freelist);
                            return;
                        }
                    }

                    self.freelist.insert(0, block);
                    // dbg!(&self.freelist);
                }
            },
        }

        self.total_free += block_size;
        if self.total_free >= self.dealloc_threshold {
            // Code adapted from `Vec::retain`
            let len = self.freelist.len();
            let mut del = 0;
            {
                let blocks = &mut *self.freelist;

                for i in 0..len {
                    if self.total_free >= self.dealloc_threshold
                        && is_arc_unique(&mut blocks[i].memory)
                    {
                        del += 1;
                    } else if del > 0 {
                        blocks.swap(i - del, i);
                    }
                }
            }
            if del > 0 {
                for block in self.freelist.drain(len - del..) {
                    let memory = arc_unwrap(block.memory);
                    device.deallocate_memory(memory);
                }
            }
        }
    }

    /// Deallocates leftover memory objects.
    /// Should be used before dropping.
    ///
    /// # Safety
    ///
    /// * `device` must be one with `DeviceProperties` that were provided to create this `GpuAllocator` instance
    /// * Same `device` instance must be used for all interactions with one `GpuAllocator` instance
    ///   and memory blocks allocated from it
    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn cleanup(&mut self, device: &impl MemoryDevice<M>) {
        // Code adapted from `Vec::retain`
        let len = self.freelist.len();
        let mut del = 0;
        {
            let blocks = &mut *self.freelist;

            for i in 0..len {
                if is_arc_unique(&mut blocks[i].memory) {
                    del += 1;
                } else if del > 0 {
                    blocks.swap(i - del, i);
                }
            }
        }
        if del > 0 {
            for block in self.freelist.drain(len - del..) {
                let memory = arc_unwrap(block.memory);
                device.deallocate_memory(memory);
            }
        }
    }

    fn host_visible(&self) -> bool {
        self.props.contains(MemoryPropertyFlags::HOST_VISIBLE)
    }
}

fn min<L, R>(l: L, r: R) -> L
where
    R: core::convert::TryInto<L>,
    L: Ord,
{
    match r.try_into() {
        Ok(r) => core::cmp::min(l, r),
        Err(_) => l,
    }
}

unsafe fn ptr_advance(ptr: &mut Option<NonNull<u8>>, size: u64) {
    if let Some(ptr) = ptr {
        *ptr = {
            // Size is within memory region started at `ptr`.
            // size is within `chunk_size` that fits `isize`.
            NonNull::new_unchecked(ptr.as_ptr().offset(size as isize))
        };
    }
}
