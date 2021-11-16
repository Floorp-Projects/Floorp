use {
    crate::{
        align_down, align_up,
        error::AllocationError,
        heap::Heap,
        util::{arc_unwrap, is_arc_unique},
        MemoryBounds,
    },
    alloc::{sync::Arc, vec::Vec},
    core::{cmp::Ordering, ptr::NonNull},
    gpu_alloc_types::{AllocationFlags, DeviceMapError, MemoryDevice, MemoryPropertyFlags},
};

unsafe fn opt_ptr_add(ptr: Option<NonNull<u8>>, size: u64) -> Option<NonNull<u8>> {
    ptr.map(|ptr| {
        // Size is within memory region started at `ptr`.
        // size is within `chunk_size` that fits `isize`.
        NonNull::new_unchecked(ptr.as_ptr().offset(size as isize))
    })
}

#[derive(Debug)]
pub(super) struct FreeList<M> {
    array: Vec<FreeListRegion<M>>,
    counter: u64,
}

impl<M> FreeList<M> {
    pub fn new() -> Self {
        FreeList {
            array: Vec::new(),
            counter: 0,
        }
    }

    pub fn get_block_from_new_memory(
        &mut self,
        memory: Arc<M>,
        memory_size: u64,
        ptr: Option<NonNull<u8>>,
        align_mask: u64,
        size: u64,
    ) -> FreeListBlock<M> {
        debug_assert!(size <= memory_size);

        self.counter += 1;
        self.array.push(FreeListRegion {
            memory,
            ptr,
            chunk: self.counter,
            start: 0,
            end: memory_size,
        });
        self.get_block_at(self.array.len() - 1, align_mask, size)
    }

    pub fn get_block(&mut self, align_mask: u64, size: u64) -> Option<FreeListBlock<M>> {
        let (index, _) = self.array.iter().enumerate().rev().find(|(_, region)| {
            match region.end.checked_sub(size) {
                Some(start) => {
                    let aligned_start = align_down(start, align_mask);
                    aligned_start >= region.start
                }
                None => false,
            }
        })?;

        Some(self.get_block_at(index, align_mask, size))
    }

    fn get_block_at(&mut self, index: usize, align_mask: u64, size: u64) -> FreeListBlock<M> {
        let region = &mut self.array[index];

        let start = region.end - size;
        let aligned_start = align_down(start, align_mask);

        if aligned_start > region.start {
            let block = FreeListBlock {
                offset: aligned_start,
                size: region.end - aligned_start,
                chunk: region.chunk,
                ptr: unsafe { opt_ptr_add(region.ptr, aligned_start - region.start) },
                memory: region.memory.clone(),
            };

            region.end = aligned_start;

            block
        } else {
            debug_assert_eq!(aligned_start, region.start);
            let region = self.array.remove(index);
            region.into_block()
        }
    }

    pub fn insert_block(&mut self, block: FreeListBlock<M>) {
        match self.array.binary_search_by(|b| b.cmp(&block)) {
            Ok(_) => {
                panic!("Overlapping block found in free list");
            }
            Err(index) if self.array.len() > index => match &mut self.array[..=index] {
                [] => unreachable!(),
                [next] => {
                    debug_assert!(!next.is_suffix_block(&block));

                    if next.is_prefix_block(&block) {
                        next.merge_prefix_block(block);
                    } else {
                        self.array.insert(0, FreeListRegion::from_block(block));
                    }
                }
                [.., prev, next] => {
                    debug_assert!(!prev.is_prefix_block(&block));
                    debug_assert!(!next.is_suffix_block(&block));

                    if next.is_prefix_block(&block) {
                        next.merge_prefix_block(block);

                        if prev.consecutive(&*next) {
                            let next = self.array.remove(index);
                            let prev = &mut self.array[index - 1];
                            prev.merge(next);
                        }
                    } else if prev.is_suffix_block(&block) {
                        prev.merge_suffix_block(block);
                    } else {
                        self.array.insert(index, FreeListRegion::from_block(block));
                    }
                }
            },
            Err(_) => match &mut self.array[..] {
                [] => self.array.push(FreeListRegion::from_block(block)),
                [.., prev] => {
                    debug_assert!(!prev.is_prefix_block(&block));
                    if prev.is_suffix_block(&block) {
                        prev.merge_suffix_block(block);
                    } else {
                        self.array.push(FreeListRegion::from_block(block));
                    }
                }
            },
        }
    }

