use crate::prelude::*;
use crate::vk;
use crate::{Entry, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_NV_coverage_reduction_mode.html>
#[derive(Clone)]
pub struct CoverageReductionMode {
    fp: vk::NvCoverageReductionModeFn,
}

impl CoverageReductionMode {
    pub fn new(entry: &Entry, instance: &Instance) -> Self {
        let fp = vk::NvCoverageReductionModeFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Self { fp }
    }

    /// Retrieve the number of elements to pass to [`get_physical_device_supported_framebuffer_mixed_samples_combinations()`][Self::get_physical_device_supported_framebuffer_mixed_samples_combinations()]
    #[inline]
    pub unsafe fn get_physical_device_supported_framebuffer_mixed_samples_combinations_len(
        &self,
        physical_device: vk::PhysicalDevice,
    ) -> VkResult<usize> {
        let mut count = 0;
        (self
            .fp
            .get_physical_device_supported_framebuffer_mixed_samples_combinations_nv)(
            physical_device,
            &mut count,
            std::ptr::null_mut(),
        )
        .result_with_success(count as usize)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV.html>
    ///
    /// Call [`get_physical_device_supported_framebuffer_mixed_samples_combinations_len()`][Self::get_physical_device_supported_framebuffer_mixed_samples_combinations_len()] to query the number of elements to pass to `out`.
    /// Be sure to [`Default::default()`]-initialize these elements and optionally set their `p_next` pointer.
    #[inline]
    pub unsafe fn get_physical_device_supported_framebuffer_mixed_samples_combinations(
        &self,
        physical_device: vk::PhysicalDevice,
        out: &mut [vk::FramebufferMixedSamplesCombinationNV],
    ) -> VkResult<()> {
        let mut count = out.len() as u32;
        (self
            .fp
            .get_physical_device_supported_framebuffer_mixed_samples_combinations_nv)(
            physical_device,
            &mut count,
            out.as_mut_ptr(),
        )
        .result()?;
        assert_eq!(count as usize, out.len());
        Ok(())
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::NvCoverageReductionModeFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::NvCoverageReductionModeFn {
        &self.fp
    }
}
