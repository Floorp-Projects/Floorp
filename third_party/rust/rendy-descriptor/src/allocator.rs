use {
    crate::ranges::*,
    gfx_hal::{
        device::{Device, OutOfMemory},
        pso::{AllocationError, DescriptorPool as _, DescriptorPoolCreateFlags},
        Backend,
    },
    smallvec::{smallvec, SmallVec},
    std::{
        collections::{HashMap, VecDeque},
        ops::Deref,
    },
};

const MIN_SETS: u32 = 64;
const MAX_SETS: u32 = 512;

/// Descriptor set from allocator.
#[derive(Debug)]
pub struct DescriptorSet<B: Backend> {
    raw: B::DescriptorSet,
    pool: u64,
    ranges: DescriptorRanges,
}

impl<B> DescriptorSet<B>
where
    B: Backend,
{
    /// Get raw set
    pub fn raw(&self) -> &B::DescriptorSet {
        &self.raw
    }

    /// Get raw set
    /// It must not be replaced.
    pub unsafe fn raw_mut(&mut self) -> &mut B::DescriptorSet {
        &mut self.raw
    }
}

impl<B> Deref for DescriptorSet<B>
where
    B: Backend,
{
    type Target = B::DescriptorSet;

    fn deref(&self) -> &B::DescriptorSet {
        &self.raw
    }
}

#[derive(Debug)]
struct Allocation<B: Backend> {
    sets: SmallVec<[B::DescriptorSet; 1]>,
    pools: Vec<u64>,
}

#[derive(Debug)]
struct DescriptorPool<B: Backend> {
    raw: B::DescriptorPool,
    size: u32,

    // Number of free sets left.
    free: u32,

    // Number of sets freed (they can't be reused until gfx-hal 0.2)
    freed: u32,
}

