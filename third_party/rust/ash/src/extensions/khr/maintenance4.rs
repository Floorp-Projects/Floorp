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

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceBufferMemoryRequirementsKHR.html>"]
    pub unsafe fn get_device_buffer_memory_requirements(
        &self,
        create_info: &vk::DeviceBufferMemoryRequirementsKHR,
        out: &mut vk::MemoryRequirements2,
    ) {
        self.fp
            .get_device_buffer_memory_requirements_khr(self.handle, create_info, out)
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceImageMemoryRequirementsKHR.html>"]
    pub unsafe fn get_device_image_memory_requirements(
        &self,
        create_info: &vk::DeviceImageMemoryRequirementsKHR,
        out: &mut vk::MemoryRequirements2,
    ) {
        self.fp
            .get_device_image_memory_requirements_khr(self.handle, create_info, out)
    }

    /// Retrieve the number of elements to pass to [`Self::get_device_image_sparse_memory_requirements()`]
    pub unsafe fn get_device_image_sparse_memory_requirements_len(
        &self,
        create_info: &vk::DeviceImageMemoryRequirementsKHR,
    ) -> usize {
        let mut count = 0;
        self.fp.get_device_image_sparse_memory_requirements_khr(
            self.handle,
            create_info,
            &mut count,
            std::ptr::null_mut(),
        );
        count as usize
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceImageSparseMemoryRequirementsKHR.html>"]
    ///
    /// Call [`Self::get_device_image_sparse_memory_requirements_len()`] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    pub unsafe fn get_device_image_sparse_memory_requirements(
        &self,
        create_info: &vk::DeviceImageMemoryRequirementsKHR,
        out: &mut [vk::SparseImageMemoryRequirements2],
    ) {
        let mut count = out.len() as u32;
        self.fp.get_device_image_sparse_memory_requirements_khr(
            self.handle,
            create_info,
            &mut count,
            out.as_mut_ptr(),
        );
        assert_eq!(count, out.len() as u32);
    }

    pub fn name() -> &'static CStr {
        vk::KhrMaintenance4Fn::name()
    }

    pub fn fp(&self) -> &vk::KhrMaintenance4Fn {
        &self.fp
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
