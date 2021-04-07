//!
//! Implementation agnostic memory allocator for Vulkan like APIs.
//!
//! This crate is intended to be used as part of safe API implementations.\
//! Use with caution. There are unsafe functions all over the place.
//!
//! # Usage
//!
//! Start with fetching `DeviceProperties` from `gfx-alloc-<backend>` crate for the backend of choice.\
//! Then create `GpuAllocator` instance and use it for all device memory allocations.\
//! `GpuAllocator` will take care for all necessary bookkeeping like memory object count limit,
//! heap budget and memory mapping.
//!
//! ### Backends implementations
//!
//! Backend supporting crates should not depend on this crate.\
//! Instead they should depend on `gpu-alloc-types` which is much more stable,
//! allowing to upgrade `gpu-alloc` version without `gfx-alloc-<backend>` upgrade.
//!

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

mod allocator;
mod block;
mod buddy;
mod config;
mod error;
mod heap;
mod linear;
mod slab;
mod usage;
mod util;

// experimental sub-allocator for large transient allocations.
#[cfg(feature = "freelist")]
mod freelist;

pub use {
    self::{allocator::*, block::MemoryBlock, config::*, error::*, usage::*},
    gpu_alloc_types::*,
};

/// Memory request for allocator.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Request {
    /// Minimal size of memory block required.
    /// Returned block may have larger size,
    /// use `MemoryBlock::size` to learn actual size of returned block.
    pub size: u64,

    /// Minimal alignment mask required.
    /// Returned block may have larger alignment,
    /// use `MemoryBlock::align` to learn actual alignment of returned block.
    pub align_mask: u64,

    /// Intended memory usage.
    /// Returned block may support additional usages,
    /// use `MemoryBlock::props` to learn memory properties of returned block.
    pub usage: UsageFlags,

    /// Bitset for memory types.
    /// Returned block will be from memory type corresponding to one of set bits,
    /// use `MemoryBlock::memory_type` to learn memory type index of returned block.
    pub memory_types: u32,
}

/// Aligns `value` up to `align_mask`
/// Returns smallest integer not lesser than `value` aligned by `align_mask`.
/// Returns `None` on overflow.
pub(crate) fn align_up(value: u64, align_mask: u64) -> Option<u64> {
    Some(value.checked_add(align_mask)? & !align_mask)
}

/// Align `value` down to `align_mask`
/// Returns largest integer not bigger than `value` aligned by `align_mask`.
pub(crate) fn align_down(value: u64, align_mask: u64) -> u64 {
    value & !align_mask
}

#[cfg(debug_assertions)]
#[allow(unused_unsafe)]
unsafe fn unreachable_unchecked() -> ! {
    unreachable!()
}

#[cfg(not(debug_assertions))]
unsafe fn unreachable_unchecked() -> ! {
    core::hint::unreachable_unchecked()
}

// #[cfg(feature = "tracing")]
use core::fmt::Debug as MemoryBounds;

// #[cfg(not(feature = "tracing"))]
// use core::any::Any as MemoryBounds;
