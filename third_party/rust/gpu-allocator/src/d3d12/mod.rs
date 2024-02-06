#![deny(clippy::unimplemented, clippy::unwrap_used, clippy::ok_expect)]

use std::{backtrace::Backtrace, fmt, sync::Arc};

use log::{debug, warn, Level};

use windows::Win32::{Foundation::E_OUTOFMEMORY, Graphics::Direct3D12::*};

#[cfg(feature = "public-winapi")]
mod public_winapi {
    use super::*;
    pub use winapi::um::d3d12 as winapi_d3d12;

    /// Trait similar to [`AsRef`]/[`AsMut`],
    pub trait ToWinapi<T> {
        fn as_winapi(&self) -> *const T;
        fn as_winapi_mut(&mut self) -> *mut T;
    }

    /// [`windows`] types hold their pointer internally and provide drop semantics. As such this trait
    /// is usually implemented on the _pointer type_ (`*const`, `*mut`) of the [`winapi`] object so that
    /// a **borrow of** that pointer becomes a borrow of the [`windows`] type.
    pub trait ToWindows<T> {
        fn as_windows(&self) -> &T;
    }

    impl ToWinapi<winapi_d3d12::ID3D12Resource> for ID3D12Resource {
        fn as_winapi(&self) -> *const winapi_d3d12::ID3D12Resource {
            unsafe { std::mem::transmute_copy(self) }
        }

        fn as_winapi_mut(&mut self) -> *mut winapi_d3d12::ID3D12Resource {
            unsafe { std::mem::transmute_copy(self) }
        }
    }

    impl ToWinapi<winapi_d3d12::ID3D12Device> for ID3D12Device {
        fn as_winapi(&self) -> *const winapi_d3d12::ID3D12Device {
            unsafe { std::mem::transmute_copy(self) }
        }

        fn as_winapi_mut(&mut self) -> *mut winapi_d3d12::ID3D12Device {
            unsafe { std::mem::transmute_copy(self) }
        }
    }

    impl ToWindows<ID3D12Device> for *const winapi_d3d12::ID3D12Device {
        fn as_windows(&self) -> &ID3D12Device {
            unsafe { std::mem::transmute(self) }
        }
    }

    impl ToWindows<ID3D12Device> for *mut winapi_d3d12::ID3D12Device {
        fn as_windows(&self) -> &ID3D12Device {
            unsafe { std::mem::transmute(self) }
        }
    }

    impl ToWindows<ID3D12Device> for &mut winapi_d3d12::ID3D12Device {
        fn as_windows(&self) -> &ID3D12Device {
            unsafe { std::mem::transmute(self) }
        }
    }

    impl ToWinapi<winapi_d3d12::ID3D12Heap> for ID3D12Heap {
        fn as_winapi(&self) -> *const winapi_d3d12::ID3D12Heap {
            unsafe { std::mem::transmute_copy(self) }
        }

        fn as_winapi_mut(&mut self) -> *mut winapi_d3d12::ID3D12Heap {
            unsafe { std::mem::transmute_copy(self) }
        }
    }
}

#[cfg(feature = "public-winapi")]
pub use public_winapi::*;

#[cfg(feature = "visualizer")]
mod visualizer;
#[cfg(feature = "visualizer")]
pub use visualizer::AllocatorVisualizer;

use super::allocator;
use super::allocator::AllocationType;

use crate::{
    allocator::fmt_bytes, AllocationError, AllocationSizes, AllocatorDebugSettings, MemoryLocation,
    Result,
};

