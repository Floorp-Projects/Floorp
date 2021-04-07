use {
    crate::{
        align_down,
        block::{MemoryBlock, MemoryBlockFlavor},
        buddy::{BuddyAllocator, BuddyBlock},
        config::Config,
        error::AllocationError,
        heap::Heap,
        linear::{LinearAllocator, LinearBlock},
        usage::{MemoryForUsage, UsageFlags},
        MemoryBounds, Request,
    },
    alloc::boxed::Box,
    core::convert::TryFrom as _,
    gpu_alloc_types::{
        AllocationFlags, DeviceProperties, MemoryDevice, MemoryPropertyFlags, MemoryType,
        OutOfMemory,
    },
};

#[cfg(feature = "freelist")]
use crate::freelist::{FreeListAllocator, FreeListBlock};

/// Memory allocator for Vulkan-like APIs.
#[derive(Debug)]
pub struct GpuAllocator<M> {
    dedicated_threshold: u64,
    preferred_dedicated_threshold: u64,
    transient_dedicated_threshold: u64,
    max_memory_allocation_size: u64,
    memory_for_usage: MemoryForUsage,
    memory_types: Box<[MemoryType]>,
    memory_heaps: Box<[Heap]>,
    max_allocation_count: u32,
    allocations_remains: u32,
    non_coherent_atom_mask: u64,
    linear_chunk: u64,
    minimal_buddy_size: u64,
    initial_buddy_dedicated_size: u64,
    buffer_device_address: bool,

    linear_allocators: Box<[Option<LinearAllocator<M>>]>,
    buddy_allocators: Box<[Option<BuddyAllocator<M>>]>,

    #[cfg(feature = "freelist")]
    freelist_threshold: u64,

    #[cfg(feature = "freelist")]
    freelist_allocators: Box<[Option<FreeListAllocator<M>>]>,
}

/// Hints for allocator to decide on allocation strategy.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum Dedicated {
    /// Allocation directly from device.\
    /// Very slow.
    /// Count of allocations is limited.\
    /// Use with caution.\
    /// Must be used if resource has to be bound to dedicated memory object.
    Required,

    /// Hint for allocator that dedicated memory object is preferred.\
    /// Should be used if it is known that resource placed in dedicated memory object
    /// would allow for better performance.\
    /// Implementation is allowed to return block to shared memory object.
    Preferred,
}

