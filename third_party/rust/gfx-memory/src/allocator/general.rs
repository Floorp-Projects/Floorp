use crate::{
    allocator::{Allocator, Kind},
    block::Block,
    mapping::MappedRange,
    memory::Memory,
    AtomSize, Size,
};

use bit_set::BitSet;
use hal::{device::Device as _, Backend};
use slab::Slab;

use std::{
    collections::{BTreeSet, HashMap},
    hash::BuildHasherDefault,
    mem,
    ops::Range,
    ptr::NonNull,
    sync::Arc,
    thread,
};

//TODO: const fn
fn max_chunks_per_size() -> usize {
    (mem::size_of::<usize>() * 8).pow(4)
}

/// Memory block allocated from `GeneralAllocator`.
#[derive(Debug)]
pub struct GeneralBlock<B: Backend> {
    block_index: u32,
    chunk_index: u32,
    count: u32,
    memory: Arc<Memory<B>>,
    ptr: Option<NonNull<u8>>,
    range: Range<Size>,
}

unsafe impl<B: Backend> Send for GeneralBlock<B> {}
unsafe impl<B: Backend> Sync for GeneralBlock<B> {}

impl<B: Backend> GeneralBlock<B> {
    /// Get the size of this block.
    pub fn size(&self) -> Size {
        self.range.end - self.range.start
    }
}

impl<B: Backend> Block<B> for GeneralBlock<B> {
    fn properties(&self) -> hal::memory::Properties {
        self.memory.properties()
    }

    fn memory(&self) -> &B::Memory {
        self.memory.raw()
    }

    fn segment(&self) -> hal::memory::Segment {
        hal::memory::Segment {
            offset: self.range.start,
            size: Some(self.range.end - self.range.start),
        }
    }

    fn map<'a>(
        &'a mut self,
        _device: &B::Device,
        segment: hal::memory::Segment,
    ) -> Result<MappedRange<'a, B>, hal::device::MapError> {
        let requested_range = crate::segment_to_sub_range(segment, &self.range)?;
        let mapping_range = match self.memory.non_coherent_atom_size {
            Some(atom) => crate::align_range(&requested_range, atom),
            None => requested_range.clone(),
        };

        Ok(unsafe {
            MappedRange::from_raw(
                &*self.memory,
                self.ptr
                    .ok_or(hal::device::MapError::MappingFailed)?
                    .as_ptr()
                    .offset((mapping_range.start - self.range.start) as isize),
                mapping_range,
                requested_range,
            )
        })
    }
}

/// Config for `GeneralAllocator`.
#[derive(Clone, Copy, Debug)]
pub struct GeneralConfig {
    /// All requests are rounded up to multiple of this value.
    pub block_size_granularity: Size,

    /// Maximum chunk of blocks size.
    /// Actual chunk size is `min(max_chunk_size, block_size * blocks_per_chunk)`
    pub max_chunk_size: Size,

    /// Minimum size of device allocation.
    pub min_device_allocation: Size,
}

/// No-fragmentation allocator.
/// Suitable for any type of small allocations.
/// Every freed block can be reused.
#[derive(Debug)]
pub struct GeneralAllocator<B: Backend> {
    /// Memory type that this allocator allocates.
    memory_type: hal::MemoryTypeId,

    /// Memory properties of the memory type.
    memory_properties: hal::memory::Properties,

    /// All requests are rounded up to multiple of this value.
    block_size_granularity: Size,

    /// Maximum chunk of blocks size.
    max_chunk_size: Size,

    /// Minimum size of device allocation.
    min_device_allocation: Size,

    /// Chunk lists.
    sizes: HashMap<Size, SizeEntry<B>, BuildHasherDefault<fxhash::FxHasher>>,

    /// Ordered set of sizes that have allocated chunks.
    chunks: BTreeSet<Size>,

    non_coherent_atom_size: Option<AtomSize>,
}

//TODO: ensure Send and Sync
unsafe impl<B: Backend> Send for GeneralAllocator<B> {}
unsafe impl<B: Backend> Sync for GeneralAllocator<B> {}

#[derive(Debug)]
struct SizeEntry<B: Backend> {
    /// Bits per ready (non-exhausted) chunks with free blocks.
    ready_chunks: BitSet,

    /// List of chunks.
    chunks: Slab<Chunk<B>>,
}