/// [`ResourceCategory`] is used for supporting [`D3D12_RESOURCE_HEAP_TIER_1`].
/// [`ResourceCategory`] will be ignored if device supports [`D3D12_RESOURCE_HEAP_TIER_2`].
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ResourceCategory {
    Buffer,
    RtvDsvTexture,
    OtherTexture,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ResourceStateOrBarrierLayout {
    ResourceState(D3D12_RESOURCE_STATES),
    BarrierLayout(D3D12_BARRIER_LAYOUT),
}

#[derive(Clone, Copy)]
pub struct ResourceCreateDesc<'a> {
    pub name: &'a str,
    pub memory_location: MemoryLocation,
    pub resource_category: ResourceCategory,
    pub resource_desc: &'a D3D12_RESOURCE_DESC,
    pub clear_value: Option<&'a D3D12_CLEAR_VALUE>,
    pub initial_state_or_layout: ResourceStateOrBarrierLayout,
    pub resource_type: &'a ResourceType<'a>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum HeapCategory {
    All,
    Buffer,
    RtvDsvTexture,
    OtherTexture,
}

impl From<ResourceCategory> for HeapCategory {
    fn from(resource_category: ResourceCategory) -> Self {
        match resource_category {
            ResourceCategory::Buffer => Self::Buffer,
            ResourceCategory::RtvDsvTexture => Self::RtvDsvTexture,
            ResourceCategory::OtherTexture => Self::OtherTexture,
        }
    }
}

impl From<&D3D12_RESOURCE_DESC> for ResourceCategory {
    fn from(desc: &D3D12_RESOURCE_DESC) -> Self {
        if desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER {
            Self::Buffer
        } else if (desc.Flags
            & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
            != D3D12_RESOURCE_FLAG_NONE
        {
            Self::RtvDsvTexture
        } else {
            Self::OtherTexture
        }
    }
}

#[cfg(feature = "public-winapi")]
impl From<&winapi_d3d12::D3D12_RESOURCE_DESC> for ResourceCategory {
    fn from(desc: &winapi_d3d12::D3D12_RESOURCE_DESC) -> Self {
        if desc.Dimension == winapi_d3d12::D3D12_RESOURCE_DIMENSION_BUFFER {
            Self::Buffer
        } else if (desc.Flags
            & (winapi_d3d12::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                | winapi_d3d12::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
            != 0
        {
            Self::RtvDsvTexture
        } else {
            Self::OtherTexture
        }
    }
}

#[derive(Clone, Debug)]
pub struct AllocationCreateDesc<'a> {
    /// Name of the allocation, for tracking and debugging purposes
    pub name: &'a str,
    /// Location where the memory allocation should be stored
    pub location: MemoryLocation,

    /// Size of allocation, should be queried using [`ID3D12Device::GetResourceAllocationInfo()`]
    pub size: u64,
    /// Alignment of allocation, should be queried using [`ID3D12Device::GetResourceAllocationInfo()`]
    pub alignment: u64,
    /// Resource category based on resource dimension and flags. Can be created from a [`D3D12_RESOURCE_DESC`]
    /// using the helper into function. The resource category is ignored when Resource Heap Tier 2 or higher
    /// is supported.
    pub resource_category: ResourceCategory,
}

impl<'a> AllocationCreateDesc<'a> {
    /// Helper conversion function utilizing [`winapi`] types.
    ///
    /// This function is also available for [`windows::Win32::Graphics::Direct3D12`]
    /// types as [`from_d3d12_resource_desc()`][Self::from_d3d12_resource_desc()].
    #[cfg(feature = "public-winapi")]
    pub fn from_winapi_d3d12_resource_desc(
        device: *const winapi_d3d12::ID3D12Device,
        desc: &winapi_d3d12::D3D12_RESOURCE_DESC,
        name: &'a str,
        location: MemoryLocation,
    ) -> AllocationCreateDesc<'a> {
        let device = device.as_windows();
        // Raw structs are binary-compatible
        let desc = unsafe { std::mem::transmute(desc) };
        let allocation_info =
            unsafe { device.GetResourceAllocationInfo(0, std::slice::from_ref(desc)) };
        let resource_category: ResourceCategory = desc.into();

        AllocationCreateDesc {
            name,
            location,
            size: allocation_info.SizeInBytes,
            alignment: allocation_info.Alignment,
            resource_category,
        }
    }

    /// Helper conversion function utilizing [`windows::Win32::Graphics::Direct3D12`] types.
    ///
    /// This function is also available for `winapi` types as `from_winapi_d3d12_resource_desc()`
    /// when the `public-winapi` feature is enabled.
    pub fn from_d3d12_resource_desc(
        device: &ID3D12Device,
        desc: &D3D12_RESOURCE_DESC,
        name: &'a str,
        location: MemoryLocation,
    ) -> AllocationCreateDesc<'a> {
        let allocation_info =
            unsafe { device.GetResourceAllocationInfo(0, std::slice::from_ref(desc)) };
        let resource_category: ResourceCategory = desc.into();

        AllocationCreateDesc {
            name,
            location,
            size: allocation_info.SizeInBytes,
            alignment: allocation_info.Alignment,
            resource_category,
        }
    }
}

#[derive(Clone, Debug)]
pub enum ID3D12DeviceVersion {
    /// Basic device compatible with legacy barriers only, i.e. can only be used in conjunction
    /// with [`ResourceStateOrBarrierLayout::ResourceState`].
    Device(ID3D12Device),
    /// Required for enhanced barrier support, i.e. when using
    /// [`ResourceStateOrBarrierLayout::BarrierLayout`].
    Device10(ID3D12Device10),
}

impl std::ops::Deref for ID3D12DeviceVersion {
    type Target = ID3D12Device;

    fn deref(&self) -> &Self::Target {
        match self {
            Self::Device(device) => device,
            // Windows-rs hides CanInto, we know that Device10 is a subclass of Device but there's not even a Deref.
            Self::Device10(device10) => windows::core::CanInto::can_into(device10),
        }
    }
}

#[derive(Debug)]
pub struct AllocatorCreateDesc {
    pub device: ID3D12DeviceVersion,
    pub debug_settings: AllocatorDebugSettings,
    pub allocation_sizes: AllocationSizes,
}

pub enum ResourceType<'a> {
    /// Allocation equivalent to Dx12's CommittedResource.
    Committed {
        heap_properties: &'a D3D12_HEAP_PROPERTIES,
        heap_flags: D3D12_HEAP_FLAGS,
    },
    /// Allocation equivalent to Dx12's PlacedResource.
    Placed,
}

