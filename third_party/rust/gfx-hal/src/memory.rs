//! Types to describe the properties of memory allocated for graphics resources.

use crate::{buffer, image, queue, Backend};
use std::ops::Range;

bitflags!(
    /// Memory property flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Properties: u16 {
        /// Device local memory on the GPU.
        const DEVICE_LOCAL = 0x1;

        /// Host visible memory can be accessed by the CPU.
        ///
        /// Backends must provide at least one cpu visible memory.
        const CPU_VISIBLE = 0x2;

        /// CPU-GPU coherent.
        ///
        /// Non-coherent memory requires explicit flushing.
        const COHERENT = 0x4;

        /// Cached memory by the CPU
        const CPU_CACHED = 0x8;

        /// Memory that may be lazily allocated as needed on the GPU
        /// and *must not* be visible to the CPU.
        const LAZILY_ALLOCATED = 0x10;
    }
);

bitflags!(
    /// Memory heap flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct HeapFlags: u16 {
        /// Device local memory on the GPU.
        const DEVICE_LOCAL = 0x1;
    }
);

bitflags!(
    /// Barrier dependency flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct Dependencies: u32 {
        /// Specifies the memory dependency to be framebuffer-local.
        const BY_REGION    = 0x1;
        ///
        const VIEW_LOCAL   = 0x2;
        ///
        const DEVICE_GROUP = 0x4;
    }
);

// DOC TODO: Could be better, but I don't know how to do this without
// trying to explain the whole synchronization model.
/// A [memory barrier](https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-memory-barriers)
/// type for either buffers or images.
#[allow(missing_docs)]
#[derive(Clone, Debug)]
pub enum Barrier<'a, B: Backend> {
    /// Applies the given access flags to all buffers in the range.
    AllBuffers(Range<buffer::Access>),
    /// Applies the given access flags to all images in the range.
    AllImages(Range<image::Access>),
    /// A memory barrier that defines access to a buffer.
    Buffer {
        /// The access flags controlling the buffer.
        states: Range<buffer::State>,
        /// The buffer the barrier controls.
        target: &'a B::Buffer,
        /// Subrange of the buffer the barrier applies to.
        range: buffer::SubRange,
        /// The source and destination Queue family IDs, for a [queue family ownership transfer](https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-queue-transfers)
        /// Can be `None` to indicate no ownership transfer.
        families: Option<Range<queue::QueueFamilyId>>,
    },
    /// A memory barrier that defines access to (a subset of) an image.
    Image {
        /// The access flags controlling the image.
        states: Range<image::State>,
        /// The image the barrier controls.
        target: &'a B::Image,
        /// A `SubresourceRange` that defines which section of an image the barrier applies to.
        range: image::SubresourceRange,
        /// The source and destination Queue family IDs, for a [queue family ownership transfer](https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-queue-transfers)
        /// Can be `None` to indicate no ownership transfer.
        families: Option<Range<queue::QueueFamilyId>>,
    },
}

impl<'a, B: Backend> Barrier<'a, B> {
    /// Create a barrier for the whole buffer between the given states.
    pub fn whole_buffer(target: &'a B::Buffer, states: Range<buffer::State>) -> Self {
        Barrier::Buffer {
            states,
            target,
            families: None,
            range: buffer::SubRange::WHOLE,
        }
    }
}

/// Memory requirements for a certain resource (buffer/image).
#[derive(Clone, Copy, Debug)]
pub struct Requirements {
    /// Size in the memory.
    pub size: u64,
    /// Memory alignment.
    pub alignment: u64,
    /// Supported memory types.
    pub type_mask: u32,
}

/// A linear segment within a memory block.
#[derive(Clone, Debug, Default, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Segment {
    /// Offset to the segment.
    pub offset: u64,
    /// Size of the segment, or None if unbound.
    pub size: Option<u64>,
}

impl Segment {
    /// All the memory available.
    pub const ALL: Self = Segment {
        offset: 0,
        size: None,
    };
}

/// Defines a single memory bind region.
///
/// This is used in the [`bind_sparse`][CommandQueue::bind_sparse] method to define a physical
/// store region for a buffer.
#[derive(Debug)]
pub struct SparseBind<M> {
    /// Offset into the (virtual) resource.
    pub resource_offset: u64,
    /// Size of the memory region to be bound.
    pub size: u64,
    /// Memory that the physical store is bound to, and the offset into the resource of the binding.
    ///
    /// Using `None` will unbind this range. Reading or writing to an unbound range is undefined
    /// behaviour in some older hardware.
    pub memory: Option<(M, u64)>,
}

/// Defines a single image memory bind region.
///
/// This is used in the [`bind_sparse`][CommandQueue::bind_sparse] method to define a physical
/// store region for a buffer.
#[derive(Debug)]
pub struct SparseImageBind<M> {
    /// Image aspect and region of interest in the image.
    pub subresource: image::Subresource,
    /// Coordinates of the first texel in the (virtual) image subresource to bind.
    pub offset: image::Offset,
    /// Extent of the (virtual) image subresource region to be bound.
    pub extent: image::Extent,
    /// Memory that the physical store is bound to, and the offset into the resource of the binding.
    ///
    /// Using `None` will unbind this range. Reading or writing to an unbound range is undefined
    /// behaviour in some older hardware.
    pub memory: Option<(M, u64)>,
}

bitflags!(
    /// Sparse flags for creating images and buffers.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct SparseFlags: u32 {
        /// Specifies the view will be backed using sparse memory binding.
        const SPARSE_BINDING = 0x0000_0001;
        /// Specifies the view can be partially backed with sparse memory binding.
        /// Must have `SPARSE_BINDING` enabled.
        const SPARSE_RESIDENCY = 0x0000_0002;
        /// Specifies the view will be backed using sparse memory binding with memory bindings that
        /// might alias other data. Must have `SPARSE_BINDING` enabled.
        const SPARSE_ALIASED = 0x0000_0004;
    }
);
