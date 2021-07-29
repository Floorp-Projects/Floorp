//! Physical graphics devices.
//!
//! The [`PhysicalDevice`][PhysicalDevice] trait specifies the API a backend
//! must provide for dealing with and querying a physical device, such as
//! a particular GPU.
//!
//! An [adapter][Adapter] is a struct containing a physical device and metadata
//! for a particular GPU, generally created from an [instance][crate::Instance]
//! of that [backend][crate::Backend].

use crate::{
    device, format, image, memory,
    queue::{QueueGroup, QueuePriority},
    Backend, Features, PhysicalDeviceProperties,
};

use std::{any::Any, fmt};

/// A description for a single type of memory in a heap.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct MemoryType {
    /// Properties of the associated memory, such as synchronization
    /// properties or whether it's on the CPU or GPU.
    pub properties: memory::Properties,
    /// Index to the underlying memory heap in `Gpu::memory_heaps`
    pub heap_index: usize,
}

/// A description for a memory heap.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct MemoryHeap {
    /// Total size of the heap.
    pub size: u64,
    /// Heap flags.
    pub flags: memory::HeapFlags,
}

/// Types of memory supported by this adapter and available memory.
#[derive(Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct MemoryProperties {
    /// Each memory type is associated with one heap of `memory_heaps`.
    /// Multiple types can point to the same heap.
    pub memory_types: Vec<MemoryType>,
    /// Memory heaps with their size in bytes.
    pub memory_heaps: Vec<MemoryHeap>,
}

/// Represents a combination of a [logical device][crate::device::Device] and the
/// [hardware queues][QueueGroup] it provides.
///
/// This structure is typically created from an [adapter][crate::adapter::Adapter].
#[derive(Debug)]
pub struct Gpu<B: Backend> {
    /// [Logical device][crate::device::Device] for a given backend.
    pub device: B::Device,
    /// The [command queues][crate::queue::Queue] that the device provides.
    pub queue_groups: Vec<QueueGroup<B>>,
}

/// Represents a physical device (such as a GPU) capable of supporting the given backend.
pub trait PhysicalDevice<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Create a new [logical device][crate::device::Device] with the requested features.
    /// If `requested_features` is [empty][crate::Features::empty], then only
    /// the core features are supported.
    ///
    /// # Arguments
    ///
    /// * `families` - which [queue families][crate::queue::family::QueueFamily]
    ///   to create queues from. The implementation may allocate more processing time to
    ///   the queues with higher [priority][QueuePriority].
    /// * `requested_features` - device features to enable. Must be a subset of
    ///   the [features][PhysicalDevice::features] supported by this device.
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
    /// used, as well as the actual platform underneath.
    fn features(&self) -> Features;

    /// Returns the properties of this `PhysicalDevice`. Similarly to `Features`, they
    // depend on the platform, but unlike features, these are immutable and can't be switched on.
    fn properties(&self) -> PhysicalDeviceProperties;

    /// Check cache compatibility with the `PhysicalDevice`.
    fn is_valid_cache(&self, _cache: &[u8]) -> bool {
        false
    }
}

/// The type of a physical graphics device
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
    /// CPU / Software Rendering
    Cpu = 4,
}

/// Metadata about a backend [adapter][Adapter].
#[derive(Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct AdapterInfo {
    /// Adapter name
    pub name: String,
    /// PCI ID of the device vendor
    pub vendor: usize,
    /// PCI ID of the device
    pub device: usize,
    /// Type of device
    pub device_type: DeviceType,
}

/// Information about a graphics device, supported by the backend.
///
/// The list of available adapters is obtained by calling
/// [`Instance::enumerate_adapters`][crate::Instance::enumerate_adapters].
///
/// To create a [`Gpu`][Gpu] from this type you can use the [`open`](PhysicalDevice::open)
/// method on its [`physical_device`][Adapter::physical_device] field.
#[derive(Debug)]
pub struct Adapter<B: Backend> {
    /// General information about this adapter.
    pub info: AdapterInfo,
    /// Actual [physical device][PhysicalDevice].
    pub physical_device: B::PhysicalDevice,
    /// [Queue families][crate::queue::family::QueueFamily] supported by this adapter.
    pub queue_families: Vec<B::QueueFamily>,
}