impl<B: Backend> Default for SizeEntry<B> {
    fn default() -> Self {
        SizeEntry {
            chunks: Slab::new(),
            ready_chunks: BitSet::new(),
        }
    }
}

impl<B: Backend> SizeEntry<B> {
    fn next_block_count(&self, block_size: Size) -> u32 {
        self.chunks.iter().fold(1u32, |max, (_, chunk)| {
            (chunk.num_blocks(block_size) as u32).max(max)
        }) * 2
    }
}

/// A bit mask of block availability.
type BlockMask = u64;

const MIN_BLOCKS_PER_CHUNK: u32 = 8;
const MAX_BLOCKS_PER_CHUNK: u32 = mem::size_of::<BlockMask>() as u32 * 8;
const LARGE_BLOCK_THRESHOLD: Size = 0x10000;

#[test]
fn test_constants() {
    assert!(MIN_BLOCKS_PER_CHUNK < MAX_BLOCKS_PER_CHUNK);
    assert!(LARGE_BLOCK_THRESHOLD * 2 >= MIN_BLOCKS_PER_CHUNK as Size);
}

impl<B: Backend> GeneralAllocator<B> {
    /// Create new `GeneralAllocator`
    /// for `memory_type` with `memory_properties` specified,
    /// with `GeneralConfig` provided.
    pub fn new(
        memory_type: hal::MemoryTypeId,
        memory_properties: hal::memory::Properties,
        config: GeneralConfig,
        non_coherent_atom_size: Size,
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

        let (block_size_granularity, non_coherent_atom_size) =
            if crate::is_non_coherent_visible(memory_properties) {
                let granularity = non_coherent_atom_size
                    .max(config.block_size_granularity)
                    .next_power_of_two();
                (granularity, AtomSize::new(non_coherent_atom_size))
            } else {
                (config.block_size_granularity, None)
            };

        GeneralAllocator {
            memory_type,
            memory_properties,
            block_size_granularity,
            max_chunk_size: config.max_chunk_size,
            min_device_allocation: config.min_device_allocation,
            sizes: HashMap::default(),
            chunks: BTreeSet::new(),
            non_coherent_atom_size,
        }
    }

    /// Maximum block size to allocate within a chunk that is another block.
    pub fn max_sub_block_size(&self) -> Size {
        self.max_chunk_size / MIN_BLOCKS_PER_CHUNK as Size
    }

    /// Allocate memory chunk from device.
    fn alloc_chunk_from_device(
        &self,
        device: &B::Device,
        block_size: Size,
        count: u32,
    ) -> Result<Chunk<B>, hal::device::AllocationError> {
        log::trace!(
            "Allocate chunk with {} blocks size {} from device",
            count,
            block_size
        );

        let (memory, ptr) = unsafe {
            super::allocate_memory_helper(
                device,
                self.memory_type,
                block_size * count as Size,
                self.memory_properties,
                self.non_coherent_atom_size,
            )?
        };

        Ok(Chunk::from_memory(block_size, memory, ptr))
    }

    /// Allocate memory chunk for given block size.
    ///
    /// The chunk will be aligned to the `block_size`.
    fn alloc_chunk(
        &mut self,
        device: &B::Device,
        block_size: Size,
        requested_count: u32,
    ) -> Result<(Chunk<B>, Size), hal::device::AllocationError> {
        log::trace!(
            "Allocate chunk for roughly {} blocks of size {}",
            requested_count,
            block_size
        );

        let min_chunk_size = MIN_BLOCKS_PER_CHUNK as Size * block_size;
        let max_chunk_size = MAX_BLOCKS_PER_CHUNK as Size * block_size;
        let clamped_count = requested_count
            .next_power_of_two() // makes it more re-usable
            .max(MIN_BLOCKS_PER_CHUNK)
            .min(MAX_BLOCKS_PER_CHUNK);
        let requested_chunk_size = clamped_count as Size * block_size;

        // If smallest possible chunk size is larger then this allocator max allocation
        if min_chunk_size > self.max_sub_block_size() {
            // Allocate memory block from the device.
            let chunk = self.alloc_chunk_from_device(device, block_size, clamped_count)?;
            return Ok((chunk, requested_chunk_size));
        }

        let (block, allocated) = match self
            .chunks
            .range(min_chunk_size..=max_chunk_size)
            .rfind(|&size| size % block_size == 0)
        {
            Some(&chunk_size) => {
                // Allocate block for the chunk.
                self.alloc_from_entry(device, chunk_size, 1, block_size)?
            }
            None if requested_chunk_size > self.min_device_allocation => {
                // Allocate memory block from the device.
                // Note: if we call into `alloc_block` instead, we are going to be
                // going larger and larger blocks until we hit the ceiling.
                let chunk = self.alloc_chunk_from_device(device, block_size, clamped_count)?;
                return Ok((chunk, requested_chunk_size));
            }
            None => {
                // Allocate a new block for the chunk.
                self.alloc_block(device, requested_chunk_size, block_size)?
            }
        };

        Ok((Chunk::from_block(block_size, block), allocated))
    }

