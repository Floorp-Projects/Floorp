//! Descriptor sets and layouts.
//!
//! A [`Descriptor`] is an object that describes the connection between a resource, such as
//! an `Image` or `Buffer`, and a variable in a shader. Descriptors are organized into
//! `DescriptorSet`s, each of which contains multiple descriptors that are bound and unbound to
//! shaders as a single unit. The contents of each descriptor in a set is defined by a
//! `DescriptorSetLayout` which is in turn built of [`DescriptorSetLayoutBinding`]s. A `DescriptorSet`
//! is then allocated from a [`DescriptorPool`] using the `DescriptorSetLayout`, and specific [`Descriptor`]s are
//! then bound to each binding point in the set using a [`DescriptorSetWrite`] and/or [`DescriptorSetCopy`].
//! Each descriptor set may contain descriptors to multiple different sorts of resources, and a shader may
//! use multiple descriptor sets at a time.
//!
//! [`Descriptor`]: enum.Descriptor.html
//! [`DescriptorSetLayoutBinding`]: struct.DescriptorSetLayoutBinding.html
//! [`DescriptorPool`]: trait.DescriptorPool.html
//! [`DescriptorSetWrite`]: struct.DescriptorSetWrite.html
//! [`DescriptorSetCopy`]: struct.DescriptorSetWrite.html

use std::{borrow::Borrow, fmt, iter};

use crate::{buffer::SubRange, image::Layout, pso::ShaderStageFlags, Backend, PseudoVec};

///
pub type DescriptorSetIndex = u16;
///
pub type DescriptorBinding = u32;
///
pub type DescriptorArrayIndex = usize;

/// Specific type of a buffer.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum BufferDescriptorType {
    /// Storage buffers allow load, store, and atomic operations.
    Storage {
        /// If true, store operations are not permitted on this buffer.
        read_only: bool,
    },
    /// Uniform buffers provide constant data to be accessed in a shader.
    Uniform,
}

/// Format of a buffer.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum BufferDescriptorFormat {
    /// The buffer is interpreted as a structure defined in a shader.
    Structured {
        /// If true, the buffer is accessed by an additional offset specified in
        /// the `offsets` parameter of `CommandBuffer::bind_*_descriptor_sets`.
        dynamic_offset: bool,
    },
    /// The buffer is interpreted as a 1-D array of texels, which undergo format
    /// conversion when loaded in a shader.
    Texel,
}

/// Specific type of an image descriptor.
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum ImageDescriptorType {
    /// A sampled image allows sampling operations.
    Sampled {
        /// If true, this descriptor corresponds to both a sampled image and a
        /// sampler to be used with that image.
        with_sampler: bool,
    },
    /// A storage image allows load, store and atomic operations.
    Storage {
        /// If true, store operations are not permitted on this image.
        read_only: bool,
    },
}

/// The type of a descriptor.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum DescriptorType {
    /// A descriptor associated with sampler.
    Sampler,
    /// A descriptor associated with an image.
    Image {
        /// The specific type of this image descriptor.
        ty: ImageDescriptorType,
    },
    /// A descriptor associated with a buffer.
    Buffer {
        /// The type of this buffer descriptor.
        ty: BufferDescriptorType,
        /// The format of this buffer descriptor.
        format: BufferDescriptorFormat,
    },
    /// A descriptor associated with an input attachment.
    InputAttachment,
}

/// Information about the contents of and in which stages descriptors may be bound to a descriptor
/// set at a certain binding point. Multiple `DescriptorSetLayoutBinding`s are assembled into
/// a `DescriptorSetLayout`, which is then allocated into a `DescriptorSet` using a
/// [`DescriptorPool`].
///
/// A descriptor set consists of multiple binding points.
/// Each binding point contains one or multiple descriptors of a certain type.
/// The binding point is only valid for the pipelines stages specified.
///
/// The binding _must_ match with the corresponding shader interface.
///
/// [`DescriptorPool`]: trait.DescriptorPool.html
#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DescriptorSetLayoutBinding {
    /// Descriptor bindings range.
    pub binding: DescriptorBinding,
    /// Type of the bound descriptors.
    pub ty: DescriptorType,
    /// Number of descriptors in the array.
    ///
    /// *Note*: If count is zero, the binding point is reserved
    /// and can't be accessed from any shader stages.
    pub count: DescriptorArrayIndex,
    /// Valid shader stages.
    pub stage_flags: ShaderStageFlags,
    /// Use the associated list of immutable samplers.
    pub immutable_samplers: bool,
}

/// Set of descriptors of a specific type.
#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DescriptorRangeDesc {
    /// Type of the stored descriptors.
    pub ty: DescriptorType,
    /// Amount of space.
    pub count: usize,
}

/// An error allocating descriptor sets from a pool.
#[derive(Clone, Debug, PartialEq)]
pub enum AllocationError {
    /// Memory allocation on the host side failed.
    /// This could be caused by a lack of memory or pool fragmentation.
    Host,
    /// Memory allocation on the host side failed.
    /// This could be caused by a lack of memory or pool fragmentation.
    Device,
    /// Memory allocation failed as there is not enough in the pool.
    /// This could be caused by too many descriptor sets being created.
    OutOfPoolMemory,
    /// Memory allocation failed due to pool fragmentation.
    FragmentedPool,
    /// Descriptor set allocation failed as the layout is incompatible with the pool.
    IncompatibleLayout,
}

