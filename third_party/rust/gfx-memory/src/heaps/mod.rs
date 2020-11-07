mod heap;
mod memory_type;

use self::{
    heap::MemoryHeap,
    memory_type::{BlockFlavor, MemoryType},
};
use crate::{
    allocator::*, block::Block, mapping::MappedRange, stats::TotalMemoryUtilization,
    usage::MemoryUsage, Size,
};

/// Possible errors returned by `Heaps`.
#[derive(Clone, Debug, PartialEq)]
pub enum HeapsError {
    /// Memory allocation failure.
    AllocationError(hal::device::AllocationError),
    /// No memory types among required for resource were found.
    NoSuitableMemory {
        /// Mask of the allowed memory types.
        mask: u32,
        /// Requested properties.
        properties: hal::memory::Properties,
    },
}

impl std::fmt::Display for HeapsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            HeapsError::AllocationError(e) => write!(f, "{:?}", e),
            HeapsError::NoSuitableMemory { mask, properties } => write!(
                f,
                "Memory type among ({}) with properties ({:?}) not found",
                mask, properties
            ),
        }
    }
}
impl std::error::Error for HeapsError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match *self {
            HeapsError::AllocationError(ref err) => Some(err),
            HeapsError::NoSuitableMemory { .. } => None,
        }
    }
}

impl From<hal::device::AllocationError> for HeapsError {
    fn from(error: hal::device::AllocationError) -> Self {
        HeapsError::AllocationError(error)
    }
}

impl From<hal::device::OutOfMemory> for HeapsError {
    fn from(error: hal::device::OutOfMemory) -> Self {
        HeapsError::AllocationError(error.into())
    }
}

/// Heaps available on particular physical device.
#[derive(Debug)]
pub struct Heaps<B: hal::Backend> {
    types: Vec<MemoryType<B>>,
    heaps: Vec<MemoryHeap>,
}

impl<B: hal::Backend> Heaps<B> {
    /// Initialize the new `Heaps` object.
    ///
    /// # Safety
    /// All later operations assume the device is not lost.
    pub unsafe fn new(
        hal_memory_properties: &hal::adapter::MemoryProperties,
        config_general: GeneralConfig,
        config_linear: LinearConfig,
        non_coherent_atom_size: Size,
    ) -> Self {
        Heaps {
            types: hal_memory_properties
                .memory_types
                .iter()
                .enumerate()
                .map(|(index, mt)| {
                    assert!(mt.heap_index < hal_memory_properties.memory_heaps.len());
                    MemoryType::new(
                        hal::MemoryTypeId(index),
                        mt,
                        config_general,
                        config_linear,
                        non_coherent_atom_size,
                    )
                })
                .collect(),
            heaps: hal_memory_properties
                .memory_heaps
                .iter()
                .map(|&size| MemoryHeap::new(size))
                .collect(),
        }
    }

    /// Allocate memory block give the `requirements` from gfx-hal.
    /// for intended `usage`, using the `kind` of allocator.
    pub fn allocate(
        &mut self,
        device: &B::Device,
        requirements: &hal::memory::Requirements,
        usage: MemoryUsage,
        kind: Kind,
    ) -> Result<MemoryBlock<B>, HeapsError> {
        let (memory_index, _, _) = {
            let suitable_types = self
                .types
                .iter()
                .enumerate()
                .filter(|(index, _)| (requirements.type_mask & (1u32 << index)) != 0)
                .filter_map(|(index, mt)| {
                    if mt.properties().contains(usage.properties_required()) {
                        let fitness = usage.memory_fitness(mt.properties());
                        Some((index, mt, fitness))
                    } else {
                        None
                    }
                });

            if suitable_types.clone().next().is_none() {
                return Err(HeapsError::NoSuitableMemory {
                    mask: requirements.type_mask,
                    properties: usage.properties_required(),
                });
            }

            suitable_types
                .filter(|(_, mt, _)| {
                    self.heaps[mt.heap_index()].available()
                        > requirements.size + requirements.alignment
                })
                .max_by_key(|&(_, _, fitness)| fitness)
                .ok_or_else(|| {
                    log::error!("All suitable heaps are exhausted. {:#?}", self);
                    hal::device::OutOfMemory::Device
                })?
        };

        self.allocate_from(
            device,
            memory_index as u32,
            kind,
            requirements.size,
            requirements.alignment,
        )
    }

