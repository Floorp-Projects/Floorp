use std::{
    collections::{BTreeSet, HashMap},
    ops::Range,
    ptr::NonNull,
    thread,
};

use {
    crate::{
        allocator::{Allocator, Kind},
        block::Block,
        mapping::*,
        memory::*,
        util::*,
    },
    gfx_hal::{device::Device as _, Backend},
    hibitset::{BitSet, BitSetLike as _},
};

/// Memory block allocated from `DynamicAllocator`
#[derive(Debug)]
pub struct DynamicBlock<B: Backend> {
    block_index: u32,
    chunk_index: u32,
    count: u32,
    memory: *const Memory<B>,
    ptr: Option<NonNull<u8>>,
    range: Range<u64>,
    relevant: relevant::Relevant,
}

unsafe impl<B> Send for DynamicBlock<B> where B: Backend {}
unsafe impl<B> Sync for DynamicBlock<B> where B: Backend {}

impl<B> DynamicBlock<B>
where
    B: Backend,
{
    fn shared_memory(&self) -> &Memory<B> {
        // Memory won't be freed until last block created from it deallocated.
        unsafe { &*self.memory }
    }

    fn size(&self) -> u64 {
        self.range.end - self.range.start
    }

    fn dispose(self) {
        self.relevant.dispose();
    }
}

impl<B> Block<B> for DynamicBlock<B>
where
    B: Backend,
{
    #[inline]
    fn properties(&self) -> gfx_hal::memory::Properties {
        self.shared_memory().properties()
    }

    #[inline]
    fn memory(&self) -> &B::Memory {
        self.shared_memory().raw()
    }

    #[inline]
    fn range(&self) -> Range<u64> {
        self.range.clone()
    }

    #[inline]
    fn map<'a>(
        &'a mut self,
        _device: &B::Device,
        range: Range<u64>,
    ) -> Result<MappedRange<'a, B>, gfx_hal::device::MapError> {
        debug_assert!(
            range.start < range.end,
            "Memory mapping region must have valid size"
        );
        if !self.shared_memory().host_visible() {
            //TODO: invalid access error
            return Err(gfx_hal::device::MapError::MappingFailed);
        }

        if let Some(ptr) = self.ptr {
            if let Some((ptr, range)) = mapped_sub_range(ptr, self.range.clone(), range) {
                let mapping = unsafe { MappedRange::from_raw(self.shared_memory(), ptr, range) };
                Ok(mapping)
            } else {
                Err(gfx_hal::device::MapError::OutOfBounds)
            }
        } else {
            Err(gfx_hal::device::MapError::MappingFailed)
        }
    }

    #[inline]
    fn unmap(&mut self, _device: &B::Device) {}
}

/// Config for `DynamicAllocator`.
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct DynamicConfig {
    /// All requests are rounded up to multiple of this value.
    pub block_size_granularity: u64,

    /// Maximum chunk of blocks size.
    /// Actual chunk size is `min(max_chunk_size, block_size * blocks_per_chunk)`
    pub max_chunk_size: u64,

    /// Minimum size of device allocation.
    pub min_device_allocation: u64,
}

/// No-fragmentation allocator.
/// Suitable for any type of small allocations.
/// Every freed block can be reused.
#[derive(Debug)]
pub struct DynamicAllocator<B: Backend> {
    /// Memory type that this allocator allocates.
    memory_type: gfx_hal::MemoryTypeId,

    /// Memory properties of the memory type.
    memory_properties: gfx_hal::memory::Properties,

    /// All requests are rounded up to multiple of this value.
    block_size_granularity: u64,

    /// Maximum chunk of blocks size.
    max_chunk_size: u64,

    /// Minimum size of device allocation.
    min_device_allocation: u64,

    /// Chunk lists.
    sizes: HashMap<u64, SizeEntry<B>>,

    /// Ordered set of sizes that have allocated chunks.
    chunks: BTreeSet<u64>,
}

unsafe impl<B> Send for DynamicAllocator<B> where B: Backend {}
unsafe impl<B> Sync for DynamicAllocator<B> where B: Backend {}

