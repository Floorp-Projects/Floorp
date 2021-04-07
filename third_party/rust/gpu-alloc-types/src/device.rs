use {
    crate::types::{MemoryHeap, MemoryType},
    alloc::borrow::Cow,
    core::ptr::NonNull,
};

/// Memory exhausted error.
#[derive(Debug)]
pub enum OutOfMemory {
    /// Device memory exhausted.
    OutOfDeviceMemory,

    /// Host memory exhausted.
    OutOfHostMemory,
}

/// Memory mapped error.
#[derive(Debug)]
pub enum DeviceMapError {
    /// Device memory exhausted.
    OutOfDeviceMemory,

    /// Host memory exhausted.
    OutOfHostMemory,

    /// Map failed due to implementation specific error.
    MapFailed,
}

/// Specifies range of the mapped memory region.
#[derive(Debug)]
pub struct MappedMemoryRange<'a, M> {
    /// Memory object reference.
    pub memory: &'a M,

    /// Offset in bytes from start of the memory object.
    pub offset: u64,

    /// Size in bytes of the memory range.
    pub size: u64,
}

/// Properties of the device that will be used for allocating memory objects.
///
/// See `gpu-alloc-<backend>` crate to learn how to obtain one for backend of choice.
#[derive(Debug)]
pub struct DeviceProperties<'a> {
    /// Array of memory types provided by the device.
    pub memory_types: Cow<'a, [MemoryType]>,

    /// Array of memory heaps provided by the device.
    pub memory_heaps: Cow<'a, [MemoryHeap]>,

    /// Maximum number of valid memory allocations that can exist simultaneously within the device.
    pub max_memory_allocation_count: u32,

    /// Maximum size for single allocation supported by the device.
    pub max_memory_allocation_size: u64,

    /// Atom size for host mappable non-coherent memory.
    pub non_coherent_atom_size: u64,

    /// Specifies if feature required to fetch device address is enabled.
    pub buffer_device_address: bool,
}

bitflags::bitflags! {
    /// Allocation flags
    #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
    pub struct AllocationFlags : u8 {
        /// Specifies that the memory can be used for buffers created
        /// with flag that allows fetching device address.
        const DEVICE_ADDRESS = 0x1;
    }
}

/// Abstract device that can be used to allocate memory objects.
pub trait MemoryDevice<M> {
    /// Allocates new memory object from device.
    /// This function may be expensive and even limit maximum number of memory
    /// objects allocated.
    /// Which is the reason for sub-allocation this crate provides.
    ///
    /// # Safety
    ///
    /// `memory_type` must be valid index for memory type associated with this device.
    /// Retrieving this information is implementation specific.
    ///
    /// `flags` must be supported by the device.
    unsafe fn allocate_memory(
        &self,
        size: u64,
        memory_type: u32,
        flags: AllocationFlags,
    ) -> Result<M, OutOfMemory>;

    /// Deallocate memory object.
    ///
    /// # Safety
    ///
    /// Memory object must have been allocated from this device.\
    /// All clones of specified memory handle must be dropped before calling this function.
    unsafe fn deallocate_memory(&self, memory: M);

    /// Map region of device memory to host memory space.
    ///
    /// # Safety
    ///
    /// * Memory object must have been allocated from this device.
    /// * Memory object must not be already mapped.
    /// * Memory must be allocated from type with `HOST_VISIBLE` property.
    /// * `offset + size` must not overflow.
    /// * `offset + size` must not be larger than memory object size specified when
    ///   memory object was allocated from this device.
    unsafe fn map_memory(
        &self,
        memory: &mut M,
        offset: u64,
        size: u64,
    ) -> Result<NonNull<u8>, DeviceMapError>;

    /// Unmap previously mapped memory region.
    ///
    /// # Safety
    ///
    /// * Memory object must have been allocated from this device.
    /// * Memory object must be mapped
    unsafe fn unmap_memory(&self, memory: &mut M);

    /// Invalidates ranges of memory mapped regions.
    ///
    /// # Safety
    ///
    /// * Memory objects must have been allocated from this device.
    /// * `offset` and `size` in each element of `ranges` must specify
    ///   subregion of currently mapped memory region
    /// * if `memory` in some element of `ranges` does not contain `HOST_COHERENT` property
    ///   then `offset` and `size` of that element must be multiple of `non_coherent_atom_size`.
    unsafe fn invalidate_memory_ranges(
        &self,
        ranges: &[MappedMemoryRange<'_, M>],
    ) -> Result<(), OutOfMemory>;

    /// Flushes ranges of memory mapped regions.
    ///
    /// # Safety
    ///
    /// * Memory objects must have been allocated from this device.
    /// * `offset` and `size` in each element of `ranges` must specify
    ///   subregion of currently mapped memory region
    /// * if `memory` in some element of `ranges` does not contain `HOST_COHERENT` property
    ///   then `offset` and `size` of that element must be multiple of `non_coherent_atom_size`.
    unsafe fn flush_memory_ranges(
        &self,
        ranges: &[MappedMemoryRange<'_, M>],
    ) -> Result<(), OutOfMemory>;
}