#[derive(Debug)]
pub struct Resource {
    name: String,
    pub allocation: Option<Allocation>,
    resource: Option<ID3D12Resource>,
    pub memory_location: MemoryLocation,
    memory_type_index: Option<usize>,
    pub size: u64,
}

impl Resource {
    pub fn resource(&self) -> &ID3D12Resource {
        self.resource.as_ref().expect("Resource was already freed.")
    }
}

impl Drop for Resource {
    fn drop(&mut self) {
        if self.resource.is_some() {
            warn!("Dropping resource `{}` that was not freed. Call `Allocator::free_resource(resource)` instead.", self.name);
        }
    }
}

#[derive(Debug)]
pub struct CommittedAllocationStatistics {
    pub num_allocations: usize,
    pub total_size: u64,
}

#[derive(Debug)]
pub struct Allocation {
    chunk_id: Option<std::num::NonZeroU64>,
    offset: u64,
    size: u64,
    memory_block_index: usize,
    memory_type_index: usize,
    heap: ID3D12Heap,

    name: Option<Box<str>>,
}

impl Allocation {
    pub fn chunk_id(&self) -> Option<std::num::NonZeroU64> {
        self.chunk_id
    }

    /// Returns the [`ID3D12Heap`] object that is backing this allocation.
    /// This heap object can be shared with multiple other allocations and shouldn't be freed (or allocated from)
    /// without this library, because that will lead to undefined behavior.
    ///
    /// # Safety
    /// The result of this function be safely passed into [`ID3D12Device::CreatePlacedResource()`].
    /// It is exposed for this reason. Keep in mind to also pass [`Self::offset()`] along to it.
    pub unsafe fn heap(&self) -> &ID3D12Heap {
        &self.heap
    }

    /// Returns the offset of the allocation on the [`ID3D12Heap`].
    /// When creating a placed resources, this offset needs to be supplied as well.
    pub fn offset(&self) -> u64 {
        self.offset
    }

    /// Returns the size of the allocation
    pub fn size(&self) -> u64 {
        self.size
    }

    pub fn is_null(&self) -> bool {
        self.chunk_id.is_none()
    }
}