#[derive(Debug)]
struct SizeEntry<B: Backend> {
    /// Total count of allocated blocks with size corresponding to this entry.
    total_blocks: u64,

    /// Bits per ready (non-exhausted) chunks with free blocks.
    ready_chunks: BitSet,

    /// List of chunks.
    chunks: slab::Slab<Chunk<B>>,
}

impl<B> Default for SizeEntry<B>
where
    B: Backend,
{
    fn default() -> Self {
        SizeEntry {
            chunks: Default::default(),
            total_blocks: 0,
            ready_chunks: Default::default(),
        }
    }
}

const MAX_BLOCKS_PER_CHUNK: u32 = 64;
const MIN_BLOCKS_PER_CHUNK: u32 = 8;

impl<B> DynamicAllocator<B>
where
    B: Backend,
{
    /// Create new `DynamicAllocator`
    /// for `memory_type` with `memory_properties` specified,
    /// with `DynamicConfig` provided.
    pub fn new(
        memory_type: gfx_hal::MemoryTypeId,
        memory_properties: gfx_hal::memory::Properties,
        config: DynamicConfig,
    ) -> Self {
        log::trace!(
            "Create new allocator: type: '{:?}', properties: '{:#?}' config: '{:#?}'",
            memory_type,
            memory_properties,
            config
        );

        assert!(
            config.block_size_granularity.is_power_of_two(),
            "Allocation granularity must be power of two"
        );

        assert!(
            config.max_chunk_size.is_power_of_two(),
            "Max chunk size must be power of two"
        );

        assert!(
            config.min_device_allocation.is_power_of_two(),
            "Min device allocation must be power of two"
        );

        assert!(
            config.min_device_allocation <= config.max_chunk_size,
            "Min device allocation must be less than or equalt to max chunk size"
        );

        if memory_properties.contains(gfx_hal::memory::Properties::CPU_VISIBLE) {
            debug_assert!(
                fits_usize(config.max_chunk_size),
                "Max chunk size must fit usize for mapping"
            );
        }

        DynamicAllocator {
            memory_type,
            memory_properties,
            block_size_granularity: config.block_size_granularity,
            max_chunk_size: config.max_chunk_size,
            min_device_allocation: config.min_device_allocation,
            sizes: HashMap::new(),
            chunks: BTreeSet::new(),
        }
    }

    /// Maximum allocation size.
    pub fn max_allocation(&self) -> u64 {
        self.max_chunk_size / MIN_BLOCKS_PER_CHUNK as u64
    }

    /// Allocate memory chunk from device.
    fn alloc_chunk_from_device(
        &self,
        device: &B::Device,
        block_size: u64,
        chunk_size: u64,
    ) -> Result<Chunk<B>, gfx_hal::device::AllocationError> {
        log::trace!(
            "Allocate chunk of size: {} for blocks of size {} from device",
            chunk_size,
            block_size
        );

        // Allocate from device.
        let (memory, mapping) = unsafe {
            // Valid memory type specified.
            let raw = device.allocate_memory(self.memory_type, chunk_size)?;

            let mapping = if self
                .memory_properties
                .contains(gfx_hal::memory::Properties::CPU_VISIBLE)
            {
                log::trace!("Map new memory object");
                match device.map_memory(&raw, 0..chunk_size) {
                    Ok(mapping) => Some(NonNull::new_unchecked(mapping)),
                    Err(gfx_hal::device::MapError::OutOfMemory(error)) => {
                        device.free_memory(raw);
                        return Err(error.into());
                    }
                    Err(_) => panic!("Unexpected mapping failure"),
                }
            } else {
                None
            };
            let memory = Memory::from_raw(raw, chunk_size, self.memory_properties);
            (memory, mapping)
        };
        Ok(Chunk::from_memory(block_size, memory, mapping))
    }

    /// Allocate memory chunk for given block size.
    fn alloc_chunk(
        &mut self,
        device: &B::Device,
        block_size: u64,
        total_blocks: u64,
    ) -> Result<(Chunk<B>, u64), gfx_hal::device::AllocationError> {
        log::trace!(
            "Allocate chunk for blocks of size {} ({} total blocks allocated)",
            block_size,
            total_blocks
        );

        let min_chunk_size = MIN_BLOCKS_PER_CHUNK as u64 * block_size;
        let min_size = min_chunk_size.min(total_blocks * block_size);
        let max_chunk_size = MAX_BLOCKS_PER_CHUNK as u64 * block_size;

        // If smallest possible chunk size is larger then this allocator max allocation
        if min_size > self.max_allocation()
            || (total_blocks < MIN_BLOCKS_PER_CHUNK as u64
                && min_size >= self.min_device_allocation)
        {
            // Allocate memory block from device.
            let chunk = self.alloc_chunk_from_device(device, block_size, min_size)?;
            return Ok((chunk, min_size));
        }

        if let Some(&chunk_size) = self
            .chunks
            .range(min_chunk_size..=max_chunk_size)
            .next_back()
        {
            // Allocate block for the chunk.
            let (block, allocated) = self.alloc_from_entry(device, chunk_size, 1, block_size)?;
            Ok((Chunk::from_block(block_size, block), allocated))
        } else {
            let total_blocks = self.sizes[&block_size].total_blocks;
            let chunk_size =
                (max_chunk_size.min(min_chunk_size.max(total_blocks * block_size)) / 2 + 1)
                    .next_power_of_two();
            let (block, allocated) = self.alloc_block(device, chunk_size, block_size)?;
            Ok((Chunk::from_block(block_size, block), allocated))
        }
    }

    /// Allocate blocks from particular chunk.
    fn alloc_from_chunk(
        chunks: &mut slab::Slab<Chunk<B>>,
        chunk_index: u32,
        block_size: u64,
        count: u32,
        align: u64,
    ) -> Option<DynamicBlock<B>> {
        log::trace!(
            "Allocate {} consecutive blocks of size {} from chunk {}",
            count,
            block_size,
            chunk_index
        );

        let ref mut chunk = chunks[chunk_index as usize];
        let block_index = chunk.acquire_blocks(count, block_size, align)?;
        let block_range = chunk.blocks_range(block_size, block_index, count);

        debug_assert_eq!((block_range.end - block_range.start) % count as u64, 0);

        Some(DynamicBlock {
            range: block_range.clone(),
            memory: chunk.shared_memory(),
            block_index,
            chunk_index,
            count,
            ptr: chunk.mapping_ptr().map(|ptr| {
                mapped_fitting_range(ptr, chunk.range(), block_range)
                    .expect("Block must be sub-range of chunk")
            }),
            relevant: relevant::Relevant,
        })
    }

    /// Allocate blocks from size entry.
    fn alloc_from_entry(
        &mut self,
        device: &B::Device,
        block_size: u64,
        count: u32,
        align: u64,
    ) -> Result<(DynamicBlock<B>, u64), gfx_hal::device::AllocationError> {
        log::trace!(
            "Allocate {} consecutive blocks for size {} from the entry",
            count,
            block_size
        );

        debug_assert!(count < MIN_BLOCKS_PER_CHUNK);
        let size_entry = self.sizes.entry(block_size).or_default();

        for chunk_index in (&size_entry.ready_chunks).iter() {
            if let Some(block) = Self::alloc_from_chunk(
                &mut size_entry.chunks,
                chunk_index,
                block_size,
                count,
                align,
            ) {
                return Ok((block, 0));
            }
        }

        if size_entry.chunks.vacant_entry().key() > max_chunks_per_size() {
            return Err(gfx_hal::device::OutOfMemory::Host.into());
        }

        let total_blocks = size_entry.total_blocks;
        let (chunk, allocated) = self.alloc_chunk(device, block_size, total_blocks)?;
        let size_entry = self.sizes.entry(block_size).or_default();
        let chunk_index = size_entry.chunks.insert(chunk) as u32;

        let block = Self::alloc_from_chunk(
            &mut size_entry.chunks,
            chunk_index,
            block_size,
            count,
            align,
        )
        .expect("New chunk should yield blocks");

        if !size_entry.chunks[chunk_index as usize].is_exhausted() {
            size_entry.ready_chunks.add(chunk_index);
        }

        Ok((block, allocated))
    }

    /// Allocate block.
    fn alloc_block(
        &mut self,
        device: &B::Device,
        block_size: u64,
        align: u64,
    ) -> Result<(DynamicBlock<B>, u64), gfx_hal::device::AllocationError> {
        log::trace!("Allocate block of size {}", block_size);

        debug_assert_eq!(block_size % self.block_size_granularity, 0);
        let size_entry = self.sizes.entry(block_size).or_default();
        size_entry.total_blocks += 1;

        let overhead = (MIN_BLOCKS_PER_CHUNK as u64 - 1) / size_entry.total_blocks;

        if overhead >= 1 {
            if let Some(&size) = self
                .chunks
                .range(block_size / 4..block_size * overhead)
                .next()
            {
                return self.alloc_from_entry(
                    device,
                    size,
                    ((block_size - 1) / size + 1) as u32,
                    align,
                );
            }
        }

        if size_entry.total_blocks == MIN_BLOCKS_PER_CHUNK as u64 {
            self.chunks.insert(block_size);
        }

        self.alloc_from_entry(device, block_size, 1, align)
    }

    fn free_chunk(&mut self, device: &B::Device, chunk: Chunk<B>, block_size: u64) -> u64 {
        log::trace!("Free chunk: {:#?}", chunk);
        assert!(chunk.is_unused(block_size));
        match chunk.flavor {
            ChunkFlavor::Dedicated(boxed, _) => {
                let size = boxed.size();
                unsafe {
                    if self
                        .memory_properties
                        .contains(gfx_hal::memory::Properties::CPU_VISIBLE)
                    {
                        log::trace!("Unmap memory: {:#?}", boxed);
                        device.unmap_memory(boxed.raw());
                    }
                    device.free_memory(boxed.into_raw());
                }
                size
            }
            ChunkFlavor::Dynamic(dynamic_block) => self.free(device, dynamic_block),
        }
    }

    fn free_block(&mut self, device: &B::Device, block: DynamicBlock<B>) -> u64 {
        log::trace!("Free block: {:#?}", block);

        let block_size = block.size() / block.count as u64;
        let ref mut size_entry = self
            .sizes
            .get_mut(&block_size)
            .expect("Unable to get size entry from which block was allocated");
        let chunk_index = block.chunk_index;
        let ref mut chunk = size_entry.chunks[chunk_index as usize];
        let block_index = block.block_index;
        let count = block.count;
        block.dispose();
        chunk.release_blocks(block_index, count);
        if chunk.is_unused(block_size) {
            size_entry.ready_chunks.remove(chunk_index);
            let chunk = size_entry.chunks.remove(chunk_index as usize);
            self.free_chunk(device, chunk, block_size)
        } else {
            size_entry.ready_chunks.add(chunk_index);
            0
        }
    }

    /// Perform full cleanup of the memory allocated.
    pub fn dispose(self) {
        if !thread::panicking() {
            for (index, size) in self.sizes {
                assert_eq!(size.chunks.len(), 0, "SizeEntry({}) is still used", index);
            }
        } else {
            for (index, size) in self.sizes {
                if size.chunks.len() != 0 {
                    log::error!("Memory leak: SizeEntry({}) is still used", index);
                }
            }
        }
    }
}

