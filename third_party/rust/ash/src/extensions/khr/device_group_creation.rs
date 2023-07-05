use crate::prelude::*;
use crate::vk;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;
use std::ptr;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_KHR_device_group_creation.html>
#[derive(Clone)]
pub struct DeviceGroupCreation {
    handle: vk::Instance,
    fp: vk::KhrDeviceGroupCreationFn,
}

impl DeviceGroupCreation {
    pub fn new(entry: Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::KhrDeviceGroupCreationFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// Retrieve the number of elements to pass to [`enumerate_physical_device_groups()`][Self::enumerate_physical_device_groups()]
    #[inline]
    pub unsafe fn enumerate_physical_device_groups_len(&self) -> VkResult<usize> {
        let mut group_count = 0;
        (self.fp.enumerate_physical_device_groups_khr)(
            self.handle,
            &mut group_count,
            ptr::null_mut(),
        )
        .result_with_success(group_count as usize)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkEnumeratePhysicalDeviceGroupsKHR.html>
    ///
    /// Call [`enumerate_physical_device_groups_len()`][Self::enumerate_physical_device_groups_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn enumerate_physical_device_groups(
        &self,
        out: &mut [vk::PhysicalDeviceGroupProperties],
    ) -> VkResult<()> {
        let mut count = out.len() as u32;
        (self.fp.enumerate_physical_device_groups_khr)(self.handle, &mut count, out.as_mut_ptr())
            .result()?;
        assert_eq!(count as usize, out.len());
        Ok(())
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrDeviceGroupCreationFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrDeviceGroupCreationFn {
        &self.fp
    }

    #[deprecated = "typo: this function is called `device()`, but returns an `Instance`."]
    #[inline]
    pub fn device(&self) -> vk::Instance {
        self.handle
    }

    #[inline]
    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
