//! GPU memory management
//!

#![warn(
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
mod stats;
mod usage;

pub use crate::{
    allocator::*,
    block::Block,
    heaps::{Heaps, HeapsError, MemoryBlock},
    mapping::{MappedRange, Writer},
    memory::Memory,
    stats::*,
    usage::MemoryUsage,
};

use std::ops::Range;

/// Type for any memory sizes.
pub type Size = u64;
/// Type for non-coherent atom sizes.
pub type AtomSize = std::num::NonZeroU64;

fn is_non_coherent_visible(properties: hal::memory::Properties) -> bool {
    properties.contains(hal::memory::Properties::CPU_VISIBLE)
        && !properties.contains(hal::memory::Properties::COHERENT)
}

fn align_range(range: &Range<Size>, align: AtomSize) -> Range<Size> {
    let start = range.start - range.start % align.get();
    let end = ((range.end - 1) / align.get() + 1) * align.get();
    start..end
}

fn align_size(size: Size, align: AtomSize) -> Size {
    ((size - 1) / align.get() + 1) * align.get()
}

fn align_offset(value: Size, align: AtomSize) -> Size {
    debug_assert_eq!(align.get().count_ones(), 1);
    if value == 0 {
        0
    } else {
        1 + ((value - 1) | (align.get() - 1))
    }
}

fn segment_to_sub_range(
    segment: hal::memory::Segment,
    whole: &Range<Size>,
) -> Result<Range<Size>, hal::device::MapError> {
    let start = whole.start + segment.offset;
    match segment.size {
        Some(s) if start + s <= whole.end => Ok(start..start + s),
        None if start < whole.end => Ok(start..whole.end),
        _ => Err(hal::device::MapError::OutOfBounds),
    }
}

fn is_sub_range(sub: &Range<Size>, range: &Range<Size>) -> bool {
    sub.start >= range.start && sub.end <= range.end
}
