use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;
use std::ptr;

#[derive(Clone)]
pub struct GetMemoryRequirements2 {
    handle: vk::Device,
    fp: vk::KhrGetMemoryRequirements2Fn,
}

impl GetMemoryRequirements2 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrGetMemoryRequirements2Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetBufferMemoryRequirements2KHR.html>
    #[inline]
    pub unsafe fn get_buffer_memory_requirements2(
        &self,
        info: &vk::BufferMemoryRequirementsInfo2KHR,
        memory_requirements: &mut vk::MemoryRequirements2KHR,
    ) {
        (self.fp.get_buffer_memory_requirements2_khr)(self.handle, info, memory_requirements);
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetImageMemoryRequirements2KHR.html>
    #[inline]
    pub unsafe fn get_image_memory_requirements2(
        &self,
        info: &vk::ImageMemoryRequirementsInfo2KHR,
        memory_requirements: &mut vk::MemoryRequirements2KHR,
    ) {
        (self.fp.get_image_memory_requirements2_khr)(self.handle, info, memory_requirements);
    }

    /// Retrieve the number of elements to pass to [`get_image_sparse_memory_requirements2()`][Self::get_image_sparse_memory_requirements2()]
    #[inline]
    pub unsafe fn get_image_sparse_memory_requirements2_len(
        &self,
        info: &vk::ImageSparseMemoryRequirementsInfo2KHR,
    ) -> usize {
        let mut count = 0;
        (self.fp.get_image_sparse_memory_requirements2_khr)(
            self.handle,
            info,
            &mut count,
            ptr::null_mut(),
        );
        count as usize
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetImageSparseMemoryRequirements2KHR.html>
    ///
    /// Call [`get_image_sparse_memory_requirements2_len()`][Self::get_image_sparse_memory_requirements2_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_image_sparse_memory_requirements2(
        &self,
        info: &vk::ImageSparseMemoryRequirementsInfo2KHR,
        out: &mut [vk::SparseImageMemoryRequirements2KHR],
    ) {
        let mut count = out.len() as u32;
        (self.fp.get_image_sparse_memory_requirements2_khr)(
            self.handle,
            info,
            &mut count,
            out.as_mut_ptr(),
        );
        assert_eq!(count as usize, out.len());
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrGetMemoryRequirements2Fn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrGetMemoryRequirements2Fn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