#[derive(Debug)]
struct MemoryBlock {
    heap: ID3D12Heap,
    size: u64,
    sub_allocator: Box<dyn allocator::SubAllocator>,
}
impl MemoryBlock {
    fn new(
        device: &ID3D12Device,
        size: u64,
        heap_properties: &D3D12_HEAP_PROPERTIES,
        heap_category: HeapCategory,
        dedicated: bool,
    ) -> Result<Self> {
        let heap = {
            let mut desc = D3D12_HEAP_DESC {
                SizeInBytes: size,
                Properties: *heap_properties,
                Alignment: D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT as u64,
                ..Default::default()
            };
            desc.Flags = match heap_category {
                HeapCategory::All => D3D12_HEAP_FLAG_NONE,
                HeapCategory::Buffer => D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
                HeapCategory::RtvDsvTexture => D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,
                HeapCategory::OtherTexture => D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
            };

            let mut heap = None;
            let hr = unsafe { device.CreateHeap(&desc, &mut heap) };
            match hr {
                Err(e) if e.code() == E_OUTOFMEMORY => Err(AllocationError::OutOfMemory),
                Err(e) => Err(AllocationError::Internal(format!(
                    "ID3D12Device::CreateHeap failed: {}",
                    e
                ))),
                Ok(()) => heap.ok_or_else(|| {
                    AllocationError::Internal(
                        "ID3D12Heap pointer is null, but should not be.".into(),
                    )
                }),
            }?
        };

        let sub_allocator: Box<dyn allocator::SubAllocator> = if dedicated {
            Box::new(allocator::DedicatedBlockAllocator::new(size))
        } else {
            Box::new(allocator::FreeListAllocator::new(size))
        };

        Ok(Self {
            heap,
            size,
            sub_allocator,
        })
    }
}

#[derive(Debug)]
struct MemoryType {
    memory_blocks: Vec<Option<MemoryBlock>>,
    committed_allocations: CommittedAllocationStatistics,
    memory_location: MemoryLocation,
    heap_category: HeapCategory,
    heap_properties: D3D12_HEAP_PROPERTIES,
    memory_type_index: usize,
    active_general_blocks: usize,
}

impl MemoryType {
    fn allocate(
        &mut self,
        device: &ID3D12DeviceVersion,
        desc: &AllocationCreateDesc<'_>,
        backtrace: Arc<Backtrace>,
        allocation_sizes: &AllocationSizes,
    ) -> Result<Allocation> {
        let allocation_type = AllocationType::Linear;

        let memblock_size = if self.heap_properties.Type == D3D12_HEAP_TYPE_DEFAULT {
            allocation_sizes.device_memblock_size
        } else {
            allocation_sizes.host_memblock_size
        };

        let size = desc.size;
        let alignment = desc.alignment;

        // Create a dedicated block for large memory allocations
        if size > memblock_size {
            let mem_block = MemoryBlock::new(
                device,
                size,
                &self.heap_properties,
                self.heap_category,
                true,
            )?;

            let block_index = self.memory_blocks.iter().position(|block| block.is_none());
            let block_index = match block_index {
                Some(i) => {
                    self.memory_blocks[i].replace(mem_block);
                    i
                }
                None => {
                    self.memory_blocks.push(Some(mem_block));
                    self.memory_blocks.len() - 1
                }
            };

            let mem_block = self.memory_blocks[block_index]
                .as_mut()
                .ok_or_else(|| AllocationError::Internal("Memory block must be Some".into()))?;

            let (offset, chunk_id) = mem_block.sub_allocator.allocate(
                size,
                alignment,
                allocation_type,
                1,
                desc.name,
                backtrace,
            )?;

            return Ok(Allocation {
                chunk_id: Some(chunk_id),
                size,
                offset,
                memory_block_index: block_index,
                memory_type_index: self.memory_type_index,
                heap: mem_block.heap.clone(),
                name: Some(desc.name.into()),
            });
        }

        let mut empty_block_index = None;
        for (mem_block_i, mem_block) in self.memory_blocks.iter_mut().enumerate().rev() {
            if let Some(mem_block) = mem_block {
                let allocation = mem_block.sub_allocator.allocate(
                    size,
                    alignment,
                    allocation_type,
                    1,
                    desc.name,
                    backtrace.clone(),
                );

                match allocation {
                    Ok((offset, chunk_id)) => {
                        return Ok(Allocation {
                            chunk_id: Some(chunk_id),
                            offset,
                            size,
                            memory_block_index: mem_block_i,
                            memory_type_index: self.memory_type_index,
                            heap: mem_block.heap.clone(),
                            name: Some(desc.name.into()),
                        });
                    }
                    Err(AllocationError::OutOfMemory) => {} // Block is full, continue search.
                    Err(err) => return Err(err),            // Unhandled error, return.
                }
            } else if empty_block_index.is_none() {
                empty_block_index = Some(mem_block_i);
            }
        }

        let new_memory_block = MemoryBlock::new(
            device,
            memblock_size,
            &self.heap_properties,
            self.heap_category,
            false,
        )?;

        let new_block_index = if let Some(block_index) = empty_block_index {
            self.memory_blocks[block_index] = Some(new_memory_block);
            block_index
        } else {
            self.memory_blocks.push(Some(new_memory_block));
            self.memory_blocks.len() - 1
        };

        self.active_general_blocks += 1;

        let mem_block = self.memory_blocks[new_block_index]
            .as_mut()
            .ok_or_else(|| AllocationError::Internal("Memory block must be Some".into()))?;
        let allocation = mem_block.sub_allocator.allocate(
            size,
            alignment,
            allocation_type,
            1,
            desc.name,
            backtrace,
        );
        let (offset, chunk_id) = match allocation {
            Err(AllocationError::OutOfMemory) => Err(AllocationError::Internal(
                "Allocation that must succeed failed. This is a bug in the allocator.".into(),
            )),
            a => a,
        }?;

        Ok(Allocation {
            chunk_id: Some(chunk_id),
            offset,
            size,
            memory_block_index: new_block_index,
            memory_type_index: self.memory_type_index,
            heap: mem_block.heap.clone(),
            name: Some(desc.name.into()),
        })
    }

