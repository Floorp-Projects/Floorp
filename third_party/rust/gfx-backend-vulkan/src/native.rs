use crate::{Backend, RawDevice, ROUGH_MAX_ATTACHMENT_COUNT};
use ash::{version::DeviceV1_0, vk};
use hal::{
    device::OutOfMemory,
    image::{Extent, SubresourceRange},
    pso,
};
use inplace_it::inplace_or_alloc_from_iter;
use parking_lot::Mutex;
use smallvec::SmallVec;
use std::{collections::HashMap, sync::Arc};

#[derive(Debug, Hash)]
pub struct Semaphore(pub vk::Semaphore);

#[derive(Debug, Hash, PartialEq, Eq)]
pub struct Fence(pub vk::Fence);

#[derive(Debug, Hash)]
pub struct Event(pub vk::Event);

#[derive(Debug, Hash)]
pub struct GraphicsPipeline(pub vk::Pipeline);

#[derive(Debug, Hash)]
pub struct ComputePipeline(pub vk::Pipeline);

#[derive(Debug, Hash)]
pub struct Memory {
    pub(crate) raw: vk::DeviceMemory,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct Buffer {
    pub(crate) raw: vk::Buffer,
}

unsafe impl Sync for Buffer {}
unsafe impl Send for Buffer {}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct BufferView {
    pub(crate) raw: vk::BufferView,
}

#[derive(Debug, Hash, PartialEq, Eq)]
pub struct Image {
    pub(crate) raw: vk::Image,
    pub(crate) ty: vk::ImageType,
    pub(crate) flags: vk::ImageCreateFlags,
    pub(crate) extent: vk::Extent3D,
}

#[derive(Debug, Hash, PartialEq, Eq)]
pub struct ImageView {
    pub(crate) image: vk::Image,
    pub(crate) raw: vk::ImageView,
    pub(crate) range: SubresourceRange,
}

#[derive(Debug, Hash)]
pub struct Sampler(pub vk::Sampler);

#[derive(Debug, Hash)]
pub struct RenderPass {
    pub raw: vk::RenderPass,
    pub attachment_count: usize,
}

pub type FramebufferKey = SmallVec<[vk::ImageView; ROUGH_MAX_ATTACHMENT_COUNT]>;

#[derive(Debug)]
pub enum Framebuffer {
    ImageLess(vk::Framebuffer),
    Legacy {
        name: String,
        map: Mutex<HashMap<FramebufferKey, vk::Framebuffer>>,
        extent: Extent,
    },
}

pub(crate) type SortedBindings = Arc<Vec<pso::DescriptorSetLayoutBinding>>;

#[derive(Debug)]
pub struct DescriptorSetLayout {
    pub(crate) raw: vk::DescriptorSetLayout,
    pub(crate) bindings: SortedBindings,
}

#[derive(Debug)]
pub struct DescriptorSet {
    pub(crate) raw: vk::DescriptorSet,
    pub(crate) bindings: SortedBindings,
}

#[derive(Debug, Hash)]
pub struct PipelineLayout {
    pub(crate) raw: vk::PipelineLayout,
}

#[derive(Debug)]
pub struct PipelineCache {
    pub(crate) raw: vk::PipelineCache,
}

#[derive(Debug, Eq, Hash, PartialEq)]
pub struct ShaderModule {
    pub(crate) raw: vk::ShaderModule,
}

#[derive(Debug)]
pub struct DescriptorPool {
    raw: vk::DescriptorPool,
    device: Arc<RawDevice>,
    /// This vec only exists to re-use allocations when `DescriptorSet`s are freed.
    temp_raw_sets: Vec<vk::DescriptorSet>,
    /// This vec only exists for collecting the layouts when allocating new sets.
    temp_raw_layouts: Vec<vk::DescriptorSetLayout>,
    /// This vec only exists for collecting the bindings when allocating new sets.
    temp_layout_bindings: Vec<SortedBindings>,
}

impl DescriptorPool {
    pub(crate) fn new(raw: vk::DescriptorPool, device: &Arc<RawDevice>) -> Self {
        DescriptorPool {
            raw,
            device: Arc::clone(device),
            temp_raw_sets: Vec::new(),
            temp_raw_layouts: Vec::new(),
            temp_layout_bindings: Vec::new(),
        }
    }

