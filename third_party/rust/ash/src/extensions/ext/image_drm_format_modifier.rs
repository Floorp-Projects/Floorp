use crate::prelude::*;
use crate::vk;
use crate::{Device, Instance};
use std::ffi::CStr;
use std::mem;

/// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VK_EXT_image_drm_format_modifier.html>
#[derive(Clone)]
pub struct ImageDrmFormatModifier {
    handle: vk::Device,
    fp: vk::ExtImageDrmFormatModifierFn,
}

impl ImageDrmFormatModifier {
    pub fn new(instance: &Instance, device: &Device) -> Self {
        let handle = device.handle();
        let fp = vk::ExtImageDrmFormatModifierFn::load(|name| unsafe {
            mem::transmute(instance.get_device_proc_addr(handle, name.as_ptr()))
        });
        Self { handle, fp }
    }

    /// <https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkGetImageDrmFormatModifierPropertiesEXT.html>
    #[inline]
    pub unsafe fn get_image_drm_format_modifier_properties(
        &self,
        image: vk::Image,
        properties: &mut vk::ImageDrmFormatModifierPropertiesEXT,
    ) -> VkResult<()> {
        (self.fp.get_image_drm_format_modifier_properties_ext)(self.handle, image, properties)
            .result()
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtImageDrmFormatModifierFn::name()
    }

    #[inline]
    pub fn fp(&self) -> &vk::ExtImageDrmFormatModifierFn {
        &self.fp
    }

    #[inline]
    pub fn device(&self) -> vk::Device {
        self.handle
    }
}
