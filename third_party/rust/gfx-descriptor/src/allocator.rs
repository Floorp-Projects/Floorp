use {
    crate::counts::DescriptorCounts,
    hal::{
        device::{Device, OutOfMemory},
        pso::{AllocationError, DescriptorPool as _, DescriptorPoolCreateFlags},
        Backend,
    },
    std::{
        collections::{HashMap, VecDeque},
        hash::BuildHasherDefault,
    },
};

type PoolIndex = u32;

const MIN_SETS: u32 = 64;
const MAX_SETS: u32 = 512;

/// Descriptor set from allocator.
#[derive(Debug)]
pub struct DescriptorSet<B: Backend> {
    raw: B::DescriptorSet,
    pool_id: PoolIndex,
    counts: DescriptorCounts,
}

impl<B: Backend> DescriptorSet<B> {
    pub fn raw(&self) -> &B::DescriptorSet {
        &self.raw
    }
}

#[derive(Debug)]
struct Allocation<B: Backend> {
    sets: Vec<B::DescriptorSet>,
    pools: Vec<PoolIndex>,
}

impl<B: Backend> Allocation<B> {
    unsafe fn grow(
        &mut self,
        pool: &mut B::DescriptorPool,
        layout: &B::DescriptorSetLayout,
        count: u32,
    ) -> Result<(), OutOfMemory> {
        let sets_were = self.sets.len();
        match pool.allocate(std::iter::repeat(layout).take(count as usize), &mut self.sets) {
            Err(err) => {
                pool.free_sets(self.sets.drain(sets_were..));
                Err(match err {
                    AllocationError::Host => OutOfMemory::Host,
                    AllocationError::Device => OutOfMemory::Device,
                    _ => {
                        // We check pool for free descriptors and sets before calling this function,
                        // so it can't be exhausted.
                        // And it can't be fragmented either according to spec
                        //
                        // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VkDescriptorPoolCreateInfo
                        //
                        // """
                        // Additionally, if all sets allocated from the pool since it was created or most recently reset
                        // use the same number of descriptors (of each type) and the requested allocation also
                        // uses that same number of descriptors (of each type), then fragmentation must not cause an allocation failure
                        // """
                        panic!("Unexpected error: {:?}", err);
                    }
                })
            }
            Ok(()) => {
                assert_eq!(self.sets.len(), sets_were + count as usize);
                Ok(())
            }
        }
    }
}

#[derive(Debug)]
struct DescriptorPool<B: Backend> {
    raw: B::DescriptorPool,
    capacity: u32,
    // Number of available sets.
    available: u32,
}

#[derive(Debug)]
struct DescriptorBucket<B: Backend> {
    pools_offset: PoolIndex,
    pools: VecDeque<DescriptorPool<B>>,
    total: u64,
}

impl<B: Backend> DescriptorBucket<B> {
    fn new() -> Self {
        DescriptorBucket {
            pools_offset: 0,
            pools: VecDeque::new(),
            total: 0,
        }
    }

    fn new_pool_size(&self, count: u32) -> u32 {
        MIN_SETS // at least MIN_SETS
            .max(count) // at least enough for allocation
            .max(self.total.min(MAX_SETS as u64) as u32) // at least as much as was allocated so far capped to MAX_SETS
            .next_power_of_two() // rounded up to nearest 2^N
    }

    unsafe fn dispose(mut self, device: &B::Device) {
        if self.total > 0 {
            log::error!("Not all descriptor sets were deallocated");
        }

        for pool in self.pools.drain(..) {
            if pool.available < pool.capacity {
                log::error!(
                    "Descriptor pool is still in use during allocator disposal. {:?}",
                    pool
                );
            }
            device.destroy_descriptor_pool(pool.raw);
        }
    }

    unsafe fn allocate(
        &mut self,
        device: &B::Device,
        layout: &B::DescriptorSetLayout,
        layout_counts: &DescriptorCounts,
        mut count: u32,
        allocation: &mut Allocation<B>,
    ) -> Result<(), OutOfMemory> {
        if count == 0 {
            return Ok(());
        }

        for (index, pool) in self.pools.iter_mut().enumerate().rev() {
            if pool.available == 0 {
                continue;
            }

            let allocate = pool.available.min(count);
            log::trace!("Allocate {} from exising pool", allocate);
            allocation.grow(&mut pool.raw, layout, allocate)?;
            allocation.pools.extend(
                std::iter::repeat(index as PoolIndex + self.pools_offset).take(allocate as usize),
            );
            count -= allocate;
            pool.available -= allocate;
            self.total += allocate as u64;

            if count == 0 {
                return Ok(());
            }
        }

        while count > 0 {
            let size = self.new_pool_size(count);
            let pool_counts = layout_counts.multiply(size);
            log::trace!(
                "Create new pool with {} sets and {:?} descriptors",
                size,
                pool_counts,
            );
            let mut raw = device.create_descriptor_pool(
                size as usize,
                pool_counts.iter(),
                DescriptorPoolCreateFlags::FREE_DESCRIPTOR_SET,
            )?;

            let allocate = size.min(count);
            allocation.grow(&mut raw, layout, allocate)?;

            let index = self.pools.len();
            allocation.pools.extend(
                std::iter::repeat(index as PoolIndex + self.pools_offset).take(allocate as usize),
            );

            count -= allocate;
            self.pools.push_back(DescriptorPool {
                raw,
                capacity: size,
                available: size - allocate,
            });
            self.total += allocate as u64;
        }

        Ok(())
    }

