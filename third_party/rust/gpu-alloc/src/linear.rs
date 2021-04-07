use {
    crate::{
        align_down, align_up,
        error::AllocationError,
        heap::Heap,
        util::{arc_unwrap, is_arc_unique},
        MemoryBounds,
    },
    alloc::{collections::VecDeque, sync::Arc},
    core::{convert::TryFrom as _, ptr::NonNull},
    gpu_alloc_types::{AllocationFlags, DeviceMapError, MemoryDevice, MemoryPropertyFlags},
};

#[derive(Debug)]
pub(crate) struct LinearBlock<M> {
    pub memory: Arc<M>,
    pub ptr: Option<NonNull<u8>>,
    pub offset: u64,
    pub size: u64,
    pub chunk: u64,
}

unsafe impl<M> Sync for LinearBlock<M> where M: Sync {}
unsafe impl<M> Send for LinearBlock<M> where M: Send {}

#[derive(Debug)]
struct ActiveChunk<M> {
    memory: Arc<M>,
    offset: u64,
    ptr: Option<NonNull<u8>>,
}

impl<M> ActiveChunk<M> {
    fn exhaust(self) -> Chunk<M> {
        Chunk {
            memory: self.memory,
            ptr: self.ptr,
        }
    }
}

#[derive(Debug)]
struct Chunk<M> {
    memory: Arc<M>,
    ptr: Option<NonNull<u8>>,
}

#[derive(Debug)]
pub(crate) struct LinearAllocator<M> {
    active: Option<ActiveChunk<M>>,
    prepared: Option<Chunk<M>>,
    exhausted: VecDeque<Option<Chunk<M>>>,
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
            active: None,
            prepared: None,
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
    ) -> Result<LinearBlock<M>, AllocationError> {
        debug_assert!(
            self.chunk_size >= size,
            "GpuAllocator must not request allocations equal or greater to chunks size"
        );

        let size = align_up(size, self.atom_mask).expect(
            "Any value not greater than chunk size (which is aligned) has to fit for alignment",
        );

        let align_mask = align_mask | self.atom_mask;
        let host_visible = self.host_visible();

        match &mut self.active {
            Some(active) if fits(self.chunk_size, active.offset, size, align_mask) => {
                let chunks_index = self.offset + self.exhausted.len() as u64;
                Ok(Self::alloc_from_chunk(
                    active,
                    self.chunk_size,
                    chunks_index,
                    size,
                    align_mask,
                ))
            }

            active => {
                self.exhausted
                    .extend(active.take().map(ActiveChunk::exhaust).map(Some));

                let active = match self.prepared.take() {
                    Some(chunk) => active.get_or_insert(ActiveChunk {
                        ptr: chunk.ptr,
                        memory: chunk.memory,
                        offset: 0,
                    }),
                    None => {
                        if *allocations_remains == 0 {
                            return Err(AllocationError::TooManyObjects);
                        }

                        let mut memory =
                            device.allocate_memory(self.chunk_size, self.memory_type, flags)?;

                        *allocations_remains -= 1;
                        heap.alloc(self.chunk_size);

                        let ptr = if host_visible {
                            match device.map_memory(&mut memory, 0, self.chunk_size) {
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

                        let memory = Arc::new(memory);

                        active.get_or_insert(ActiveChunk {
                            ptr,
                            memory,
                            offset: 0,
                        })
                    }
                };

                let chunks_index = self.offset + self.exhausted.len() as u64;
                Ok(Self::alloc_from_chunk(
                    active,
                    self.chunk_size,
                    chunks_index,
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
                    let chunk = self.active.as_mut().expect(
                        "Chunk index is out of bounds. Probably incorrect allocator instance",
                    );
                    drop(block);

                    if is_arc_unique(&mut chunk.memory) {
                        // Reuse chunk.
                        chunk.offset = 0;
                    }
                } else {
                    let chunk = self.exhausted[chunk_offset].as_mut().expect("Chunk index points to deallocated chunk. Probably incorrect allocator instance");
                    drop(block);

                    if is_arc_unique(&mut chunk.memory) {
                        if self.prepared.is_some() {
                            let memory = self.exhausted[chunk_offset].take().unwrap().memory;
                            let memory = arc_unwrap(memory);
                            device.deallocate_memory(memory);
                            *allocations_remains += 1;
                            heap.dealloc(self.chunk_size);
                        } else {
                            // Keep one free chunk prepared.
                            let chunk = self.exhausted[chunk_offset].take().unwrap();
                            self.prepared = Some(chunk);
                        }

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
        if let Some(mut chunk) = self.active.take() {
            debug_assert!(
                is_arc_unique(&mut chunk.memory),
                "All blocks must be deallocated before cleanup"
            );

            match Arc::try_unwrap(chunk.memory) {
                Ok(memory) => device.deallocate_memory(memory),
                #[cfg(feature = "tracing")]
                Err(_) => {
                    tracing::error!("Cleanup before all memory blocks are deallocated");
                }
                #[cfg(not(feature = "tracing"))]
                _ => {}
            }
        }

        if let Some(mut chunk) = self.prepared.take() {
            debug_assert!(
                is_arc_unique(&mut chunk.memory),
                "All blocks must be deallocated before cleanup"
            );

            match Arc::try_unwrap(chunk.memory) {
                Ok(memory) => device.deallocate_memory(memory),
                #[cfg(feature = "tracing")]
                Err(_) => {
                    tracing::error!("Cleanup before all memory blocks are deallocated");
                }
                #[cfg(not(feature = "tracing"))]
                _ => {}
            }
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
        chunk: &mut ActiveChunk<M>,
        chunk_size: u64,
        chunks_index: u64,
        size: u64,
        align_mask: u64,
    ) -> LinearBlock<M> {
        debug_assert!(
            fits(chunk_size, chunk.offset, size, align_mask),
            "Must be checked in caller"
        );

        let offset = align_up(chunk.offset, align_mask)
            .expect("ActiveChunk must be checked to fit allocation");

        debug_assert!(
            offset
                .checked_add(size)
                .map_or(false, |end| end <= chunk_size),
            "Offset + size is not in chunk bounds"
        );

        chunk.offset = offset + size;

        LinearBlock {
            memory: chunk.memory.clone(),
            ptr: chunk
                .ptr
                .map(|ptr| NonNull::new_unchecked(ptr.as_ptr().offset(offset as isize))),
            offset,
            size,
            chunk: chunks_index,
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