unsafe fn allocate_from_pool<B: Backend>(
    raw: &mut B::DescriptorPool,
    layout: &B::DescriptorSetLayout,
    count: u32,
    allocation: &mut SmallVec<[B::DescriptorSet; 1]>,
) -> Result<(), OutOfMemory> {
    let sets_were = allocation.len();
    raw.allocate_sets(std::iter::repeat(layout).take(count as usize), allocation)
        .map_err(|err| match err {
            AllocationError::Host => OutOfMemory::Host,
            AllocationError::Device => OutOfMemory::Device,
            err => {
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
        })?;
    assert_eq!(allocation.len(), sets_were + count as usize);
    Ok(())
}

#[derive(Debug)]
struct DescriptorBucket<B: Backend> {
    pools_offset: u64,
    pools: VecDeque<DescriptorPool<B>>,
    total: u64,
}

impl<B> DescriptorBucket<B>
where
    B: Backend,
{
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

        while let Some(pool) = self.pools.pop_front() {
            assert!(pool.freed + pool.free <= pool.size);
            if pool.freed + pool.free < pool.size {
                log::error!(
                    "Descriptor pool is still in use during allocator disposal. {:?}",
                    pool
                );
            } else {
                log::trace!("Destroying used up descriptor pool");
                device.destroy_descriptor_pool(pool.raw);
                self.pools_offset += 1;
            }
        }

        self.pools
            .drain(..)
            .for_each(|pool| device.destroy_descriptor_pool(pool.raw));
    }

    unsafe fn allocate(
        &mut self,
        device: &B::Device,
        layout: &B::DescriptorSetLayout,
        layout_ranges: DescriptorRanges,
        mut count: u32,
        allocation: &mut Allocation<B>,
    ) -> Result<(), OutOfMemory> {
        if count == 0 {
            return Ok(());
        }

        for (index, pool) in self.pools.iter_mut().enumerate().rev() {
            if pool.free == 0 {
                continue;
            }

            let allocate = pool.free.min(count);
            log::trace!("Allocate {} from exising pool", allocate);
            allocate_from_pool::<B>(&mut pool.raw, layout, allocate, &mut allocation.sets)?;
            allocation.pools.extend(
                std::iter::repeat(index as u64 + self.pools_offset).take(allocate as usize),
            );
            count -= allocate;
            pool.free -= allocate;
            self.total += allocate as u64;

            if count == 0 {
                return Ok(());
            }
        }

        while count > 0 {
            let size = self.new_pool_size(count);
            let pool_ranges = layout_ranges * size;
            log::trace!(
                "Create new pool with {} sets and {:?} descriptors",
                size,
                pool_ranges,
            );
            let raw = device.create_descriptor_pool(
                size as usize,
                &pool_ranges,
                DescriptorPoolCreateFlags::empty(),
            )?;
            let allocate = size.min(count);

            self.pools.push_back(DescriptorPool {
                raw,
                size,
                free: size,
                freed: 0,
            });
            let index = self.pools.len() - 1;
            let pool = self.pools.back_mut().unwrap();

            allocate_from_pool::<B>(&mut pool.raw, layout, allocate, &mut allocation.sets)?;
            allocation.pools.extend(
                std::iter::repeat(index as u64 + self.pools_offset).take(allocate as usize),
            );

            count -= allocate;
            pool.free -= allocate;
            self.total += allocate as u64;
        }

        Ok(())
    }

    unsafe fn free(&mut self, sets: impl IntoIterator<Item = B::DescriptorSet>, pool: u64) {
        let pool = &mut self.pools[(pool - self.pools_offset) as usize];
        let freed = sets.into_iter().count() as u32;
        pool.freed += freed;
        self.total -= freed as u64;
        log::trace!("Freed {} from descriptor bucket", freed);
    }

    unsafe fn cleanup(&mut self, device: &B::Device) {
        while let Some(pool) = self.pools.pop_front() {
            if pool.freed < pool.size {
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
    buckets: HashMap<DescriptorRanges, DescriptorBucket<B>>,
    allocation: Allocation<B>,
    relevant: relevant::Relevant,
    total: u64,
}

impl<B> DescriptorAllocator<B>
where
    B: Backend,
{
    /// Create new allocator instance.
    pub fn new() -> Self {
        DescriptorAllocator {
            buckets: HashMap::new(),
            allocation: Allocation {
                sets: SmallVec::new(),
                pools: Vec::new(),
            },
            relevant: relevant::Relevant,
            total: 0,
        }
    }

    /// Destroy allocator instance.
    /// All sets allocated from this allocator become invalid.
    pub unsafe fn dispose(mut self, device: &B::Device) {
        self.buckets
            .drain()
            .for_each(|(_, bucket)| bucket.dispose(device));
        self.relevant.dispose();
    }

    /// Allocate descriptor set with specified layout.
    /// `DescriptorRanges` must match descriptor numbers of the layout.
    /// `DescriptorRanges` can be constructed [from bindings] that were used
    /// to create layout instance.
    ///
    /// [from bindings]: .
    pub unsafe fn allocate(
        &mut self,
        device: &B::Device,
        layout: &B::DescriptorSetLayout,
        layout_ranges: DescriptorRanges,
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
            layout_ranges
        );

        let bucket = self
            .buckets
            .entry(layout_ranges)
            .or_insert_with(|| DescriptorBucket::new());
        match bucket.allocate(device, layout, layout_ranges, count, &mut self.allocation) {
            Ok(()) => {
                extend.extend(
                    Iterator::zip(
                        self.allocation.pools.drain(..),
                        self.allocation.sets.drain(),
                    )
                    .map(|(pool, set)| DescriptorSet {
                        raw: set,
                        ranges: layout_ranges,
                        pool,
                    }),
                );
                Ok(())
            }
            Err(err) => {
                // Free sets allocated so far.
                let mut last = None;
                for (index, pool) in self.allocation.pools.drain(..).enumerate().rev() {
                    match last {
                        Some(last) if last == pool => {
                            // same pool, continue
                        }
                        Some(last) => {
                            let sets = &mut self.allocation.sets;
                            // Free contiguous range of sets from one pool in one go.
                            bucket.free((index + 1..sets.len()).map(|_| sets.pop().unwrap()), last);
                        }
                        None => last = Some(pool),
                    }
                }

                if let Some(last) = last {
                    bucket.free(self.allocation.sets.drain(), last);
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
        let mut free: Option<(DescriptorRanges, u64, SmallVec<[B::DescriptorSet; 32]>)> = None;

        // Collect contig
        for set in all_sets {
            match &mut free {
                slot @ None => {
                    slot.replace((set.ranges, set.pool, smallvec![set.raw]));
                }
                Some((ranges, pool, raw_sets)) if *ranges == set.ranges && *pool == set.pool => {
                    raw_sets.push(set.raw);
                }
                Some((ranges, pool, raw_sets)) => {
                    let bucket = self
                        .buckets
                        .get_mut(ranges)
                        .expect("Set should be allocated from this allocator");
                    debug_assert!(bucket.total >= raw_sets.len() as u64);

                    bucket.free(raw_sets.drain(), *pool);
                    *pool = set.pool;
                    *ranges = set.ranges;
                    raw_sets.push(set.raw);
                }
            }
        }

        if let Some((ranges, pool, raw_sets)) = free {
            let bucket = self
                .buckets
                .get_mut(&ranges)
                .expect("Set should be allocated from this allocator");
            debug_assert!(bucket.total >= raw_sets.len() as u64);

            bucket.free(raw_sets, pool);
        }
    }

    /// Perform cleanup to allow resources reuse.
    pub unsafe fn cleanup(&mut self, device: &B::Device) {
        self.buckets
            .values_mut()
            .for_each(|bucket| bucket.cleanup(device));
    }
}