    unsafe fn free(&mut self, sets: impl IntoIterator<Item = B::DescriptorSet>, pool_id: PoolIndex) {
        let pool = &mut self.pools[(pool_id - self.pools_offset) as usize];
        let mut count = 0;
        pool.raw.free_sets(sets.into_iter().map(|set| {
            count += 1;
            set
        }));
        pool.available += count;
        self.total -= count as u64;
        log::trace!("Freed {} from descriptor bucket", count);
    }

    unsafe fn cleanup(&mut self, device: &B::Device) {
        while let Some(pool) = self.pools.pop_front() {
            if pool.available < pool.capacity {
                self.pools.push_front(pool);
                break;
            }
            log::trace!("Destroying used up descriptor pool");
            device.destroy_descriptor_pool(pool.raw);
            self.pools_offset += 1;
        }
    }
}

/// Descriptor allocator.
/// Can be used to allocate descriptor sets for any layout.
#[derive(Debug)]
pub struct DescriptorAllocator<B: Backend> {
    buckets: HashMap<DescriptorCounts, DescriptorBucket<B>, BuildHasherDefault<fxhash::FxHasher>>,
    allocation: Allocation<B>,
    total: u64,
    free_sets: Vec<B::DescriptorSet>,
}

impl<B: Backend> Drop for DescriptorAllocator<B> {
    fn drop(&mut self) {
        if !self.buckets.is_empty() {
            log::error!("DescriptorAllocator is dropped");
        }
    }
}

impl<B: Backend> DescriptorAllocator<B> {
    /// Create new allocator instance.
    pub fn new() -> Self {
        DescriptorAllocator {
            buckets: HashMap::default(),
            allocation: Allocation {
                sets: Vec::new(),
                pools: Vec::new(),
            },
            total: 0,
            free_sets: Vec::new(),
        }
    }

    /// Clear the allocator instance.
    /// All sets allocated from this allocator become invalid.
    pub unsafe fn clear(&mut self, device: &B::Device) {
        for (_, bucket) in self.buckets.drain() {
            bucket.dispose(device);
        }
    }

    /// Allocate descriptor set with specified layout.
    /// `DescriptorCounts` must match descriptor numbers of the layout.
    /// `DescriptorCounts` can be constructed [from bindings] that were used
    /// to create layout instance.
    ///
    /// [from bindings]: .
    pub unsafe fn allocate(
        &mut self,
        device: &B::Device,
        layout: &B::DescriptorSetLayout,
        layout_counts: &DescriptorCounts,
        count: u32,
        extend: &mut impl Extend<DescriptorSet<B>>,
    ) -> Result<(), OutOfMemory> {
        if count == 0 {
            return Ok(());
        }

        log::trace!(
            "Allocating {} sets with layout {:?} @ {:?}",
            count,
            layout,
            layout_counts
        );

        let bucket = self
            .buckets
            .entry(layout_counts.clone())
            .or_insert_with(DescriptorBucket::new);
        match bucket.allocate(device, layout, layout_counts, count, &mut self.allocation) {
            Ok(()) => {
                extend.extend(
                    self.allocation.pools
                        .drain(..)
                        .zip(self.allocation.sets.drain(..))
                        .map(|(pool_id, set)| DescriptorSet {
                            raw: set,
                            counts: layout_counts.clone(),
                            pool_id,
                        }),
                );
                Ok(())
            }
            Err(err) => {
                // Free sets allocated so far.
                let mut last = None;
                for (index, pool_id) in self.allocation.pools.drain(..).enumerate().rev() {
                    if Some(pool_id) != last {
                        if let Some(last_id) = last {
                            // Free contiguous range of sets from one pool in one go.
                            bucket.free(self.allocation.sets.drain(index + 1..), last_id);
                        }
                        last = Some(pool_id);
                    }
                }

                if let Some(last_id) = last {
                    bucket.free(self.allocation.sets.drain(..), last_id);
                }

                Err(err)
            }
        }
    }

    /// Free descriptor sets.
    ///
    /// # Safety
    ///
    /// None of descriptor sets can be referenced in any pending command buffers.
    /// All command buffers where at least one of descriptor sets referenced
    /// move to invalid state.
    pub unsafe fn free(&mut self, all_sets: impl IntoIterator<Item = DescriptorSet<B>>) {
        let mut free_counts = DescriptorCounts::EMPTY;
        let mut free_pool_id: PoolIndex = !0;

        for set in all_sets {
            if free_counts != set.counts || free_pool_id != set.pool_id {
                if free_pool_id != !0 {
                    let bucket = self
                        .buckets
                        .get_mut(&free_counts)
                        .expect("Set should be allocated from this allocator");
                    debug_assert!(bucket.total >= self.free_sets.len() as u64);
                    bucket.free(self.free_sets.drain(..), free_pool_id);
                }
                free_counts = set.counts;
                free_pool_id = set.pool_id;
            }
            self.free_sets.push(set.raw);
        }

        if free_pool_id != !0 {
            let bucket = self
                .buckets
                .get_mut(&free_counts)
                .expect("Set should be allocated from this allocator");
            debug_assert!(bucket.total >= self.free_sets.len() as u64);

            bucket.free(self.free_sets.drain(..), free_pool_id);
        }
    }

    /// Perform cleanup to allow resources reuse.
    pub unsafe fn cleanup(&mut self, device: &B::Device) {
        for bucket in self.buckets.values_mut() {
            bucket.cleanup(device)
        }
    }
}
