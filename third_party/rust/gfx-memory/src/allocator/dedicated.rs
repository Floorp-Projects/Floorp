use crate::{
    allocator::{Allocator, Kind},
    block::Block,
    mapping::MappedRange,
    memory::Memory,
    AtomSize, Size,
};
use hal::{device::Device as _, Backend};
use std::ptr::NonNull;

/// Memory block allocated from `DedicatedAllocator`.
#[derive(Debug)]
pub struct DedicatedBlock<B: Backend> {
    memory: Memory<B>,
    ptr: Option<NonNull<u8>>,
}

unsafe impl<B: Backend> Send for DedicatedBlock<B> {}
unsafe impl<B: Backend> Sync for DedicatedBlock<B> {}

impl<B: Backend> DedicatedBlock<B> {
    /// Get inner memory.
    /// Panics if mapped.
    pub fn unwrap_memory(self) -> Memory<B> {
        assert_eq!(self.ptr, None);
        self.memory
    }

    /// Make a non-mappable block.
    pub fn from_memory(memory: Memory<B>) -> Self {
        DedicatedBlock { memory, ptr: None }
    }

    /// Get the size of the block.
    pub fn size(&self) -> Size {
        self.memory.size()
    }
}

impl<B: Backend> Block<B> for DedicatedBlock<B> {
    fn properties(&self) -> hal::memory::Properties {
        self.memory.properties()
    }

    fn memory(&self) -> &B::Memory {
        self.memory.raw()
    }

    fn segment(&self) -> hal::memory::Segment {
        hal::memory::Segment {
            offset: 0,
            size: Some(self.memory.size()),
        }
    }

    fn map<'a>(
        &'a mut self,
        _device: &B::Device,
        segment: hal::memory::Segment,
    ) -> Result<MappedRange<'a, B>, hal::device::MapError> {
        let requested_range = segment.offset..match segment.size {
            Some(s) => segment.offset + s,
            None => self.memory.size(),
        };
        let mapping_range = match self.memory.non_coherent_atom_size {
            Some(atom) => crate::align_range(&requested_range, atom),
            None => requested_range.clone(),
        };

        Ok(unsafe {
            MappedRange::from_raw(
                &self.memory,
                self.ptr
                    //TODO: https://github.com/gfx-rs/gfx/issues/3182
                    .ok_or(hal::device::MapError::MappingFailed)?
                    .as_ptr()
                    .offset(mapping_range.start as isize),
                mapping_range,
                requested_range,
            )
        })
    }
}

/// Dedicated memory allocator that uses memory object per allocation requested.
///
/// This allocator suites best huge allocations.
/// From 32 MiB when GPU has 4-8 GiB memory total.
///
/// `Heaps` use this allocator when none of sub-allocators bound to the memory type
/// can handle size required.
/// TODO: Check if resource prefers dedicated memory.
#[derive(Debug)]
pub struct DedicatedAllocator {
    memory_type: hal::MemoryTypeId,
    memory_properties: hal::memory::Properties,
    non_coherent_atom_size: Option<AtomSize>,
    used: Size,
}

impl DedicatedAllocator {
    /// Create new `LinearAllocator`
    /// for `memory_type` with `memory_properties` specified
    pub fn new(
        memory_type: hal::MemoryTypeId,
        memory_properties: hal::memory::Properties,
        non_coherent_atom_size: Size,
    ) -> Self {
        DedicatedAllocator {
            memory_type,
            memory_properties,
            non_coherent_atom_size: if crate::is_non_coherent_visible(memory_properties) {
                AtomSize::new(non_coherent_atom_size)
            } else {
                None
            },
            used: 0,
        }
    }
}

impl<B: Backend> Allocator<B> for DedicatedAllocator {
    type Block = DedicatedBlock<B>;

    const KIND: Kind = Kind::Dedicated;

    fn alloc(
        &mut self,
        device: &B::Device,
        size: Size,
        _align: Size,
    ) -> Result<(DedicatedBlock<B>, Size), hal::device::AllocationError> {
        let size = match self.non_coherent_atom_size {
            Some(atom) => crate::align_size(size, atom),
            None => size,
        };
        log::trace!("Allocate block of size: {}", size);

        let (memory, ptr) = unsafe {
            super::allocate_memory_helper(
                device,
                self.memory_type,
                size,
                self.memory_properties,
                self.non_coherent_atom_size,
            )?
        };

        self.used += size;
        Ok((DedicatedBlock { memory, ptr }, size))
    }

    fn free(&mut self, device: &B::Device, block: DedicatedBlock<B>) -> Size {
        let size = block.memory.size();
        log::trace!("Free block of size: {}", size);
        self.used -= size;
        unsafe {
            device.unmap_memory(block.memory.raw());
            device.free_memory(block.memory.into_raw());
        }
        size
    }
}

impl Drop for DedicatedAllocator {
    fn drop(&mut self) {
        if self.used != 0 {
            log::error!("Not all allocation from DedicatedAllocator was freed");
        }
    }
}