    pub fn drain(&mut self, keep_last: bool) -> Option<impl Iterator<Item = (M, u64)> + '_> {
        // Time to deallocate

        let len = self.array.len();

        let mut del = 0;
        {
            let regions = &mut self.array[..];

            for i in 0..len {
                if (i < len - 1 || !keep_last) && is_arc_unique(&mut regions[i].memory) {
                    del += 1;
                } else if del > 0 {
                    regions.swap(i - del, i);
                }
            }
        }

        if del > 0 {
            Some(self.array.drain(len - del..).map(move |region| {
                debug_assert_eq!(region.start, 0);
                (unsafe { arc_unwrap(region.memory) }, region.end)
            }))
        } else {
            None
        }
    }
}

#[derive(Debug)]
struct FreeListRegion<M> {
    memory: Arc<M>,
    ptr: Option<NonNull<u8>>,
    chunk: u64,
    start: u64,
    end: u64,
}

unsafe impl<M> Sync for FreeListRegion<M> where M: Sync {}
unsafe impl<M> Send for FreeListRegion<M> where M: Send {}

impl<M> FreeListRegion<M> {
    pub fn cmp(&self, block: &FreeListBlock<M>) -> Ordering {
        debug_assert_eq!(
            Arc::ptr_eq(&self.memory, &block.memory),
            self.chunk == block.chunk
        );

        if self.chunk == block.chunk {
            debug_assert_eq!(
                Ord::cmp(&self.start, &block.offset),
                Ord::cmp(&self.end, &(block.offset + block.size)),
                "Free region {{ start: {}, end: {} }} overlaps with block {{ offset: {}, size: {} }}",
                self.start,
                self.end,
                block.offset,
                block.size,
            );
        }

        Ord::cmp(&self.chunk, &block.chunk).then(Ord::cmp(&self.start, &block.offset))
    }

    fn from_block(block: FreeListBlock<M>) -> Self {
        FreeListRegion {
            memory: block.memory,
            chunk: block.chunk,
            ptr: block.ptr,
            start: block.offset,
            end: block.offset + block.size,
        }
    }

    fn into_block(self) -> FreeListBlock<M> {
        FreeListBlock {
            memory: self.memory,
            chunk: self.chunk,
            ptr: self.ptr,
            offset: self.start,
            size: self.end - self.start,
        }
    }

    fn consecutive(&self, other: &Self) -> bool {
        if self.chunk != other.chunk {
            return false;
        }

        debug_assert!(Arc::ptr_eq(&self.memory, &other.memory));

        debug_assert_eq!(
            Ord::cmp(&self.start, &other.start),
            Ord::cmp(&self.end, &other.end)
        );

        self.end == other.start
    }

    fn merge(&mut self, next: FreeListRegion<M>) {
        debug_assert!(self.consecutive(&next));
        self.end = next.end;
    }

    fn is_prefix_block(&self, block: &FreeListBlock<M>) -> bool {
        if self.chunk != block.chunk {
            return false;
        }

        debug_assert!(Arc::ptr_eq(&self.memory, &block.memory));

        debug_assert_eq!(
            Ord::cmp(&self.start, &block.offset),
            Ord::cmp(&self.end, &(block.offset + block.size))
        );

        self.start == (block.offset + block.size)
    }

    fn merge_prefix_block(&mut self, block: FreeListBlock<M>) {
        debug_assert!(self.is_prefix_block(&block));
        self.start = block.offset;
        self.ptr = block.ptr;
    }

    fn is_suffix_block(&self, block: &FreeListBlock<M>) -> bool {
        if self.chunk != block.chunk {
            return false;
        }

        debug_assert!(Arc::ptr_eq(&self.memory, &block.memory));

        debug_assert_eq!(
            Ord::cmp(&self.start, &block.offset),
            Ord::cmp(&self.end, &(block.offset + block.size))
        );

        self.end == block.offset
    }

    fn merge_suffix_block(&mut self, block: FreeListBlock<M>) {
        debug_assert!(self.is_suffix_block(&block));
        self.end += block.size;
    }
}

#[derive(Debug)]
pub struct FreeListBlock<M> {
    pub memory: Arc<M>,
    pub ptr: Option<NonNull<u8>>,
    pub chunk: u64,
    pub offset: u64,
    pub size: u64,
}

unsafe impl<M> Sync for FreeListBlock<M> where M: Sync {}
unsafe impl<M> Send for FreeListBlock<M> where M: Send {}