impl<B> Allocator<B> for DynamicAllocator<B>
where
    B: Backend,
{
    type Block = DynamicBlock<B>;

    fn kind() -> Kind {
        Kind::Dynamic
    }

    fn alloc(
        &mut self,
        device: &B::Device,
        size: u64,
        align: u64,
    ) -> Result<(DynamicBlock<B>, u64), gfx_hal::device::AllocationError> {
        debug_assert!(size <= self.max_allocation());
        debug_assert!(align.is_power_of_two());
        let aligned_size = ((size - 1) | (align - 1) | (self.block_size_granularity - 1)) + 1;

        log::trace!(
            "Allocate dynamic block: size: {}, align: {}, aligned size: {}, type: {}",
            size,
            align,
            aligned_size,
            self.memory_type.0
        );

        self.alloc_block(device, aligned_size, align)
    }

    fn free(&mut self, device: &B::Device, block: DynamicBlock<B>) -> u64 {
        self.free_block(device, block)
    }
}

/// Block allocated for chunk.
#[derive(Debug)]
enum ChunkFlavor<B: Backend> {
    /// Allocated from device.
    Dedicated(Box<Memory<B>>, Option<NonNull<u8>>),

    /// Allocated from chunk of bigger blocks.
    Dynamic(DynamicBlock<B>),
}

