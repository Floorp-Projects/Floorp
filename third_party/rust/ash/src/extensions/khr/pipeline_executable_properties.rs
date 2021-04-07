#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::vk;
use std::ffi::CStr;
use std::mem;
use std::ptr;

#[derive(Clone)]
pub struct PipelineExecutableProperties {
    handle: vk::Instance,
    pipeline_executable_properties_fn: vk::KhrPipelineExecutablePropertiesFn,
}

impl PipelineExecutableProperties {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(
        entry: &E,
        instance: &I,
    ) -> PipelineExecutableProperties {
        let pipeline_executable_properties_fn =
            vk::KhrPipelineExecutablePropertiesFn::load(|name| unsafe {
                mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
            });

        PipelineExecutableProperties {
            handle: instance.handle(),
            pipeline_executable_properties_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrPipelineExecutablePropertiesFn::name()
    }

    #[doc = "https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutableInternalRepresentationsKHR.html"]
    pub unsafe fn get_pipeline_executable_internal_representations(
        &self,
        device: vk::Device,
        executable_info: &vk::PipelineExecutableInfoKHR,
    ) -> VkResult<Vec<vk::PipelineExecutableInternalRepresentationKHR>> {
        let mut count = 0;
        let err_code = self
            .pipeline_executable_properties_fn
            .get_pipeline_executable_internal_representations_khr(
                device,
                executable_info,
                &mut count,
                ptr::null_mut(),
            );
        if err_code != vk::Result::SUCCESS {
            return Err(err_code);
        }
        let mut v: Vec<_> = vec![Default::default(); count as usize];
        self.pipeline_executable_properties_fn
            .get_pipeline_executable_internal_representations_khr(
                device,
                executable_info,
                &mut count,
                v.as_mut_ptr(),
            )
            .result_with_success(v)
    }

    #[doc = "https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutablePropertiesKHR.html"]
    pub unsafe fn get_pipeline_executable_properties(
        &self,
        device: vk::Device,
        pipeline_info: &vk::PipelineInfoKHR,
    ) -> VkResult<Vec<vk::PipelineExecutablePropertiesKHR>> {
        let mut count = 0;
        let err_code = self
            .pipeline_executable_properties_fn
            .get_pipeline_executable_properties_khr(
                device,
                pipeline_info,
                &mut count,
                ptr::null_mut(),
            );
        if err_code != vk::Result::SUCCESS {
            return Err(err_code);
        }
        let mut v: Vec<_> = vec![Default::default(); count as usize];
        self.pipeline_executable_properties_fn
            .get_pipeline_executable_properties_khr(
                device,
                pipeline_info,
                &mut count,
                v.as_mut_ptr(),
            )
            .result_with_success(v)
    }

    #[doc = "https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutableStatisticsKHR.html"]
    pub unsafe fn get_pipeline_executable_statistics(
        &self,
        device: vk::Device,
        executable_info: &vk::PipelineExecutableInfoKHR,
    ) -> VkResult<Vec<vk::PipelineExecutableStatisticKHR>> {
        let mut count = 0;
        let err_code = self
            .pipeline_executable_properties_fn
            .get_pipeline_executable_statistics_khr(
                device,
                executable_info,
                &mut count,
                ptr::null_mut(),
            );
        if err_code != vk::Result::SUCCESS {
            return Err(err_code);
        }
        let mut v: Vec<_> = vec![Default::default(); count as usize];
        self.pipeline_executable_properties_fn
            .get_pipeline_executable_statistics_khr(
                device,
                executable_info,
                &mut count,
                v.as_mut_ptr(),
            )
            .result_with_success(v)
    }

    pub fn fp(&self) -> &vk::KhrPipelineExecutablePropertiesFn {
        &self.pipeline_executable_properties_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
