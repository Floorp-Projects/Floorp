use crate::{
    allocator::*,
    stats::MemoryTypeUtilization, MemoryUtilization,
    Size,
};
use hal::memory::Properties;


#[derive(Debug)]
pub(super) enum BlockFlavor<B: hal::Backend> {
    Dedicated(DedicatedBlock<B>),
    General(GeneralBlock<B>),
    Linear(LinearBlock<B>),
}

impl<B: hal::Backend> BlockFlavor<B> {
    pub(super) fn size(&self) -> Size {
        match self {
            BlockFlavor::Dedicated(block) => block.size(),
            BlockFlavor::General(block) => block.size(),
            BlockFlavor::Linear(block) => block.size(),
        }
    }
}

#[derive(Debug)]
pub(super) struct MemoryType<B: hal::Backend> {
    heap_index: usize,
    properties: Properties,
    dedicated: DedicatedAllocator,
    general: GeneralAllocator<B>,
    linear: LinearAllocator<B>,
    used: Size,
    effective: Size,
}

impl<B: hal::Backend> MemoryType<B> {
    pub(super) fn new(
        type_id: hal::MemoryTypeId,
        hal_memory_type: &hal::adapter::MemoryType,
        general_config: &GeneralConfig,
        linear_config: &LinearConfig,
        non_coherent_atom_size: Size,
    ) -> Self {
        MemoryType {
            heap_index: hal_memory_type.heap_index,
            properties: hal_memory_type.properties,
            dedicated: DedicatedAllocator::new(
                type_id,
                hal_memory_type.properties,
                non_coherent_atom_size,
            ),
            general: GeneralAllocator::new(
                type_id,
                hal_memory_type.properties,
                general_config,
                non_coherent_atom_size,
            ),
            linear: LinearAllocator::new(
                type_id,
                hal_memory_type.properties,
                linear_config,
                non_coherent_atom_size,
            ),
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
        kind: Kind,
        size: Size,
        align: Size,
    ) -> Result<(BlockFlavor<B>, Size), hal::device::AllocationError> {
        let (block, allocated) = match kind {
            Kind::Dedicated => {
                self.dedicated
                    .alloc(device, size, align)
                    .map(|(block, size)| (BlockFlavor::Dedicated(block), size))
            }
            Kind::General => {
                self.general
                    .alloc(device, size, align)
                    .map(|(block, size)| (BlockFlavor::General(block), size))
            }
            Kind::Linear => {
                self.linear
                    .alloc(device, size, align)
                    .map(|(block, size)| (BlockFlavor::Linear(block), size))
            }
        }?;
        self.effective += block.size();
        self.used += allocated;
        Ok((block, allocated))
    }

    pub(super) fn free(&mut self, device: &B::Device, block: BlockFlavor<B>) -> Size {
        match block {
            BlockFlavor::Dedicated(block) => self.dedicated.free(device, block),
            BlockFlavor::General(block) => self.general.free(device, block),
            BlockFlavor::Linear(block) => self.linear.free(device, block),
        }
    }

    pub(super) fn clear(&mut self, device: &B::Device) {
        log::trace!("Dispose memory allocators");
        self.general.clear(device);
        self.linear.clear(device);
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