#[derive(Debug)]
pub(crate) struct FreeListAllocator<M> {
    freelist: FreeList<M>,
    chunk_size: u64,
    final_chunk_size: u64,
    memory_type: u32,
    props: MemoryPropertyFlags,
    atom_mask: u64,

    total_allocations: u64,
    total_deallocations: u64,
}

impl<M> Drop for FreeListAllocator<M> {
    fn drop(&mut self) {
        match Ord::cmp(&self.total_allocations, &self.total_deallocations) {
            Ordering::Equal => {}
            Ordering::Greater => {
                report_error_on_drop!("Not all blocks were deallocated")
            }
            Ordering::Less => {
                report_error_on_drop!("More blocks deallocated than allocated")
            }
        }

        if !self.freelist.array.is_empty() {
            report_error_on_drop!(
                "FreeListAllocator has free blocks on drop. Allocator should be cleaned"
            );
        }
    }
}

impl<M> FreeListAllocator<M>
where
    M: MemoryBounds + 'static,
{
    pub fn new(
        starting_chunk_size: u64,
        final_chunk_size: u64,
        memory_type: u32,
        props: MemoryPropertyFlags,
        atom_mask: u64,
    ) -> Self {
        debug_assert_eq!(
            align_down(starting_chunk_size, atom_mask),
            starting_chunk_size
        );

        let starting_chunk_size = min(starting_chunk_size, isize::max_value());

        debug_assert_eq!(align_down(final_chunk_size, atom_mask), final_chunk_size);
        let final_chunk_size = min(final_chunk_size, isize::max_value());

        FreeListAllocator {
            freelist: FreeList::new(),
            chunk_size: starting_chunk_size,
            final_chunk_size,
            memory_type,
            props,
            atom_mask,

            total_allocations: 0,
            total_deallocations: 0,
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
            self.final_chunk_size >= size,
            "GpuAllocator must not request allocations equal or greater to chunks size"
        );

        let size = align_up(size, self.atom_mask).expect(
            "Any value not greater than final chunk size (which is aligned) has to fit for alignment",
        );

        let align_mask = align_mask | self.atom_mask;
        let host_visible = self.host_visible();

        if size <= self.chunk_size {
            // Otherwise there can't be any sufficiently large free blocks
            if let Some(block) = self.freelist.get_block(align_mask, size) {
                self.total_allocations += 1;
                return Ok(block);
            }
        }

        // New allocation is required.
        if *allocations_remains == 0 {
            return Err(AllocationError::TooManyObjects);
        }

        if size > self.chunk_size {
            let multiple = (size - 1) / self.chunk_size + 1;
            let multiple = multiple.next_power_of_two();

            self.chunk_size = (self.chunk_size * multiple).min(self.final_chunk_size);
        }

        let mut memory = device.allocate_memory(self.chunk_size, self.memory_type, flags)?;
        *allocations_remains -= 1;
        heap.alloc(self.chunk_size);

        // Map host visible allocations
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
        let block =
            self.freelist
                .get_block_from_new_memory(memory, self.chunk_size, ptr, align_mask, size);

        if self.chunk_size < self.final_chunk_size {
            // Double next chunk size
            // Limit to final value.
            self.chunk_size = (self.chunk_size * 2).min(self.final_chunk_size);
        }

        self.total_allocations += 1;
        Ok(block)
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
        self.freelist.insert_block(block);
        self.total_deallocations += 1;

        if let Some(memory) = self.freelist.drain(true) {
            memory.for_each(|(memory, size)| {
                device.deallocate_memory(memory);
                *allocations_remains += 1;
                heap.dealloc(size);
            });
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
    pub unsafe fn cleanup(
        &mut self,
        device: &impl MemoryDevice<M>,
        heap: &mut Heap,
        allocations_remains: &mut u32,
    ) {
        if let Some(memory) = self.freelist.drain(false) {
            memory.for_each(|(memory, size)| {
                device.deallocate_memory(memory);
                *allocations_remains += 1;
                heap.dealloc(size);
            });
        }

        #[cfg(feature = "tracing")]
        {
            if self.total_allocations == self.total_deallocations && !self.freelist.array.is_empty()
            {
                tracing::error!(
                    "Some regions were not deallocated on cleanup, although all blocks are free.
                        This is a bug in `FreeBlockAllocator`.
                        See array of free blocks left:
                        {:#?}",
                    self.freelist.array,
                );
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
