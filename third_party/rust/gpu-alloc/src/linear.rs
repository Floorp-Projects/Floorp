use {
    crate::{align_down, align_up, error::AllocationError, heap::Heap, MemoryBounds},
    alloc::collections::VecDeque,
    core::{convert::TryFrom as _, ptr::NonNull},
    gpu_alloc_types::{AllocationFlags, DeviceMapError, MemoryDevice, MemoryPropertyFlags},
};

#[derive(Debug)]
pub(crate) struct LinearBlock<M> {
    pub memory: M,
    pub ptr: Option<NonNull<u8>>,
    pub offset: u64,
    pub size: u64,
    pub chunk: u64,
}

unsafe impl<M> Sync for LinearBlock<M> where M: Sync {}
unsafe impl<M> Send for LinearBlock<M> where M: Send {}

#[derive(Debug)]
struct Chunk<M> {
    memory: M,
    offset: u64,
    allocated: u64,
    ptr: Option<NonNull<u8>>,
}

impl<M> Chunk<M> {
    fn exhaust(self) -> ExhaustedChunk<M> {
        debug_assert_ne!(self.allocated, 0, "Unused chunk cannot be exhaused");

        ExhaustedChunk {
            memory: self.memory,
            allocated: self.allocated,
        }
    }
}

#[derive(Debug)]
struct ExhaustedChunk<M> {
    memory: M,
    allocated: u64,
}

#[derive(Debug)]
pub(crate) struct LinearAllocator<M> {
    ready: Option<Chunk<M>>,
    exhausted: VecDeque<Option<ExhaustedChunk<M>>>,
    offset: u64,
    chunk_size: u64,
    memory_type: u32,
    props: MemoryPropertyFlags,
    atom_mask: u64,
}

unsafe impl<M> Sync for LinearAllocator<M> where M: Sync {}
unsafe impl<M> Send for LinearAllocator<M> where M: Send {}