    /// Allocate blocks from particular chunk.
    fn alloc_from_chunk(
        chunks: &mut Slab<Chunk<B>>,
        chunk_index: usize,
        block_size: Size,
        count: u32,
        align: Size,
    ) -> Option<GeneralBlock<B>> {
        log::trace!(
            "Allocate {} consecutive blocks of size {} from chunk {}",
            count,
            block_size,
            chunk_index
        );

        let chunk = &mut chunks[chunk_index];
        let block_index = chunk.acquire_blocks(count, block_size, align)?;
        let block_range = chunk.blocks_range(block_size, block_index, count);

        let block_start = block_range.start;
        debug_assert_eq!((block_range.end - block_start) % count as Size, 0);

        Some(GeneralBlock {
            range: block_range,
            memory: Arc::clone(chunk.shared_memory()),
            block_index,
            chunk_index: chunk_index as u32,
            count,
            ptr: chunk.mapping_ptr().map(|ptr| unsafe {
                let offset = (block_start - chunk.range().start) as isize;
                NonNull::new_unchecked(ptr.as_ptr().offset(offset))
            }),
        })
    }

    /// Allocate `count` blocks from size entry.
    ///
    /// Note: at this level, `align` is no longer a power of 2.
    fn alloc_from_entry(
        &mut self,
        device: &B::Device,
        block_size: Size,
        count: u32,
        align: Size,
    ) -> Result<(GeneralBlock<B>, Size), hal::device::AllocationError> {
        log::trace!(
            "Allocate {} consecutive blocks for size {} from the entry",
            count,
            block_size
        );

        debug_assert!(count < MIN_BLOCKS_PER_CHUNK);
        debug_assert_eq!(
            block_size % align,
            0,
            "Requested entry block size {} is not aligned to {}",
            block_size,
            align
        );
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
            return Err(hal::device::OutOfMemory::Host.into());
        }

        // The estimated block count is a hint.
        // The actual count will be clamped between MIN and MAX.
        let estimated_block_count = size_entry.next_block_count(block_size);
        let (chunk, allocated) = self.alloc_chunk(device, block_size, estimated_block_count)?;
        log::trace!("\tChunk init mask: 0x{:x}", chunk.blocks);
        let size_entry = self.sizes.entry(block_size).or_default();
        let chunk_index = size_entry.chunks.insert(chunk);

        let block = Self::alloc_from_chunk(
            &mut size_entry.chunks,
            chunk_index,
            block_size,
            count,
            align,
        )
        .expect("New chunk should yield blocks");

        if !size_entry.chunks[chunk_index].is_exhausted() {
            size_entry.ready_chunks.insert(chunk_index);
        }