#[derive(Debug)]
struct Chunk<B: Backend> {
    flavor: ChunkFlavor<B>,
    blocks: u64,
}

impl<B> Chunk<B>
where
    B: Backend,
{
    fn from_memory(block_size: u64, memory: Memory<B>, mapping: Option<NonNull<u8>>) -> Self {
        let blocks = memory.size() / block_size;
        debug_assert!(blocks <= MAX_BLOCKS_PER_CHUNK as u64);

        let high_bit = 1 << (blocks - 1);

        Chunk {
            flavor: ChunkFlavor::Dedicated(Box::new(memory), mapping),
            blocks: (high_bit - 1) | high_bit,
        }
    }

    fn from_block(block_size: u64, chunk_block: DynamicBlock<B>) -> Self {
        let blocks = (chunk_block.size() / block_size).min(MAX_BLOCKS_PER_CHUNK as u64);

        let high_bit = 1 << (blocks - 1);

        Chunk {
            flavor: ChunkFlavor::Dynamic(chunk_block),
            blocks: (high_bit - 1) | high_bit,
        }
    }

    fn shared_memory(&self) -> &Memory<B> {
        match &self.flavor {
            ChunkFlavor::Dedicated(boxed, _) => &*boxed,
            ChunkFlavor::Dynamic(chunk_block) => chunk_block.shared_memory(),
        }
    }

    fn range(&self) -> Range<u64> {
        match &self.flavor {
            ChunkFlavor::Dedicated(boxed, _) => 0..boxed.size(),
            ChunkFlavor::Dynamic(chunk_block) => chunk_block.range(),
        }
    }

    fn size(&self) -> u64 {
        let range = self.range();
        range.end - range.start
    }

    // Get block bytes range
    fn blocks_range(&self, block_size: u64, block_index: u32, count: u32) -> Range<u64> {
        let range = self.range();
        let start = range.start + block_size * block_index as u64;
        let end = start + block_size * count as u64;
        debug_assert!(end <= range.end);
        start..end
    }

    /// Check if there are free blocks.
    fn is_unused(&self, block_size: u64) -> bool {
        let blocks = (self.size() / block_size).min(MAX_BLOCKS_PER_CHUNK as u64);

        let high_bit = 1 << (blocks - 1);
        let mask = (high_bit - 1) | high_bit;

        debug_assert!(self.blocks <= mask);
        self.blocks == mask
    }

    /// Check if there are free blocks.
    fn is_exhausted(&self) -> bool {
        self.blocks == 0
    }

    fn acquire_blocks(&mut self, count: u32, block_size: u64, align: u64) -> Option<u32> {
        debug_assert!(count > 0 && count <= MAX_BLOCKS_PER_CHUNK);

        // Holds a bit-array of all positions with `count` free blocks.
        let mut blocks = !0;
        for i in 0..count {
            blocks &= self.blocks >> i;
        }
        // Find a position in `blocks` that is aligned.
        while blocks != 0 {
            let index = blocks.trailing_zeros();
            blocks &= !(1 << index);

            if (index as u64 * block_size) & (align - 1) == 0 {
                let mask = ((1 << count) - 1) << index;
                self.blocks &= !mask;
                return Some(index);
            }
        }
        None
    }

    fn release_blocks(&mut self, index: u32, count: u32) {
        let mask = ((1 << count) - 1) << index;
        debug_assert_eq!(self.blocks & mask, 0);
        self.blocks |= mask;
    }

    fn mapping_ptr(&self) -> Option<NonNull<u8>> {
        match &self.flavor {
            ChunkFlavor::Dedicated(_, ptr) => *ptr,
            ChunkFlavor::Dynamic(chunk_block) => chunk_block.ptr,
        }
    }
}

fn max_chunks_per_size() -> usize {
    let value = (std::mem::size_of::<usize>() * 8).pow(4);
    debug_assert!(fits_u32(value));
    value
}
