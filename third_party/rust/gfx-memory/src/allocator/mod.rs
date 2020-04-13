//! This module provides `Allocator` trait and few allocators that implements the trait.

mod dedicated;
mod general;
mod linear;

pub use self::{
    dedicated::{DedicatedAllocator, DedicatedBlock},
    general::{GeneralAllocator, GeneralBlock, GeneralConfig},
    linear::{LinearAllocator, LinearBlock, LinearConfig},
};
use crate::{block::Block, memory::Memory, AtomSize, Size};
use std::ptr::NonNull;

/// Allocator kind.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Kind {
    /// Memory object per allocation.
    Dedicated,

    /// General purpose allocator.
    General,

    /// Allocates linearly.
    /// Fast and low overhead.
    /// Suitable for one-time-use allocations.
    Linear,
}

/// Allocator trait implemented for various allocators.
pub trait Allocator<B: hal::Backend> {
    /// Block type returned by allocator.
    type Block: Block<B>;

    /// Allocator kind.
    const KIND: Kind;

    /// Allocate block of memory.
    /// On success returns allocated block and amount of memory consumed from device.
    fn alloc(
        &mut self,
        device: &B::Device,
        size: Size,
        align: Size,
    ) -> Result<(Self::Block, Size), hal::device::AllocationError>;

    /// Free block of memory.
    /// Returns amount of memory returned to the device.
    fn free(&mut self, device: &B::Device, block: Self::Block) -> Size;
}

unsafe fn allocate_memory_helper<B: hal::Backend>(
    device: &B::Device,
    memory_type: hal::MemoryTypeId,
    size: Size,
    memory_properties: hal::memory::Properties,
    non_coherent_atom_size: Option<AtomSize>,
) -> Result<(Memory<B>, Option<NonNull<u8>>), hal::device::AllocationError> {
    use hal::device::Device as _;

    let raw = device.allocate_memory(memory_type, size)?;

    let ptr = if memory_properties.contains(hal::memory::Properties::CPU_VISIBLE) {
        match device.map_memory(&raw, hal::memory::Segment::ALL) {
            Ok(ptr) => NonNull::new(ptr),
            Err(hal::device::MapError::OutOfMemory(error)) => {
                device.free_memory(raw);
                return Err(error.into());
            }
            Err(e) => panic!("Unexpected mapping failure: {:?}", e),
        }
    } else {
        None
    };

    let memory = Memory::from_raw(raw, size, memory_properties, non_coherent_atom_size);

    Ok((memory, ptr))
}
