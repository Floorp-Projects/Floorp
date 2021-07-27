//! Structures and enums related to external memory errors.

use crate::device::OutOfMemory;

#[derive(Clone, Debug, PartialEq, thiserror::Error)]
/// Error while enumerating external image properties. Returned from [PhysicalDevice::external_image_properties][crate::adapter::PhysicalDevice::external_image_properties].
pub enum ExternalImagePropertiesError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] OutOfMemory),

    /// Requested image format not supported in combination with other parameters.
    #[error("Format not supported")]
    FormatNotSupported,
}

#[derive(Clone, Debug, PartialEq, thiserror::Error)]
/// Error while creating and allocating or importing an external buffer.
pub enum ExternalResourceError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] OutOfMemory),

    /// Cannot create any more objects.
    #[error("Too many objects")]
    TooManyObjects,

    /// All the desired memory type ids are invalid for the implementation..
    #[error("No valid memory type id among the desired ones")]
    NoValidMemoryTypeId,

    /// Invalid external handle.
    #[error("The used external handle or the combination of them is invalid")]
    InvalidExternalHandle,
}

#[derive(Clone, Debug, PartialEq, thiserror::Error)]
/// Error while exporting a memory. Returned from [Device::export_memory][crate::device::Device::export_memory].
pub enum ExternalMemoryExportError {
    /// Too many objects.
    #[error("Too many objects")]
    TooManyObjects,

    /// Out of host memory.
    #[error("Out of host memory")]
    OutOfHostMemory,

    /// Invalid external handle.
    #[error("Invalid external handle")]
    InvalidExternalHandle,
}
