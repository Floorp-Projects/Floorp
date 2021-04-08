#![allow(dead_code)]
use crate::prelude::*;
use crate::version::{EntryV1_0, InstanceV1_0};
use crate::vk;
use crate::RawPtr;
use std::ffi::CStr;
use std::mem;

#[derive(Clone)]
pub struct Win32Surface {
    handle: vk::Instance,
    win32_surface_fn: vk::KhrWin32SurfaceFn,
}

impl Win32Surface {
    pub fn new<E: EntryV1_0, I: InstanceV1_0>(entry: &E, instance: &I) -> Win32Surface {
        let surface_fn = vk::KhrWin32SurfaceFn::load(|name| unsafe {
            mem::transmute(entry.get_instance_proc_addr(instance.handle(), name.as_ptr()))
        });
        Win32Surface {
            handle: instance.handle(),
            win32_surface_fn: surface_fn,
        }
    }

    pub fn name() -> &'static CStr {
        vk::KhrWin32SurfaceFn::name()
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateWin32SurfaceKHR.html>"]
    pub unsafe fn create_win32_surface(
        &self,
        create_info: &vk::Win32SurfaceCreateInfoKHR,
        allocation_callbacks: Option<&vk::AllocationCallbacks>,
    ) -> VkResult<vk::SurfaceKHR> {
        let mut surface = mem::zeroed();
        let err_code = self.win32_surface_fn.create_win32_surface_khr(
            self.handle,
            create_info,
            allocation_callbacks.as_raw_ptr(),
            &mut surface,
        );
        match err_code {
            vk::Result::SUCCESS => Ok(surface),
            _ => Err(err_code),
        }
    }

    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceWin32PresentationSupportKHR.html"]
    pub unsafe fn get_physical_device_win32_presentation_support(
        &self,
        physical_device: vk::PhysicalDevice,
        queue_family_index: u32,
    ) -> bool {
        let b = self
            .win32_surface_fn
            .get_physical_device_win32_presentation_support_khr(
                physical_device,
                queue_family_index,
            );

        b > 0
    }

    pub fn fp(&self) -> &vk::KhrWin32SurfaceFn {
        &self.win32_surface_fn
    }

    pub fn instance(&self) -> vk::Instance {
        self.handle
    }
}