        Ok((block, allocated))
    }

    /// Allocate block.
    fn alloc_block(
        &mut self,
        device: &B::Device,
        block_size: Size,
        align: Size,
    ) -> Result<(GeneralBlock<B>, Size), hal::device::AllocationError> {
        log::trace!("Allocate block of size {}", block_size);

        debug_assert_eq!(
            block_size & (self.block_size_granularity - 1),
            0,
            "Requested block size {} is not aligned to the size granularity {}",
            block_size,
            self.block_size_granularity
        );
        debug_assert_eq!(
            block_size % align,
            0,
            "Requested block size {} is not aligned to {}",
            block_size,
            align
        );
        let size_entry = self.sizes.entry(block_size).or_default();

        let overhead = (MIN_BLOCKS_PER_CHUNK - 1) / size_entry.next_block_count(block_size);
        if overhead >= 1 && block_size >= LARGE_BLOCK_THRESHOLD {
            // this is chosen is such a way that the required `count`
            // is less than `MIN_BLOCKS_PER_CHUNK`.
            let ideal_chunk_size = crate::align_size(
                block_size * 2 / MIN_BLOCKS_PER_CHUNK as Size,
                crate::AtomSize::new(align).unwrap(),
            );
            let chunk_size = match self
                .chunks
                .range(ideal_chunk_size..block_size * overhead as Size)
                .find(|&size| size % align == 0)
            {
                Some(&size) => size,
                None => {
                    self.chunks.insert(ideal_chunk_size);
                    ideal_chunk_size
                }
            };

            self.alloc_from_entry(
                device,
                chunk_size,
                ((block_size - 1) / chunk_size + 1) as u32,
                align,
            )
        } else {
            self.chunks.insert(block_size);
            self.alloc_from_entry(device, block_size, 1, align)
        }
    }

    fn free_chunk(&mut self, device: &B::Device, chunk: Chunk<B>, block_size: Size) -> Size {
        log::trace!("Free chunk: {:#?}", chunk);
        assert!(chunk.is_unused(block_size));
        match chunk.flavor {
            ChunkFlavor::Dedicated { memory, .. } => {
                let size = memory.size();
                match Arc::try_unwrap(memory) {
                    Ok(mem) => unsafe {
                        if mem.is_mappable() {
                            device.unmap_memory(mem.raw());
                        }
                        device.free_memory(mem.into_raw());
                    },
                    Err(_) => {
                        log::error!("Allocated `Chunk` was freed, but memory is still shared and never will be destroyed");
                    }
                }
                size
            }
            ChunkFlavor::General(block) => self.free(device, block),
        }
    }

    fn free_block(&mut self, device: &B::Device, block: GeneralBlock<B>) -> Size {
        log::trace!("Free block: {:#?}", block);

        let block_size = block.size() / block.count as Size;
        let size_entry = self
            .sizes
            .get_mut(&block_size)
            .expect("Unable to get size entry from which block was allocated");
        let chunk_index = block.chunk_index as usize;
        let chunk = &mut size_entry.chunks[chunk_index];
        let block_index = block.block_index;
        let count = block.count;

        chunk.release_blocks(block_index, count);
        if chunk.is_unused(block_size) {
            size_entry.ready_chunks.remove(chunk_index);
            let chunk = size_entry.chunks.remove(chunk_index);
            drop(block); // it keeps an Arc reference to the chunk
            self.free_chunk(device, chunk, block_size)
        } else {
            size_entry.ready_chunks.insert(chunk_index);
            0
        }
    }

    /// Free the contents of the allocator.
    pub fn clear(&mut self, _device: &B::Device) -> Size {
        0
    }
}

impl<B: Backend> Allocator<B> for GeneralAllocator<B> {
    type Block = GeneralBlock<B>;

    const KIND: Kind = Kind::General;

    fn alloc(
        &mut self,
        device: &B::Device,
        size: Size,
        align: Size,
    ) -> Result<(GeneralBlock<B>, Size), hal::device::AllocationError> {
        debug_assert!(align.is_power_of_two());
        let aligned_size = ((size - 1) | (align - 1) | (self.block_size_granularity - 1)) + 1;
        let map_aligned_size = match self.non_coherent_atom_size {
            Some(atom) => crate::align_size(aligned_size, atom),
            None => aligned_size,
        };

        log::trace!(
            "Allocate general block: size: {}, align: {}, aligned size: {}, type: {}",
            size,
            align,
            map_aligned_size,
            self.memory_type.0
        );

        self.alloc_block(device, map_aligned_size, align)
    }

    fn free(&mut self, device: &B::Device, block: GeneralBlock<B>) -> Size {
        self.free_block(device, block)
    }
}

impl<B: Backend> Drop for GeneralAllocator<B> {
    fn drop(&mut self) {
        for (index, size) in self.sizes.drain() {
            if !thread::panicking() {
                assert_eq!(size.chunks.len(), 0, "SizeEntry({}) is still used", index);
            } else {
                log::error!("Memory leak: SizeEntry({}) is still used", index);
            }
        }
    }
}

/// Block allocated for chunk.
#[derive(Debug)]
enum ChunkFlavor<B: Backend> {
    /// Allocated from device.
    Dedicated {
        memory: Arc<Memory<B>>,
        ptr: Option<NonNull<u8>>,
    },
    /// Allocated from chunk of bigger blocks.
    General(GeneralBlock<B>),
}

