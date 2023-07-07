bitflags::bitflags! {
    /// Memory properties type.
    #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
    #[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
    pub struct MemoryPropertyFlags: u8 {
        /// This flag is set for device-local memory types.
        /// Device-local memory is situated "close" to the GPU cores
        /// and allows for fast access.
        const DEVICE_LOCAL = 0x01;

        /// This flag is set for host-visible memory types.
        /// Host-visible memory can be mapped to the host memory range.
        const HOST_VISIBLE = 0x02;

        /// This flag is set for host-coherent memory types.
        /// Host-coherent memory does not requires manual invalidation for
        /// modifications on GPU to become visible on host;
        /// nor flush for modification on host to become visible on GPU.
        /// Access synchronization is still required.
        const HOST_COHERENT = 0x04;

        /// This flag is set for host-cached memory types.
        /// Host-cached memory uses cache in host memory for faster reads from host.
        const HOST_CACHED = 0x08;

        /// This flag is set for lazily-allocated memory types.
        /// Lazily-allocated memory must be used (and only) for transient image attachments.
        const LAZILY_ALLOCATED = 0x10;

        /// This flag is set for protected memory types.
        /// Protected memory can be used for writing by protected operations
        /// and can be read only by protected operations.
        /// Protected memory cannot be host-visible.
        /// Implementation must guarantee that there is no way for data to flow
        /// from protected to unprotected memory.
        const PROTECTED = 0x20;
    }
}

/// Defines memory type.
#[derive(Clone, Copy, Debug)]
pub struct MemoryType {
    /// Heap index of the memory type.
    pub heap: u32,

    /// Property flags of the memory type.
    pub props: MemoryPropertyFlags,
}

/// Defines memory heap.
#[derive(Clone, Copy, Debug)]
pub struct MemoryHeap {
    /// Size of memory heap in bytes.
    pub size: u64,
}