impl<M> LinearAllocator<M>
where
    M: MemoryBounds + 'static,
{
    pub fn new(
        chunk_size: u64,
        memory_type: u32,
        props: MemoryPropertyFlags,
        atom_mask: u64,
    ) -> Self {
        debug_assert_eq!(align_down(chunk_size, atom_mask), chunk_size);

        LinearAllocator {
            ready: None,
            exhausted: VecDeque::new(),
            offset: 0,
            chunk_size: min(chunk_size, isize::max_value()),
            memory_type,
            props,
            atom_mask,
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
    ) -> Result<LinearBlock<M>, AllocationError>
    where
        M: Clone,
    {
        debug_assert!(
            self.chunk_size >= size,
            "GpuAllocator must not request allocations equal or greater to chunks size"
        );

        let size = align_up(size, self.atom_mask)
            .expect("Any value not greater than aligned chunk size must fit for alignment");

        let align_mask = align_mask | self.atom_mask;
        let host_visible = self.host_visible();

        match &mut self.ready {
            Some(ready) if fits(self.chunk_size, ready.offset, size, align_mask) => {
                let chunks_offset = self.offset;
                let exhausted = self.exhausted.len() as u64;
                Ok(Self::alloc_from_chunk(
                    ready,
                    self.chunk_size,
                    chunks_offset,
                    exhausted,
                    size,
                    align_mask,
                ))
            }

            ready => {
                self.exhausted
                    .extend(ready.take().map(Chunk::exhaust).map(Some));

                if *allocations_remains == 0 {
                    return Err(AllocationError::TooManyObjects);
                }

                let memory = device.allocate_memory(self.chunk_size, self.memory_type, flags)?;

                *allocations_remains -= 1;
                heap.alloc(self.chunk_size);

                let ptr = if host_visible {
                    match device.map_memory(&memory, 0, self.chunk_size) {
                        Ok(ptr) => Some(ptr),
                        Err(DeviceMapError::MapFailed) => {
                            #[cfg(feature = "tracing")]
                            tracing::error!(
                                "Failed to map host-visible memory in linear allocator"
                            );
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

                let ready = ready.get_or_insert(Chunk {
                    ptr,
                    memory,
                    allocated: 0,
                    offset: 0,
                });

                let chunks_offset = self.offset;
                let exhausted = self.exhausted.len() as u64;
                Ok(Self::alloc_from_chunk(
                    ready,
                    self.chunk_size,
                    chunks_offset,
                    exhausted,
                    size,
                    align_mask,
                ))
            }
        }
    }

    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn dealloc(
        &mut self,
        device: &impl MemoryDevice<M>,
        block: LinearBlock<M>,
        heap: &mut Heap,
        allocations_remains: &mut u32,
    ) {
        debug_assert_eq!(block.ptr.is_some(), self.host_visible());

        debug_assert!(block.chunk >= self.offset, "Chunk index is less than chunk offset in this allocator. Probably incorrect allocator instance");
        let chunk_offset = block.chunk - self.offset;
        match usize::try_from(chunk_offset) {
            Ok(chunk_offset) => {
                if chunk_offset > self.exhausted.len() {
                    panic!("Chunk index is out of bounds. Probably incorrect allocator instance")
                }

                if chunk_offset == self.exhausted.len() {
                    let chunk = self.ready.as_mut().expect(
                        "Chunk index is out of bounds. Probably incorrect allocator instance",
                    );
                    chunk.allocated -= 1;
                    if chunk.allocated == 0 {
                        // Reuse chunk.
                        chunk.offset = 0;
                    }
                } else {
                    let chunk = &mut self.exhausted[chunk_offset].as_mut().expect("Chunk index points to deallocated chunk. Probably incorrect allocator instance");
                    chunk.allocated -= 1;

                    if chunk.allocated == 0 {
                        let memory = self.exhausted[chunk_offset].take().unwrap().memory;
                        drop(block);
                        device.deallocate_memory(memory);
                        *allocations_remains += 1;
                        heap.dealloc(self.chunk_size);

                        if chunk_offset == 0 {
                            while let Some(None) = self.exhausted.get(0) {
                                self.exhausted.pop_front();
                                self.offset += 1;
                            }
                        }
                    }
                }
            }
            Err(_) => {
                panic!("Chunk index does not fit `usize`. Probably incorrect allocator instance")
            }
        }
    }

    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn cleanup(&mut self, device: &impl MemoryDevice<M>) {
        if let Some(chunk) = self.ready.take() {
            assert_eq!(
                chunk.allocated, 0,
                "All blocks must be deallocated before cleanup"
            );
            device.deallocate_memory(chunk.memory);
        }

        assert!(
            self.exhausted.is_empty(),
            "All blocks must be deallocated before cleanup"
        );
    }

    fn host_visible(&self) -> bool {
        self.props.contains(MemoryPropertyFlags::HOST_VISIBLE)
    }

    unsafe fn alloc_from_chunk(
        chunk: &mut Chunk<M>,
        chunk_size: u64,
        chunks_offset: u64,
        exhausted: u64,
        size: u64,
        align_mask: u64,
    ) -> LinearBlock<M>
    where
        M: Clone,
    {
        debug_assert!(
            fits(chunk_size, chunk.offset, size, align_mask),
            "Must be checked in caller"
        );

        let offset =
            align_up(chunk.offset, align_mask).expect("Chunk must be checked to fit allocation");

        chunk.offset = offset + size;
        chunk.allocated += 1;

        debug_assert!(
            offset
                .checked_add(size)
                .map_or(false, |end| end <= chunk_size),
            "Offset + size is not in chunk bounds"
        );
        LinearBlock {
            memory: chunk.memory.clone(),
            ptr: chunk
                .ptr
                .map(|ptr| NonNull::new_unchecked(ptr.as_ptr().offset(offset as isize))),
            offset,
            size,
            chunk: chunks_offset + exhausted,
        }
    }
}

fn fits(chunk_size: u64, chunk_offset: u64, size: u64, align_mask: u64) -> bool {
    align_up(chunk_offset, align_mask)
        .and_then(|aligned| aligned.checked_add(size))
        .map_or(false, |end| end <= chunk_size)
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
