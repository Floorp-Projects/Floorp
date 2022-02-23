use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct PipelineExecutableProperties {
    handle: vk::Device,
    fp: vk::KhrPipelineExecutablePropertiesFn,
}

impl PipelineExecutableProperties {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrPipelineExecutablePropertiesFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutableInternalRepresentationsKHR.html>"]
    pub unsafe fn get_pipeline_executable_internal_representations(
        &self,
        executable_info: &vk::PipelineExecutableInfoKHR,
    ) -> VkResult<Vec<vk::PipelineExecutableInternalRepresentationKHR>> {
        read_into_defaulted_vector(|count, data| {
            self.fp
                .get_pipeline_executable_internal_representations_khr(
                    self.handle,
                    executable_info,
                    count,
                    data,
                )
        })
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutablePropertiesKHR.html>"]
    pub unsafe fn get_pipeline_executable_properties(
        &self,
        pipeline_info: &vk::PipelineInfoKHR,
    ) -> VkResult<Vec<vk::PipelineExecutablePropertiesKHR>> {
        read_into_defaulted_vector(|count, data| {
            self.fp
                .get_pipeline_executable_properties_khr(self.handle, pipeline_info, count, data)
        })
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutableStatisticsKHR.html>"]
    pub unsafe fn get_pipeline_executable_statistics(
        &self,
        executable_info: &vk::PipelineExecutableInfoKHR,
    ) -> VkResult<Vec<vk::PipelineExecutableStatisticKHR>> {
        read_into_defaulted_vector(|count, data| {
            self.fp.get_pipeline_executable_statistics_khr(
                self.handle,
                executable_info,
                count,
                data,
            )
        })
    }

    pub fn name() -> &'static CStr {
        vk::KhrPipelineExecutablePropertiesFn::name()
    }

    pub fn fp(&self) -> &vk::KhrPipelineExecutablePropertiesFn {
        &self.fp
    }

    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