    #[allow(clippy::needless_pass_by_value)]
    fn free(&mut self, allocation: Allocation) -> Result<()> {
        let block_idx = allocation.memory_block_index;

        let mem_block = self.memory_blocks[block_idx]
            .as_mut()
            .ok_or_else(|| AllocationError::Internal("Memory block must be Some.".into()))?;

        mem_block.sub_allocator.free(allocation.chunk_id)?;

        if mem_block.sub_allocator.is_empty() {
            if mem_block.sub_allocator.supports_general_allocations() {
                if self.active_general_blocks > 1 {
                    let block = self.memory_blocks[block_idx].take();
                    if block.is_none() {
                        return Err(AllocationError::Internal(
                            "Memory block must be Some.".into(),
                        ));
                    }
                    // Note that `block` will be destroyed on `drop` here

                    self.active_general_blocks -= 1;
                }
            } else {
                let block = self.memory_blocks[block_idx].take();
                if block.is_none() {
                    return Err(AllocationError::Internal(
                        "Memory block must be Some.".into(),
                    ));
                }
                // Note that `block` will be destroyed on `drop` here
            }
        }

        Ok(())
    }
}

pub struct Allocator {
    device: ID3D12DeviceVersion,
    debug_settings: AllocatorDebugSettings,
    memory_types: Vec<MemoryType>,
    allocation_sizes: AllocationSizes,
}

impl Allocator {
    pub fn device(&self) -> &ID3D12DeviceVersion {
        &self.device
    }

    pub fn new(desc: &AllocatorCreateDesc) -> Result<Self> {
        // Perform AddRef on the device
        let device = desc.device.clone();

        // Query device for feature level
        let mut options = Default::default();
        unsafe {
            device.CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS,
                <*mut D3D12_FEATURE_DATA_D3D12_OPTIONS>::cast(&mut options),
                std::mem::size_of_val(&options) as u32,
            )
        }
        .map_err(|e| {
            AllocationError::Internal(format!("ID3D12Device::CheckFeatureSupport failed: {}", e))
        })?;

        let is_heap_tier1 = options.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1;

        let heap_types = [
            (
                MemoryLocation::GpuOnly,
                D3D12_HEAP_PROPERTIES {
                    Type: D3D12_HEAP_TYPE_DEFAULT,
                    ..Default::default()
                },
            ),
            (
                MemoryLocation::CpuToGpu,
                D3D12_HEAP_PROPERTIES {
                    Type: D3D12_HEAP_TYPE_CUSTOM,
                    CPUPageProperty: D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE,
                    MemoryPoolPreference: D3D12_MEMORY_POOL_L0,
                    ..Default::default()
                },
            ),
            (
                MemoryLocation::GpuToCpu,
                D3D12_HEAP_PROPERTIES {
                    Type: D3D12_HEAP_TYPE_CUSTOM,
                    CPUPageProperty: D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
                    MemoryPoolPreference: D3D12_MEMORY_POOL_L0,
                    ..Default::default()
                },
            ),
        ];

