use crate::{window::FramebufferCachePtr, Backend, RawDevice};
use ash::{version::DeviceV1_0, vk};
use hal::{image::SubresourceRange, pso};
use std::{borrow::Borrow, sync::Arc};

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
pub enum ImageViewOwner {
    User,
    Surface(FramebufferCachePtr),
}

#[derive(Debug, Hash, PartialEq, Eq)]
pub struct ImageView {
    pub(crate) image: vk::Image,
    pub(crate) view: vk::ImageView,
    pub(crate) range: SubresourceRange,
    pub(crate) owner: ImageViewOwner,
}

#[derive(Debug, Hash)]
pub struct Sampler(pub vk::Sampler);

#[derive(Debug, Hash)]
pub struct RenderPass {
    pub raw: vk::RenderPass,
    pub clear_attachments_mask: u64,
}

#[derive(Debug, Hash)]
pub struct Framebuffer {
    pub(crate) raw: vk::Framebuffer,
    pub(crate) owned: bool,
}

#[derive(Debug)]
pub struct DescriptorSetLayout {
    pub(crate) raw: vk::DescriptorSetLayout,
    pub(crate) bindings: Arc<Vec<pso::DescriptorSetLayoutBinding>>,
}

#[derive(Debug)]
pub struct DescriptorSet {
    pub(crate) raw: vk::DescriptorSet,
    pub(crate) bindings: Arc<Vec<pso::DescriptorSetLayoutBinding>>,
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
    pub(crate) raw: vk::DescriptorPool,
    pub(crate) device: Arc<RawDevice>,
    /// This vec only exists to re-use allocations when `DescriptorSet`s are freed.
    pub(crate) set_free_vec: Vec<vk::DescriptorSet>,
}

impl pso::DescriptorPool<Backend> for DescriptorPool {
    unsafe fn allocate<I, E>(
        &mut self,
        layout_iter: I,
        list: &mut E,
    ) -> Result<(), pso::AllocationError>
    where
        I: IntoIterator,
        I::Item: Borrow<DescriptorSetLayout>,
        E: Extend<DescriptorSet>,
    {
        use std::ptr;

        let mut raw_layouts = Vec::new();
        let mut layout_bindings = Vec::new();
        for layout in layout_iter {
            raw_layouts.push(layout.borrow().raw);
            layout_bindings.push(layout.borrow().bindings.clone());
        }

        let info = vk::DescriptorSetAllocateInfo {
            s_type: vk::StructureType::DESCRIPTOR_SET_ALLOCATE_INFO,
            p_next: ptr::null(),
            descriptor_pool: self.raw,
            descriptor_set_count: raw_layouts.len() as u32,
            p_set_layouts: raw_layouts.as_ptr(),
        };

        self.device
            .raw
            .allocate_descriptor_sets(&info)
            .map(|sets| {
                list.extend(
                    sets.into_iter()
                        .zip(layout_bindings)
                        .map(|(raw, bindings)| DescriptorSet { raw, bindings }),
                )
            })
            .map_err(|err| match err {
                vk::Result::ERROR_OUT_OF_HOST_MEMORY => pso::AllocationError::Host,
                vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => pso::AllocationError::Device,
                vk::Result::ERROR_OUT_OF_POOL_MEMORY => pso::AllocationError::OutOfPoolMemory,
                _ => pso::AllocationError::FragmentedPool,
            })
    }

    unsafe fn free_sets<I>(&mut self, descriptor_sets: I)
    where
        I: IntoIterator<Item = DescriptorSet>,
    {
        self.set_free_vec.clear();
        self.set_free_vec
            .extend(descriptor_sets.into_iter().map(|d| d.raw));
        self.device
            .raw
            .free_descriptor_sets(self.raw, &self.set_free_vec);
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
