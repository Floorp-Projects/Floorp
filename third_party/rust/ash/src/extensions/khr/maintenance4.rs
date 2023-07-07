use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Maintenance4 {
    handle: vk::Device,
    fp: vk::KhrMaintenance4Fn,
}

impl Maintenance4 {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrMaintenance4Fn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceBufferMemoryRequirementsKHR.html>
    #[inline]
    pub unsafe fn get_device_buffer_memory_requirements(
        &self,
        memory_requirements: &vk::DeviceBufferMemoryRequirementsKHR,
        out: &mut vk::MemoryRequirements2,
    ) {
        (self.fp.get_device_buffer_memory_requirements_khr)(self.handle, memory_requirements, out)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceImageMemoryRequirementsKHR.html>
    #[inline]
    pub unsafe fn get_device_image_memory_requirements(
        &self,
        memory_requirements: &vk::DeviceImageMemoryRequirementsKHR,
        out: &mut vk::MemoryRequirements2,
    ) {
        (self.fp.get_device_image_memory_requirements_khr)(self.handle, memory_requirements, out)
    }

    /// Retrieve the number of elements to pass to [`get_device_image_sparse_memory_requirements()`][Self::get_device_image_sparse_memory_requirements()]
    #[inline]
    pub unsafe fn get_device_image_sparse_memory_requirements_len(
        &self,
        memory_requirements: &vk::DeviceImageMemoryRequirementsKHR,
    ) -> usize {
        let mut count = 0;
        (self.fp.get_device_image_sparse_memory_requirements_khr)(
            self.handle,
            memory_requirements,
            &mut count,
            std::ptr::null_mut(),
        );
        count as usize
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeviceImageSparseMemoryRequirementsKHR.html>
    ///
    /// Call [`get_device_image_sparse_memory_requirements_len()`][Self::get_device_image_sparse_memory_requirements_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_device_image_sparse_memory_requirements(
        &self,
        memory_requirements: &vk::DeviceImageMemoryRequirementsKHR,
        out: &mut [vk::SparseImageMemoryRequirements2],
    ) {
        let mut count = out.len() as u32;
        (self.fp.get_device_image_sparse_memory_requirements_khr)(
            self.handle,
            memory_requirements,
            &mut count,
            out.as_mut_ptr(),
        );
        assert_eq!(count as usize, out.len());
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrMaintenance4Fn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrMaintenance4Fn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