    pub(crate) fn finish(self) -> vk::DescriptorPool {
        self.raw
    }
}

impl pso::DescriptorPool<Backend> for DescriptorPool {
    unsafe fn allocate_one(
        &mut self,
        layout: &DescriptorSetLayout,
    ) -> Result<DescriptorSet, pso::AllocationError> {
        let raw_layouts = [layout.raw];
        let info = vk::DescriptorSetAllocateInfo::builder()
            .descriptor_pool(self.raw)
            .set_layouts(&raw_layouts);

        self.device
            .raw
            .allocate_descriptor_sets(&info)
            //Note: https://github.com/MaikKlein/ash/issues/358
            .map(|mut sets| DescriptorSet {
                raw: sets.pop().unwrap(),
                bindings: Arc::clone(&layout.bindings),
            })
            .map_err(|err| match err {
                vk::Result::ERROR_OUT_OF_HOST_MEMORY => {
                    pso::AllocationError::OutOfMemory(OutOfMemory::Host)
                }
                vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => {
                    pso::AllocationError::OutOfMemory(OutOfMemory::Device)
                }
                vk::Result::ERROR_OUT_OF_POOL_MEMORY => pso::AllocationError::OutOfPoolMemory,
                _ => pso::AllocationError::FragmentedPool,
            })
    }

    unsafe fn allocate<'a, I, E>(
        &mut self,
        layouts: I,
        list: &mut E,
    ) -> Result<(), pso::AllocationError>
    where
        I: Iterator<Item = &'a DescriptorSetLayout>,
        E: Extend<DescriptorSet>,
    {
        self.temp_raw_layouts.clear();
        self.temp_layout_bindings.clear();
        for layout in layouts {
            self.temp_raw_layouts.push(layout.raw);
            self.temp_layout_bindings.push(Arc::clone(&layout.bindings));
        }

        let info = vk::DescriptorSetAllocateInfo::builder()
            .descriptor_pool(self.raw)
            .set_layouts(&self.temp_raw_layouts);

        self.device
            .raw
            .allocate_descriptor_sets(&info)
            .map(|sets| {
                list.extend(
                    sets.into_iter()
                        .zip(self.temp_layout_bindings.drain(..))
                        .map(|(raw, bindings)| DescriptorSet { raw, bindings }),
                )
            })
            .map_err(|err| match err {
                vk::Result::ERROR_OUT_OF_HOST_MEMORY => {
                    pso::AllocationError::OutOfMemory(OutOfMemory::Host)
                }
                vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => {
                    pso::AllocationError::OutOfMemory(OutOfMemory::Device)
                }
                vk::Result::ERROR_OUT_OF_POOL_MEMORY => pso::AllocationError::OutOfPoolMemory,
                _ => pso::AllocationError::FragmentedPool,
            })
    }

    unsafe fn free<I>(&mut self, descriptor_sets: I)
    where
        I: Iterator<Item = DescriptorSet>,
    {
        let sets_iter = descriptor_sets.map(|d| d.raw);
        inplace_or_alloc_from_iter(sets_iter, |sets| {
            if !sets.is_empty() {
                if let Err(e) = self.device.raw.free_descriptor_sets(self.raw, sets) {
                    error!("free_descriptor_sets error {}", e);
                }
            }
        })
    }

    unsafe fn reset(&mut self) {
        assert_eq!(
            Ok(()),
            self.device
                .raw
                .reset_descriptor_pool(self.raw, vk::DescriptorPoolResetFlags::empty())
        );
    }
}

#[derive(Debug, Hash)]
pub struct QueryPool(pub vk::QueryPool);

#[derive(Debug, Hash)]
pub struct Display(pub vk::DisplayKHR);

#[derive(Debug, Hash)]
pub struct DisplayMode(pub vk::DisplayModeKHR);
