use crate::prelude::*;
use crate::vk;
use crate::RawPtr;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct DeferredHostOperations {
    handle: vk::Device,
    fp: vk::KhrDeferredHostOperationsFn,
}

impl DeferredHostOperations {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::KhrDeferredHostOperationsFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCreateDeferredOperationKHR.html>
    #[inline]
    pub unsafe fn create_deferred_operation(
        &self,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::DeferredOperationKHR> {
        let mut operation = mem::zeroed();
        (self.fp.create_deferred_operation_khr)(
            self.handle,
            allocation_callbacks.as_raw_ptr(),
            &mut operation,
        )
        .result_with_success(operation)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDeferredOperationJoinKHR.html>
    #[inline]
    pub unsafe fn deferred_operation_join(
        &self,
        operation: vk::DeferredOperationKHR,
    ) -> VkResult<()> {
        (self.fp.deferred_operation_join_khr)(self.handle, operation).result()
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkDestroyDeferredOperationKHR.html>
    #[inline]
    pub unsafe fn destroy_deferred_operation(
        &self,
        operation: vk::DeferredOperationKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) {
        (self.fp.destroy_deferred_operation_khr)(
            self.handle,
            operation,
            allocation_callbacks.as_raw_ptr(),
        );
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeferredOperationMaxConcurrencyKHR.html>
    #[inline]
    pub unsafe fn get_deferred_operation_max_concurrency(
        &self,
        operation: vk::DeferredOperationKHR,
    ) -> u32 {
        (self.fp.get_deferred_operation_max_concurrency_khr)(self.handle, operation)
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetDeferredOperationResultKHR.html>
    #[inline]
    pub unsafe fn get_deferred_operation_result(
        &self,
        operation: vk::DeferredOperationKHR,
    ) -> VkResult<()> {
        (self.fp.get_deferred_operation_result_khr)(self.handle, operation).result()
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::KhrDeferredHostOperationsFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::KhrDeferredHostOperationsFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
