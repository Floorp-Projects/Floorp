use {
    core::fmt::{self, Display},
    gpu_alloc_types::{DeviceMapError, OutOfMemory},
};

/// Enumeration of possible errors that may occur during memory allocation.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum AllocationError {
    /// Backend reported that device memory has been exhausted.\
    /// Deallocating device memory from the same heap may increase chance
    /// that another allocation would succeed.
    OutOfDeviceMemory,

    /// Backend reported that host memory has been exhausted.\
    /// Deallocating host memory may increase chance that another allocation would succeed.
    OutOfHostMemory,

    /// Allocation request cannot be fulfilled as no available memory types allowed
    /// by `Request.memory_types` mask is compatible with `request.usage`.
    NoCompatibleMemoryTypes,

    /// Reached limit on allocated memory objects count.\
    /// Deallocating device memory may increase chance that another allocation would succeed.
    /// Especially dedicated memory blocks.
    ///
    /// If this error is returned when memory heaps are far from exhausted
    /// `Config` should be tweaked to allocate larger memory objects.
    TooManyObjects,
}

impl From<OutOfMemory> for AllocationError {
    fn from(err: OutOfMemory) -> Self {
        match err {
            OutOfMemory::OutOfDeviceMemory => AllocationError::OutOfDeviceMemory,
            OutOfMemory::OutOfHostMemory => AllocationError::OutOfHostMemory,
        }
    }
}

impl Display for AllocationError {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AllocationError::OutOfDeviceMemory => fmt.write_str("Device memory exhausted"),
            AllocationError::OutOfHostMemory => fmt.write_str("Host memory exhausted"),
            AllocationError::NoCompatibleMemoryTypes => fmt.write_str(
                "No compatible memory types from requested types support requested usage",
            ),
            AllocationError::TooManyObjects => {
                fmt.write_str("Reached limit on allocated memory objects count")
            }
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for AllocationError {}

/// Enumeration of possible errors that may occur during memory mapping.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum MapError {
    /// Backend reported that device memory has been exhausted.\
    /// Deallocating device memory from the same heap may increase chance
    /// that another mapping would succeed.
    OutOfDeviceMemory,

    /// Backend reported that host memory has been exhausted.\
    /// Deallocating host memory may increase chance that another mapping would succeed.
    OutOfHostMemory,

    /// Attempt to map memory block with non-host-visible memory type.\
    /// Ensure to include `UsageFlags::HOST_ACCESS` into allocation request
    /// when memory mapping is intended.
    NonHostVisible,

    /// Map failed for implementation specific reason.\
    /// For Vulkan backend this includes failed attempt
    /// to allocate large enough virtual address space.
    MapFailed,

    /// Mapping failed due to block being already mapped.
    AlreadyMapped,
}

impl From<DeviceMapError> for MapError {
    fn from(err: DeviceMapError) -> Self {
        match err {
            DeviceMapError::OutOfDeviceMemory => MapError::OutOfDeviceMemory,
            DeviceMapError::OutOfHostMemory => MapError::OutOfHostMemory,
            DeviceMapError::MapFailed => MapError::MapFailed,
        }
    }
}

impl From<OutOfMemory> for MapError {
    fn from(err: OutOfMemory) -> Self {
        match err {
            OutOfMemory::OutOfDeviceMemory => MapError::OutOfDeviceMemory,
            OutOfMemory::OutOfHostMemory => MapError::OutOfHostMemory,
        }
    }
}

impl Display for MapError {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            MapError::OutOfDeviceMemory => fmt.write_str("Device memory exhausted"),
            MapError::OutOfHostMemory => fmt.write_str("Host memory exhausted"),
            MapError::MapFailed => fmt.write_str("Failed to map memory object"),
            MapError::NonHostVisible => fmt.write_str("Impossible to map non-host-visible memory"),
            MapError::AlreadyMapped => fmt.write_str("Block is already mapped"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for MapError {}
