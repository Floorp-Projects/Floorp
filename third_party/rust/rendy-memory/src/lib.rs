//! GPU memory management
//!

#![warn(
    missing_debug_implementations,
    missing_copy_implementations,
    missing_docs,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications
)]
mod allocator;
mod block;
mod heaps;
mod mapping;
mod memory;
mod usage;
mod util;
mod utilization;

pub use crate::{
    allocator::*,
    block::Block,
    heaps::{Heaps, HeapsConfig, HeapsError, MemoryBlock},
    mapping::{write::Write, Coherent, MappedRange, MaybeCoherent, NonCoherent},
    memory::Memory,
    usage::*,
    utilization::*,
};
