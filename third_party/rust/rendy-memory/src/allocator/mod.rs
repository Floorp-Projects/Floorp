//! This module provides `Allocator` trait and few allocators that implements the trait.

mod dedicated;
mod dynamic;
mod linear;

use crate::block::Block;

pub use self::{
    dedicated::{DedicatedAllocator, DedicatedBlock},
    dynamic::{DynamicAllocator, DynamicBlock, DynamicConfig},
    linear::{LinearAllocator, LinearBlock, LinearConfig},
};

/// Allocator kind.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Kind {
    /// Memory object per allocation.
    Dedicated,

    /// General purpose allocator.
    Dynamic,

    /// Allocates linearly.
    /// Fast and low overhead.
    /// Suitable for one-time-use allocations.
    Linear,
}

/// Allocator trait implemented for various allocators.
pub trait Allocator<B: gfx_hal::Backend> {
    /// Block type returned by allocator.
    type Block: Block<B>;

    /// Get allocator kind.
    fn kind() -> Kind;

    /// Allocate block of memory.
    /// On success returns allocated block and amount of memory consumed from device.
    fn alloc(
        &mut self,
        device: &B::Device,
        size: u64,
        align: u64,
    ) -> Result<(Self::Block, u64), gfx_hal::device::AllocationError>;

    /// Free block of memory.
    /// Returns amount of memory returned to the device.
    fn free(&mut self, device: &B::Device, block: Self::Block) -> u64;
}
