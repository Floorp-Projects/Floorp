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
    buffer, device, display, external_memory, format, image, memory,
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

    /// Get external buffer properties. The parameters specify how the buffer is going to used.
    /// # Arguments
    ///
    /// * `usage` - the usage of the buffer.
    /// * `sparse` - the sparse flags of the buffer.
    /// * `memory_type` - the external memory type for the buffer.
    fn external_buffer_properties(
        &self,
        usage: buffer::Usage,
        sparse: memory::SparseFlags,
        memory_type: external_memory::ExternalMemoryType,
    ) -> external_memory::ExternalMemoryProperties;

    /// Get external image properties. The parameters specify how the image is going to used.
    /// # Arguments
    ///
    /// * `format` - the format of the image.
    /// * `dimensions` - the dimensions of the image.
    /// * `tiling` - the tiling mode of the image.
    /// * `usage` - the usage of the image.
    /// * `view_caps` - the view capabilities of the image.
    /// * `external_memory_type` - the external memory type for the image.
    /// # Errors
    ///
    /// - Returns `OutOfMemory` if the implementation goes out of memory during the operation.
    /// - Returns `FormatNotSupported` if the implementation does not support the requested image format.
    ///
    fn external_image_properties(
        &self,
        format: format::Format,
        dimensions: u8,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
        external_memory_type: external_memory::ExternalMemoryType,
    ) -> Result<external_memory::ExternalMemoryProperties, external_memory::ExternalImagePropertiesError>;

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

    /// Enumerate active displays [surface][display::Display] from display.
    /// Please notice that, even if a system has displays attached, they could be not returned because they are managed by some other components.
    /// This function only return the display that are available to be managed by the current application.
    /// Since, generally, while compositor are running they take the control of every display connected, it could be better to run the application directly from the tty to avoid the return of an empty list.
    /// # Arguments
    ///
    /// * `adapter` - the [adapter][crate::adapter::Adapter] from which the displays will be enumerated.
    unsafe fn enumerate_displays(&self) -> Vec<display::Display<B>>;

    /// Enumerate compatibles planes with the provided display.
    /// # Arguments
    ///
    /// * `display` - display on which the the compatible planes will be listed.
    unsafe fn enumerate_compatible_planes(
        &self,
        display: &display::Display<B>,
    ) -> Vec<display::Plane>;

    /// Create a new display mode from a display, a resolution, a refresh_rate and the plane index.
    /// If the builtin display modes does not satisfy the requirements, this function will try to create a new one.
    /// # Arguments
    ///
    /// * `display` - display on which the display mode will be created.
    /// * `resolution` - the desired resolution.
    /// * `refresh_rate` - the desired refresh_rate.
    unsafe fn create_display_mode(
        &self,
        display: &display::Display<B>,
        resolution: (u32, u32),
        refresh_rate: u32,
    ) -> Result<display::DisplayMode<B>, display::DisplayModeError>;

    /// Create a display plane from a display, a resolution, a refresh_rate and a plane.
    /// If the builtin display modes does not satisfy the requirements, this function will try to create a new one.
    /// # Arguments
    ///
    /// * `display` - display on which the display plane will be created.
    /// * `plane` - the plane on which the surface will be rendered on.
    /// * `resolution` - the desired resolution.
    /// * `refresh_rate` - the desired refresh_rate.
    unsafe fn create_display_plane<'a>(
        &self,
        display: &'a display::DisplayMode<B>,
        plane: &'a display::Plane,
    ) -> Result<display::DisplayPlane<'a, B>, device::OutOfMemory>;
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