#[derive(Debug)]
struct Chunk<B: Backend> {
    flavor: ChunkFlavor<B>,
    /// Each bit corresponds to a block, which is free if the bit is 1.
    blocks: BlockMask,
}

impl<B: Backend> Chunk<B> {
    fn from_memory(block_size: Size, memory: Memory<B>, ptr: Option<NonNull<u8>>) -> Self {
        let blocks = memory.size() / block_size;
        debug_assert!(blocks <= MAX_BLOCKS_PER_CHUNK as Size);

        let high_bit = 1 << (blocks - 1);

        Chunk {
            flavor: ChunkFlavor::Dedicated {
                memory: Arc::new(memory),
                ptr,
            },
            blocks: (high_bit - 1) | high_bit,
        }
    }

    fn from_block(block_size: Size, chunk_block: GeneralBlock<B>) -> Self {
        let blocks = (chunk_block.size() / block_size).min(MAX_BLOCKS_PER_CHUNK as Size);

        let high_bit = 1 << (blocks - 1);

        Chunk {
            flavor: ChunkFlavor::General(chunk_block),
            blocks: (high_bit - 1) | high_bit,
        }
    }

    fn shared_memory(&self) -> &Arc<Memory<B>> {
        match self.flavor {
            ChunkFlavor::Dedicated { ref memory, .. } => memory,
            ChunkFlavor::General(ref block) => &block.memory,
        }
    }

    fn range(&self) -> Range<Size> {
        match self.flavor {
            ChunkFlavor::Dedicated { ref memory, .. } => 0..memory.size(),
            ChunkFlavor::General(ref block) => block.range.clone(),
        }
    }

    // Get block bytes range
    fn blocks_range(&self, block_size: Size, block_index: u32, count: u32) -> Range<Size> {
        let range = self.range();
        let start = range.start + block_size * block_index as Size;
        let end = start + block_size * count as Size;
        debug_assert!(end <= range.end);
        start..end
    }

    fn num_blocks(&self, block_size: Size) -> Size {
        let range = self.range();
        ((range.end - range.start) / block_size).min(MAX_BLOCKS_PER_CHUNK as Size)
    }

    /// Check if there are free blocks.
    fn is_unused(&self, block_size: Size) -> bool {
        let blocks = self.num_blocks(block_size);

        let high_bit = 1 << (blocks - 1);
        let mask = (high_bit - 1) | high_bit;

        debug_assert!(self.blocks <= mask);
        self.blocks == mask
    }

    /// Check if there are free blocks.
    fn is_exhausted(&self) -> bool {
        self.blocks == 0
    }

    fn acquire_blocks(&mut self, count: u32, block_size: Size, align: Size) -> Option<u32> {
        debug_assert!(count > 0 && count <= MAX_BLOCKS_PER_CHUNK);

        // Holds a bit-array of all positions with `count` free blocks.
        let mut blocks: BlockMask = !0;
        for i in 0..count {
            blocks &= self.blocks >> i;
        }
        // Find a position in `blocks` that is aligned.
        while blocks != 0 {
            let index = blocks.trailing_zeros();
            blocks ^= 1 << index;

            if (index as Size * block_size) & (align - 1) == 0 {
                let mask = ((1 << count) - 1) << index;
                debug_assert_eq!(self.blocks & mask, mask);
                self.blocks ^= mask;
                log::trace!(
                    "Chunk acquired at {}, mask: 0x{:x} -> 0x{:x}",
                    index,
                    mask,
                    self.blocks
                );
                return Some(index);
            }
        }
        None
    }

    fn release_blocks(&mut self, index: u32, count: u32) {
        debug_assert!(index + count <= MAX_BLOCKS_PER_CHUNK);
        let mask = ((1 << count) - 1) << index;
        debug_assert_eq!(self.blocks & mask, 0);
        self.blocks |= mask;
        log::trace!(
            "Chunk released at {}, mask: 0x{:x} -> 0x{:x}",
            index,
            mask,
            self.blocks
        );
    }

    fn mapping_ptr(&self) -> Option<NonNull<u8>> {
        match self.flavor {
            ChunkFlavor::Dedicated { ptr, .. } => ptr,
            ChunkFlavor::General(ref block) => block.ptr,
        }
    }
}