        let heap_types = if is_heap_tier1 {
            heap_types
                .iter()
                .flat_map(|(memory_location, heap_properties)| {
                    [
                        (HeapCategory::Buffer, *memory_location, *heap_properties),
                        (
                            HeapCategory::RtvDsvTexture,
                            *memory_location,
                            *heap_properties,
                        ),
                        (
                            HeapCategory::OtherTexture,
                            *memory_location,
                            *heap_properties,
                        ),
                    ]
                    .to_vec()
                })
                .collect::<Vec<_>>()
        } else {
            heap_types
                .iter()
                .map(|(memory_location, heap_properties)| {
                    (HeapCategory::All, *memory_location, *heap_properties)
                })
                .collect::<Vec<_>>()
        };

        let memory_types = heap_types
            .iter()
            .enumerate()
            .map(
                |(i, &(heap_category, memory_location, heap_properties))| MemoryType {
                    memory_blocks: Vec::default(),
                    memory_location,
                    heap_category,
                    heap_properties,
                    memory_type_index: i,
                    active_general_blocks: 0,
                    committed_allocations: CommittedAllocationStatistics {
                        num_allocations: 0,
                        total_size: 0,
                    },
                },
            )
            .collect::<Vec<_>>();

        Ok(Self {
            memory_types,
            device,
            debug_settings: desc.debug_settings,
            allocation_sizes: desc.allocation_sizes,
        })
    }

    pub fn allocate(&mut self, desc: &AllocationCreateDesc<'_>) -> Result<Allocation> {
        let size = desc.size;
        let alignment = desc.alignment;

        let backtrace = Arc::new(if self.debug_settings.store_stack_traces {
            Backtrace::force_capture()
        } else {
            Backtrace::disabled()
        });

        if self.debug_settings.log_allocations {
            debug!(
                "Allocating `{}` of {} bytes with an alignment of {}.",
                &desc.name, size, alignment
            );
            if self.debug_settings.log_stack_traces {
                let backtrace = Backtrace::force_capture();
                debug!("Allocation stack trace: {}", backtrace);
            }
        }

        if size == 0 || !alignment.is_power_of_two() {
            return Err(AllocationError::InvalidAllocationCreateDesc);
        }

        // Find memory type
        let memory_type = self
            .memory_types
            .iter_mut()
            .find(|memory_type| {
                let is_location_compatible = desc.location == MemoryLocation::Unknown
                    || desc.location == memory_type.memory_location;

                let is_category_compatible = memory_type.heap_category == HeapCategory::All
                    || memory_type.heap_category == desc.resource_category.into();

                is_location_compatible && is_category_compatible
            })
            .ok_or(AllocationError::NoCompatibleMemoryTypeFound)?;

        memory_type.allocate(&self.device, desc, backtrace, &self.allocation_sizes)
    }

    pub fn free(&mut self, allocation: Allocation) -> Result<()> {
        if self.debug_settings.log_frees {
            let name = allocation.name.as_deref().unwrap_or("<null>");
            debug!("Freeing `{}`.", name);
            if self.debug_settings.log_stack_traces {
                let backtrace = Backtrace::force_capture();
                debug!("Free stack trace: {}", backtrace);
            }
        }

        if allocation.is_null() {
            return Ok(());
        }

        self.memory_types[allocation.memory_type_index].free(allocation)?;

        Ok(())
    }

    pub fn rename_allocation(&mut self, allocation: &mut Allocation, name: &str) -> Result<()> {
        allocation.name = Some(name.into());

        if allocation.is_null() {
            return Ok(());
        }

        let mem_type = &mut self.memory_types[allocation.memory_type_index];
        let mem_block = mem_type.memory_blocks[allocation.memory_block_index]
            .as_mut()
            .ok_or_else(|| AllocationError::Internal("Memory block must be Some.".into()))?;

        mem_block
            .sub_allocator
            .rename_allocation(allocation.chunk_id, name)?;

        Ok(())
    }

    pub fn report_memory_leaks(&self, log_level: Level) {
        for (mem_type_i, mem_type) in self.memory_types.iter().enumerate() {
            for (block_i, mem_block) in mem_type.memory_blocks.iter().enumerate() {
                if let Some(mem_block) = mem_block {
                    mem_block
                        .sub_allocator
                        .report_memory_leaks(log_level, mem_type_i, block_i);
                }
            }
        }
    }

    /// Create a resource according to the provided parameters.
    /// Created resources should be freed at the end of their lifetime by calling [`Self::free_resource()`].
    pub fn create_resource(&mut self, desc: &ResourceCreateDesc<'_>) -> Result<Resource> {
        match desc.resource_type {
            ResourceType::Committed {
                heap_properties,
                heap_flags,
            } => {
                let mut result: Option<ID3D12Resource> = None;

                let clear_value: Option<*const D3D12_CLEAR_VALUE> =
                    desc.clear_value.map(|v| -> *const _ { v });

                if let Err(e) = unsafe {
                    match (&self.device, desc.initial_state_or_layout) {
                        (device, ResourceStateOrBarrierLayout::ResourceState(initial_state)) => {
                            device.CreateCommittedResource(
                                *heap_properties,
                                *heap_flags,
                                desc.resource_desc,
                                initial_state,
                                clear_value,
                                &mut result,
                            )
                        }
                        (
                            ID3D12DeviceVersion::Device10(device),
                            ResourceStateOrBarrierLayout::BarrierLayout(initial_layout),
                        ) => {
                            let resource_desc1 = D3D12_RESOURCE_DESC1 {
                                Dimension: desc.resource_desc.Dimension,
                                Alignment: desc.resource_desc.Alignment,
                                Width: desc.resource_desc.Width,
                                Height: desc.resource_desc.Height,
                                DepthOrArraySize: desc.resource_desc.DepthOrArraySize,
                                MipLevels: desc.resource_desc.MipLevels,
                                Format: desc.resource_desc.Format,
                                SampleDesc: desc.resource_desc.SampleDesc,
                                Layout: desc.resource_desc.Layout,
                                Flags: desc.resource_desc.Flags,
                                // TODO: This is the only new field
                                SamplerFeedbackMipRegion: D3D12_MIP_REGION::default(),
                            };

                            device.CreateCommittedResource3(
                                *heap_properties,
                                *heap_flags,
                                &resource_desc1,
                                initial_layout,
                                clear_value,
                                None, // TODO
                                None, // TODO: https://github.com/microsoft/DirectX-Specs/blob/master/d3d/VulkanOn12.md#format-list-casting
                                &mut result,
                            )
                        }
                        _ => return Err(AllocationError::BarrierLayoutNeedsDevice10),
                    }
                } {
                    return Err(AllocationError::Internal(format!(
                        "ID3D12Device::CreateCommittedResource failed: {}",
                        e
                    )));
                }

                let resource = result.expect("Allocation succeeded but no resource was returned?");

                let allocation_info = unsafe {
                    self.device
                        .GetResourceAllocationInfo(0, &[*desc.resource_desc])
                };

                let memory_type = self
                    .memory_types
                    .iter_mut()
                    .find(|memory_type| {
                        let is_location_compatible = desc.memory_location
                            == MemoryLocation::Unknown
                            || desc.memory_location == memory_type.memory_location;

                        let is_category_compatible = memory_type.heap_category == HeapCategory::All
                            || memory_type.heap_category == desc.resource_category.into();

                        is_location_compatible && is_category_compatible
                    })
                    .ok_or(AllocationError::NoCompatibleMemoryTypeFound)?;

                memory_type.committed_allocations.num_allocations += 1;
                memory_type.committed_allocations.total_size += allocation_info.SizeInBytes;

                Ok(Resource {
                    name: desc.name.into(),
                    allocation: None,
                    resource: Some(resource),
                    size: allocation_info.SizeInBytes,
                    memory_location: desc.memory_location,
                    memory_type_index: Some(memory_type.memory_type_index),
                })
            }
            ResourceType::Placed => {
                let allocation_desc = {
                    let allocation_info = unsafe {
                        self.device
                            .GetResourceAllocationInfo(0, &[*desc.resource_desc])
                    };

                    AllocationCreateDesc {
                        name: desc.name,
                        location: desc.memory_location,
                        size: allocation_info.SizeInBytes,
                        alignment: allocation_info.Alignment,
                        resource_category: desc.resource_category,
                    }
                };

                let allocation = self.allocate(&allocation_desc)?;

                let mut result: Option<ID3D12Resource> = None;
                if let Err(e) = unsafe {
                    match (&self.device, desc.initial_state_or_layout) {
                        (device, ResourceStateOrBarrierLayout::ResourceState(initial_state)) => {
                            device.CreatePlacedResource(
                                allocation.heap(),
                                allocation.offset(),
                                desc.resource_desc,
                                initial_state,
                                None,
                                &mut result,
                            )
                        }
                        (
                            ID3D12DeviceVersion::Device10(device),
                            ResourceStateOrBarrierLayout::BarrierLayout(initial_layout),
                        ) => {
                            let resource_desc1 = D3D12_RESOURCE_DESC1 {
                                Dimension: desc.resource_desc.Dimension,
                                Alignment: desc.resource_desc.Alignment,
                                Width: desc.resource_desc.Width,
                                Height: desc.resource_desc.Height,
                                DepthOrArraySize: desc.resource_desc.DepthOrArraySize,
                                MipLevels: desc.resource_desc.MipLevels,
                                Format: desc.resource_desc.Format,
                                SampleDesc: desc.resource_desc.SampleDesc,
                                Layout: desc.resource_desc.Layout,
                                Flags: desc.resource_desc.Flags,
                                // TODO: This is the only new field
                                SamplerFeedbackMipRegion: D3D12_MIP_REGION::default(),
                            };
                            device.CreatePlacedResource2(
                                allocation.heap(),
                                allocation.offset(),
                                &resource_desc1,
                                initial_layout,
                                None,
                                None, // TODO: https://github.com/microsoft/DirectX-Specs/blob/master/d3d/VulkanOn12.md#format-list-casting
                                &mut result,
                            )
                        }
                        _ => return Err(AllocationError::BarrierLayoutNeedsDevice10),
                    }
                } {
                    return Err(AllocationError::Internal(format!(
                        "ID3D12Device::CreatePlacedResource failed: {}",
                        e
                    )));
                }

                let resource = result.expect("Allocation succeeded but no resource was returned?");
                let size = allocation.size();
                Ok(Resource {
                    name: desc.name.into(),
                    allocation: Some(allocation),
                    resource: Some(resource),
                    size,
                    memory_location: desc.memory_location,
                    memory_type_index: None,
                })
            }
        }
    }

    /// Free a resource and its memory.
    pub fn free_resource(&mut self, mut resource: Resource) -> Result<()> {
        // Explicitly drop the resource (which is backed by a refcounted COM object)
        // before freeing allocated memory. Windows-rs performs a Release() on drop().
        let _ = resource
            .resource
            .take()
            .expect("Resource was already freed.");

        if let Some(allocation) = resource.allocation.take() {
            self.free(allocation)
        } else {
            // Dx12 CommittedResources do not have an application managed allocation.
            // We only have to update the tracked allocation count and memory usage.
            if let Some(memory_type_index) = resource.memory_type_index {
                let memory_type = &mut self.memory_types[memory_type_index];

                memory_type.committed_allocations.num_allocations -= 1;
                memory_type.committed_allocations.total_size -= resource.size;
            }
            Ok(())
        }
    }
}

