use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;
use std::ptr;

#[derive(Clone)]
pub struct GetMemoryRequirements2 {
    handle: vk::Device,
    get_memory_requirements2_fn: vk::KhrGetMemoryRequirements2Fn,
}

impl GetMemoryRequirements2 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let get_memory_requirements2_fn = vk::KhrGetMemoryRequirements2Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(device.handle(), name.as_ptr()))
        });
        Self {
            handle: device.handle(),
            get_memory_requirements2_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrGetMemoryRequirements2Fn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferMemoryRequirements2KHR.html>"]
    pub unsafe fn get_buffer_memory_requirements2(
        &self,
        info: &vk::BufferMemoryRequirementsInfo2KHR,
        memory_requirements: &mut vk::MemoryRequirements2KHR,
    ) {
        self.get_memory_requirements2_fn
            .get_buffer_memory_requirements2_khr(self.handle, info, memory_requirements);
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageMemoryRequirements2KHR.html>"]
    pub unsafe fn get_image_memory_requirements2(
        &self,
        info: &vk::ImageMemoryRequirementsInfo2KHR,
        memory_requirements: &mut vk::MemoryRequirements2KHR,
    ) {
        self.get_memory_requirements2_fn
            .get_image_memory_requirements2_khr(self.handle, info, memory_requirements);
    }

    pub unsafe fn get_image_sparse_memory_requirements2_len(
        &self,
        info: &vk::ImageSparseMemoryRequirementsInfo2KHR,
    ) -> usize {
        let mut count = 0;
        self.get_memory_requirements2_fn
            .get_image_sparse_memory_requirements2_khr(
                self.handle,
                info,
                &mut count,
                ptr::null_mut(),
            );
        count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageSparseMemoryRequirements2KHR.html>"]
    pub unsafe fn get_image_sparse_memory_requirements2(
        &self,
        info: &vk::ImageSparseMemoryRequirementsInfo2KHR,
        sparse_memory_requirements: &mut [vk::SparseImageMemoryRequirements2KHR],
    ) {
        let mut count = sparse_memory_requirements.len() as u32;
        self.get_memory_requirements2_fn
            .get_image_sparse_memory_requirements2_khr(
                self.handle,
                info,
                &mut count,
                sparse_memory_requirements.as_mut_ptr(),
            );
    }

    pub fn fp(&self) -> &vk::KhrGetMemoryRequirements2Fn {
        &self.get_memory_requirements2_fn
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
