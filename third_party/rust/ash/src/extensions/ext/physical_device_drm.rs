use crate::vk;
use crate::Instance;
use std::ffi::CStr;

#[derive(Clone)]
pub struct PhysicalDeviceDrm;

impl PhysicalDeviceDrm {
    #[inline]
    pub unsafe fn get_properties(
        instance: &Instance,
        pdevice: vk::PhysicalDevice,
    ) -> vk::PhysicalDeviceDrmPropertiesEXT {
        let mut props_drm = vk::PhysicalDeviceDrmPropertiesEXT::default();
        {
            let mut props = vk::PhysicalDeviceProperties2::builder().push_next(&mut props_drm);
            instance.get_physical_device_properties2(pdevice, &mut props);
        }
        props_drm
    }

    #[inline]
    pub const fn name() -> &'static CStr {
        vk::ExtPhysicalDeviceDrmFn::name()
    }
}
