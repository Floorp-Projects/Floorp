//! Physical devices and adapters.
//!
//! The `PhysicalDevice` trait specifies the API a backend must provide for dealing with
//! and querying a physical device, such as a particular GPU.  An `Adapter` is a struct
//! containing a `PhysicalDevice` and metadata for a particular GPU, generally created
//! from an `Instance` of that backend.  `adapter.open_with(...)` will return a `Device`
//! that has the properties specified.

use std::{any::Any, fmt};

use crate::{
    device,
    format,
    image,
    memory,
    queue::{QueueGroup, QueuePriority},
    Backend,
    Features,
    Hints,
    Limits,
};

/// A description for a single chunk of memory in a heap.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct MemoryType {
    /// Properties of the associated memory, such as synchronization
    /// properties or whether it's on the CPU or GPU.
    pub properties: memory::Properties,
    /// Index to the underlying memory heap in `Gpu::memory_heaps`
    pub heap_index: usize,
}

/// Types of memory supported by this adapter and available memory.
#[derive(Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct MemoryProperties {
    /// Each memory type is associated with one heap of `memory_heaps`.
    /// Multiple types can point to the same heap.
    pub memory_types: Vec<MemoryType>,
    /// Memory heaps with their size in bytes.
    pub memory_heaps: Vec<u64>,
}

/// Represents a combination of a logical device and the
/// hardware queues it provides.
///
/// This structure is typically created using an `Adapter`.
#[derive(Debug)]
pub struct Gpu<B: Backend> {
    /// Logical device for a given backend.
    pub device: B::Device,
    /// The command queues that the device provides.
    pub queue_groups: Vec<QueueGroup<B>>,
}

/// Represents a physical device (such as a GPU) capable of supporting the given backend.
pub trait PhysicalDevice<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Create a new logical device with the requested features. If `requested_features` is
    /// empty (e.g. through `Features::empty()`) then only the core features are supported.
    ///
    /// # Errors
    ///
    /// - Returns `TooManyObjects` if the implementation can't create a new logical device.
    /// - Returns `MissingFeature` if the implementation does not support a requested feature.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # extern crate gfx_backend_empty as empty;
    /// # extern crate gfx_hal;
    /// # fn main() {
    /// use gfx_hal::{adapter::PhysicalDevice, Features};
    ///
    /// # let physical_device: empty::PhysicalDevice = return;
    /// # let family: empty::QueueFamily = return;
    /// # unsafe {
    /// let gpu = physical_device.open(&[(&family, &[1.0; 1])], Features::empty());
    /// # }}
    /// ```
    unsafe fn open(
        &self,
        families: &[(&B::QueueFamily, &[QueuePriority])],
        requested_features: Features,
    ) -> Result<Gpu<B>, device::CreationError>;

    /// Fetch details for a particular format.
    fn format_properties(&self, format: Option<format::Format>) -> format::Properties;

    /// Fetch details for a particular image format.
    fn image_format_properties(
        &self,
        format: format::Format,
        dimensions: u8,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Option<image::FormatProperties>;

    /// Fetch details for the memory regions provided by the device.
    fn memory_properties(&self) -> MemoryProperties;

    /// Returns the features of this `PhysicalDevice`. This usually depends on the graphics API being
    /// used.
    fn features(&self) -> Features;

    /// Returns the performance hints of this `PhysicalDevice`.
    fn hints(&self) -> Hints;

    /// Returns the resource limits of this `PhysicalDevice`.
    fn limits(&self) -> Limits;

    /// Check cache compatibility with the `PhysicalDevice`.
    fn is_valid_cache(&self, _cache: &[u8]) -> bool {
        false
    }
}

/// Supported physical device types
#[derive(Clone, PartialEq, Eq, Debug)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum DeviceType {
    /// Other
    Other = 0,
    /// Integrated
    IntegratedGpu = 1,
    /// Discrete
    DiscreteGpu = 2,
    /// Virtual / Hosted
    VirtualGpu = 3,
    /// Cpu / Software Rendering
    Cpu = 4,
}

/// Metadata about a backend adapter.
#[derive(Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct AdapterInfo {
    /// Adapter name
    pub name: String,
    /// Vendor PCI id of the adapter
    pub vendor: usize,
    /// PCI id of the adapter
    pub device: usize,
    /// Type of device
    pub device_type: DeviceType,
}

/// The list of `Adapter` instances is obtained by calling `Instance::enumerate_adapters()`.
///
/// Given an `Adapter` a `Gpu` can be constructed by calling `PhysicalDevice::open()` on its
/// `physical_device` field. However, if only a single queue family is needed or if no
/// additional device features are required, then the `Adapter::open_with` convenience method
/// can be used instead.
#[derive(Debug)]
pub struct Adapter<B: Backend> {
    /// General information about this adapter.
    pub info: AdapterInfo,
    /// Actual physical device.
    pub physical_device: B::PhysicalDevice,
    /// Queue families supported by this adapter.
    pub queue_families: Vec<B::QueueFamily>,
}