impl std::fmt::Display for AllocationError {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AllocationError::Host => {
                write!(fmt, "Failed to allocate descriptor set: Out of host memory")
            }
            AllocationError::Device => write!(
                fmt,
                "Failed to allocate descriptor set: Out of device memory"
            ),
            AllocationError::OutOfPoolMemory => {
                write!(fmt, "Failed to allocate descriptor set: Out of pool memory")
            }
            AllocationError::FragmentedPool => {
                write!(fmt, "Failed to allocate descriptor set: Pool is fragmented")
            }
            AllocationError::IncompatibleLayout => write!(
                fmt,
                "Failed to allocate descriptor set: Incompatible layout"
            ),
        }
    }
}

impl std::error::Error for AllocationError {}

/// A descriptor pool is a collection of memory from which descriptor sets are allocated.
pub trait DescriptorPool<B: Backend>: Send + Sync + fmt::Debug {
    /// Allocate a descriptor set from the pool.
    ///
    /// The descriptor set will be allocated from the pool according to the corresponding set layout. However,
    /// specific descriptors must still be written to the set before use using a [`DescriptorSetWrite`] or
    /// [`DescriptorSetCopy`].
    ///
    /// Descriptors will become invalid once the pool is reset. Usage of invalidated descriptor sets results
    /// in undefined behavior.
    ///
    /// [`DescriptorSetWrite`]: struct.DescriptorSetWrite.html
    /// [`DescriptorSetCopy`]: struct.DescriptorSetCopy.html
    unsafe fn allocate_set(
        &mut self,
        layout: &B::DescriptorSetLayout,
    ) -> Result<B::DescriptorSet, AllocationError> {
        let mut result = PseudoVec(None);
        self.allocate(iter::once(layout), &mut result)?;
        Ok(result.0.unwrap())
    }

    /// Allocate multiple descriptor sets from the pool.
    ///
    /// The descriptor set will be allocated from the pool according to the corresponding set layout. However,
    /// specific descriptors must still be written to the set before use using a [`DescriptorSetWrite`] or
    /// [`DescriptorSetCopy`].
    ///
    /// Each descriptor set will be allocated from the pool according to the corresponding set layout.
    /// Descriptors will become invalid once the pool is reset. Usage of invalidated descriptor sets results
    /// in undefined behavior.
    ///
    /// [`DescriptorSetWrite`]: struct.DescriptorSetWrite.html
    /// [`DescriptorSetCopy`]: struct.DescriptorSetCopy.html
    unsafe fn allocate<I, E>(&mut self, layouts: I, list: &mut E) -> Result<(), AllocationError>
    where
        I: IntoIterator,
        I::Item: Borrow<B::DescriptorSetLayout>,
        E: Extend<B::DescriptorSet>,
    {
        for layout in layouts {
            let set = self.allocate_set(layout.borrow())?;
            list.extend(iter::once(set));
        }
        Ok(())
    }

    /// Free the given descriptor sets provided as an iterator.
    unsafe fn free_sets<I>(&mut self, descriptor_sets: I)
    where
        I: IntoIterator<Item = B::DescriptorSet>;

    /// Resets a descriptor pool, releasing all resources from all the descriptor sets
    /// allocated from it and freeing the descriptor sets. Invalidates all descriptor
    /// sets allocated from the pool; trying to use one after the pool has been reset
    /// is undefined behavior.
    unsafe fn reset(&mut self);
}

/// Writes the actual descriptors to be bound into a descriptor set. Should be provided
/// to the `write_descriptor_sets` method of a `Device`.
#[allow(missing_docs)]
#[derive(Debug)]
pub struct DescriptorSetWrite<'a, B: Backend, WI>
where
    WI: IntoIterator,
    WI::Item: Borrow<Descriptor<'a, B>>,
{
    pub set: &'a B::DescriptorSet,
    /// *Note*: when there is more descriptors provided than
    /// array elements left in the specified binding starting
    /// at specified, offset, the updates are spilled onto
    /// the next binding (starting with offset 0), and so on.
    pub binding: DescriptorBinding,
    pub array_offset: DescriptorArrayIndex,
    pub descriptors: WI,
}

/// A handle to a specific shader resource that can be bound for use in a `DescriptorSet`.
/// Usually provided in a [`DescriptorSetWrite`]
///
/// [`DescriptorSetWrite`]: struct.DescriptorSetWrite.html
#[allow(missing_docs)]
#[derive(Clone, Debug)]
pub enum Descriptor<'a, B: Backend> {
    Sampler(&'a B::Sampler),
    Image(&'a B::ImageView, Layout),
    CombinedImageSampler(&'a B::ImageView, Layout, &'a B::Sampler),
    Buffer(&'a B::Buffer, SubRange),
    TexelBuffer(&'a B::BufferView),
}

/// Copies a range of descriptors to be bound from one descriptor set to another Should be
/// provided to the `copy_descriptor_sets` method of a `Device`.
#[allow(missing_docs)]
#[derive(Clone, Copy, Debug)]
pub struct DescriptorSetCopy<'a, B: Backend> {
    pub src_set: &'a B::DescriptorSet,
    pub src_binding: DescriptorBinding,
    pub src_array_offset: DescriptorArrayIndex,
    pub dst_set: &'a B::DescriptorSet,
    pub dst_binding: DescriptorBinding,
    pub dst_array_offset: DescriptorArrayIndex,
    pub count: usize,
}

bitflags! {
    /// Descriptor pool creation flags.
    pub struct DescriptorPoolCreateFlags: u32 {
        /// Specifies that descriptor sets are allowed to be freed from the pool
        /// individually.
        const FREE_DESCRIPTOR_SET = 0x1;
    }
}