    /// Allocate memory block
    /// from `memory_index` specified,
    /// for intended `usage`,
    /// with `size`
    /// and `align` requirements.
    fn allocate_from(
        &mut self,
        device: &B::Device,
        memory_index: u32,
        kind: Kind,
        size: Size,
        align: Size,
    ) -> Result<MemoryBlock<B>, HeapsError> {
        log::trace!(
            "Allocate memory block: type '{}', kind  '{:?}', size: '{}', align: '{}'",
            memory_index,
            kind,
            size,
            align
        );

        let memory_type = &mut self.types[memory_index as usize];
        let memory_heap = &mut self.heaps[memory_type.heap_index()];

        if memory_heap.available() < size {
            return Err(hal::device::OutOfMemory::Device.into());
        }

        let (flavor, allocated) = match memory_type.alloc(device, kind, size, align) {
            Ok(mapping) => mapping,
            Err(e) if kind == Kind::Linear => {
                log::warn!("Unable to allocate {:?} with {:?}: {:?}", size, kind, e);
                memory_type.alloc(device, Kind::Dedicated, size, align)?
            }
            Err(e) => return Err(e.into()),
        };
        memory_heap.allocated(allocated, flavor.size());

        Ok(MemoryBlock {
            flavor,
            memory_index,
        })
    }

    /// Free memory block.
    ///
    /// Memory block must be allocated from this heap.
    pub fn free(&mut self, device: &B::Device, block: MemoryBlock<B>) {
        let memory_index = block.memory_index;
        let size = block.flavor.size();
        log::trace!(
            "Free memory block: type '{}', size: '{}'",
            memory_index,
            size,
        );

        let memory_type = &mut self.types[memory_index as usize];
        let memory_heap = &mut self.heaps[memory_type.heap_index()];
        let freed = memory_type.free(device, block.flavor);
        memory_heap.freed(freed, size);
    }

    /// Clear allocators.
    /// Call this before dropping an instance of [`Heaps`]
    /// or if you are low on memory.
    ///
    /// Internally calls the clear methods on all
    /// internal [`LinearAllocator`] and [`GeneralAllocator`] instances.
    pub fn clear(&mut self, device: &B::Device) {
        for memory_type in self.types.iter_mut() {
            let memory_heap = &mut self.heaps[memory_type.heap_index()];
            let freed = memory_type.clear(device);
            memory_heap.freed(freed, 0);
        }
    }

    /// Get memory utilization.
    pub fn utilization(&self) -> TotalMemoryUtilization {
        TotalMemoryUtilization {
            heaps: self.heaps.iter().map(MemoryHeap::utilization).collect(),
            types: self.types.iter().map(MemoryType::utilization).collect(),
        }
    }
}

impl<B: hal::Backend> Drop for Heaps<B> {
    fn drop(&mut self) {
        for memory_heap in &self.heaps {
            let utilization = memory_heap.utilization();
            if utilization.utilization.used != 0 || utilization.utilization.effective != 0 {
                log::error!(
                    "Heaps not completely freed before drop. Utilization: {:?}",
                    utilization
                );
            }
        }
    }
}

/// Memory block allocated from `Heaps`.
#[derive(Debug)]
pub struct MemoryBlock<B: hal::Backend> {
    flavor: BlockFlavor<B>,
    memory_index: u32,
}

impl<B: hal::Backend> MemoryBlock<B> {
    /// Get memory type id.
    pub fn memory_type(&self) -> u32 {
        self.memory_index
    }
}

impl<B: hal::Backend> Block<B> for MemoryBlock<B> {
    fn properties(&self) -> hal::memory::Properties {
        match self.flavor {
            BlockFlavor::Dedicated(ref block) => block.properties(),
            BlockFlavor::General(ref block) => block.properties(),
            BlockFlavor::Linear(ref block) => block.properties(),
        }
    }

    fn memory(&self) -> &B::Memory {
        match self.flavor {
            BlockFlavor::Dedicated(ref block) => block.memory(),
            BlockFlavor::General(ref block) => block.memory(),
            BlockFlavor::Linear(ref block) => block.memory(),
        }
    }

    fn segment(&self) -> hal::memory::Segment {
        match self.flavor {
            BlockFlavor::Dedicated(ref block) => block.segment(),
            BlockFlavor::General(ref block) => block.segment(),
            BlockFlavor::Linear(ref block) => block.segment(),
        }
    }

    fn map<'a>(
        &'a mut self,
        device: &B::Device,
        segment: hal::memory::Segment,
    ) -> Result<MappedRange<'a, B>, hal::device::MapError> {
        match self.flavor {
            BlockFlavor::Dedicated(ref mut block) => block.map(device, segment),
            BlockFlavor::General(ref mut block) => block.map(device, segment),
            BlockFlavor::Linear(ref mut block) => block.map(device, segment),
        }
    }
}
