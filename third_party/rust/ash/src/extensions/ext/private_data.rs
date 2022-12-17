use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_private_data.html>
#[derive(Clone)]
pub struct PrivateData {
    handle: vk::Device,
    fp: vk::ExtPrivateDataFn,
}

impl PrivateData {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::ExtPrivateDataFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreatePrivateDataSlotEXT.html>
    #[inline]
    pub unsafe fn create_private_data_slot(
        &self,
        create_info: &vk::PrivateDataSlotCreateInfoEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::PrivateDataSlotEXT> {
        let mut private_data_slot = mem::zeroed();
        (self.fp.create_private_data_slot_ext)(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut private_data_slot,
        )
        .result_with_success(private_data_slot)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDestroyPrivateDataSlotEXT.html>
    #[inline]
    pub unsafe fn destroy_private_data_slot(
        &self,
        private_data_slot: vk::PrivateDataSlotEXT,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        (self.fp.destroy_private_data_slot_ext)(
            self.handle,
            private_data_slot,
            allocation_callbacks.as_raw_ptr(),
        )
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkSetPrivateDataEXT.html>
    #[inline]
    pub unsafe fn set_private_data<T: vk::Handle>(
        &self,
        object: T,
        private_data_slot: vk::PrivateDataSlotEXT,
        data: u64,
    ) -> VkResult<()> {
        (self.fp.set_private_data_ext)(
            self.handle,
            T::TYPE,
            object.as_raw(),
            private_data_slot,
            data,
        )
        .result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPrivateDataEXT.html>
    #[inline]
    pub unsafe fn get_private_data<T: vk::Handle>(
        &self,
        object: T,
        private_data_slot: vk::PrivateDataSlotEXT,
    ) -> u64 {
        let mut data = mem::zeroed();
        (self.fp.get_private_data_ext)(
            self.handle,
            T::TYPE,
            object.as_raw(),
            private_data_slot,
            &mut data,
        );
        data
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtPrivateDataFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtPrivateDataFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