impl<M> GpuAllocator<M>
where
    M: MemoryBounds + 'static,
{
    /// Creates  new instance of `GpuAllocator`.
    /// Provided `DeviceProperties` should match properties of `MemoryDevice` that will be used
    /// with created `GpuAllocator` instance.
    #[cfg_attr(feature = "tracing", tracing::instrument)]
    pub fn new(config: Config, props: DeviceProperties<'_>) -> Self {
        assert!(
            props.non_coherent_atom_size.is_power_of_two(),
            "`non_coherent_atom_size` must be power of two"
        );

        assert!(
            isize::try_from(props.non_coherent_atom_size).is_ok(),
            "`non_coherent_atom_size` must fit host address space"
        );

        GpuAllocator {
            dedicated_threshold: config
                .dedicated_threshold
                .max(props.max_memory_allocation_size),
            preferred_dedicated_threshold: config
                .preferred_dedicated_threshold
                .min(config.dedicated_threshold)
                .max(props.max_memory_allocation_size),

            transient_dedicated_threshold: config
                .transient_dedicated_threshold
                .max(config.dedicated_threshold)
                .max(props.max_memory_allocation_size),

            max_memory_allocation_size: props.max_memory_allocation_size,

            memory_for_usage: MemoryForUsage::new(props.memory_types.as_ref()),

            memory_types: props.memory_types.as_ref().iter().copied().collect(),
            memory_heaps: props
                .memory_heaps
                .as_ref()
                .iter()
                .map(|heap| Heap::new(heap.size))
                .collect(),

            buffer_device_address: props.buffer_device_address,

            max_allocation_count: props.max_memory_allocation_count,
            allocations_remains: props.max_memory_allocation_count,
            non_coherent_atom_mask: props.non_coherent_atom_size - 1,

            linear_chunk: config.linear_chunk,
            minimal_buddy_size: config.minimal_buddy_size,
            initial_buddy_dedicated_size: config.initial_buddy_dedicated_size,

            linear_allocators: props.memory_types.as_ref().iter().map(|_| None).collect(),
            buddy_allocators: props.memory_types.as_ref().iter().map(|_| None).collect(),

            #[cfg(feature = "freelist")]
            freelist_threshold: 0,
            #[cfg(feature = "freelist")]
            freelist_allocators: props.memory_types.as_ref().iter().map(|_| None).collect(),
        }
    }

    /// Allocates memory block from specified `device` according to the `request`.
    ///
    /// # Safety
    ///
    /// * `device` must be one with `DeviceProperties` that were provided to create this `GpuAllocator` instance.
    /// * Same `device` instance must be used for all interactions with one `GpuAllocator` instance
    ///   and memory blocks allocated from it.
    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn alloc(
        &mut self,
        device: &impl MemoryDevice<M>,
        request: Request,
    ) -> Result<MemoryBlock<M>, AllocationError> {
        self.alloc_internal(device, request, None)
    }

    /// Allocates memory block from specified `device` according to the `request`.
    /// This function allows user to force specific allocation strategy.
    /// Improper use can lead to suboptimal performance or too large overhead.
    /// Prefer `GpuAllocator::alloc` if doubt.
    ///
    /// # Safety
    ///
    /// * `device` must be one with `DeviceProperties` that were provided to create this `GpuAllocator` instance.
    /// * Same `device` instance must be used for all interactions with one `GpuAllocator` instance
    ///   and memory blocks allocated from it.
    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn alloc_with_dedicated(
        &mut self,
        device: &impl MemoryDevice<M>,
        request: Request,
        dedicated: Dedicated,
    ) -> Result<MemoryBlock<M>, AllocationError> {
        self.alloc_internal(device, request, Some(dedicated))
    }

    unsafe fn alloc_internal(
        &mut self,
        device: &impl MemoryDevice<M>,
        mut request: Request,
        dedicated: Option<Dedicated>,
    ) -> Result<MemoryBlock<M>, AllocationError> {
        enum Strategy {
            Linear,
            Buddy,
            Dedicated,
            #[cfg(feature = "freelist")]
            FreeList,
        }

        request.usage = with_implicit_usage_flags(request.usage);

        if request.usage.contains(UsageFlags::DEVICE_ADDRESS) {
            assert!(self.buffer_device_address, "`DEVICE_ADDRESS` cannot be requested when `DeviceProperties::buffer_device_address` is false");
        }

        if request.size > self.max_memory_allocation_size {
            return Err(AllocationError::OutOfDeviceMemory);
        }

        if let Some(Dedicated::Required) = dedicated {
            if self.allocations_remains == 0 {
                return Err(AllocationError::TooManyObjects);
            }
        }

        if 0 == self.memory_for_usage.mask(request.usage) & request.memory_types {
            #[cfg(feature = "tracing")]
            tracing::error!(
                "Cannot serve request {:?}, no memory among bitset `{}` support usage {:?}",
                request,
                request.memory_types,
                request.usage
            );

            return Err(AllocationError::NoCompatibleMemoryTypes);
        }

        let transient = request.usage.contains(UsageFlags::TRANSIENT);

        for &index in self.memory_for_usage.types(request.usage) {
            if 0 == request.memory_types & (1 << index) {
                // Skip memory type incompatible with the request.
                continue;
            }

            let memory_type = &self.memory_types[index as usize];
            let heap = memory_type.heap;
            let heap = &mut self.memory_heaps[heap as usize];

            let atom_mask = if host_visible_non_coherent(memory_type.props) {
                self.non_coherent_atom_mask
            } else {
                0
            };

            let flags = if self.buffer_device_address {
                AllocationFlags::DEVICE_ADDRESS
            } else {
                AllocationFlags::empty()
            };

            let linear_chunk = align_down(self.linear_chunk.min(heap.size() / 32), atom_mask);

            let strategy = match (dedicated, transient) {
                (Some(Dedicated::Required), _) => Strategy::Dedicated,
                (Some(Dedicated::Preferred), _)
                    if request.size >= self.preferred_dedicated_threshold =>
                {
                    Strategy::Dedicated
                }
                (_, true) => {
                    let threshold = self.transient_dedicated_threshold.min(linear_chunk);

                    if request.size < threshold {
                        #[cfg(feature = "freelist")]
                        {
                            if request.size < self.freelist_threshold {
                                Strategy::Linear
                            } else {
                                Strategy::FreeList
                            }
                        }

                        #[cfg(not(feature = "freelist"))]
                        {
                            Strategy::Linear
                        }
                    } else {
                        Strategy::Dedicated
                    }
                }
                (_, false) => {
                    let threshold = self.dedicated_threshold.min(heap.size() / 32);

                    if request.size < threshold {
                        Strategy::Buddy
                    } else {
                        Strategy::Dedicated
                    }
                }
            };

            match strategy {
                Strategy::Dedicated => {
                    #[cfg(feature = "tracing")]
                    tracing::debug!(
                        "Allocating memory object `{}@{:?}`",
                        request.size,
                        memory_type
                    );

                    match device.allocate_memory(request.size, index, flags) {
                        Ok(memory) => {
                            self.allocations_remains -= 1;
                            heap.alloc(request.size);

                            return Ok(MemoryBlock::new(
                                index,
                                memory_type.props,
                                0,
                                request.size,
                                atom_mask,
                                MemoryBlockFlavor::Dedicated { memory },
                            ));
                        }
                        Err(OutOfMemory::OutOfDeviceMemory) => continue,
                        Err(OutOfMemory::OutOfHostMemory) => {
                            return Err(AllocationError::OutOfHostMemory)
                        }
                    }
                }
                Strategy::Linear => {
                    let allocator = match &mut self.linear_allocators[index as usize] {
                        Some(allocator) => allocator,
                        slot => {
                            let memory_type = &self.memory_types[index as usize];
                            slot.get_or_insert(LinearAllocator::new(
                                linear_chunk,
                                index,
                                memory_type.props,
                                if host_visible_non_coherent(memory_type.props) {
                                    self.non_coherent_atom_mask
                                } else {
                                    0
                                },
                            ))
                        }
                    };
                    let result = allocator.alloc(
                        device,
                        request.size,
                        request.align_mask,
                        flags,
                        heap,
                        &mut self.allocations_remains,
                    );

                    match result {
                        Ok(block) => {
                            return Ok(MemoryBlock::new(
                                index,
                                memory_type.props,
                                block.offset,
                                block.size,
                                atom_mask,
                                MemoryBlockFlavor::Linear {
                                    chunk: block.chunk,
                                    ptr: block.ptr,
                                    memory: block.memory,
                                },
                            ))
                        }
                        Err(AllocationError::OutOfDeviceMemory) => continue,
                        Err(err) => return Err(err),
                    }
                }
                #[cfg(feature = "freelist")]
                Strategy::FreeList => {
                    let allocator = match &mut self.freelist_allocators[index as usize] {
                        Some(allocator) => allocator,
                        slot => {
                            let memory_type = &self.memory_types[index as usize];

                            let dealloc_threshold = linear_chunk
                                .checked_mul(2)
                                .unwrap_or_else(|| (i64::MAX as u64).max(linear_chunk));

                            slot.get_or_insert(FreeListAllocator::new(
                                linear_chunk,
                                dealloc_threshold,
                                index,
                                memory_type.props,
                                if host_visible_non_coherent(memory_type.props) {
                                    self.non_coherent_atom_mask
                                } else {
                                    0
                                },
                            ))
                        }
                    };
                    let result = allocator.alloc(
                        device,
                        request.size,
                        request.align_mask,
                        flags,
                        heap,
                        &mut self.allocations_remains,
                    );

                    match result {
                        Ok(block) => {
                            return Ok(MemoryBlock::new(
                                index,
                                memory_type.props,
                                block.offset,
                                block.size,
                                atom_mask,
                                MemoryBlockFlavor::FreeList {
                                    chunk: block.chunk,
                                    ptr: block.ptr,
                                    memory: block.memory,
                                },
                            ))
                        }
                        Err(AllocationError::OutOfDeviceMemory) => continue,
                        Err(err) => return Err(err),
                    }
                }

                Strategy::Buddy => {
                    let allocator = match &mut self.buddy_allocators[index as usize] {
                        Some(allocator) => allocator,
                        slot => {
                            let minimal_buddy_size = self
                                .minimal_buddy_size
                                .min(heap.size() / 1024)
                                .next_power_of_two();

                            let initial_buddy_dedicated_size = self
                                .initial_buddy_dedicated_size
                                .min(heap.size() / 32)
                                .next_power_of_two();

                            let memory_type = &self.memory_types[index as usize];
                            slot.get_or_insert(BuddyAllocator::new(
                                minimal_buddy_size,
                                initial_buddy_dedicated_size,
                                index,
                                memory_type.props,
                                if host_visible_non_coherent(memory_type.props) {
                                    self.non_coherent_atom_mask
                                } else {
                                    0
                                },
                            ))
                        }
                    };
                    let result = allocator.alloc(
                        device,
                        request.size,
                        request.align_mask,
                        flags,
                        heap,
                        &mut self.allocations_remains,
                    );

                    match result {
                        Ok(block) => {
                            return Ok(MemoryBlock::new(
                                index,
                                memory_type.props,
                                block.offset,
                                block.size,
                                atom_mask,
                                MemoryBlockFlavor::Buddy {
                                    chunk: block.chunk,
                                    ptr: block.ptr,
                                    index: block.index,
                                    memory: block.memory,
                                },
                            ))
                        }
                        Err(AllocationError::OutOfDeviceMemory) => continue,
                        Err(err) => return Err(err),
                    }
                }
            }
        }

        Err(AllocationError::OutOfDeviceMemory)
    }

    /// Deallocates memory block previously allocated from this `GpuAllocator` instance.
    ///
    /// # Safety
    ///
    /// * Memory block must have been allocated by this `GpuAllocator` instance
    /// * `device` must be one with `DeviceProperties` that were provided to create this `GpuAllocator` instance
    /// * Same `device` instance must be used for all interactions with one `GpuAllocator` instance
    ///   and memory blocks allocated from it
    #[cfg_attr(feature = "tracing", tracing::instrument(skip(self, device)))]
    pub unsafe fn dealloc(&mut self, device: &impl MemoryDevice<M>, block: MemoryBlock<M>) {
        let memory_type = block.memory_type();
        let offset = block.offset();
        let size = block.size();
        let flavor = block.deallocate();
        match flavor {
            MemoryBlockFlavor::Dedicated { memory } => {
                let heap = self.memory_types[memory_type as usize].heap;
                device.deallocate_memory(memory);
                self.allocations_remains += 1;
                self.memory_heaps[heap as usize].dealloc(size);
            }
            MemoryBlockFlavor::Linear { chunk, ptr, memory } => {
                let heap = self.memory_types[memory_type as usize].heap;
                let heap = &mut self.memory_heaps[heap as usize];

                let allocator = self.linear_allocators[memory_type as usize]
                    .as_mut()
                    .expect("Allocator should exist");

                allocator.dealloc(
                    device,
                    LinearBlock {
                        memory,
                        ptr,
                        offset,
                        size,
                        chunk,
                    },
                    heap,
                    &mut self.allocations_remains,
                );
            }
            MemoryBlockFlavor::Buddy {
                chunk,
                ptr,
                index,
                memory,
            } => {
                let heap = self.memory_types[memory_type as usize].heap;
                let heap = &mut self.memory_heaps[heap as usize];

                let allocator = self.buddy_allocators[memory_type as usize]
                    .as_mut()
                    .expect("Allocator should exist");

                allocator.dealloc(
                    device,
                    BuddyBlock {
                        memory,
                        ptr,
                        offset,
                        size,
                        chunk,
                        index,
                    },
                    heap,
                    &mut self.allocations_remains,
                );
            }
            #[cfg(feature = "freelist")]
            MemoryBlockFlavor::FreeList { chunk, ptr, memory } => {
                let heap = self.memory_types[memory_type as usize].heap;
                let heap = &mut self.memory_heaps[heap as usize];

                let allocator = self.freelist_allocators[memory_type as usize]
                    .as_mut()
                    .expect("Allocator should exist");

                allocator.dealloc(
                    device,
                    FreeListBlock {
                        memory,
                        ptr,
                        offset,
                        size,
                        chunk,
                    },
                    heap,
                    &mut self.allocations_remains,
                );
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
        for allocator in self.linear_allocators.iter_mut().filter_map(Option::as_mut) {
            allocator.cleanup(device);
        }

        #[cfg(feature = "freelist")]
        {
            for allocator in self
                .freelist_allocators
                .iter_mut()
                .filter_map(Option::as_mut)
            {
                allocator.cleanup(device);
            }
        }
    }
}

fn host_visible_non_coherent(props: MemoryPropertyFlags) -> bool {
    (props & (MemoryPropertyFlags::HOST_COHERENT | MemoryPropertyFlags::HOST_VISIBLE))
        == MemoryPropertyFlags::HOST_VISIBLE
}

fn with_implicit_usage_flags(usage: UsageFlags) -> UsageFlags {
    if usage.is_empty() {
        UsageFlags::FAST_DEVICE_ACCESS
    } else if usage.intersects(UsageFlags::DOWNLOAD | UsageFlags::UPLOAD) {
        usage | UsageFlags::HOST_ACCESS
    } else {
        usage
    }
}
