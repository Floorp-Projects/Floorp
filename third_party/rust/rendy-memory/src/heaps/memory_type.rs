use {
    super::{BlockFlavor, HeapsConfig},
    crate::{allocator::*, usage::MemoryUsage, utilization::*},
    gfx_hal::memory::Properties,
};

#[derive(Debug)]
pub(super) struct MemoryType<B: gfx_hal::Backend> {
    heap_index: usize,
    properties: Properties,
    dedicated: DedicatedAllocator,
    linear: Option<LinearAllocator<B>>,
    dynamic: Option<DynamicAllocator<B>>,
    // chunk: Option<ChunkAllocator>,
    used: u64,
    effective: u64,
}

impl<B> MemoryType<B>
where
    B: gfx_hal::Backend,
{
    pub(super) fn new(
        memory_type: gfx_hal::MemoryTypeId,
        heap_index: usize,
        properties: Properties,
        config: HeapsConfig,
    ) -> Self {
        MemoryType {
            properties,
            heap_index,
            dedicated: DedicatedAllocator::new(memory_type, properties),
            linear: if properties.contains(Properties::CPU_VISIBLE) {
                config
                    .linear
                    .map(|config| LinearAllocator::new(memory_type, properties, config))
            } else {
                None
            },
            dynamic: config
                .dynamic
                .map(|config| DynamicAllocator::new(memory_type, properties, config)),
            used: 0,
            effective: 0,
        }
    }

    pub(super) fn properties(&self) -> Properties {
        self.properties
    }

    pub(super) fn heap_index(&self) -> usize {
        self.heap_index
    }

    pub(super) fn alloc(
        &mut self,
        device: &B::Device,
        usage: impl MemoryUsage,
        size: u64,
        align: u64,
    ) -> Result<(BlockFlavor<B>, u64), gfx_hal::device::AllocationError> {
        let (block, allocated) = self.alloc_impl(device, usage, size, align)?;
        self.effective += block.size();
        self.used += allocated;
        Ok((block, allocated))
    }

    fn alloc_impl(
        &mut self,
        device: &B::Device,
        usage: impl MemoryUsage,
        size: u64,
        align: u64,
    ) -> Result<(BlockFlavor<B>, u64), gfx_hal::device::AllocationError> {
        match (self.dynamic.as_mut(), self.linear.as_mut()) {
            (Some(dynamic), Some(linear)) => {
                if dynamic.max_allocation() >= size
                    && usage.allocator_fitness(Kind::Dynamic)
                        > usage.allocator_fitness(Kind::Linear)
                {
                    dynamic
                        .alloc(device, size, align)
                        .map(|(block, size)| (BlockFlavor::Dynamic(block), size))
                } else if linear.max_allocation() >= size
                    && usage.allocator_fitness(Kind::Linear) > 0
                {
                    linear
                        .alloc(device, size, align)
                        .map(|(block, size)| (BlockFlavor::Linear(block), size))
                } else {
                    self.dedicated
                        .alloc(device, size, align)
                        .map(|(block, size)| (BlockFlavor::Dedicated(block), size))
                }
            }
            (Some(dynamic), None) => {
                if dynamic.max_allocation() >= size && usage.allocator_fitness(Kind::Dynamic) > 0 {
                    dynamic
                        .alloc(device, size, align)
                        .map(|(block, size)| (BlockFlavor::Dynamic(block), size))
                } else {
                    self.dedicated
                        .alloc(device, size, align)
                        .map(|(block, size)| (BlockFlavor::Dedicated(block), size))
                }
            }
            (None, Some(linear)) => {
                if linear.max_allocation() >= size && usage.allocator_fitness(Kind::Linear) > 0 {
                    linear
                        .alloc(device, size, align)
                        .map(|(block, size)| (BlockFlavor::Linear(block), size))
                } else {
                    self.dedicated
                        .alloc(device, size, align)
                        .map(|(block, size)| (BlockFlavor::Dedicated(block), size))
                }
            }
            (None, None) => self
                .dedicated
                .alloc(device, size, align)
                .map(|(block, size)| (BlockFlavor::Dedicated(block), size)),
        }
    }

    pub(super) fn free(&mut self, device: &B::Device, block: BlockFlavor<B>) -> u64 {
        match block {
            BlockFlavor::Dedicated(block) => self.dedicated.free(device, block),
            BlockFlavor::Linear(block) => self.linear.as_mut().unwrap().free(device, block),
            BlockFlavor::Dynamic(block) => self.dynamic.as_mut().unwrap().free(device, block),
        }
    }

    pub(super) fn dispose(self, device: &B::Device) {
        log::trace!("Dispose memory allocators");

        if let Some(linear) = self.linear {
            linear.dispose(device);
            log::trace!("Linear allocator disposed");
        }
        if let Some(dynamic) = self.dynamic {
            dynamic.dispose();
            log::trace!("Dynamic allocator disposed");
        }
    }

    pub(super) fn utilization(&self) -> MemoryTypeUtilization {
        MemoryTypeUtilization {
            utilization: MemoryUtilization {
                used: self.used,
                effective: self.effective,
            },
            properties: self.properties,
            heap_index: self.heap_index,
        }
    }
}
