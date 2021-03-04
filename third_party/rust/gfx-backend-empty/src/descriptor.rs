use hal::pso;
use log::debug;

/// Dummy descriptor pool.
#[derive(Debug)]
pub struct DescriptorPool;

impl pso::DescriptorPool<crate::Backend> for DescriptorPool {
    unsafe fn allocate_one(
        &mut self,
        _layout: &DescriptorSetLayout,
    ) -> Result<DescriptorSet, pso::AllocationError> {
        Ok(DescriptorSet {
            name: String::new(),
        })
    }

    unsafe fn free<I>(&mut self, descriptor_sets: I)
    where
        I: Iterator<Item = DescriptorSet>,
    {
        for _ in descriptor_sets {
            // Let the descriptor set drop
        }
    }

    unsafe fn reset(&mut self) {
        debug!("Resetting descriptor pool");
    }
}

#[derive(Debug)]
pub struct DescriptorSetLayout {
    /// User-defined name for this descriptor set layout
    pub(crate) name: String,
}

#[derive(Debug)]
pub struct DescriptorSet {
    /// User-defined name for this descriptor set
    pub(crate) name: String,
}