impl fmt::Debug for Allocator {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut allocation_report = vec![];
        let mut total_reserved_size_in_bytes = 0;

        for memory_type in &self.memory_types {
            for block in memory_type.memory_blocks.iter().flatten() {
                total_reserved_size_in_bytes += block.size;
                allocation_report.extend(block.sub_allocator.report_allocations())
            }
        }

        let total_used_size_in_bytes = allocation_report.iter().map(|report| report.size).sum();

        allocation_report.sort_by_key(|alloc| std::cmp::Reverse(alloc.size));

        writeln!(
            f,
            "================================================================",
        )?;
        writeln!(
            f,
            "ALLOCATION BREAKDOWN ({} / {})",
            fmt_bytes(total_used_size_in_bytes),
            fmt_bytes(total_reserved_size_in_bytes),
        )?;

        let max_num_allocations_to_print = f.precision().map_or(usize::MAX, |n| n);
        for (idx, alloc) in allocation_report.iter().enumerate() {
            if idx >= max_num_allocations_to_print {
                break;
            }
            writeln!(
                f,
                "{:max_len$.max_len$}\t- {}",
                alloc.name,
                fmt_bytes(alloc.size),
                max_len = allocator::VISUALIZER_TABLE_MAX_ENTRY_NAME_LEN,
            )?;
        }

        Ok(())
    }
}

impl Drop for Allocator {
    fn drop(&mut self) {
        if self.debug_settings.log_leaks_on_shutdown {
            self.report_memory_leaks(Level::Warn);
        }

        // Because Rust drop rules drop members in source-code order (that would be the
        // ID3D12Device before the ID3D12Heaps nested in these memory blocks), free
        // all remaining memory blocks manually first by dropping.
        for mem_type in self.memory_types.iter_mut() {
            mem_type.memory_blocks.clear();
        }
    }
}
