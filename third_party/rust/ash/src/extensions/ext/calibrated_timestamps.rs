use crate::prelude::*;
use crate::vk;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct CalibratedTimestamps {
    handle: vk::Instance,
    fp: vk::ExtCalibratedTimestampsFn,
}

impl CalibratedTimestamps {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let handle = instance.handle();
        let fp = vk::ExtCalibratedTimestampsFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceCalibrateableTimeDomainsEXT.html>
    #[inline]
    pub unsafe fn get_physical_device_calibrateable_time_domains(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> VkResult<Vec<vk::TimeDomainEXT>> {
        read_into_uninitialized_vector(|count, data| {
            (self.fp.get_physical_device_calibrateable_time_domains_ext)(
                physical_device,
                count,
                data,
            )
        })
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetCalibratedTimestampsEXT.html>
    ///
    /// Returns a tuple containing `(timestamps, max_deviation)`
    #[inline]
    pub unsafe fn get_calibrated_timestamps(
        &self,
        device: vk::Device,
        info: &[vk::CalibratedTimestampInfoEXT],
    ) -> VkResult<(Vec<u64>, Vec<u64>)> {
        let mut timestamps = vec![0u64; info.len()];
        let mut max_deviation = vec![0u64; info.len()];
        (self.fp.get_calibrated_timestamps_ext)(
            device,
            info.len() as u32,
            info.as_ptr(),
            timestamps.as_mut_ptr(),
            max_deviation.as_mut_ptr(),
        )
        .result_with_success((timestamps, max_deviation))
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtCalibratedTimestampsFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtCalibratedTimestampsFn {
        &self.fp
    }

    #[inline]
    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
