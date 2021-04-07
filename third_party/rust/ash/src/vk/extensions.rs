use crate::vk::aliases::*;
use crate::vk::bitflags::*;
use crate::vk::definitions::*;
use crate::vk::enums::*;
use crate::vk::platform_types::*;
use std::os::raw::*;
impl KhrSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_surface\0").expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 25u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkDestroySurfaceKHR = extern "system" fn(
    instance: Instance,
    surface: SurfaceKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceSupportKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    surface: SurfaceKHR,
    p_supported: *mut Bool32,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_surface_capabilities: *mut SurfaceCapabilitiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceFormatsKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_surface_format_count: *mut u32,
    p_surface_formats: *mut SurfaceFormatKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfacePresentModesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_present_mode_count: *mut u32,
    p_present_modes: *mut PresentModeKHR,
) -> Result;
pub struct KhrSurfaceFn {
    pub destroy_surface_khr: extern "system" fn(
        instance: Instance,
        surface: SurfaceKHR,
        p_allocator: *const AllocationCallbacks,
    ),
    pub get_physical_device_surface_support_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        surface: SurfaceKHR,
        p_supported: *mut Bool32,
    ) -> Result,
    pub get_physical_device_surface_capabilities_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_surface_capabilities: *mut SurfaceCapabilitiesKHR,
    ) -> Result,
    pub get_physical_device_surface_formats_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_surface_format_count: *mut u32,
        p_surface_formats: *mut SurfaceFormatKHR,
    ) -> Result,
    pub get_physical_device_surface_present_modes_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_present_mode_count: *mut u32,
        p_present_modes: *mut PresentModeKHR,
    ) -> Result,
}
unsafe impl Send for KhrSurfaceFn {}
unsafe impl Sync for KhrSurfaceFn {}
impl ::std::clone::Clone for KhrSurfaceFn {
    fn clone(&self) -> Self {
        KhrSurfaceFn {
            destroy_surface_khr: self.destroy_surface_khr,
            get_physical_device_surface_support_khr: self.get_physical_device_surface_support_khr,
            get_physical_device_surface_capabilities_khr: self
                .get_physical_device_surface_capabilities_khr,
            get_physical_device_surface_formats_khr: self.get_physical_device_surface_formats_khr,
            get_physical_device_surface_present_modes_khr: self
                .get_physical_device_surface_present_modes_khr,
        }
    }
}
impl KhrSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSurfaceFn {
            destroy_surface_khr: unsafe {
                extern "system" fn destroy_surface_khr(
                    _instance: Instance,
                    _surface: SurfaceKHR,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_surface_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroySurfaceKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_surface_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_surface_support_khr: unsafe {
                extern "system" fn get_physical_device_surface_support_khr(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                    _surface: SurfaceKHR,
                    _p_supported: *mut Bool32,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_support_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfaceSupportKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_support_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_surface_capabilities_khr: unsafe {
                extern "system" fn get_physical_device_surface_capabilities_khr(
                    _physical_device: PhysicalDevice,
                    _surface: SurfaceKHR,
                    _p_surface_capabilities: *mut SurfaceCapabilitiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_capabilities_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfaceCapabilitiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_capabilities_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_surface_formats_khr: unsafe {
                extern "system" fn get_physical_device_surface_formats_khr(
                    _physical_device: PhysicalDevice,
                    _surface: SurfaceKHR,
                    _p_surface_format_count: *mut u32,
                    _p_surface_formats: *mut SurfaceFormatKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_formats_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfaceFormatsKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_formats_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_surface_present_modes_khr: unsafe {
                extern "system" fn get_physical_device_surface_present_modes_khr(
                    _physical_device: PhysicalDevice,
                    _surface: SurfaceKHR,
                    _p_present_mode_count: *mut u32,
                    _p_present_modes: *mut PresentModeKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_present_modes_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfacePresentModesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_present_modes_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySurfaceKHR.html>"]
    pub unsafe fn destroy_surface_khr(
        &self,
        instance: Instance,
        surface: SurfaceKHR,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_surface_khr)(instance, surface, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceSupportKHR.html>"]
    pub unsafe fn get_physical_device_surface_support_khr(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        surface: SurfaceKHR,
        p_supported: *mut Bool32,
    ) -> Result {
        (self.get_physical_device_surface_support_khr)(
            physical_device,
            queue_family_index,
            surface,
            p_supported,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceCapabilitiesKHR.html>"]
    pub unsafe fn get_physical_device_surface_capabilities_khr(
        &self,
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_surface_capabilities: *mut SurfaceCapabilitiesKHR,
    ) -> Result {
        (self.get_physical_device_surface_capabilities_khr)(
            physical_device,
            surface,
            p_surface_capabilities,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceFormatsKHR.html>"]
    pub unsafe fn get_physical_device_surface_formats_khr(
        &self,
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_surface_format_count: *mut u32,
        p_surface_formats: *mut SurfaceFormatKHR,
    ) -> Result {
        (self.get_physical_device_surface_formats_khr)(
            physical_device,
            surface,
            p_surface_format_count,
            p_surface_formats,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfacePresentModesKHR.html>"]
    pub unsafe fn get_physical_device_surface_present_modes_khr(
        &self,
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_present_mode_count: *mut u32,
        p_present_modes: *mut PresentModeKHR,
    ) -> Result {
        (self.get_physical_device_surface_present_modes_khr)(
            physical_device,
            surface,
            p_present_mode_count,
            p_present_modes,
        )
    }
}
#[doc = "Generated from 'VK_KHR_surface'"]
impl Result {
    pub const ERROR_SURFACE_LOST_KHR: Self = Self(-1000000000);
}
#[doc = "Generated from 'VK_KHR_surface'"]
impl Result {
    pub const ERROR_NATIVE_WINDOW_IN_USE_KHR: Self = Self(-1000000001);
}
#[doc = "Generated from 'VK_KHR_surface'"]
impl ObjectType {
    pub const SURFACE_KHR: Self = Self(1_000_000_000);
}
impl KhrSwapchainFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_swapchain\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 70u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateSwapchainKHR = extern "system" fn(
    device: Device,
    p_create_info: *const SwapchainCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_swapchain: *mut SwapchainKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroySwapchainKHR = extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainImagesKHR = extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_swapchain_image_count: *mut u32,
    p_swapchain_images: *mut Image,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireNextImageKHR = extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    timeout: u64,
    semaphore: Semaphore,
    fence: Fence,
    p_image_index: *mut u32,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueuePresentKHR =
    extern "system" fn(queue: Queue, p_present_info: *const PresentInfoKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupPresentCapabilitiesKHR = extern "system" fn(
    device: Device,
    p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupSurfacePresentModesKHR = extern "system" fn(
    device: Device,
    surface: SurfaceKHR,
    p_modes: *mut DeviceGroupPresentModeFlagsKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDevicePresentRectanglesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_rect_count: *mut u32,
    p_rects: *mut Rect2D,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireNextImage2KHR = extern "system" fn(
    device: Device,
    p_acquire_info: *const AcquireNextImageInfoKHR,
    p_image_index: *mut u32,
) -> Result;
pub struct KhrSwapchainFn {
    pub create_swapchain_khr: extern "system" fn(
        device: Device,
        p_create_info: *const SwapchainCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_swapchain: *mut SwapchainKHR,
    ) -> Result,
    pub destroy_swapchain_khr: extern "system" fn(
        device: Device,
        swapchain: SwapchainKHR,
        p_allocator: *const AllocationCallbacks,
    ),
    pub get_swapchain_images_khr: extern "system" fn(
        device: Device,
        swapchain: SwapchainKHR,
        p_swapchain_image_count: *mut u32,
        p_swapchain_images: *mut Image,
    ) -> Result,
    pub acquire_next_image_khr: extern "system" fn(
        device: Device,
        swapchain: SwapchainKHR,
        timeout: u64,
        semaphore: Semaphore,
        fence: Fence,
        p_image_index: *mut u32,
    ) -> Result,
    pub queue_present_khr:
        extern "system" fn(queue: Queue, p_present_info: *const PresentInfoKHR) -> Result,
    pub get_device_group_present_capabilities_khr: extern "system" fn(
        device: Device,
        p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
    ) -> Result,
    pub get_device_group_surface_present_modes_khr: extern "system" fn(
        device: Device,
        surface: SurfaceKHR,
        p_modes: *mut DeviceGroupPresentModeFlagsKHR,
    ) -> Result,
    pub get_physical_device_present_rectangles_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_rect_count: *mut u32,
        p_rects: *mut Rect2D,
    ) -> Result,
    pub acquire_next_image2_khr: extern "system" fn(
        device: Device,
        p_acquire_info: *const AcquireNextImageInfoKHR,
        p_image_index: *mut u32,
    ) -> Result,
}
unsafe impl Send for KhrSwapchainFn {}
unsafe impl Sync for KhrSwapchainFn {}
impl ::std::clone::Clone for KhrSwapchainFn {
    fn clone(&self) -> Self {
        KhrSwapchainFn {
            create_swapchain_khr: self.create_swapchain_khr,
            destroy_swapchain_khr: self.destroy_swapchain_khr,
            get_swapchain_images_khr: self.get_swapchain_images_khr,
            acquire_next_image_khr: self.acquire_next_image_khr,
            queue_present_khr: self.queue_present_khr,
            get_device_group_present_capabilities_khr: self
                .get_device_group_present_capabilities_khr,
            get_device_group_surface_present_modes_khr: self
                .get_device_group_surface_present_modes_khr,
            get_physical_device_present_rectangles_khr: self
                .get_physical_device_present_rectangles_khr,
            acquire_next_image2_khr: self.acquire_next_image2_khr,
        }
    }
}
impl KhrSwapchainFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSwapchainFn {
            create_swapchain_khr: unsafe {
                extern "system" fn create_swapchain_khr(
                    _device: Device,
                    _p_create_info: *const SwapchainCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_swapchain: *mut SwapchainKHR,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_swapchain_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateSwapchainKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_swapchain_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_swapchain_khr: unsafe {
                extern "system" fn destroy_swapchain_khr(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_swapchain_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroySwapchainKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_swapchain_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_swapchain_images_khr: unsafe {
                extern "system" fn get_swapchain_images_khr(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                    _p_swapchain_image_count: *mut u32,
                    _p_swapchain_images: *mut Image,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_swapchain_images_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetSwapchainImagesKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    get_swapchain_images_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            acquire_next_image_khr: unsafe {
                extern "system" fn acquire_next_image_khr(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                    _timeout: u64,
                    _semaphore: Semaphore,
                    _fence: Fence,
                    _p_image_index: *mut u32,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_next_image_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireNextImageKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_next_image_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_present_khr: unsafe {
                extern "system" fn queue_present_khr(
                    _queue: Queue,
                    _p_present_info: *const PresentInfoKHR,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(queue_present_khr)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkQueuePresentKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    queue_present_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_group_present_capabilities_khr: unsafe {
                extern "system" fn get_device_group_present_capabilities_khr(
                    _device: Device,
                    _p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_group_present_capabilities_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceGroupPresentCapabilitiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_group_present_capabilities_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_group_surface_present_modes_khr: unsafe {
                extern "system" fn get_device_group_surface_present_modes_khr(
                    _device: Device,
                    _surface: SurfaceKHR,
                    _p_modes: *mut DeviceGroupPresentModeFlagsKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_group_surface_present_modes_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceGroupSurfacePresentModesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_group_surface_present_modes_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_present_rectangles_khr: unsafe {
                extern "system" fn get_physical_device_present_rectangles_khr(
                    _physical_device: PhysicalDevice,
                    _surface: SurfaceKHR,
                    _p_rect_count: *mut u32,
                    _p_rects: *mut Rect2D,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_present_rectangles_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDevicePresentRectanglesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_present_rectangles_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            acquire_next_image2_khr: unsafe {
                extern "system" fn acquire_next_image2_khr(
                    _device: Device,
                    _p_acquire_info: *const AcquireNextImageInfoKHR,
                    _p_image_index: *mut u32,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_next_image2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireNextImage2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_next_image2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSwapchainKHR.html>"]
    pub unsafe fn create_swapchain_khr(
        &self,
        device: Device,
        p_create_info: *const SwapchainCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_swapchain: *mut SwapchainKHR,
    ) -> Result {
        (self.create_swapchain_khr)(device, p_create_info, p_allocator, p_swapchain)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySwapchainKHR.html>"]
    pub unsafe fn destroy_swapchain_khr(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_swapchain_khr)(device, swapchain, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSwapchainImagesKHR.html>"]
    pub unsafe fn get_swapchain_images_khr(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
        p_swapchain_image_count: *mut u32,
        p_swapchain_images: *mut Image,
    ) -> Result {
        (self.get_swapchain_images_khr)(
            device,
            swapchain,
            p_swapchain_image_count,
            p_swapchain_images,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireNextImageKHR.html>"]
    pub unsafe fn acquire_next_image_khr(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
        timeout: u64,
        semaphore: Semaphore,
        fence: Fence,
        p_image_index: *mut u32,
    ) -> Result {
        (self.acquire_next_image_khr)(device, swapchain, timeout, semaphore, fence, p_image_index)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueuePresentKHR.html>"]
    pub unsafe fn queue_present_khr(
        &self,
        queue: Queue,
        p_present_info: *const PresentInfoKHR,
    ) -> Result {
        (self.queue_present_khr)(queue, p_present_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupPresentCapabilitiesKHR.html>"]
    pub unsafe fn get_device_group_present_capabilities_khr(
        &self,
        device: Device,
        p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
    ) -> Result {
        (self.get_device_group_present_capabilities_khr)(
            device,
            p_device_group_present_capabilities,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupSurfacePresentModesKHR.html>"]
    pub unsafe fn get_device_group_surface_present_modes_khr(
        &self,
        device: Device,
        surface: SurfaceKHR,
        p_modes: *mut DeviceGroupPresentModeFlagsKHR,
    ) -> Result {
        (self.get_device_group_surface_present_modes_khr)(device, surface, p_modes)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDevicePresentRectanglesKHR.html>"]
    pub unsafe fn get_physical_device_present_rectangles_khr(
        &self,
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_rect_count: *mut u32,
        p_rects: *mut Rect2D,
    ) -> Result {
        (self.get_physical_device_present_rectangles_khr)(
            physical_device,
            surface,
            p_rect_count,
            p_rects,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireNextImage2KHR.html>"]
    pub unsafe fn acquire_next_image2_khr(
        &self,
        device: Device,
        p_acquire_info: *const AcquireNextImageInfoKHR,
        p_image_index: *mut u32,
    ) -> Result {
        (self.acquire_next_image2_khr)(device, p_acquire_info, p_image_index)
    }
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const SWAPCHAIN_CREATE_INFO_KHR: Self = Self(1_000_001_000);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const PRESENT_INFO_KHR: Self = Self(1_000_001_001);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl ImageLayout {
    pub const PRESENT_SRC_KHR: Self = Self(1_000_001_002);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl Result {
    pub const SUBOPTIMAL_KHR: Self = Self(1_000_001_003);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl Result {
    pub const ERROR_OUT_OF_DATE_KHR: Self = Self(-1000001004);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl ObjectType {
    pub const SWAPCHAIN_KHR: Self = Self(1_000_001_000);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const DEVICE_GROUP_PRESENT_CAPABILITIES_KHR: Self = Self(1_000_060_007);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const IMAGE_SWAPCHAIN_CREATE_INFO_KHR: Self = Self(1_000_060_008);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR: Self = Self(1_000_060_009);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const ACQUIRE_NEXT_IMAGE_INFO_KHR: Self = Self(1_000_060_010);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const DEVICE_GROUP_PRESENT_INFO_KHR: Self = Self(1_000_060_011);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl StructureType {
    pub const DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR: Self = Self(1_000_060_012);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl SwapchainCreateFlagsKHR {
    pub const SPLIT_INSTANCE_BIND_REGIONS: Self = Self(0b1);
}
#[doc = "Generated from 'VK_KHR_swapchain'"]
impl SwapchainCreateFlagsKHR {
    pub const PROTECTED: Self = Self(0b10);
}
impl KhrDisplayFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_display\0").expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 23u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDisplayPropertiesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayPropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayPlanePropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayPlaneSupportedDisplaysKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    plane_index: u32,
    p_display_count: *mut u32,
    p_displays: *mut DisplayKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayModePropertiesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    display: DisplayKHR,
    p_property_count: *mut u32,
    p_properties: *mut DisplayModePropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDisplayModeKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    display: DisplayKHR,
    p_create_info: *const DisplayModeCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_mode: *mut DisplayModeKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayPlaneCapabilitiesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    mode: DisplayModeKHR,
    plane_index: u32,
    p_capabilities: *mut DisplayPlaneCapabilitiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDisplayPlaneSurfaceKHR = extern "system" fn(
    instance: Instance,
    p_create_info: *const DisplaySurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct KhrDisplayFn {
    pub get_physical_device_display_properties_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayPropertiesKHR,
    ) -> Result,
    pub get_physical_device_display_plane_properties_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayPlanePropertiesKHR,
    ) -> Result,
    pub get_display_plane_supported_displays_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        plane_index: u32,
        p_display_count: *mut u32,
        p_displays: *mut DisplayKHR,
    ) -> Result,
    pub get_display_mode_properties_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        display: DisplayKHR,
        p_property_count: *mut u32,
        p_properties: *mut DisplayModePropertiesKHR,
    ) -> Result,
    pub create_display_mode_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        display: DisplayKHR,
        p_create_info: *const DisplayModeCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_mode: *mut DisplayModeKHR,
    ) -> Result,
    pub get_display_plane_capabilities_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        mode: DisplayModeKHR,
        plane_index: u32,
        p_capabilities: *mut DisplayPlaneCapabilitiesKHR,
    ) -> Result,
    pub create_display_plane_surface_khr: extern "system" fn(
        instance: Instance,
        p_create_info: *const DisplaySurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for KhrDisplayFn {}
unsafe impl Sync for KhrDisplayFn {}
impl ::std::clone::Clone for KhrDisplayFn {
    fn clone(&self) -> Self {
        KhrDisplayFn {
            get_physical_device_display_properties_khr: self
                .get_physical_device_display_properties_khr,
            get_physical_device_display_plane_properties_khr: self
                .get_physical_device_display_plane_properties_khr,
            get_display_plane_supported_displays_khr: self.get_display_plane_supported_displays_khr,
            get_display_mode_properties_khr: self.get_display_mode_properties_khr,
            create_display_mode_khr: self.create_display_mode_khr,
            get_display_plane_capabilities_khr: self.get_display_plane_capabilities_khr,
            create_display_plane_surface_khr: self.create_display_plane_surface_khr,
        }
    }
}
impl KhrDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDisplayFn {
            get_physical_device_display_properties_khr: unsafe {
                extern "system" fn get_physical_device_display_properties_khr(
                    _physical_device: PhysicalDevice,
                    _p_property_count: *mut u32,
                    _p_properties: *mut DisplayPropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_display_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceDisplayPropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_display_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_display_plane_properties_khr: unsafe {
                extern "system" fn get_physical_device_display_plane_properties_khr(
                    _physical_device: PhysicalDevice,
                    _p_property_count: *mut u32,
                    _p_properties: *mut DisplayPlanePropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_display_plane_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceDisplayPlanePropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_display_plane_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_display_plane_supported_displays_khr: unsafe {
                extern "system" fn get_display_plane_supported_displays_khr(
                    _physical_device: PhysicalDevice,
                    _plane_index: u32,
                    _p_display_count: *mut u32,
                    _p_displays: *mut DisplayKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_display_plane_supported_displays_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDisplayPlaneSupportedDisplaysKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_display_plane_supported_displays_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_display_mode_properties_khr: unsafe {
                extern "system" fn get_display_mode_properties_khr(
                    _physical_device: PhysicalDevice,
                    _display: DisplayKHR,
                    _p_property_count: *mut u32,
                    _p_properties: *mut DisplayModePropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_display_mode_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDisplayModePropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_display_mode_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_display_mode_khr: unsafe {
                extern "system" fn create_display_mode_khr(
                    _physical_device: PhysicalDevice,
                    _display: DisplayKHR,
                    _p_create_info: *const DisplayModeCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_mode: *mut DisplayModeKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_display_mode_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateDisplayModeKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_display_mode_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_display_plane_capabilities_khr: unsafe {
                extern "system" fn get_display_plane_capabilities_khr(
                    _physical_device: PhysicalDevice,
                    _mode: DisplayModeKHR,
                    _plane_index: u32,
                    _p_capabilities: *mut DisplayPlaneCapabilitiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_display_plane_capabilities_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDisplayPlaneCapabilitiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_display_plane_capabilities_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_display_plane_surface_khr: unsafe {
                extern "system" fn create_display_plane_surface_khr(
                    _instance: Instance,
                    _p_create_info: *const DisplaySurfaceCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_display_plane_surface_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDisplayPlaneSurfaceKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_display_plane_surface_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceDisplayPropertiesKHR.html>"]
    pub unsafe fn get_physical_device_display_properties_khr(
        &self,
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayPropertiesKHR,
    ) -> Result {
        (self.get_physical_device_display_properties_khr)(
            physical_device,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceDisplayPlanePropertiesKHR.html>"]
    pub unsafe fn get_physical_device_display_plane_properties_khr(
        &self,
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayPlanePropertiesKHR,
    ) -> Result {
        (self.get_physical_device_display_plane_properties_khr)(
            physical_device,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDisplayPlaneSupportedDisplaysKHR.html>"]
    pub unsafe fn get_display_plane_supported_displays_khr(
        &self,
        physical_device: PhysicalDevice,
        plane_index: u32,
        p_display_count: *mut u32,
        p_displays: *mut DisplayKHR,
    ) -> Result {
        (self.get_display_plane_supported_displays_khr)(
            physical_device,
            plane_index,
            p_display_count,
            p_displays,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDisplayModePropertiesKHR.html>"]
    pub unsafe fn get_display_mode_properties_khr(
        &self,
        physical_device: PhysicalDevice,
        display: DisplayKHR,
        p_property_count: *mut u32,
        p_properties: *mut DisplayModePropertiesKHR,
    ) -> Result {
        (self.get_display_mode_properties_khr)(
            physical_device,
            display,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDisplayModeKHR.html>"]
    pub unsafe fn create_display_mode_khr(
        &self,
        physical_device: PhysicalDevice,
        display: DisplayKHR,
        p_create_info: *const DisplayModeCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_mode: *mut DisplayModeKHR,
    ) -> Result {
        (self.create_display_mode_khr)(physical_device, display, p_create_info, p_allocator, p_mode)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDisplayPlaneCapabilitiesKHR.html>"]
    pub unsafe fn get_display_plane_capabilities_khr(
        &self,
        physical_device: PhysicalDevice,
        mode: DisplayModeKHR,
        plane_index: u32,
        p_capabilities: *mut DisplayPlaneCapabilitiesKHR,
    ) -> Result {
        (self.get_display_plane_capabilities_khr)(
            physical_device,
            mode,
            plane_index,
            p_capabilities,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDisplayPlaneSurfaceKHR.html>"]
    pub unsafe fn create_display_plane_surface_khr(
        &self,
        instance: Instance,
        p_create_info: *const DisplaySurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_display_plane_surface_khr)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_KHR_display'"]
impl StructureType {
    pub const DISPLAY_MODE_CREATE_INFO_KHR: Self = Self(1_000_002_000);
}
#[doc = "Generated from 'VK_KHR_display'"]
impl StructureType {
    pub const DISPLAY_SURFACE_CREATE_INFO_KHR: Self = Self(1_000_002_001);
}
#[doc = "Generated from 'VK_KHR_display'"]
impl ObjectType {
    pub const DISPLAY_KHR: Self = Self(1_000_002_000);
}
#[doc = "Generated from 'VK_KHR_display'"]
impl ObjectType {
    pub const DISPLAY_MODE_KHR: Self = Self(1_000_002_001);
}
impl KhrDisplaySwapchainFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_display_swapchain\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 10u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateSharedSwapchainsKHR = extern "system" fn(
    device: Device,
    swapchain_count: u32,
    p_create_infos: *const SwapchainCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_swapchains: *mut SwapchainKHR,
) -> Result;
pub struct KhrDisplaySwapchainFn {
    pub create_shared_swapchains_khr: extern "system" fn(
        device: Device,
        swapchain_count: u32,
        p_create_infos: *const SwapchainCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_swapchains: *mut SwapchainKHR,
    ) -> Result,
}
unsafe impl Send for KhrDisplaySwapchainFn {}
unsafe impl Sync for KhrDisplaySwapchainFn {}
impl ::std::clone::Clone for KhrDisplaySwapchainFn {
    fn clone(&self) -> Self {
        KhrDisplaySwapchainFn {
            create_shared_swapchains_khr: self.create_shared_swapchains_khr,
        }
    }
}
impl KhrDisplaySwapchainFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDisplaySwapchainFn {
            create_shared_swapchains_khr: unsafe {
                extern "system" fn create_shared_swapchains_khr(
                    _device: Device,
                    _swapchain_count: u32,
                    _p_create_infos: *const SwapchainCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_swapchains: *mut SwapchainKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_shared_swapchains_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateSharedSwapchainsKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_shared_swapchains_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSharedSwapchainsKHR.html>"]
    pub unsafe fn create_shared_swapchains_khr(
        &self,
        device: Device,
        swapchain_count: u32,
        p_create_infos: *const SwapchainCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_swapchains: *mut SwapchainKHR,
    ) -> Result {
        (self.create_shared_swapchains_khr)(
            device,
            swapchain_count,
            p_create_infos,
            p_allocator,
            p_swapchains,
        )
    }
}
#[doc = "Generated from 'VK_KHR_display_swapchain'"]
impl StructureType {
    pub const DISPLAY_PRESENT_INFO_KHR: Self = Self(1_000_003_000);
}
#[doc = "Generated from 'VK_KHR_display_swapchain'"]
impl Result {
    pub const ERROR_INCOMPATIBLE_DISPLAY_KHR: Self = Self(-1000003001);
}
impl KhrXlibSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_xlib_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 6u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateXlibSurfaceKHR = extern "system" fn(
    instance: Instance,
    p_create_info: *const XlibSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    dpy: *mut Display,
    visual_id: VisualID,
) -> Bool32;
pub struct KhrXlibSurfaceFn {
    pub create_xlib_surface_khr: extern "system" fn(
        instance: Instance,
        p_create_info: *const XlibSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
    pub get_physical_device_xlib_presentation_support_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        dpy: *mut Display,
        visual_id: VisualID,
    ) -> Bool32,
}
unsafe impl Send for KhrXlibSurfaceFn {}
unsafe impl Sync for KhrXlibSurfaceFn {}
impl ::std::clone::Clone for KhrXlibSurfaceFn {
    fn clone(&self) -> Self {
        KhrXlibSurfaceFn {
            create_xlib_surface_khr: self.create_xlib_surface_khr,
            get_physical_device_xlib_presentation_support_khr: self
                .get_physical_device_xlib_presentation_support_khr,
        }
    }
}
impl KhrXlibSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrXlibSurfaceFn {
            create_xlib_surface_khr: unsafe {
                extern "system" fn create_xlib_surface_khr(
                    _instance: Instance,
                    _p_create_info: *const XlibSurfaceCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_xlib_surface_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateXlibSurfaceKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_xlib_surface_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_xlib_presentation_support_khr: unsafe {
                extern "system" fn get_physical_device_xlib_presentation_support_khr(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                    _dpy: *mut Display,
                    _visual_id: VisualID,
                ) -> Bool32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_xlib_presentation_support_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceXlibPresentationSupportKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_xlib_presentation_support_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateXlibSurfaceKHR.html>"]
    pub unsafe fn create_xlib_surface_khr(
        &self,
        instance: Instance,
        p_create_info: *const XlibSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_xlib_surface_khr)(instance, p_create_info, p_allocator, p_surface)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceXlibPresentationSupportKHR.html>"]
    pub unsafe fn get_physical_device_xlib_presentation_support_khr(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        dpy: *mut Display,
        visual_id: VisualID,
    ) -> Bool32 {
        (self.get_physical_device_xlib_presentation_support_khr)(
            physical_device,
            queue_family_index,
            dpy,
            visual_id,
        )
    }
}
#[doc = "Generated from 'VK_KHR_xlib_surface'"]
impl StructureType {
    pub const XLIB_SURFACE_CREATE_INFO_KHR: Self = Self(1_000_004_000);
}
impl KhrXcbSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_xcb_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 6u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateXcbSurfaceKHR = extern "system" fn(
    instance: Instance,
    p_create_info: *const XcbSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    connection: *mut xcb_connection_t,
    visual_id: xcb_visualid_t,
) -> Bool32;
pub struct KhrXcbSurfaceFn {
    pub create_xcb_surface_khr: extern "system" fn(
        instance: Instance,
        p_create_info: *const XcbSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
    pub get_physical_device_xcb_presentation_support_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        connection: *mut xcb_connection_t,
        visual_id: xcb_visualid_t,
    ) -> Bool32,
}
unsafe impl Send for KhrXcbSurfaceFn {}
unsafe impl Sync for KhrXcbSurfaceFn {}
impl ::std::clone::Clone for KhrXcbSurfaceFn {
    fn clone(&self) -> Self {
        KhrXcbSurfaceFn {
            create_xcb_surface_khr: self.create_xcb_surface_khr,
            get_physical_device_xcb_presentation_support_khr: self
                .get_physical_device_xcb_presentation_support_khr,
        }
    }
}
impl KhrXcbSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrXcbSurfaceFn {
            create_xcb_surface_khr: unsafe {
                extern "system" fn create_xcb_surface_khr(
                    _instance: Instance,
                    _p_create_info: *const XcbSurfaceCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_xcb_surface_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateXcbSurfaceKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_xcb_surface_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_xcb_presentation_support_khr: unsafe {
                extern "system" fn get_physical_device_xcb_presentation_support_khr(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                    _connection: *mut xcb_connection_t,
                    _visual_id: xcb_visualid_t,
                ) -> Bool32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_xcb_presentation_support_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceXcbPresentationSupportKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_xcb_presentation_support_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateXcbSurfaceKHR.html>"]
    pub unsafe fn create_xcb_surface_khr(
        &self,
        instance: Instance,
        p_create_info: *const XcbSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_xcb_surface_khr)(instance, p_create_info, p_allocator, p_surface)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceXcbPresentationSupportKHR.html>"]
    pub unsafe fn get_physical_device_xcb_presentation_support_khr(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        connection: *mut xcb_connection_t,
        visual_id: xcb_visualid_t,
    ) -> Bool32 {
        (self.get_physical_device_xcb_presentation_support_khr)(
            physical_device,
            queue_family_index,
            connection,
            visual_id,
        )
    }
}
#[doc = "Generated from 'VK_KHR_xcb_surface'"]
impl StructureType {
    pub const XCB_SURFACE_CREATE_INFO_KHR: Self = Self(1_000_005_000);
}
impl KhrWaylandSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_wayland_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 6u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateWaylandSurfaceKHR = extern "system" fn(
    instance: Instance,
    p_create_info: *const WaylandSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    display: *mut wl_display,
) -> Bool32;
pub struct KhrWaylandSurfaceFn {
    pub create_wayland_surface_khr: extern "system" fn(
        instance: Instance,
        p_create_info: *const WaylandSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
    pub get_physical_device_wayland_presentation_support_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        display: *mut wl_display,
    ) -> Bool32,
}
unsafe impl Send for KhrWaylandSurfaceFn {}
unsafe impl Sync for KhrWaylandSurfaceFn {}
impl ::std::clone::Clone for KhrWaylandSurfaceFn {
    fn clone(&self) -> Self {
        KhrWaylandSurfaceFn {
            create_wayland_surface_khr: self.create_wayland_surface_khr,
            get_physical_device_wayland_presentation_support_khr: self
                .get_physical_device_wayland_presentation_support_khr,
        }
    }
}
impl KhrWaylandSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrWaylandSurfaceFn {
            create_wayland_surface_khr: unsafe {
                extern "system" fn create_wayland_surface_khr(
                    _instance: Instance,
                    _p_create_info: *const WaylandSurfaceCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_wayland_surface_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateWaylandSurfaceKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_wayland_surface_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_wayland_presentation_support_khr: unsafe {
                extern "system" fn get_physical_device_wayland_presentation_support_khr(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                    _display: *mut wl_display,
                ) -> Bool32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_wayland_presentation_support_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceWaylandPresentationSupportKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_wayland_presentation_support_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateWaylandSurfaceKHR.html>"]
    pub unsafe fn create_wayland_surface_khr(
        &self,
        instance: Instance,
        p_create_info: *const WaylandSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_wayland_surface_khr)(instance, p_create_info, p_allocator, p_surface)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceWaylandPresentationSupportKHR.html>"]
    pub unsafe fn get_physical_device_wayland_presentation_support_khr(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        display: *mut wl_display,
    ) -> Bool32 {
        (self.get_physical_device_wayland_presentation_support_khr)(
            physical_device,
            queue_family_index,
            display,
        )
    }
}
#[doc = "Generated from 'VK_KHR_wayland_surface'"]
impl StructureType {
    pub const WAYLAND_SURFACE_CREATE_INFO_KHR: Self = Self(1_000_006_000);
}
impl KhrMirSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_mir_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
pub struct KhrMirSurfaceFn {}
unsafe impl Send for KhrMirSurfaceFn {}
unsafe impl Sync for KhrMirSurfaceFn {}
impl ::std::clone::Clone for KhrMirSurfaceFn {
    fn clone(&self) -> Self {
        KhrMirSurfaceFn {}
    }
}
impl KhrMirSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrMirSurfaceFn {}
    }
}
impl KhrAndroidSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_android_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 6u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateAndroidSurfaceKHR = extern "system" fn(
    instance: Instance,
    p_create_info: *const AndroidSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct KhrAndroidSurfaceFn {
    pub create_android_surface_khr: extern "system" fn(
        instance: Instance,
        p_create_info: *const AndroidSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for KhrAndroidSurfaceFn {}
unsafe impl Sync for KhrAndroidSurfaceFn {}
impl ::std::clone::Clone for KhrAndroidSurfaceFn {
    fn clone(&self) -> Self {
        KhrAndroidSurfaceFn {
            create_android_surface_khr: self.create_android_surface_khr,
        }
    }
}
impl KhrAndroidSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrAndroidSurfaceFn {
            create_android_surface_khr: unsafe {
                extern "system" fn create_android_surface_khr(
                    _instance: Instance,
                    _p_create_info: *const AndroidSurfaceCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_android_surface_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateAndroidSurfaceKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_android_surface_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateAndroidSurfaceKHR.html>"]
    pub unsafe fn create_android_surface_khr(
        &self,
        instance: Instance,
        p_create_info: *const AndroidSurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_android_surface_khr)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_KHR_android_surface'"]
impl StructureType {
    pub const ANDROID_SURFACE_CREATE_INFO_KHR: Self = Self(1_000_008_000);
}
impl KhrWin32SurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_win32_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 6u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateWin32SurfaceKHR = extern "system" fn(
    instance: Instance,
    p_create_info: *const Win32SurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR =
    extern "system" fn(physical_device: PhysicalDevice, queue_family_index: u32) -> Bool32;
pub struct KhrWin32SurfaceFn {
    pub create_win32_surface_khr: extern "system" fn(
        instance: Instance,
        p_create_info: *const Win32SurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
    pub get_physical_device_win32_presentation_support_khr:
        extern "system" fn(physical_device: PhysicalDevice, queue_family_index: u32) -> Bool32,
}
unsafe impl Send for KhrWin32SurfaceFn {}
unsafe impl Sync for KhrWin32SurfaceFn {}
impl ::std::clone::Clone for KhrWin32SurfaceFn {
    fn clone(&self) -> Self {
        KhrWin32SurfaceFn {
            create_win32_surface_khr: self.create_win32_surface_khr,
            get_physical_device_win32_presentation_support_khr: self
                .get_physical_device_win32_presentation_support_khr,
        }
    }
}
impl KhrWin32SurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrWin32SurfaceFn {
            create_win32_surface_khr: unsafe {
                extern "system" fn create_win32_surface_khr(
                    _instance: Instance,
                    _p_create_info: *const Win32SurfaceCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_win32_surface_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateWin32SurfaceKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_win32_surface_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_win32_presentation_support_khr: unsafe {
                extern "system" fn get_physical_device_win32_presentation_support_khr(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                ) -> Bool32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_win32_presentation_support_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceWin32PresentationSupportKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_win32_presentation_support_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateWin32SurfaceKHR.html>"]
    pub unsafe fn create_win32_surface_khr(
        &self,
        instance: Instance,
        p_create_info: *const Win32SurfaceCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_win32_surface_khr)(instance, p_create_info, p_allocator, p_surface)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceWin32PresentationSupportKHR.html>"]
    pub unsafe fn get_physical_device_win32_presentation_support_khr(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
    ) -> Bool32 {
        (self.get_physical_device_win32_presentation_support_khr)(
            physical_device,
            queue_family_index,
        )
    }
}
#[doc = "Generated from 'VK_KHR_win32_surface'"]
impl StructureType {
    pub const WIN32_SURFACE_CREATE_INFO_KHR: Self = Self(1_000_009_000);
}
impl AndroidNativeBufferFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ANDROID_native_buffer\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 8u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainGrallocUsageANDROID = extern "system" fn(
    device: Device,
    format: Format,
    image_usage: ImageUsageFlags,
    gralloc_usage: *mut c_int,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireImageANDROID = extern "system" fn(
    device: Device,
    image: Image,
    native_fence_fd: c_int,
    semaphore: Semaphore,
    fence: Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueueSignalReleaseImageANDROID = extern "system" fn(
    queue: Queue,
    wait_semaphore_count: u32,
    p_wait_semaphores: *const Semaphore,
    image: Image,
    p_native_fence_fd: *mut c_int,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainGrallocUsage2ANDROID = extern "system" fn(
    device: Device,
    format: Format,
    image_usage: ImageUsageFlags,
    swapchain_image_usage: SwapchainImageUsageFlagsANDROID,
    gralloc_consumer_usage: *mut u64,
    gralloc_producer_usage: *mut u64,
) -> Result;
pub struct AndroidNativeBufferFn {
    pub get_swapchain_gralloc_usage_android: extern "system" fn(
        device: Device,
        format: Format,
        image_usage: ImageUsageFlags,
        gralloc_usage: *mut c_int,
    ) -> Result,
    pub acquire_image_android: extern "system" fn(
        device: Device,
        image: Image,
        native_fence_fd: c_int,
        semaphore: Semaphore,
        fence: Fence,
    ) -> Result,
    pub queue_signal_release_image_android: extern "system" fn(
        queue: Queue,
        wait_semaphore_count: u32,
        p_wait_semaphores: *const Semaphore,
        image: Image,
        p_native_fence_fd: *mut c_int,
    ) -> Result,
    pub get_swapchain_gralloc_usage2_android: extern "system" fn(
        device: Device,
        format: Format,
        image_usage: ImageUsageFlags,
        swapchain_image_usage: SwapchainImageUsageFlagsANDROID,
        gralloc_consumer_usage: *mut u64,
        gralloc_producer_usage: *mut u64,
    ) -> Result,
}
unsafe impl Send for AndroidNativeBufferFn {}
unsafe impl Sync for AndroidNativeBufferFn {}
impl ::std::clone::Clone for AndroidNativeBufferFn {
    fn clone(&self) -> Self {
        AndroidNativeBufferFn {
            get_swapchain_gralloc_usage_android: self.get_swapchain_gralloc_usage_android,
            acquire_image_android: self.acquire_image_android,
            queue_signal_release_image_android: self.queue_signal_release_image_android,
            get_swapchain_gralloc_usage2_android: self.get_swapchain_gralloc_usage2_android,
        }
    }
}
impl AndroidNativeBufferFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AndroidNativeBufferFn {
            get_swapchain_gralloc_usage_android: unsafe {
                extern "system" fn get_swapchain_gralloc_usage_android(
                    _device: Device,
                    _format: Format,
                    _image_usage: ImageUsageFlags,
                    _gralloc_usage: *mut c_int,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_swapchain_gralloc_usage_android)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetSwapchainGrallocUsageANDROID\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_swapchain_gralloc_usage_android
                } else {
                    ::std::mem::transmute(val)
                }
            },
            acquire_image_android: unsafe {
                extern "system" fn acquire_image_android(
                    _device: Device,
                    _image: Image,
                    _native_fence_fd: c_int,
                    _semaphore: Semaphore,
                    _fence: Fence,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_image_android)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireImageANDROID\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_image_android
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_signal_release_image_android: unsafe {
                extern "system" fn queue_signal_release_image_android(
                    _queue: Queue,
                    _wait_semaphore_count: u32,
                    _p_wait_semaphores: *const Semaphore,
                    _image: Image,
                    _p_native_fence_fd: *mut c_int,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(queue_signal_release_image_android)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkQueueSignalReleaseImageANDROID\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    queue_signal_release_image_android
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_swapchain_gralloc_usage2_android: unsafe {
                extern "system" fn get_swapchain_gralloc_usage2_android(
                    _device: Device,
                    _format: Format,
                    _image_usage: ImageUsageFlags,
                    _swapchain_image_usage: SwapchainImageUsageFlagsANDROID,
                    _gralloc_consumer_usage: *mut u64,
                    _gralloc_producer_usage: *mut u64,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_swapchain_gralloc_usage2_android)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetSwapchainGrallocUsage2ANDROID\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_swapchain_gralloc_usage2_android
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSwapchainGrallocUsageANDROID.html>"]
    pub unsafe fn get_swapchain_gralloc_usage_android(
        &self,
        device: Device,
        format: Format,
        image_usage: ImageUsageFlags,
        gralloc_usage: *mut c_int,
    ) -> Result {
        (self.get_swapchain_gralloc_usage_android)(device, format, image_usage, gralloc_usage)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireImageANDROID.html>"]
    pub unsafe fn acquire_image_android(
        &self,
        device: Device,
        image: Image,
        native_fence_fd: c_int,
        semaphore: Semaphore,
        fence: Fence,
    ) -> Result {
        (self.acquire_image_android)(device, image, native_fence_fd, semaphore, fence)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueSignalReleaseImageANDROID.html>"]
    pub unsafe fn queue_signal_release_image_android(
        &self,
        queue: Queue,
        wait_semaphore_count: u32,
        p_wait_semaphores: *const Semaphore,
        image: Image,
        p_native_fence_fd: *mut c_int,
    ) -> Result {
        (self.queue_signal_release_image_android)(
            queue,
            wait_semaphore_count,
            p_wait_semaphores,
            image,
            p_native_fence_fd,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSwapchainGrallocUsage2ANDROID.html>"]
    pub unsafe fn get_swapchain_gralloc_usage2_android(
        &self,
        device: Device,
        format: Format,
        image_usage: ImageUsageFlags,
        swapchain_image_usage: SwapchainImageUsageFlagsANDROID,
        gralloc_consumer_usage: *mut u64,
        gralloc_producer_usage: *mut u64,
    ) -> Result {
        (self.get_swapchain_gralloc_usage2_android)(
            device,
            format,
            image_usage,
            swapchain_image_usage,
            gralloc_consumer_usage,
            gralloc_producer_usage,
        )
    }
}
#[doc = "Generated from 'VK_ANDROID_native_buffer'"]
impl StructureType {
    pub const NATIVE_BUFFER_ANDROID: Self = Self(1_000_010_000);
}
#[doc = "Generated from 'VK_ANDROID_native_buffer'"]
impl StructureType {
    pub const SWAPCHAIN_IMAGE_CREATE_INFO_ANDROID: Self = Self(1_000_010_001);
}
#[doc = "Generated from 'VK_ANDROID_native_buffer'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PRESENTATION_PROPERTIES_ANDROID: Self = Self(1_000_010_002);
}
impl ExtDebugReportFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_debug_report\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 9u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDebugReportCallbackEXT = extern "system" fn(
    instance: Instance,
    p_create_info: *const DebugReportCallbackCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_callback: *mut DebugReportCallbackEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDebugReportCallbackEXT = extern "system" fn(
    instance: Instance,
    callback: DebugReportCallbackEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkDebugReportMessageEXT = extern "system" fn(
    instance: Instance,
    flags: DebugReportFlagsEXT,
    object_type: DebugReportObjectTypeEXT,
    object: u64,
    location: usize,
    message_code: i32,
    p_layer_prefix: *const c_char,
    p_message: *const c_char,
);
pub struct ExtDebugReportFn {
    pub create_debug_report_callback_ext: extern "system" fn(
        instance: Instance,
        p_create_info: *const DebugReportCallbackCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_callback: *mut DebugReportCallbackEXT,
    ) -> Result,
    pub destroy_debug_report_callback_ext: extern "system" fn(
        instance: Instance,
        callback: DebugReportCallbackEXT,
        p_allocator: *const AllocationCallbacks,
    ),
    pub debug_report_message_ext: extern "system" fn(
        instance: Instance,
        flags: DebugReportFlagsEXT,
        object_type: DebugReportObjectTypeEXT,
        object: u64,
        location: usize,
        message_code: i32,
        p_layer_prefix: *const c_char,
        p_message: *const c_char,
    ),
}
unsafe impl Send for ExtDebugReportFn {}
unsafe impl Sync for ExtDebugReportFn {}
impl ::std::clone::Clone for ExtDebugReportFn {
    fn clone(&self) -> Self {
        ExtDebugReportFn {
            create_debug_report_callback_ext: self.create_debug_report_callback_ext,
            destroy_debug_report_callback_ext: self.destroy_debug_report_callback_ext,
            debug_report_message_ext: self.debug_report_message_ext,
        }
    }
}
impl ExtDebugReportFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDebugReportFn {
            create_debug_report_callback_ext: unsafe {
                extern "system" fn create_debug_report_callback_ext(
                    _instance: Instance,
                    _p_create_info: *const DebugReportCallbackCreateInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_callback: *mut DebugReportCallbackEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_debug_report_callback_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDebugReportCallbackEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_debug_report_callback_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_debug_report_callback_ext: unsafe {
                extern "system" fn destroy_debug_report_callback_ext(
                    _instance: Instance,
                    _callback: DebugReportCallbackEXT,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_debug_report_callback_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyDebugReportCallbackEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_debug_report_callback_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            debug_report_message_ext: unsafe {
                extern "system" fn debug_report_message_ext(
                    _instance: Instance,
                    _flags: DebugReportFlagsEXT,
                    _object_type: DebugReportObjectTypeEXT,
                    _object: u64,
                    _location: usize,
                    _message_code: i32,
                    _p_layer_prefix: *const c_char,
                    _p_message: *const c_char,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(debug_report_message_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDebugReportMessageEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    debug_report_message_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDebugReportCallbackEXT.html>"]
    pub unsafe fn create_debug_report_callback_ext(
        &self,
        instance: Instance,
        p_create_info: *const DebugReportCallbackCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_callback: *mut DebugReportCallbackEXT,
    ) -> Result {
        (self.create_debug_report_callback_ext)(instance, p_create_info, p_allocator, p_callback)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDebugReportCallbackEXT.html>"]
    pub unsafe fn destroy_debug_report_callback_ext(
        &self,
        instance: Instance,
        callback: DebugReportCallbackEXT,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_debug_report_callback_ext)(instance, callback, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDebugReportMessageEXT.html>"]
    pub unsafe fn debug_report_message_ext(
        &self,
        instance: Instance,
        flags: DebugReportFlagsEXT,
        object_type: DebugReportObjectTypeEXT,
        object: u64,
        location: usize,
        message_code: i32,
        p_layer_prefix: *const c_char,
        p_message: *const c_char,
    ) {
        (self.debug_report_message_ext)(
            instance,
            flags,
            object_type,
            object,
            location,
            message_code,
            p_layer_prefix,
            p_message,
        )
    }
}
#[doc = "Generated from 'VK_EXT_debug_report'"]
impl StructureType {
    pub const DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT: Self = Self(1_000_011_000);
}
#[doc = "Generated from 'VK_EXT_debug_report'"]
impl StructureType {
    pub const DEBUG_REPORT_CREATE_INFO_EXT: Self =
        StructureType::DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
}
#[doc = "Generated from 'VK_EXT_debug_report'"]
impl Result {
    pub const ERROR_VALIDATION_FAILED_EXT: Self = Self(-1000011001);
}
#[doc = "Generated from 'VK_EXT_debug_report'"]
impl ObjectType {
    pub const DEBUG_REPORT_CALLBACK_EXT: Self = Self(1_000_011_000);
}
#[doc = "Generated from 'VK_EXT_debug_report'"]
impl DebugReportObjectTypeEXT {
    pub const SAMPLER_YCBCR_CONVERSION: Self = Self(1_000_156_000);
}
#[doc = "Generated from 'VK_EXT_debug_report'"]
impl DebugReportObjectTypeEXT {
    pub const DESCRIPTOR_UPDATE_TEMPLATE: Self = Self(1_000_085_000);
}
impl NvGlslShaderFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_glsl_shader\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvGlslShaderFn {}
unsafe impl Send for NvGlslShaderFn {}
unsafe impl Sync for NvGlslShaderFn {}
impl ::std::clone::Clone for NvGlslShaderFn {
    fn clone(&self) -> Self {
        NvGlslShaderFn {}
    }
}
impl NvGlslShaderFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvGlslShaderFn {}
    }
}
#[doc = "Generated from 'VK_NV_glsl_shader'"]
impl Result {
    pub const ERROR_INVALID_SHADER_NV: Self = Self(-1000012000);
}
impl ExtDepthRangeUnrestrictedFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_depth_range_unrestricted\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtDepthRangeUnrestrictedFn {}
unsafe impl Send for ExtDepthRangeUnrestrictedFn {}
unsafe impl Sync for ExtDepthRangeUnrestrictedFn {}
impl ::std::clone::Clone for ExtDepthRangeUnrestrictedFn {
    fn clone(&self) -> Self {
        ExtDepthRangeUnrestrictedFn {}
    }
}
impl ExtDepthRangeUnrestrictedFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDepthRangeUnrestrictedFn {}
    }
}
impl KhrSamplerMirrorClampToEdgeFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_sampler_mirror_clamp_to_edge\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
pub struct KhrSamplerMirrorClampToEdgeFn {}
unsafe impl Send for KhrSamplerMirrorClampToEdgeFn {}
unsafe impl Sync for KhrSamplerMirrorClampToEdgeFn {}
impl ::std::clone::Clone for KhrSamplerMirrorClampToEdgeFn {
    fn clone(&self) -> Self {
        KhrSamplerMirrorClampToEdgeFn {}
    }
}
impl KhrSamplerMirrorClampToEdgeFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSamplerMirrorClampToEdgeFn {}
    }
}
#[doc = "Generated from 'VK_KHR_sampler_mirror_clamp_to_edge'"]
impl SamplerAddressMode {
    pub const MIRROR_CLAMP_TO_EDGE: Self = Self(4);
}
#[doc = "Generated from 'VK_KHR_sampler_mirror_clamp_to_edge'"]
impl SamplerAddressMode {
    pub const MIRROR_CLAMP_TO_EDGE_KHR: Self = SamplerAddressMode::MIRROR_CLAMP_TO_EDGE;
}
impl ImgFilterCubicFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_filter_cubic\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ImgFilterCubicFn {}
unsafe impl Send for ImgFilterCubicFn {}
unsafe impl Sync for ImgFilterCubicFn {}
impl ::std::clone::Clone for ImgFilterCubicFn {
    fn clone(&self) -> Self {
        ImgFilterCubicFn {}
    }
}
impl ImgFilterCubicFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ImgFilterCubicFn {}
    }
}
#[doc = "Generated from 'VK_IMG_filter_cubic'"]
impl Filter {
    pub const CUBIC_IMG: Self = Self(1_000_015_000);
}
#[doc = "Generated from 'VK_IMG_filter_cubic'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_FILTER_CUBIC_IMG: Self = Self(0b10_0000_0000_0000);
}
impl AmdExtension17Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_17\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension17Fn {}
unsafe impl Send for AmdExtension17Fn {}
unsafe impl Sync for AmdExtension17Fn {}
impl ::std::clone::Clone for AmdExtension17Fn {
    fn clone(&self) -> Self {
        AmdExtension17Fn {}
    }
}
impl AmdExtension17Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension17Fn {}
    }
}
impl AmdExtension18Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_18\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension18Fn {}
unsafe impl Send for AmdExtension18Fn {}
unsafe impl Sync for AmdExtension18Fn {}
impl ::std::clone::Clone for AmdExtension18Fn {
    fn clone(&self) -> Self {
        AmdExtension18Fn {}
    }
}
impl AmdExtension18Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension18Fn {}
    }
}
impl AmdRasterizationOrderFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_rasterization_order\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdRasterizationOrderFn {}
unsafe impl Send for AmdRasterizationOrderFn {}
unsafe impl Sync for AmdRasterizationOrderFn {}
impl ::std::clone::Clone for AmdRasterizationOrderFn {
    fn clone(&self) -> Self {
        AmdRasterizationOrderFn {}
    }
}
impl AmdRasterizationOrderFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdRasterizationOrderFn {}
    }
}
#[doc = "Generated from 'VK_AMD_rasterization_order'"]
impl StructureType {
    pub const PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD: Self = Self(1_000_018_000);
}
impl AmdExtension20Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_20\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension20Fn {}
unsafe impl Send for AmdExtension20Fn {}
unsafe impl Sync for AmdExtension20Fn {}
impl ::std::clone::Clone for AmdExtension20Fn {
    fn clone(&self) -> Self {
        AmdExtension20Fn {}
    }
}
impl AmdExtension20Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension20Fn {}
    }
}
impl AmdShaderTrinaryMinmaxFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_trinary_minmax\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdShaderTrinaryMinmaxFn {}
unsafe impl Send for AmdShaderTrinaryMinmaxFn {}
unsafe impl Sync for AmdShaderTrinaryMinmaxFn {}
impl ::std::clone::Clone for AmdShaderTrinaryMinmaxFn {
    fn clone(&self) -> Self {
        AmdShaderTrinaryMinmaxFn {}
    }
}
impl AmdShaderTrinaryMinmaxFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderTrinaryMinmaxFn {}
    }
}
impl AmdShaderExplicitVertexParameterFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_explicit_vertex_parameter\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdShaderExplicitVertexParameterFn {}
unsafe impl Send for AmdShaderExplicitVertexParameterFn {}
unsafe impl Sync for AmdShaderExplicitVertexParameterFn {}
impl ::std::clone::Clone for AmdShaderExplicitVertexParameterFn {
    fn clone(&self) -> Self {
        AmdShaderExplicitVertexParameterFn {}
    }
}
impl AmdShaderExplicitVertexParameterFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderExplicitVertexParameterFn {}
    }
}
impl ExtDebugMarkerFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_debug_marker\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkDebugMarkerSetObjectTagEXT =
    extern "system" fn(device: Device, p_tag_info: *const DebugMarkerObjectTagInfoEXT) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDebugMarkerSetObjectNameEXT =
    extern "system" fn(device: Device, p_name_info: *const DebugMarkerObjectNameInfoEXT) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDebugMarkerBeginEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const DebugMarkerMarkerInfoEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDebugMarkerEndEXT = extern "system" fn(command_buffer: CommandBuffer);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDebugMarkerInsertEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const DebugMarkerMarkerInfoEXT,
);
pub struct ExtDebugMarkerFn {
    pub debug_marker_set_object_tag_ext: extern "system" fn(
        device: Device,
        p_tag_info: *const DebugMarkerObjectTagInfoEXT,
    ) -> Result,
    pub debug_marker_set_object_name_ext: extern "system" fn(
        device: Device,
        p_name_info: *const DebugMarkerObjectNameInfoEXT,
    ) -> Result,
    pub cmd_debug_marker_begin_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        p_marker_info: *const DebugMarkerMarkerInfoEXT,
    ),
    pub cmd_debug_marker_end_ext: extern "system" fn(command_buffer: CommandBuffer),
    pub cmd_debug_marker_insert_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        p_marker_info: *const DebugMarkerMarkerInfoEXT,
    ),
}
unsafe impl Send for ExtDebugMarkerFn {}
unsafe impl Sync for ExtDebugMarkerFn {}
impl ::std::clone::Clone for ExtDebugMarkerFn {
    fn clone(&self) -> Self {
        ExtDebugMarkerFn {
            debug_marker_set_object_tag_ext: self.debug_marker_set_object_tag_ext,
            debug_marker_set_object_name_ext: self.debug_marker_set_object_name_ext,
            cmd_debug_marker_begin_ext: self.cmd_debug_marker_begin_ext,
            cmd_debug_marker_end_ext: self.cmd_debug_marker_end_ext,
            cmd_debug_marker_insert_ext: self.cmd_debug_marker_insert_ext,
        }
    }
}
impl ExtDebugMarkerFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDebugMarkerFn {
            debug_marker_set_object_tag_ext: unsafe {
                extern "system" fn debug_marker_set_object_tag_ext(
                    _device: Device,
                    _p_tag_info: *const DebugMarkerObjectTagInfoEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(debug_marker_set_object_tag_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDebugMarkerSetObjectTagEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    debug_marker_set_object_tag_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            debug_marker_set_object_name_ext: unsafe {
                extern "system" fn debug_marker_set_object_name_ext(
                    _device: Device,
                    _p_name_info: *const DebugMarkerObjectNameInfoEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(debug_marker_set_object_name_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDebugMarkerSetObjectNameEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    debug_marker_set_object_name_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_debug_marker_begin_ext: unsafe {
                extern "system" fn cmd_debug_marker_begin_ext(
                    _command_buffer: CommandBuffer,
                    _p_marker_info: *const DebugMarkerMarkerInfoEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_debug_marker_begin_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDebugMarkerBeginEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_debug_marker_begin_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_debug_marker_end_ext: unsafe {
                extern "system" fn cmd_debug_marker_end_ext(_command_buffer: CommandBuffer) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_debug_marker_end_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDebugMarkerEndEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_debug_marker_end_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_debug_marker_insert_ext: unsafe {
                extern "system" fn cmd_debug_marker_insert_ext(
                    _command_buffer: CommandBuffer,
                    _p_marker_info: *const DebugMarkerMarkerInfoEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_debug_marker_insert_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDebugMarkerInsertEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_debug_marker_insert_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDebugMarkerSetObjectTagEXT.html>"]
    pub unsafe fn debug_marker_set_object_tag_ext(
        &self,
        device: Device,
        p_tag_info: *const DebugMarkerObjectTagInfoEXT,
    ) -> Result {
        (self.debug_marker_set_object_tag_ext)(device, p_tag_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDebugMarkerSetObjectNameEXT.html>"]
    pub unsafe fn debug_marker_set_object_name_ext(
        &self,
        device: Device,
        p_name_info: *const DebugMarkerObjectNameInfoEXT,
    ) -> Result {
        (self.debug_marker_set_object_name_ext)(device, p_name_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDebugMarkerBeginEXT.html>"]
    pub unsafe fn cmd_debug_marker_begin_ext(
        &self,
        command_buffer: CommandBuffer,
        p_marker_info: *const DebugMarkerMarkerInfoEXT,
    ) {
        (self.cmd_debug_marker_begin_ext)(command_buffer, p_marker_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDebugMarkerEndEXT.html>"]
    pub unsafe fn cmd_debug_marker_end_ext(&self, command_buffer: CommandBuffer) {
        (self.cmd_debug_marker_end_ext)(command_buffer)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDebugMarkerInsertEXT.html>"]
    pub unsafe fn cmd_debug_marker_insert_ext(
        &self,
        command_buffer: CommandBuffer,
        p_marker_info: *const DebugMarkerMarkerInfoEXT,
    ) {
        (self.cmd_debug_marker_insert_ext)(command_buffer, p_marker_info)
    }
}
#[doc = "Generated from 'VK_EXT_debug_marker'"]
impl StructureType {
    pub const DEBUG_MARKER_OBJECT_NAME_INFO_EXT: Self = Self(1_000_022_000);
}
#[doc = "Generated from 'VK_EXT_debug_marker'"]
impl StructureType {
    pub const DEBUG_MARKER_OBJECT_TAG_INFO_EXT: Self = Self(1_000_022_001);
}
#[doc = "Generated from 'VK_EXT_debug_marker'"]
impl StructureType {
    pub const DEBUG_MARKER_MARKER_INFO_EXT: Self = Self(1_000_022_002);
}
impl AmdExtension24Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_24\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension24Fn {}
unsafe impl Send for AmdExtension24Fn {}
unsafe impl Sync for AmdExtension24Fn {}
impl ::std::clone::Clone for AmdExtension24Fn {
    fn clone(&self) -> Self {
        AmdExtension24Fn {}
    }
}
impl AmdExtension24Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension24Fn {}
    }
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl QueueFlags {
    pub const RESERVED_6_KHR: Self = Self(0b100_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl PipelineStageFlags {
    pub const RESERVED_27_KHR: Self = Self(0b1000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl AccessFlags {
    pub const RESERVED_30_KHR: Self = Self(0b100_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl BufferUsageFlags {
    pub const RESERVED_15_KHR: Self = Self(0b1000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl BufferUsageFlags {
    pub const RESERVED_16_KHR: Self = Self(0b1_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl ImageUsageFlags {
    pub const RESERVED_13_KHR: Self = Self(0b10_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl ImageUsageFlags {
    pub const RESERVED_14_KHR: Self = Self(0b100_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl ImageUsageFlags {
    pub const RESERVED_15_KHR: Self = Self(0b1000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl FormatFeatureFlags {
    pub const RESERVED_27_KHR: Self = Self(0b1000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl FormatFeatureFlags {
    pub const RESERVED_28_KHR: Self = Self(0b1_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_24'"]
impl QueryType {
    pub const RESERVED_8: Self = Self(1_000_023_008);
}
impl AmdExtension25Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_25\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension25Fn {}
unsafe impl Send for AmdExtension25Fn {}
unsafe impl Sync for AmdExtension25Fn {}
impl ::std::clone::Clone for AmdExtension25Fn {
    fn clone(&self) -> Self {
        AmdExtension25Fn {}
    }
}
impl AmdExtension25Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension25Fn {}
    }
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl QueueFlags {
    pub const RESERVED_5_KHR: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl PipelineStageFlags {
    pub const RESERVED_26_KHR: Self = Self(0b100_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl AccessFlags {
    pub const RESERVED_28_KHR: Self = Self(0b1_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl AccessFlags {
    pub const RESERVED_29_KHR: Self = Self(0b10_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl BufferUsageFlags {
    pub const RESERVED_13_KHR: Self = Self(0b10_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl BufferUsageFlags {
    pub const RESERVED_14_KHR: Self = Self(0b100_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl ImageUsageFlags {
    pub const RESERVED_10_KHR: Self = Self(0b100_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl ImageUsageFlags {
    pub const RESERVED_11_KHR: Self = Self(0b1000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl ImageUsageFlags {
    pub const RESERVED_12_KHR: Self = Self(0b1_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl FormatFeatureFlags {
    pub const RESERVED_25_KHR: Self = Self(0b10_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl FormatFeatureFlags {
    pub const RESERVED_26_KHR: Self = Self(0b100_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_25'"]
impl QueryType {
    pub const RESERVED_4: Self = Self(1_000_024_004);
}
impl AmdGcnShaderFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_gcn_shader\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdGcnShaderFn {}
unsafe impl Send for AmdGcnShaderFn {}
unsafe impl Sync for AmdGcnShaderFn {}
impl ::std::clone::Clone for AmdGcnShaderFn {
    fn clone(&self) -> Self {
        AmdGcnShaderFn {}
    }
}
impl AmdGcnShaderFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdGcnShaderFn {}
    }
}
impl NvDedicatedAllocationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_dedicated_allocation\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvDedicatedAllocationFn {}
unsafe impl Send for NvDedicatedAllocationFn {}
unsafe impl Sync for NvDedicatedAllocationFn {}
impl ::std::clone::Clone for NvDedicatedAllocationFn {
    fn clone(&self) -> Self {
        NvDedicatedAllocationFn {}
    }
}
impl NvDedicatedAllocationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvDedicatedAllocationFn {}
    }
}
#[doc = "Generated from 'VK_NV_dedicated_allocation'"]
impl StructureType {
    pub const DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV: Self = Self(1_000_026_000);
}
#[doc = "Generated from 'VK_NV_dedicated_allocation'"]
impl StructureType {
    pub const DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV: Self = Self(1_000_026_001);
}
#[doc = "Generated from 'VK_NV_dedicated_allocation'"]
impl StructureType {
    pub const DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV: Self = Self(1_000_026_002);
}
impl ExtExtension28Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_28\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension28Fn {}
unsafe impl Send for ExtExtension28Fn {}
unsafe impl Sync for ExtExtension28Fn {}
impl ::std::clone::Clone for ExtExtension28Fn {
    fn clone(&self) -> Self {
        ExtExtension28Fn {}
    }
}
impl ExtExtension28Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension28Fn {}
    }
}
impl ExtTransformFeedbackFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_transform_feedback\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindTransformFeedbackBuffersEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    first_binding: u32,
    binding_count: u32,
    p_buffers: *const Buffer,
    p_offsets: *const DeviceSize,
    p_sizes: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginTransformFeedbackEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    first_counter_buffer: u32,
    counter_buffer_count: u32,
    p_counter_buffers: *const Buffer,
    p_counter_buffer_offsets: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndTransformFeedbackEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    first_counter_buffer: u32,
    counter_buffer_count: u32,
    p_counter_buffers: *const Buffer,
    p_counter_buffer_offsets: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginQueryIndexedEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    query_pool: QueryPool,
    query: u32,
    flags: QueryControlFlags,
    index: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndQueryIndexedEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    query_pool: QueryPool,
    query: u32,
    index: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndirectByteCountEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    instance_count: u32,
    first_instance: u32,
    counter_buffer: Buffer,
    counter_buffer_offset: DeviceSize,
    counter_offset: u32,
    vertex_stride: u32,
);
pub struct ExtTransformFeedbackFn {
    pub cmd_bind_transform_feedback_buffers_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        first_binding: u32,
        binding_count: u32,
        p_buffers: *const Buffer,
        p_offsets: *const DeviceSize,
        p_sizes: *const DeviceSize,
    ),
    pub cmd_begin_transform_feedback_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        first_counter_buffer: u32,
        counter_buffer_count: u32,
        p_counter_buffers: *const Buffer,
        p_counter_buffer_offsets: *const DeviceSize,
    ),
    pub cmd_end_transform_feedback_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        first_counter_buffer: u32,
        counter_buffer_count: u32,
        p_counter_buffers: *const Buffer,
        p_counter_buffer_offsets: *const DeviceSize,
    ),
    pub cmd_begin_query_indexed_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        query: u32,
        flags: QueryControlFlags,
        index: u32,
    ),
    pub cmd_end_query_indexed_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        query: u32,
        index: u32,
    ),
    pub cmd_draw_indirect_byte_count_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        instance_count: u32,
        first_instance: u32,
        counter_buffer: Buffer,
        counter_buffer_offset: DeviceSize,
        counter_offset: u32,
        vertex_stride: u32,
    ),
}
unsafe impl Send for ExtTransformFeedbackFn {}
unsafe impl Sync for ExtTransformFeedbackFn {}
impl ::std::clone::Clone for ExtTransformFeedbackFn {
    fn clone(&self) -> Self {
        ExtTransformFeedbackFn {
            cmd_bind_transform_feedback_buffers_ext: self.cmd_bind_transform_feedback_buffers_ext,
            cmd_begin_transform_feedback_ext: self.cmd_begin_transform_feedback_ext,
            cmd_end_transform_feedback_ext: self.cmd_end_transform_feedback_ext,
            cmd_begin_query_indexed_ext: self.cmd_begin_query_indexed_ext,
            cmd_end_query_indexed_ext: self.cmd_end_query_indexed_ext,
            cmd_draw_indirect_byte_count_ext: self.cmd_draw_indirect_byte_count_ext,
        }
    }
}
impl ExtTransformFeedbackFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtTransformFeedbackFn {
            cmd_bind_transform_feedback_buffers_ext: unsafe {
                extern "system" fn cmd_bind_transform_feedback_buffers_ext(
                    _command_buffer: CommandBuffer,
                    _first_binding: u32,
                    _binding_count: u32,
                    _p_buffers: *const Buffer,
                    _p_offsets: *const DeviceSize,
                    _p_sizes: *const DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_transform_feedback_buffers_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBindTransformFeedbackBuffersEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_transform_feedback_buffers_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_transform_feedback_ext: unsafe {
                extern "system" fn cmd_begin_transform_feedback_ext(
                    _command_buffer: CommandBuffer,
                    _first_counter_buffer: u32,
                    _counter_buffer_count: u32,
                    _p_counter_buffers: *const Buffer,
                    _p_counter_buffer_offsets: *const DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_transform_feedback_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBeginTransformFeedbackEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_transform_feedback_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_transform_feedback_ext: unsafe {
                extern "system" fn cmd_end_transform_feedback_ext(
                    _command_buffer: CommandBuffer,
                    _first_counter_buffer: u32,
                    _counter_buffer_count: u32,
                    _p_counter_buffers: *const Buffer,
                    _p_counter_buffer_offsets: *const DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_transform_feedback_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdEndTransformFeedbackEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_transform_feedback_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_query_indexed_ext: unsafe {
                extern "system" fn cmd_begin_query_indexed_ext(
                    _command_buffer: CommandBuffer,
                    _query_pool: QueryPool,
                    _query: u32,
                    _flags: QueryControlFlags,
                    _index: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_query_indexed_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBeginQueryIndexedEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_query_indexed_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_query_indexed_ext: unsafe {
                extern "system" fn cmd_end_query_indexed_ext(
                    _command_buffer: CommandBuffer,
                    _query_pool: QueryPool,
                    _query: u32,
                    _index: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_query_indexed_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdEndQueryIndexedEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_query_indexed_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_indirect_byte_count_ext: unsafe {
                extern "system" fn cmd_draw_indirect_byte_count_ext(
                    _command_buffer: CommandBuffer,
                    _instance_count: u32,
                    _first_instance: u32,
                    _counter_buffer: Buffer,
                    _counter_buffer_offset: DeviceSize,
                    _counter_offset: u32,
                    _vertex_stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_indirect_byte_count_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdDrawIndirectByteCountEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indirect_byte_count_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindTransformFeedbackBuffersEXT.html>"]
    pub unsafe fn cmd_bind_transform_feedback_buffers_ext(
        &self,
        command_buffer: CommandBuffer,
        first_binding: u32,
        binding_count: u32,
        p_buffers: *const Buffer,
        p_offsets: *const DeviceSize,
        p_sizes: *const DeviceSize,
    ) {
        (self.cmd_bind_transform_feedback_buffers_ext)(
            command_buffer,
            first_binding,
            binding_count,
            p_buffers,
            p_offsets,
            p_sizes,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginTransformFeedbackEXT.html>"]
    pub unsafe fn cmd_begin_transform_feedback_ext(
        &self,
        command_buffer: CommandBuffer,
        first_counter_buffer: u32,
        counter_buffer_count: u32,
        p_counter_buffers: *const Buffer,
        p_counter_buffer_offsets: *const DeviceSize,
    ) {
        (self.cmd_begin_transform_feedback_ext)(
            command_buffer,
            first_counter_buffer,
            counter_buffer_count,
            p_counter_buffers,
            p_counter_buffer_offsets,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndTransformFeedbackEXT.html>"]
    pub unsafe fn cmd_end_transform_feedback_ext(
        &self,
        command_buffer: CommandBuffer,
        first_counter_buffer: u32,
        counter_buffer_count: u32,
        p_counter_buffers: *const Buffer,
        p_counter_buffer_offsets: *const DeviceSize,
    ) {
        (self.cmd_end_transform_feedback_ext)(
            command_buffer,
            first_counter_buffer,
            counter_buffer_count,
            p_counter_buffers,
            p_counter_buffer_offsets,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginQueryIndexedEXT.html>"]
    pub unsafe fn cmd_begin_query_indexed_ext(
        &self,
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        query: u32,
        flags: QueryControlFlags,
        index: u32,
    ) {
        (self.cmd_begin_query_indexed_ext)(command_buffer, query_pool, query, flags, index)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndQueryIndexedEXT.html>"]
    pub unsafe fn cmd_end_query_indexed_ext(
        &self,
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        query: u32,
        index: u32,
    ) {
        (self.cmd_end_query_indexed_ext)(command_buffer, query_pool, query, index)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirectByteCountEXT.html>"]
    pub unsafe fn cmd_draw_indirect_byte_count_ext(
        &self,
        command_buffer: CommandBuffer,
        instance_count: u32,
        first_instance: u32,
        counter_buffer: Buffer,
        counter_buffer_offset: DeviceSize,
        counter_offset: u32,
        vertex_stride: u32,
    ) {
        (self.cmd_draw_indirect_byte_count_ext)(
            command_buffer,
            instance_count,
            first_instance,
            counter_buffer,
            counter_buffer_offset,
            counter_offset,
            vertex_stride,
        )
    }
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT: Self = Self(1_000_028_000);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT: Self = Self(1_000_028_001);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl StructureType {
    pub const PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT: Self = Self(1_000_028_002);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl QueryType {
    pub const TRANSFORM_FEEDBACK_STREAM_EXT: Self = Self(1_000_028_004);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl BufferUsageFlags {
    pub const TRANSFORM_FEEDBACK_BUFFER_EXT: Self = Self(0b1000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl BufferUsageFlags {
    pub const TRANSFORM_FEEDBACK_COUNTER_BUFFER_EXT: Self = Self(0b1_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl AccessFlags {
    pub const TRANSFORM_FEEDBACK_WRITE_EXT: Self = Self(0b10_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl AccessFlags {
    pub const TRANSFORM_FEEDBACK_COUNTER_READ_EXT: Self = Self(0b100_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl AccessFlags {
    pub const TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT: Self =
        Self(0b1000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_transform_feedback'"]
impl PipelineStageFlags {
    pub const TRANSFORM_FEEDBACK_EXT: Self = Self(0b1_0000_0000_0000_0000_0000_0000);
}
impl NvxExtension30Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NVX_extension_30\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvxExtension30Fn {}
unsafe impl Send for NvxExtension30Fn {}
unsafe impl Sync for NvxExtension30Fn {}
impl ::std::clone::Clone for NvxExtension30Fn {
    fn clone(&self) -> Self {
        NvxExtension30Fn {}
    }
}
impl NvxExtension30Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvxExtension30Fn {}
    }
}
impl NvxImageViewHandleFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NVX_image_view_handle\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageViewHandleNVX =
    extern "system" fn(device: Device, p_info: *const ImageViewHandleInfoNVX) -> u32;
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageViewAddressNVX = extern "system" fn(
    device: Device,
    image_view: ImageView,
    p_properties: *mut ImageViewAddressPropertiesNVX,
) -> Result;
pub struct NvxImageViewHandleFn {
    pub get_image_view_handle_nvx:
        extern "system" fn(device: Device, p_info: *const ImageViewHandleInfoNVX) -> u32,
    pub get_image_view_address_nvx: extern "system" fn(
        device: Device,
        image_view: ImageView,
        p_properties: *mut ImageViewAddressPropertiesNVX,
    ) -> Result,
}
unsafe impl Send for NvxImageViewHandleFn {}
unsafe impl Sync for NvxImageViewHandleFn {}
impl ::std::clone::Clone for NvxImageViewHandleFn {
    fn clone(&self) -> Self {
        NvxImageViewHandleFn {
            get_image_view_handle_nvx: self.get_image_view_handle_nvx,
            get_image_view_address_nvx: self.get_image_view_address_nvx,
        }
    }
}
impl NvxImageViewHandleFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvxImageViewHandleFn {
            get_image_view_handle_nvx: unsafe {
                extern "system" fn get_image_view_handle_nvx(
                    _device: Device,
                    _p_info: *const ImageViewHandleInfoNVX,
                ) -> u32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_view_handle_nvx)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetImageViewHandleNVX\0");
                let val = _f(cname);
                if val.is_null() {
                    get_image_view_handle_nvx
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_image_view_address_nvx: unsafe {
                extern "system" fn get_image_view_address_nvx(
                    _device: Device,
                    _image_view: ImageView,
                    _p_properties: *mut ImageViewAddressPropertiesNVX,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_view_address_nvx)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetImageViewAddressNVX\0");
                let val = _f(cname);
                if val.is_null() {
                    get_image_view_address_nvx
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageViewHandleNVX.html>"]
    pub unsafe fn get_image_view_handle_nvx(
        &self,
        device: Device,
        p_info: *const ImageViewHandleInfoNVX,
    ) -> u32 {
        (self.get_image_view_handle_nvx)(device, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageViewAddressNVX.html>"]
    pub unsafe fn get_image_view_address_nvx(
        &self,
        device: Device,
        image_view: ImageView,
        p_properties: *mut ImageViewAddressPropertiesNVX,
    ) -> Result {
        (self.get_image_view_address_nvx)(device, image_view, p_properties)
    }
}
#[doc = "Generated from 'VK_NVX_image_view_handle'"]
impl StructureType {
    pub const IMAGE_VIEW_HANDLE_INFO_NVX: Self = Self(1_000_030_000);
}
#[doc = "Generated from 'VK_NVX_image_view_handle'"]
impl StructureType {
    pub const IMAGE_VIEW_ADDRESS_PROPERTIES_NVX: Self = Self(1_000_030_001);
}
impl AmdExtension32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension32Fn {}
unsafe impl Send for AmdExtension32Fn {}
unsafe impl Sync for AmdExtension32Fn {}
impl ::std::clone::Clone for AmdExtension32Fn {
    fn clone(&self) -> Self {
        AmdExtension32Fn {}
    }
}
impl AmdExtension32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension32Fn {}
    }
}
impl AmdExtension33Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_33\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension33Fn {}
unsafe impl Send for AmdExtension33Fn {}
unsafe impl Sync for AmdExtension33Fn {}
impl ::std::clone::Clone for AmdExtension33Fn {
    fn clone(&self) -> Self {
        AmdExtension33Fn {}
    }
}
impl AmdExtension33Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension33Fn {}
    }
}
impl AmdDrawIndirectCountFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_draw_indirect_count\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndirectCount = extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    count_buffer: Buffer,
    count_buffer_offset: DeviceSize,
    max_draw_count: u32,
    stride: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndexedIndirectCount = extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    count_buffer: Buffer,
    count_buffer_offset: DeviceSize,
    max_draw_count: u32,
    stride: u32,
);
pub struct AmdDrawIndirectCountFn {
    pub cmd_draw_indirect_count_amd: extern "system" fn(
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ),
    pub cmd_draw_indexed_indirect_count_amd: extern "system" fn(
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ),
}
unsafe impl Send for AmdDrawIndirectCountFn {}
unsafe impl Sync for AmdDrawIndirectCountFn {}
impl ::std::clone::Clone for AmdDrawIndirectCountFn {
    fn clone(&self) -> Self {
        AmdDrawIndirectCountFn {
            cmd_draw_indirect_count_amd: self.cmd_draw_indirect_count_amd,
            cmd_draw_indexed_indirect_count_amd: self.cmd_draw_indexed_indirect_count_amd,
        }
    }
}
impl AmdDrawIndirectCountFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdDrawIndirectCountFn {
            cmd_draw_indirect_count_amd: unsafe {
                extern "system" fn cmd_draw_indirect_count_amd(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _count_buffer: Buffer,
                    _count_buffer_offset: DeviceSize,
                    _max_draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_indirect_count_amd)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawIndirectCountAMD\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indirect_count_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_indexed_indirect_count_amd: unsafe {
                extern "system" fn cmd_draw_indexed_indirect_count_amd(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _count_buffer: Buffer,
                    _count_buffer_offset: DeviceSize,
                    _max_draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_indexed_indirect_count_amd)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdDrawIndexedIndirectCountAMD\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indexed_indirect_count_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirectCountAMD.html>"]
    pub unsafe fn cmd_draw_indirect_count_amd(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indirect_count_amd)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexedIndirectCountAMD.html>"]
    pub unsafe fn cmd_draw_indexed_indirect_count_amd(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indexed_indirect_count_amd)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        )
    }
}
impl AmdExtension35Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_35\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension35Fn {}
unsafe impl Send for AmdExtension35Fn {}
unsafe impl Sync for AmdExtension35Fn {}
impl ::std::clone::Clone for AmdExtension35Fn {
    fn clone(&self) -> Self {
        AmdExtension35Fn {}
    }
}
impl AmdExtension35Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension35Fn {}
    }
}
impl AmdNegativeViewportHeightFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_negative_viewport_height\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdNegativeViewportHeightFn {}
unsafe impl Send for AmdNegativeViewportHeightFn {}
unsafe impl Sync for AmdNegativeViewportHeightFn {}
impl ::std::clone::Clone for AmdNegativeViewportHeightFn {
    fn clone(&self) -> Self {
        AmdNegativeViewportHeightFn {}
    }
}
impl AmdNegativeViewportHeightFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdNegativeViewportHeightFn {}
    }
}
impl AmdGpuShaderHalfFloatFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_gpu_shader_half_float\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct AmdGpuShaderHalfFloatFn {}
unsafe impl Send for AmdGpuShaderHalfFloatFn {}
unsafe impl Sync for AmdGpuShaderHalfFloatFn {}
impl ::std::clone::Clone for AmdGpuShaderHalfFloatFn {
    fn clone(&self) -> Self {
        AmdGpuShaderHalfFloatFn {}
    }
}
impl AmdGpuShaderHalfFloatFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdGpuShaderHalfFloatFn {}
    }
}
impl AmdShaderBallotFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_ballot\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdShaderBallotFn {}
unsafe impl Send for AmdShaderBallotFn {}
unsafe impl Sync for AmdShaderBallotFn {}
impl ::std::clone::Clone for AmdShaderBallotFn {
    fn clone(&self) -> Self {
        AmdShaderBallotFn {}
    }
}
impl AmdShaderBallotFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderBallotFn {}
    }
}
impl AmdExtension39Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_39\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension39Fn {}
unsafe impl Send for AmdExtension39Fn {}
unsafe impl Sync for AmdExtension39Fn {}
impl ::std::clone::Clone for AmdExtension39Fn {
    fn clone(&self) -> Self {
        AmdExtension39Fn {}
    }
}
impl AmdExtension39Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension39Fn {}
    }
}
impl AmdExtension40Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_40\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension40Fn {}
unsafe impl Send for AmdExtension40Fn {}
unsafe impl Sync for AmdExtension40Fn {}
impl ::std::clone::Clone for AmdExtension40Fn {
    fn clone(&self) -> Self {
        AmdExtension40Fn {}
    }
}
impl AmdExtension40Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension40Fn {}
    }
}
impl AmdExtension41Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_41\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension41Fn {}
unsafe impl Send for AmdExtension41Fn {}
unsafe impl Sync for AmdExtension41Fn {}
impl ::std::clone::Clone for AmdExtension41Fn {
    fn clone(&self) -> Self {
        AmdExtension41Fn {}
    }
}
impl AmdExtension41Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension41Fn {}
    }
}
impl AmdTextureGatherBiasLodFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_texture_gather_bias_lod\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdTextureGatherBiasLodFn {}
unsafe impl Send for AmdTextureGatherBiasLodFn {}
unsafe impl Sync for AmdTextureGatherBiasLodFn {}
impl ::std::clone::Clone for AmdTextureGatherBiasLodFn {
    fn clone(&self) -> Self {
        AmdTextureGatherBiasLodFn {}
    }
}
impl AmdTextureGatherBiasLodFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdTextureGatherBiasLodFn {}
    }
}
#[doc = "Generated from 'VK_AMD_texture_gather_bias_lod'"]
impl StructureType {
    pub const TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD: Self = Self(1_000_041_000);
}
impl AmdShaderInfoFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_info\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetShaderInfoAMD = extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    shader_stage: ShaderStageFlags,
    info_type: ShaderInfoTypeAMD,
    p_info_size: *mut usize,
    p_info: *mut c_void,
) -> Result;
pub struct AmdShaderInfoFn {
    pub get_shader_info_amd: extern "system" fn(
        device: Device,
        pipeline: Pipeline,
        shader_stage: ShaderStageFlags,
        info_type: ShaderInfoTypeAMD,
        p_info_size: *mut usize,
        p_info: *mut c_void,
    ) -> Result,
}
unsafe impl Send for AmdShaderInfoFn {}
unsafe impl Sync for AmdShaderInfoFn {}
impl ::std::clone::Clone for AmdShaderInfoFn {
    fn clone(&self) -> Self {
        AmdShaderInfoFn {
            get_shader_info_amd: self.get_shader_info_amd,
        }
    }
}
impl AmdShaderInfoFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderInfoFn {
            get_shader_info_amd: unsafe {
                extern "system" fn get_shader_info_amd(
                    _device: Device,
                    _pipeline: Pipeline,
                    _shader_stage: ShaderStageFlags,
                    _info_type: ShaderInfoTypeAMD,
                    _p_info_size: *mut usize,
                    _p_info: *mut c_void,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_shader_info_amd)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetShaderInfoAMD\0");
                let val = _f(cname);
                if val.is_null() {
                    get_shader_info_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetShaderInfoAMD.html>"]
    pub unsafe fn get_shader_info_amd(
        &self,
        device: Device,
        pipeline: Pipeline,
        shader_stage: ShaderStageFlags,
        info_type: ShaderInfoTypeAMD,
        p_info_size: *mut usize,
        p_info: *mut c_void,
    ) -> Result {
        (self.get_shader_info_amd)(
            device,
            pipeline,
            shader_stage,
            info_type,
            p_info_size,
            p_info,
        )
    }
}
impl AmdExtension44Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_44\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension44Fn {}
unsafe impl Send for AmdExtension44Fn {}
unsafe impl Sync for AmdExtension44Fn {}
impl ::std::clone::Clone for AmdExtension44Fn {
    fn clone(&self) -> Self {
        AmdExtension44Fn {}
    }
}
impl AmdExtension44Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension44Fn {}
    }
}
impl AmdExtension45Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_45\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension45Fn {}
unsafe impl Send for AmdExtension45Fn {}
unsafe impl Sync for AmdExtension45Fn {}
impl ::std::clone::Clone for AmdExtension45Fn {
    fn clone(&self) -> Self {
        AmdExtension45Fn {}
    }
}
impl AmdExtension45Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension45Fn {}
    }
}
impl AmdExtension46Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_46\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension46Fn {}
unsafe impl Send for AmdExtension46Fn {}
unsafe impl Sync for AmdExtension46Fn {}
impl ::std::clone::Clone for AmdExtension46Fn {
    fn clone(&self) -> Self {
        AmdExtension46Fn {}
    }
}
impl AmdExtension46Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension46Fn {}
    }
}
impl AmdShaderImageLoadStoreLodFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_image_load_store_lod\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdShaderImageLoadStoreLodFn {}
unsafe impl Send for AmdShaderImageLoadStoreLodFn {}
unsafe impl Sync for AmdShaderImageLoadStoreLodFn {}
impl ::std::clone::Clone for AmdShaderImageLoadStoreLodFn {
    fn clone(&self) -> Self {
        AmdShaderImageLoadStoreLodFn {}
    }
}
impl AmdShaderImageLoadStoreLodFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderImageLoadStoreLodFn {}
    }
}
impl NvxExtension48Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NVX_extension_48\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvxExtension48Fn {}
unsafe impl Send for NvxExtension48Fn {}
unsafe impl Sync for NvxExtension48Fn {}
impl ::std::clone::Clone for NvxExtension48Fn {
    fn clone(&self) -> Self {
        NvxExtension48Fn {}
    }
}
impl NvxExtension48Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvxExtension48Fn {}
    }
}
impl GoogleExtension49Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_extension_49\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct GoogleExtension49Fn {}
unsafe impl Send for GoogleExtension49Fn {}
unsafe impl Sync for GoogleExtension49Fn {}
impl ::std::clone::Clone for GoogleExtension49Fn {
    fn clone(&self) -> Self {
        GoogleExtension49Fn {}
    }
}
impl GoogleExtension49Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleExtension49Fn {}
    }
}
impl GgpStreamDescriptorSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_stream_descriptor_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateStreamDescriptorSurfaceGGP = extern "system" fn(
    instance: Instance,
    p_create_info: *const StreamDescriptorSurfaceCreateInfoGGP,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct GgpStreamDescriptorSurfaceFn {
    pub create_stream_descriptor_surface_ggp: extern "system" fn(
        instance: Instance,
        p_create_info: *const StreamDescriptorSurfaceCreateInfoGGP,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for GgpStreamDescriptorSurfaceFn {}
unsafe impl Sync for GgpStreamDescriptorSurfaceFn {}
impl ::std::clone::Clone for GgpStreamDescriptorSurfaceFn {
    fn clone(&self) -> Self {
        GgpStreamDescriptorSurfaceFn {
            create_stream_descriptor_surface_ggp: self.create_stream_descriptor_surface_ggp,
        }
    }
}
impl GgpStreamDescriptorSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpStreamDescriptorSurfaceFn {
            create_stream_descriptor_surface_ggp: unsafe {
                extern "system" fn create_stream_descriptor_surface_ggp(
                    _instance: Instance,
                    _p_create_info: *const StreamDescriptorSurfaceCreateInfoGGP,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_stream_descriptor_surface_ggp)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateStreamDescriptorSurfaceGGP\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_stream_descriptor_surface_ggp
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateStreamDescriptorSurfaceGGP.html>"]
    pub unsafe fn create_stream_descriptor_surface_ggp(
        &self,
        instance: Instance,
        p_create_info: *const StreamDescriptorSurfaceCreateInfoGGP,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_stream_descriptor_surface_ggp)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_GGP_stream_descriptor_surface'"]
impl StructureType {
    pub const STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP: Self = Self(1_000_049_000);
}
impl NvCornerSampledImageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_corner_sampled_image\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct NvCornerSampledImageFn {}
unsafe impl Send for NvCornerSampledImageFn {}
unsafe impl Sync for NvCornerSampledImageFn {}
impl ::std::clone::Clone for NvCornerSampledImageFn {
    fn clone(&self) -> Self {
        NvCornerSampledImageFn {}
    }
}
impl NvCornerSampledImageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvCornerSampledImageFn {}
    }
}
#[doc = "Generated from 'VK_NV_corner_sampled_image'"]
impl ImageCreateFlags {
    pub const CORNER_SAMPLED_NV: Self = Self(0b10_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_corner_sampled_image'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV: Self = Self(1_000_050_000);
}
impl NvExtension52Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_52\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension52Fn {}
unsafe impl Send for NvExtension52Fn {}
unsafe impl Sync for NvExtension52Fn {}
impl ::std::clone::Clone for NvExtension52Fn {
    fn clone(&self) -> Self {
        NvExtension52Fn {}
    }
}
impl NvExtension52Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension52Fn {}
    }
}
#[doc = "Generated from 'VK_NV_extension_52'"]
impl ShaderModuleCreateFlags {
    pub const RESERVED_0_NV: Self = Self(0b1);
}
#[doc = "Generated from 'VK_NV_extension_52'"]
impl PipelineShaderStageCreateFlags {
    pub const RESERVED_2_NV: Self = Self(0b100);
}
impl NvExtension53Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_53\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension53Fn {}
unsafe impl Send for NvExtension53Fn {}
unsafe impl Sync for NvExtension53Fn {}
impl ::std::clone::Clone for NvExtension53Fn {
    fn clone(&self) -> Self {
        NvExtension53Fn {}
    }
}
impl NvExtension53Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension53Fn {}
    }
}
impl KhrMultiviewFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_multiview\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrMultiviewFn {}
unsafe impl Send for KhrMultiviewFn {}
unsafe impl Sync for KhrMultiviewFn {}
impl ::std::clone::Clone for KhrMultiviewFn {
    fn clone(&self) -> Self {
        KhrMultiviewFn {}
    }
}
impl KhrMultiviewFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrMultiviewFn {}
    }
}
#[doc = "Generated from 'VK_KHR_multiview'"]
impl StructureType {
    pub const RENDER_PASS_MULTIVIEW_CREATE_INFO_KHR: Self =
        StructureType::RENDER_PASS_MULTIVIEW_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_multiview'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
}
#[doc = "Generated from 'VK_KHR_multiview'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_multiview'"]
impl DependencyFlags {
    pub const VIEW_LOCAL_KHR: Self = DependencyFlags::VIEW_LOCAL;
}
impl ImgFormatPvrtcFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_format_pvrtc\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ImgFormatPvrtcFn {}
unsafe impl Send for ImgFormatPvrtcFn {}
unsafe impl Sync for ImgFormatPvrtcFn {}
impl ::std::clone::Clone for ImgFormatPvrtcFn {
    fn clone(&self) -> Self {
        ImgFormatPvrtcFn {}
    }
}
impl ImgFormatPvrtcFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ImgFormatPvrtcFn {}
    }
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC1_2BPP_UNORM_BLOCK_IMG: Self = Self(1_000_054_000);
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC1_4BPP_UNORM_BLOCK_IMG: Self = Self(1_000_054_001);
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC2_2BPP_UNORM_BLOCK_IMG: Self = Self(1_000_054_002);
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC2_4BPP_UNORM_BLOCK_IMG: Self = Self(1_000_054_003);
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC1_2BPP_SRGB_BLOCK_IMG: Self = Self(1_000_054_004);
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC1_4BPP_SRGB_BLOCK_IMG: Self = Self(1_000_054_005);
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC2_2BPP_SRGB_BLOCK_IMG: Self = Self(1_000_054_006);
}
#[doc = "Generated from 'VK_IMG_format_pvrtc'"]
impl Format {
    pub const PVRTC2_4BPP_SRGB_BLOCK_IMG: Self = Self(1_000_054_007);
}
impl NvExternalMemoryCapabilitiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_external_memory_capabilities\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV = extern "system" fn(
    physical_device: PhysicalDevice,
    format: Format,
    ty: ImageType,
    tiling: ImageTiling,
    usage: ImageUsageFlags,
    flags: ImageCreateFlags,
    external_handle_type: ExternalMemoryHandleTypeFlagsNV,
    p_external_image_format_properties: *mut ExternalImageFormatPropertiesNV,
) -> Result;
pub struct NvExternalMemoryCapabilitiesFn {
    pub get_physical_device_external_image_format_properties_nv: extern "system" fn(
        physical_device: PhysicalDevice,
        format: Format,
        ty: ImageType,
        tiling: ImageTiling,
        usage: ImageUsageFlags,
        flags: ImageCreateFlags,
        external_handle_type: ExternalMemoryHandleTypeFlagsNV,
        p_external_image_format_properties: *mut ExternalImageFormatPropertiesNV,
    ) -> Result,
}
unsafe impl Send for NvExternalMemoryCapabilitiesFn {}
unsafe impl Sync for NvExternalMemoryCapabilitiesFn {}
impl ::std::clone::Clone for NvExternalMemoryCapabilitiesFn {
    fn clone(&self) -> Self {
        NvExternalMemoryCapabilitiesFn {
            get_physical_device_external_image_format_properties_nv: self
                .get_physical_device_external_image_format_properties_nv,
        }
    }
}
impl NvExternalMemoryCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExternalMemoryCapabilitiesFn {
            get_physical_device_external_image_format_properties_nv: unsafe {
                extern "system" fn get_physical_device_external_image_format_properties_nv(
                    _physical_device: PhysicalDevice,
                    _format: Format,
                    _ty: ImageType,
                    _tiling: ImageTiling,
                    _usage: ImageUsageFlags,
                    _flags: ImageCreateFlags,
                    _external_handle_type: ExternalMemoryHandleTypeFlagsNV,
                    _p_external_image_format_properties: *mut ExternalImageFormatPropertiesNV,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_external_image_format_properties_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceExternalImageFormatPropertiesNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_external_image_format_properties_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalImageFormatPropertiesNV.html>"]
    pub unsafe fn get_physical_device_external_image_format_properties_nv(
        &self,
        physical_device: PhysicalDevice,
        format: Format,
        ty: ImageType,
        tiling: ImageTiling,
        usage: ImageUsageFlags,
        flags: ImageCreateFlags,
        external_handle_type: ExternalMemoryHandleTypeFlagsNV,
        p_external_image_format_properties: *mut ExternalImageFormatPropertiesNV,
    ) -> Result {
        (self.get_physical_device_external_image_format_properties_nv)(
            physical_device,
            format,
            ty,
            tiling,
            usage,
            flags,
            external_handle_type,
            p_external_image_format_properties,
        )
    }
}
impl NvExternalMemoryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_external_memory\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvExternalMemoryFn {}
unsafe impl Send for NvExternalMemoryFn {}
unsafe impl Sync for NvExternalMemoryFn {}
impl ::std::clone::Clone for NvExternalMemoryFn {
    fn clone(&self) -> Self {
        NvExternalMemoryFn {}
    }
}
impl NvExternalMemoryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExternalMemoryFn {}
    }
}
#[doc = "Generated from 'VK_NV_external_memory'"]
impl StructureType {
    pub const EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV: Self = Self(1_000_056_000);
}
#[doc = "Generated from 'VK_NV_external_memory'"]
impl StructureType {
    pub const EXPORT_MEMORY_ALLOCATE_INFO_NV: Self = Self(1_000_056_001);
}
impl NvExternalMemoryWin32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_external_memory_win32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryWin32HandleNV = extern "system" fn(
    device: Device,
    memory: DeviceMemory,
    handle_type: ExternalMemoryHandleTypeFlagsNV,
    p_handle: *mut HANDLE,
) -> Result;
pub struct NvExternalMemoryWin32Fn {
    pub get_memory_win32_handle_nv: extern "system" fn(
        device: Device,
        memory: DeviceMemory,
        handle_type: ExternalMemoryHandleTypeFlagsNV,
        p_handle: *mut HANDLE,
    ) -> Result,
}
unsafe impl Send for NvExternalMemoryWin32Fn {}
unsafe impl Sync for NvExternalMemoryWin32Fn {}
impl ::std::clone::Clone for NvExternalMemoryWin32Fn {
    fn clone(&self) -> Self {
        NvExternalMemoryWin32Fn {
            get_memory_win32_handle_nv: self.get_memory_win32_handle_nv,
        }
    }
}
impl NvExternalMemoryWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExternalMemoryWin32Fn {
            get_memory_win32_handle_nv: unsafe {
                extern "system" fn get_memory_win32_handle_nv(
                    _device: Device,
                    _memory: DeviceMemory,
                    _handle_type: ExternalMemoryHandleTypeFlagsNV,
                    _p_handle: *mut HANDLE,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_win32_handle_nv)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetMemoryWin32HandleNV\0");
                let val = _f(cname);
                if val.is_null() {
                    get_memory_win32_handle_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryWin32HandleNV.html>"]
    pub unsafe fn get_memory_win32_handle_nv(
        &self,
        device: Device,
        memory: DeviceMemory,
        handle_type: ExternalMemoryHandleTypeFlagsNV,
        p_handle: *mut HANDLE,
    ) -> Result {
        (self.get_memory_win32_handle_nv)(device, memory, handle_type, p_handle)
    }
}
#[doc = "Generated from 'VK_NV_external_memory_win32'"]
impl StructureType {
    pub const IMPORT_MEMORY_WIN32_HANDLE_INFO_NV: Self = Self(1_000_057_000);
}
#[doc = "Generated from 'VK_NV_external_memory_win32'"]
impl StructureType {
    pub const EXPORT_MEMORY_WIN32_HANDLE_INFO_NV: Self = Self(1_000_057_001);
}
impl NvWin32KeyedMutexFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_win32_keyed_mutex\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct NvWin32KeyedMutexFn {}
unsafe impl Send for NvWin32KeyedMutexFn {}
unsafe impl Sync for NvWin32KeyedMutexFn {}
impl ::std::clone::Clone for NvWin32KeyedMutexFn {
    fn clone(&self) -> Self {
        NvWin32KeyedMutexFn {}
    }
}
impl NvWin32KeyedMutexFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvWin32KeyedMutexFn {}
    }
}
#[doc = "Generated from 'VK_NV_win32_keyed_mutex'"]
impl StructureType {
    pub const WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV: Self = Self(1_000_058_000);
}
impl KhrGetPhysicalDeviceProperties2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_get_physical_device_properties2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceFeatures2 =
    extern "system" fn(physical_device: PhysicalDevice, p_features: *mut PhysicalDeviceFeatures2);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceProperties2 = extern "system" fn(
    physical_device: PhysicalDevice,
    p_properties: *mut PhysicalDeviceProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceFormatProperties2 = extern "system" fn(
    physical_device: PhysicalDevice,
    format: Format,
    p_format_properties: *mut FormatProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceImageFormatProperties2 = extern "system" fn(
    physical_device: PhysicalDevice,
    p_image_format_info: *const PhysicalDeviceImageFormatInfo2,
    p_image_format_properties: *mut ImageFormatProperties2,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceQueueFamilyProperties2 = extern "system" fn(
    physical_device: PhysicalDevice,
    p_queue_family_property_count: *mut u32,
    p_queue_family_properties: *mut QueueFamilyProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceMemoryProperties2 = extern "system" fn(
    physical_device: PhysicalDevice,
    p_memory_properties: *mut PhysicalDeviceMemoryProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 = extern "system" fn(
    physical_device: PhysicalDevice,
    p_format_info: *const PhysicalDeviceSparseImageFormatInfo2,
    p_property_count: *mut u32,
    p_properties: *mut SparseImageFormatProperties2,
);
pub struct KhrGetPhysicalDeviceProperties2Fn {
    pub get_physical_device_features2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_features: *mut PhysicalDeviceFeatures2,
    ),
    pub get_physical_device_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_properties: *mut PhysicalDeviceProperties2,
    ),
    pub get_physical_device_format_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        format: Format,
        p_format_properties: *mut FormatProperties2,
    ),
    pub get_physical_device_image_format_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_image_format_info: *const PhysicalDeviceImageFormatInfo2,
        p_image_format_properties: *mut ImageFormatProperties2,
    ) -> Result,
    pub get_physical_device_queue_family_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_queue_family_property_count: *mut u32,
        p_queue_family_properties: *mut QueueFamilyProperties2,
    ),
    pub get_physical_device_memory_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_memory_properties: *mut PhysicalDeviceMemoryProperties2,
    ),
    pub get_physical_device_sparse_image_format_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_format_info: *const PhysicalDeviceSparseImageFormatInfo2,
        p_property_count: *mut u32,
        p_properties: *mut SparseImageFormatProperties2,
    ),
}
unsafe impl Send for KhrGetPhysicalDeviceProperties2Fn {}
unsafe impl Sync for KhrGetPhysicalDeviceProperties2Fn {}
impl ::std::clone::Clone for KhrGetPhysicalDeviceProperties2Fn {
    fn clone(&self) -> Self {
        KhrGetPhysicalDeviceProperties2Fn {
            get_physical_device_features2_khr: self.get_physical_device_features2_khr,
            get_physical_device_properties2_khr: self.get_physical_device_properties2_khr,
            get_physical_device_format_properties2_khr: self
                .get_physical_device_format_properties2_khr,
            get_physical_device_image_format_properties2_khr: self
                .get_physical_device_image_format_properties2_khr,
            get_physical_device_queue_family_properties2_khr: self
                .get_physical_device_queue_family_properties2_khr,
            get_physical_device_memory_properties2_khr: self
                .get_physical_device_memory_properties2_khr,
            get_physical_device_sparse_image_format_properties2_khr: self
                .get_physical_device_sparse_image_format_properties2_khr,
        }
    }
}
impl KhrGetPhysicalDeviceProperties2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetPhysicalDeviceProperties2Fn {
            get_physical_device_features2_khr: unsafe {
                extern "system" fn get_physical_device_features2_khr(
                    _physical_device: PhysicalDevice,
                    _p_features: *mut PhysicalDeviceFeatures2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_features2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceFeatures2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_features2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_properties2_khr: unsafe {
                extern "system" fn get_physical_device_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _p_properties: *mut PhysicalDeviceProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_format_properties2_khr: unsafe {
                extern "system" fn get_physical_device_format_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _format: Format,
                    _p_format_properties: *mut FormatProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_format_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceFormatProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_format_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_image_format_properties2_khr: unsafe {
                extern "system" fn get_physical_device_image_format_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _p_image_format_info: *const PhysicalDeviceImageFormatInfo2,
                    _p_image_format_properties: *mut ImageFormatProperties2,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_image_format_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceImageFormatProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_image_format_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_queue_family_properties2_khr: unsafe {
                extern "system" fn get_physical_device_queue_family_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _p_queue_family_property_count: *mut u32,
                    _p_queue_family_properties: *mut QueueFamilyProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_queue_family_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceQueueFamilyProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_queue_family_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_memory_properties2_khr: unsafe {
                extern "system" fn get_physical_device_memory_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _p_memory_properties: *mut PhysicalDeviceMemoryProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_memory_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceMemoryProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_memory_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_sparse_image_format_properties2_khr: unsafe {
                extern "system" fn get_physical_device_sparse_image_format_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _p_format_info: *const PhysicalDeviceSparseImageFormatInfo2,
                    _p_property_count: *mut u32,
                    _p_properties: *mut SparseImageFormatProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_sparse_image_format_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSparseImageFormatProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_sparse_image_format_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFeatures2KHR.html>"]
    pub unsafe fn get_physical_device_features2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_features: *mut PhysicalDeviceFeatures2,
    ) {
        (self.get_physical_device_features2_khr)(physical_device, p_features)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceProperties2KHR.html>"]
    pub unsafe fn get_physical_device_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_properties: *mut PhysicalDeviceProperties2,
    ) {
        (self.get_physical_device_properties2_khr)(physical_device, p_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFormatProperties2KHR.html>"]
    pub unsafe fn get_physical_device_format_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        format: Format,
        p_format_properties: *mut FormatProperties2,
    ) {
        (self.get_physical_device_format_properties2_khr)(
            physical_device,
            format,
            p_format_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties2KHR.html>"]
    pub unsafe fn get_physical_device_image_format_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_image_format_info: *const PhysicalDeviceImageFormatInfo2,
        p_image_format_properties: *mut ImageFormatProperties2,
    ) -> Result {
        (self.get_physical_device_image_format_properties2_khr)(
            physical_device,
            p_image_format_info,
            p_image_format_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties2KHR.html>"]
    pub unsafe fn get_physical_device_queue_family_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_queue_family_property_count: *mut u32,
        p_queue_family_properties: *mut QueueFamilyProperties2,
    ) {
        (self.get_physical_device_queue_family_properties2_khr)(
            physical_device,
            p_queue_family_property_count,
            p_queue_family_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceMemoryProperties2KHR.html>"]
    pub unsafe fn get_physical_device_memory_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_memory_properties: *mut PhysicalDeviceMemoryProperties2,
    ) {
        (self.get_physical_device_memory_properties2_khr)(physical_device, p_memory_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSparseImageFormatProperties2KHR.html>"]
    pub unsafe fn get_physical_device_sparse_image_format_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_format_info: *const PhysicalDeviceSparseImageFormatInfo2,
        p_property_count: *mut u32,
        p_properties: *mut SparseImageFormatProperties2,
    ) {
        (self.get_physical_device_sparse_image_format_properties2_khr)(
            physical_device,
            p_format_info,
            p_property_count,
            p_properties,
        )
    }
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FEATURES_2_KHR: Self = StructureType::PHYSICAL_DEVICE_FEATURES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PROPERTIES_2_KHR: Self = StructureType::PHYSICAL_DEVICE_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const FORMAT_PROPERTIES_2_KHR: Self = StructureType::FORMAT_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const IMAGE_FORMAT_PROPERTIES_2_KHR: Self = StructureType::IMAGE_FORMAT_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR: Self =
        StructureType::PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const QUEUE_FAMILY_PROPERTIES_2_KHR: Self = StructureType::QUEUE_FAMILY_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR: Self =
        StructureType::PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const SPARSE_IMAGE_FORMAT_PROPERTIES_2_KHR: Self =
        StructureType::SPARSE_IMAGE_FORMAT_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2_KHR: Self =
        StructureType::PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2;
}
impl KhrDeviceGroupFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_device_group\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupPeerMemoryFeatures = extern "system" fn(
    device: Device,
    heap_index: u32,
    local_device_index: u32,
    remote_device_index: u32,
    p_peer_memory_features: *mut PeerMemoryFeatureFlags,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDeviceMask =
    extern "system" fn(command_buffer: CommandBuffer, device_mask: u32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDispatchBase = extern "system" fn(
    command_buffer: CommandBuffer,
    base_group_x: u32,
    base_group_y: u32,
    base_group_z: u32,
    group_count_x: u32,
    group_count_y: u32,
    group_count_z: u32,
);
pub struct KhrDeviceGroupFn {
    pub get_device_group_peer_memory_features_khr: extern "system" fn(
        device: Device,
        heap_index: u32,
        local_device_index: u32,
        remote_device_index: u32,
        p_peer_memory_features: *mut PeerMemoryFeatureFlags,
    ),
    pub cmd_set_device_mask_khr:
        extern "system" fn(command_buffer: CommandBuffer, device_mask: u32),
    pub cmd_dispatch_base_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        base_group_x: u32,
        base_group_y: u32,
        base_group_z: u32,
        group_count_x: u32,
        group_count_y: u32,
        group_count_z: u32,
    ),
    pub get_device_group_present_capabilities_khr: extern "system" fn(
        device: Device,
        p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
    ) -> Result,
    pub get_device_group_surface_present_modes_khr: extern "system" fn(
        device: Device,
        surface: SurfaceKHR,
        p_modes: *mut DeviceGroupPresentModeFlagsKHR,
    ) -> Result,
    pub get_physical_device_present_rectangles_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_rect_count: *mut u32,
        p_rects: *mut Rect2D,
    ) -> Result,
    pub acquire_next_image2_khr: extern "system" fn(
        device: Device,
        p_acquire_info: *const AcquireNextImageInfoKHR,
        p_image_index: *mut u32,
    ) -> Result,
}
unsafe impl Send for KhrDeviceGroupFn {}
unsafe impl Sync for KhrDeviceGroupFn {}
impl ::std::clone::Clone for KhrDeviceGroupFn {
    fn clone(&self) -> Self {
        KhrDeviceGroupFn {
            get_device_group_peer_memory_features_khr: self
                .get_device_group_peer_memory_features_khr,
            cmd_set_device_mask_khr: self.cmd_set_device_mask_khr,
            cmd_dispatch_base_khr: self.cmd_dispatch_base_khr,
            get_device_group_present_capabilities_khr: self
                .get_device_group_present_capabilities_khr,
            get_device_group_surface_present_modes_khr: self
                .get_device_group_surface_present_modes_khr,
            get_physical_device_present_rectangles_khr: self
                .get_physical_device_present_rectangles_khr,
            acquire_next_image2_khr: self.acquire_next_image2_khr,
        }
    }
}
impl KhrDeviceGroupFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDeviceGroupFn {
            get_device_group_peer_memory_features_khr: unsafe {
                extern "system" fn get_device_group_peer_memory_features_khr(
                    _device: Device,
                    _heap_index: u32,
                    _local_device_index: u32,
                    _remote_device_index: u32,
                    _p_peer_memory_features: *mut PeerMemoryFeatureFlags,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_group_peer_memory_features_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceGroupPeerMemoryFeaturesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_group_peer_memory_features_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_device_mask_khr: unsafe {
                extern "system" fn cmd_set_device_mask_khr(
                    _command_buffer: CommandBuffer,
                    _device_mask: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_device_mask_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetDeviceMaskKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_device_mask_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_dispatch_base_khr: unsafe {
                extern "system" fn cmd_dispatch_base_khr(
                    _command_buffer: CommandBuffer,
                    _base_group_x: u32,
                    _base_group_y: u32,
                    _base_group_z: u32,
                    _group_count_x: u32,
                    _group_count_y: u32,
                    _group_count_z: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_dispatch_base_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDispatchBaseKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_dispatch_base_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_group_present_capabilities_khr: unsafe {
                extern "system" fn get_device_group_present_capabilities_khr(
                    _device: Device,
                    _p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_group_present_capabilities_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceGroupPresentCapabilitiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_group_present_capabilities_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_group_surface_present_modes_khr: unsafe {
                extern "system" fn get_device_group_surface_present_modes_khr(
                    _device: Device,
                    _surface: SurfaceKHR,
                    _p_modes: *mut DeviceGroupPresentModeFlagsKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_group_surface_present_modes_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceGroupSurfacePresentModesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_group_surface_present_modes_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_present_rectangles_khr: unsafe {
                extern "system" fn get_physical_device_present_rectangles_khr(
                    _physical_device: PhysicalDevice,
                    _surface: SurfaceKHR,
                    _p_rect_count: *mut u32,
                    _p_rects: *mut Rect2D,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_present_rectangles_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDevicePresentRectanglesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_present_rectangles_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            acquire_next_image2_khr: unsafe {
                extern "system" fn acquire_next_image2_khr(
                    _device: Device,
                    _p_acquire_info: *const AcquireNextImageInfoKHR,
                    _p_image_index: *mut u32,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_next_image2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireNextImage2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_next_image2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupPeerMemoryFeaturesKHR.html>"]
    pub unsafe fn get_device_group_peer_memory_features_khr(
        &self,
        device: Device,
        heap_index: u32,
        local_device_index: u32,
        remote_device_index: u32,
        p_peer_memory_features: *mut PeerMemoryFeatureFlags,
    ) {
        (self.get_device_group_peer_memory_features_khr)(
            device,
            heap_index,
            local_device_index,
            remote_device_index,
            p_peer_memory_features,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDeviceMaskKHR.html>"]
    pub unsafe fn cmd_set_device_mask_khr(&self, command_buffer: CommandBuffer, device_mask: u32) {
        (self.cmd_set_device_mask_khr)(command_buffer, device_mask)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDispatchBaseKHR.html>"]
    pub unsafe fn cmd_dispatch_base_khr(
        &self,
        command_buffer: CommandBuffer,
        base_group_x: u32,
        base_group_y: u32,
        base_group_z: u32,
        group_count_x: u32,
        group_count_y: u32,
        group_count_z: u32,
    ) {
        (self.cmd_dispatch_base_khr)(
            command_buffer,
            base_group_x,
            base_group_y,
            base_group_z,
            group_count_x,
            group_count_y,
            group_count_z,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupPresentCapabilitiesKHR.html>"]
    pub unsafe fn get_device_group_present_capabilities_khr(
        &self,
        device: Device,
        p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
    ) -> Result {
        (self.get_device_group_present_capabilities_khr)(
            device,
            p_device_group_present_capabilities,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupSurfacePresentModesKHR.html>"]
    pub unsafe fn get_device_group_surface_present_modes_khr(
        &self,
        device: Device,
        surface: SurfaceKHR,
        p_modes: *mut DeviceGroupPresentModeFlagsKHR,
    ) -> Result {
        (self.get_device_group_surface_present_modes_khr)(device, surface, p_modes)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDevicePresentRectanglesKHR.html>"]
    pub unsafe fn get_physical_device_present_rectangles_khr(
        &self,
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_rect_count: *mut u32,
        p_rects: *mut Rect2D,
    ) -> Result {
        (self.get_physical_device_present_rectangles_khr)(
            physical_device,
            surface,
            p_rect_count,
            p_rects,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireNextImage2KHR.html>"]
    pub unsafe fn acquire_next_image2_khr(
        &self,
        device: Device,
        p_acquire_info: *const AcquireNextImageInfoKHR,
        p_image_index: *mut u32,
    ) -> Result {
        (self.acquire_next_image2_khr)(device, p_acquire_info, p_image_index)
    }
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const MEMORY_ALLOCATE_FLAGS_INFO_KHR: Self = StructureType::MEMORY_ALLOCATE_FLAGS_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_RENDER_PASS_BEGIN_INFO_KHR: Self =
        StructureType::DEVICE_GROUP_RENDER_PASS_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHR: Self =
        StructureType::DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_SUBMIT_INFO_KHR: Self = StructureType::DEVICE_GROUP_SUBMIT_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_BIND_SPARSE_INFO_KHR: Self =
        StructureType::DEVICE_GROUP_BIND_SPARSE_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const COPY_SRC_KHR: Self = PeerMemoryFeatureFlags::COPY_SRC;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const COPY_DST_KHR: Self = PeerMemoryFeatureFlags::COPY_DST;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const GENERIC_SRC_KHR: Self = PeerMemoryFeatureFlags::GENERIC_SRC;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const GENERIC_DST_KHR: Self = PeerMemoryFeatureFlags::GENERIC_DST;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl MemoryAllocateFlags {
    pub const DEVICE_MASK_KHR: Self = MemoryAllocateFlags::DEVICE_MASK;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PipelineCreateFlags {
    pub const VIEW_INDEX_FROM_DEVICE_INDEX_KHR: Self =
        PipelineCreateFlags::VIEW_INDEX_FROM_DEVICE_INDEX;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl DependencyFlags {
    pub const DEVICE_GROUP_KHR: Self = DependencyFlags::DEVICE_GROUP;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO_KHR: Self =
        StructureType::BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHR: Self =
        StructureType::BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl ImageCreateFlags {
    pub const SPLIT_INSTANCE_BIND_REGIONS_KHR: Self = ImageCreateFlags::SPLIT_INSTANCE_BIND_REGIONS;
}
impl ExtValidationFlagsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_validation_flags\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtValidationFlagsFn {}
unsafe impl Send for ExtValidationFlagsFn {}
unsafe impl Sync for ExtValidationFlagsFn {}
impl ::std::clone::Clone for ExtValidationFlagsFn {
    fn clone(&self) -> Self {
        ExtValidationFlagsFn {}
    }
}
impl ExtValidationFlagsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtValidationFlagsFn {}
    }
}
#[doc = "Generated from 'VK_EXT_validation_flags'"]
impl StructureType {
    pub const VALIDATION_FLAGS_EXT: Self = Self(1_000_061_000);
}
impl NnViSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NN_vi_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateViSurfaceNN = extern "system" fn(
    instance: Instance,
    p_create_info: *const ViSurfaceCreateInfoNN,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct NnViSurfaceFn {
    pub create_vi_surface_nn: extern "system" fn(
        instance: Instance,
        p_create_info: *const ViSurfaceCreateInfoNN,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for NnViSurfaceFn {}
unsafe impl Sync for NnViSurfaceFn {}
impl ::std::clone::Clone for NnViSurfaceFn {
    fn clone(&self) -> Self {
        NnViSurfaceFn {
            create_vi_surface_nn: self.create_vi_surface_nn,
        }
    }
}
impl NnViSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NnViSurfaceFn {
            create_vi_surface_nn: unsafe {
                extern "system" fn create_vi_surface_nn(
                    _instance: Instance,
                    _p_create_info: *const ViSurfaceCreateInfoNN,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_vi_surface_nn)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateViSurfaceNN\0");
                let val = _f(cname);
                if val.is_null() {
                    create_vi_surface_nn
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateViSurfaceNN.html>"]
    pub unsafe fn create_vi_surface_nn(
        &self,
        instance: Instance,
        p_create_info: *const ViSurfaceCreateInfoNN,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_vi_surface_nn)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_NN_vi_surface'"]
impl StructureType {
    pub const VI_SURFACE_CREATE_INFO_NN: Self = Self(1_000_062_000);
}
impl KhrShaderDrawParametersFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_draw_parameters\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrShaderDrawParametersFn {}
unsafe impl Send for KhrShaderDrawParametersFn {}
unsafe impl Sync for KhrShaderDrawParametersFn {}
impl ::std::clone::Clone for KhrShaderDrawParametersFn {
    fn clone(&self) -> Self {
        KhrShaderDrawParametersFn {}
    }
}
impl KhrShaderDrawParametersFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderDrawParametersFn {}
    }
}
impl ExtShaderSubgroupBallotFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_subgroup_ballot\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtShaderSubgroupBallotFn {}
unsafe impl Send for ExtShaderSubgroupBallotFn {}
unsafe impl Sync for ExtShaderSubgroupBallotFn {}
impl ::std::clone::Clone for ExtShaderSubgroupBallotFn {
    fn clone(&self) -> Self {
        ExtShaderSubgroupBallotFn {}
    }
}
impl ExtShaderSubgroupBallotFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderSubgroupBallotFn {}
    }
}
impl ExtShaderSubgroupVoteFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_subgroup_vote\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtShaderSubgroupVoteFn {}
unsafe impl Send for ExtShaderSubgroupVoteFn {}
unsafe impl Sync for ExtShaderSubgroupVoteFn {}
impl ::std::clone::Clone for ExtShaderSubgroupVoteFn {
    fn clone(&self) -> Self {
        ExtShaderSubgroupVoteFn {}
    }
}
impl ExtShaderSubgroupVoteFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderSubgroupVoteFn {}
    }
}
impl ExtTextureCompressionAstcHdrFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_texture_compression_astc_hdr\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtTextureCompressionAstcHdrFn {}
unsafe impl Send for ExtTextureCompressionAstcHdrFn {}
unsafe impl Sync for ExtTextureCompressionAstcHdrFn {}
impl ::std::clone::Clone for ExtTextureCompressionAstcHdrFn {
    fn clone(&self) -> Self {
        ExtTextureCompressionAstcHdrFn {}
    }
}
impl ExtTextureCompressionAstcHdrFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtTextureCompressionAstcHdrFn {}
    }
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES_EXT: Self = Self(1_000_066_000);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_4X4_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_000);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_5X4_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_001);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_5X5_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_002);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_6X5_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_003);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_6X6_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_004);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_8X5_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_005);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_8X6_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_006);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_8X8_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_007);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_10X5_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_008);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_10X6_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_009);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_10X8_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_010);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_10X10_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_011);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_12X10_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_012);
}
#[doc = "Generated from 'VK_EXT_texture_compression_astc_hdr'"]
impl Format {
    pub const ASTC_12X12_SFLOAT_BLOCK_EXT: Self = Self(1_000_066_013);
}
impl ExtAstcDecodeModeFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_astc_decode_mode\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtAstcDecodeModeFn {}
unsafe impl Send for ExtAstcDecodeModeFn {}
unsafe impl Sync for ExtAstcDecodeModeFn {}
impl ::std::clone::Clone for ExtAstcDecodeModeFn {
    fn clone(&self) -> Self {
        ExtAstcDecodeModeFn {}
    }
}
impl ExtAstcDecodeModeFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtAstcDecodeModeFn {}
    }
}
#[doc = "Generated from 'VK_EXT_astc_decode_mode'"]
impl StructureType {
    pub const IMAGE_VIEW_ASTC_DECODE_MODE_EXT: Self = Self(1_000_067_000);
}
#[doc = "Generated from 'VK_EXT_astc_decode_mode'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT: Self = Self(1_000_067_001);
}
impl ImgExtension69Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_extension_69\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ImgExtension69Fn {}
unsafe impl Send for ImgExtension69Fn {}
unsafe impl Sync for ImgExtension69Fn {}
impl ::std::clone::Clone for ImgExtension69Fn {
    fn clone(&self) -> Self {
        ImgExtension69Fn {}
    }
}
impl ImgExtension69Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ImgExtension69Fn {}
    }
}
impl KhrMaintenance1Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_maintenance1\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkTrimCommandPool =
    extern "system" fn(device: Device, command_pool: CommandPool, flags: CommandPoolTrimFlags);
pub struct KhrMaintenance1Fn {
    pub trim_command_pool_khr:
        extern "system" fn(device: Device, command_pool: CommandPool, flags: CommandPoolTrimFlags),
}
unsafe impl Send for KhrMaintenance1Fn {}
unsafe impl Sync for KhrMaintenance1Fn {}
impl ::std::clone::Clone for KhrMaintenance1Fn {
    fn clone(&self) -> Self {
        KhrMaintenance1Fn {
            trim_command_pool_khr: self.trim_command_pool_khr,
        }
    }
}
impl KhrMaintenance1Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrMaintenance1Fn {
            trim_command_pool_khr: unsafe {
                extern "system" fn trim_command_pool_khr(
                    _device: Device,
                    _command_pool: CommandPool,
                    _flags: CommandPoolTrimFlags,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(trim_command_pool_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkTrimCommandPoolKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    trim_command_pool_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkTrimCommandPoolKHR.html>"]
    pub unsafe fn trim_command_pool_khr(
        &self,
        device: Device,
        command_pool: CommandPool,
        flags: CommandPoolTrimFlags,
    ) {
        (self.trim_command_pool_khr)(device, command_pool, flags)
    }
}
#[doc = "Generated from 'VK_KHR_maintenance1'"]
impl Result {
    pub const ERROR_OUT_OF_POOL_MEMORY_KHR: Self = Result::ERROR_OUT_OF_POOL_MEMORY;
}
#[doc = "Generated from 'VK_KHR_maintenance1'"]
impl FormatFeatureFlags {
    pub const TRANSFER_SRC_KHR: Self = FormatFeatureFlags::TRANSFER_SRC;
}
#[doc = "Generated from 'VK_KHR_maintenance1'"]
impl FormatFeatureFlags {
    pub const TRANSFER_DST_KHR: Self = FormatFeatureFlags::TRANSFER_DST;
}
#[doc = "Generated from 'VK_KHR_maintenance1'"]
impl ImageCreateFlags {
    pub const TYPE_2D_ARRAY_COMPATIBLE_KHR: Self = ImageCreateFlags::TYPE_2D_ARRAY_COMPATIBLE;
}
impl KhrDeviceGroupCreationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_device_group_creation\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkEnumeratePhysicalDeviceGroups = extern "system" fn(
    instance: Instance,
    p_physical_device_group_count: *mut u32,
    p_physical_device_group_properties: *mut PhysicalDeviceGroupProperties,
) -> Result;
pub struct KhrDeviceGroupCreationFn {
    pub enumerate_physical_device_groups_khr: extern "system" fn(
        instance: Instance,
        p_physical_device_group_count: *mut u32,
        p_physical_device_group_properties: *mut PhysicalDeviceGroupProperties,
    ) -> Result,
}
unsafe impl Send for KhrDeviceGroupCreationFn {}
unsafe impl Sync for KhrDeviceGroupCreationFn {}
impl ::std::clone::Clone for KhrDeviceGroupCreationFn {
    fn clone(&self) -> Self {
        KhrDeviceGroupCreationFn {
            enumerate_physical_device_groups_khr: self.enumerate_physical_device_groups_khr,
        }
    }
}
impl KhrDeviceGroupCreationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDeviceGroupCreationFn {
            enumerate_physical_device_groups_khr: unsafe {
                extern "system" fn enumerate_physical_device_groups_khr(
                    _instance: Instance,
                    _p_physical_device_group_count: *mut u32,
                    _p_physical_device_group_properties: *mut PhysicalDeviceGroupProperties,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_physical_device_groups_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumeratePhysicalDeviceGroupsKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_physical_device_groups_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumeratePhysicalDeviceGroupsKHR.html>"]
    pub unsafe fn enumerate_physical_device_groups_khr(
        &self,
        instance: Instance,
        p_physical_device_group_count: *mut u32,
        p_physical_device_group_properties: *mut PhysicalDeviceGroupProperties,
    ) -> Result {
        (self.enumerate_physical_device_groups_khr)(
            instance,
            p_physical_device_group_count,
            p_physical_device_group_properties,
        )
    }
}
#[doc = "Generated from 'VK_KHR_device_group_creation'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_GROUP_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_device_group_creation'"]
impl StructureType {
    pub const DEVICE_GROUP_DEVICE_CREATE_INFO_KHR: Self =
        StructureType::DEVICE_GROUP_DEVICE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group_creation'"]
impl MemoryHeapFlags {
    pub const MULTI_INSTANCE_KHR: Self = MemoryHeapFlags::MULTI_INSTANCE;
}
impl KhrExternalMemoryCapabilitiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_memory_capabilities\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceExternalBufferProperties = extern "system" fn(
    physical_device: PhysicalDevice,
    p_external_buffer_info: *const PhysicalDeviceExternalBufferInfo,
    p_external_buffer_properties: *mut ExternalBufferProperties,
);
pub struct KhrExternalMemoryCapabilitiesFn {
    pub get_physical_device_external_buffer_properties_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_external_buffer_info: *const PhysicalDeviceExternalBufferInfo,
        p_external_buffer_properties: *mut ExternalBufferProperties,
    ),
}
unsafe impl Send for KhrExternalMemoryCapabilitiesFn {}
unsafe impl Sync for KhrExternalMemoryCapabilitiesFn {}
impl ::std::clone::Clone for KhrExternalMemoryCapabilitiesFn {
    fn clone(&self) -> Self {
        KhrExternalMemoryCapabilitiesFn {
            get_physical_device_external_buffer_properties_khr: self
                .get_physical_device_external_buffer_properties_khr,
        }
    }
}
impl KhrExternalMemoryCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalMemoryCapabilitiesFn {
            get_physical_device_external_buffer_properties_khr: unsafe {
                extern "system" fn get_physical_device_external_buffer_properties_khr(
                    _physical_device: PhysicalDevice,
                    _p_external_buffer_info: *const PhysicalDeviceExternalBufferInfo,
                    _p_external_buffer_properties: *mut ExternalBufferProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_external_buffer_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceExternalBufferPropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_external_buffer_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalBufferPropertiesKHR.html>"]
    pub unsafe fn get_physical_device_external_buffer_properties_khr(
        &self,
        physical_device: PhysicalDevice,
        p_external_buffer_info: *const PhysicalDeviceExternalBufferInfo,
        p_external_buffer_properties: *mut ExternalBufferProperties,
    ) {
        (self.get_physical_device_external_buffer_properties_khr)(
            physical_device,
            p_external_buffer_info,
            p_external_buffer_properties,
        )
    }
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR: Self =
        StructureType::PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR: Self =
        StructureType::EXTERNAL_IMAGE_FORMAT_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO_KHR: Self =
        StructureType::PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const EXTERNAL_BUFFER_PROPERTIES_KHR: Self = StructureType::EXTERNAL_BUFFER_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ID_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_ID_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const OPAQUE_FD_KHR: Self = ExternalMemoryHandleTypeFlags::OPAQUE_FD;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const OPAQUE_WIN32_KHR: Self = ExternalMemoryHandleTypeFlags::OPAQUE_WIN32;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const OPAQUE_WIN32_KMT_KHR: Self = ExternalMemoryHandleTypeFlags::OPAQUE_WIN32_KMT;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D11_TEXTURE_KHR: Self = ExternalMemoryHandleTypeFlags::D3D11_TEXTURE;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D11_TEXTURE_KMT_KHR: Self = ExternalMemoryHandleTypeFlags::D3D11_TEXTURE_KMT;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D12_HEAP_KHR: Self = ExternalMemoryHandleTypeFlags::D3D12_HEAP;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D12_RESOURCE_KHR: Self = ExternalMemoryHandleTypeFlags::D3D12_RESOURCE;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryFeatureFlags {
    pub const DEDICATED_ONLY_KHR: Self = ExternalMemoryFeatureFlags::DEDICATED_ONLY;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryFeatureFlags {
    pub const EXPORTABLE_KHR: Self = ExternalMemoryFeatureFlags::EXPORTABLE;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryFeatureFlags {
    pub const IMPORTABLE_KHR: Self = ExternalMemoryFeatureFlags::IMPORTABLE;
}
impl KhrExternalMemoryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_memory\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrExternalMemoryFn {}
unsafe impl Send for KhrExternalMemoryFn {}
unsafe impl Sync for KhrExternalMemoryFn {}
impl ::std::clone::Clone for KhrExternalMemoryFn {
    fn clone(&self) -> Self {
        KhrExternalMemoryFn {}
    }
}
impl KhrExternalMemoryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalMemoryFn {}
    }
}
#[doc = "Generated from 'VK_KHR_external_memory'"]
impl StructureType {
    pub const EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR: Self =
        StructureType::EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory'"]
impl StructureType {
    pub const EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR: Self =
        StructureType::EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory'"]
impl StructureType {
    pub const EXPORT_MEMORY_ALLOCATE_INFO_KHR: Self = StructureType::EXPORT_MEMORY_ALLOCATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory'"]
impl Result {
    pub const ERROR_INVALID_EXTERNAL_HANDLE_KHR: Self = Result::ERROR_INVALID_EXTERNAL_HANDLE;
}
impl KhrExternalMemoryWin32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_memory_win32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryWin32HandleKHR = extern "system" fn(
    device: Device,
    p_get_win32_handle_info: *const MemoryGetWin32HandleInfoKHR,
    p_handle: *mut HANDLE,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryWin32HandlePropertiesKHR = extern "system" fn(
    device: Device,
    handle_type: ExternalMemoryHandleTypeFlags,
    handle: HANDLE,
    p_memory_win32_handle_properties: *mut MemoryWin32HandlePropertiesKHR,
) -> Result;
pub struct KhrExternalMemoryWin32Fn {
    pub get_memory_win32_handle_khr: extern "system" fn(
        device: Device,
        p_get_win32_handle_info: *const MemoryGetWin32HandleInfoKHR,
        p_handle: *mut HANDLE,
    ) -> Result,
    pub get_memory_win32_handle_properties_khr: extern "system" fn(
        device: Device,
        handle_type: ExternalMemoryHandleTypeFlags,
        handle: HANDLE,
        p_memory_win32_handle_properties: *mut MemoryWin32HandlePropertiesKHR,
    ) -> Result,
}
unsafe impl Send for KhrExternalMemoryWin32Fn {}
unsafe impl Sync for KhrExternalMemoryWin32Fn {}
impl ::std::clone::Clone for KhrExternalMemoryWin32Fn {
    fn clone(&self) -> Self {
        KhrExternalMemoryWin32Fn {
            get_memory_win32_handle_khr: self.get_memory_win32_handle_khr,
            get_memory_win32_handle_properties_khr: self.get_memory_win32_handle_properties_khr,
        }
    }
}
impl KhrExternalMemoryWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalMemoryWin32Fn {
            get_memory_win32_handle_khr: unsafe {
                extern "system" fn get_memory_win32_handle_khr(
                    _device: Device,
                    _p_get_win32_handle_info: *const MemoryGetWin32HandleInfoKHR,
                    _p_handle: *mut HANDLE,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_win32_handle_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetMemoryWin32HandleKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    get_memory_win32_handle_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_memory_win32_handle_properties_khr: unsafe {
                extern "system" fn get_memory_win32_handle_properties_khr(
                    _device: Device,
                    _handle_type: ExternalMemoryHandleTypeFlags,
                    _handle: HANDLE,
                    _p_memory_win32_handle_properties: *mut MemoryWin32HandlePropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_win32_handle_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetMemoryWin32HandlePropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_memory_win32_handle_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryWin32HandleKHR.html>"]
    pub unsafe fn get_memory_win32_handle_khr(
        &self,
        device: Device,
        p_get_win32_handle_info: *const MemoryGetWin32HandleInfoKHR,
        p_handle: *mut HANDLE,
    ) -> Result {
        (self.get_memory_win32_handle_khr)(device, p_get_win32_handle_info, p_handle)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryWin32HandlePropertiesKHR.html>"]
    pub unsafe fn get_memory_win32_handle_properties_khr(
        &self,
        device: Device,
        handle_type: ExternalMemoryHandleTypeFlags,
        handle: HANDLE,
        p_memory_win32_handle_properties: *mut MemoryWin32HandlePropertiesKHR,
    ) -> Result {
        (self.get_memory_win32_handle_properties_khr)(
            device,
            handle_type,
            handle,
            p_memory_win32_handle_properties,
        )
    }
}
#[doc = "Generated from 'VK_KHR_external_memory_win32'"]
impl StructureType {
    pub const IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_073_000);
}
#[doc = "Generated from 'VK_KHR_external_memory_win32'"]
impl StructureType {
    pub const EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_073_001);
}
#[doc = "Generated from 'VK_KHR_external_memory_win32'"]
impl StructureType {
    pub const MEMORY_WIN32_HANDLE_PROPERTIES_KHR: Self = Self(1_000_073_002);
}
#[doc = "Generated from 'VK_KHR_external_memory_win32'"]
impl StructureType {
    pub const MEMORY_GET_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_073_003);
}
impl KhrExternalMemoryFdFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_memory_fd\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryFdKHR = extern "system" fn(
    device: Device,
    p_get_fd_info: *const MemoryGetFdInfoKHR,
    p_fd: *mut c_int,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryFdPropertiesKHR = extern "system" fn(
    device: Device,
    handle_type: ExternalMemoryHandleTypeFlags,
    fd: c_int,
    p_memory_fd_properties: *mut MemoryFdPropertiesKHR,
) -> Result;
pub struct KhrExternalMemoryFdFn {
    pub get_memory_fd_khr: extern "system" fn(
        device: Device,
        p_get_fd_info: *const MemoryGetFdInfoKHR,
        p_fd: *mut c_int,
    ) -> Result,
    pub get_memory_fd_properties_khr: extern "system" fn(
        device: Device,
        handle_type: ExternalMemoryHandleTypeFlags,
        fd: c_int,
        p_memory_fd_properties: *mut MemoryFdPropertiesKHR,
    ) -> Result,
}
unsafe impl Send for KhrExternalMemoryFdFn {}
unsafe impl Sync for KhrExternalMemoryFdFn {}
impl ::std::clone::Clone for KhrExternalMemoryFdFn {
    fn clone(&self) -> Self {
        KhrExternalMemoryFdFn {
            get_memory_fd_khr: self.get_memory_fd_khr,
            get_memory_fd_properties_khr: self.get_memory_fd_properties_khr,
        }
    }
}
impl KhrExternalMemoryFdFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalMemoryFdFn {
            get_memory_fd_khr: unsafe {
                extern "system" fn get_memory_fd_khr(
                    _device: Device,
                    _p_get_fd_info: *const MemoryGetFdInfoKHR,
                    _p_fd: *mut c_int,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_memory_fd_khr)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetMemoryFdKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    get_memory_fd_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_memory_fd_properties_khr: unsafe {
                extern "system" fn get_memory_fd_properties_khr(
                    _device: Device,
                    _handle_type: ExternalMemoryHandleTypeFlags,
                    _fd: c_int,
                    _p_memory_fd_properties: *mut MemoryFdPropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_fd_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetMemoryFdPropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_memory_fd_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryFdKHR.html>"]
    pub unsafe fn get_memory_fd_khr(
        &self,
        device: Device,
        p_get_fd_info: *const MemoryGetFdInfoKHR,
        p_fd: *mut c_int,
    ) -> Result {
        (self.get_memory_fd_khr)(device, p_get_fd_info, p_fd)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryFdPropertiesKHR.html>"]
    pub unsafe fn get_memory_fd_properties_khr(
        &self,
        device: Device,
        handle_type: ExternalMemoryHandleTypeFlags,
        fd: c_int,
        p_memory_fd_properties: *mut MemoryFdPropertiesKHR,
    ) -> Result {
        (self.get_memory_fd_properties_khr)(device, handle_type, fd, p_memory_fd_properties)
    }
}
#[doc = "Generated from 'VK_KHR_external_memory_fd'"]
impl StructureType {
    pub const IMPORT_MEMORY_FD_INFO_KHR: Self = Self(1_000_074_000);
}
#[doc = "Generated from 'VK_KHR_external_memory_fd'"]
impl StructureType {
    pub const MEMORY_FD_PROPERTIES_KHR: Self = Self(1_000_074_001);
}
#[doc = "Generated from 'VK_KHR_external_memory_fd'"]
impl StructureType {
    pub const MEMORY_GET_FD_INFO_KHR: Self = Self(1_000_074_002);
}
impl KhrWin32KeyedMutexFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_win32_keyed_mutex\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrWin32KeyedMutexFn {}
unsafe impl Send for KhrWin32KeyedMutexFn {}
unsafe impl Sync for KhrWin32KeyedMutexFn {}
impl ::std::clone::Clone for KhrWin32KeyedMutexFn {
    fn clone(&self) -> Self {
        KhrWin32KeyedMutexFn {}
    }
}
impl KhrWin32KeyedMutexFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrWin32KeyedMutexFn {}
    }
}
#[doc = "Generated from 'VK_KHR_win32_keyed_mutex'"]
impl StructureType {
    pub const WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR: Self = Self(1_000_075_000);
}
impl KhrExternalSemaphoreCapabilitiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_semaphore_capabilities\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceExternalSemaphoreProperties = extern "system" fn(
    physical_device: PhysicalDevice,
    p_external_semaphore_info: *const PhysicalDeviceExternalSemaphoreInfo,
    p_external_semaphore_properties: *mut ExternalSemaphoreProperties,
);
pub struct KhrExternalSemaphoreCapabilitiesFn {
    pub get_physical_device_external_semaphore_properties_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_external_semaphore_info: *const PhysicalDeviceExternalSemaphoreInfo,
        p_external_semaphore_properties: *mut ExternalSemaphoreProperties,
    ),
}
unsafe impl Send for KhrExternalSemaphoreCapabilitiesFn {}
unsafe impl Sync for KhrExternalSemaphoreCapabilitiesFn {}
impl ::std::clone::Clone for KhrExternalSemaphoreCapabilitiesFn {
    fn clone(&self) -> Self {
        KhrExternalSemaphoreCapabilitiesFn {
            get_physical_device_external_semaphore_properties_khr: self
                .get_physical_device_external_semaphore_properties_khr,
        }
    }
}
impl KhrExternalSemaphoreCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalSemaphoreCapabilitiesFn {
            get_physical_device_external_semaphore_properties_khr: unsafe {
                extern "system" fn get_physical_device_external_semaphore_properties_khr(
                    _physical_device: PhysicalDevice,
                    _p_external_semaphore_info: *const PhysicalDeviceExternalSemaphoreInfo,
                    _p_external_semaphore_properties: *mut ExternalSemaphoreProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_external_semaphore_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceExternalSemaphorePropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_external_semaphore_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalSemaphorePropertiesKHR.html>"]
    pub unsafe fn get_physical_device_external_semaphore_properties_khr(
        &self,
        physical_device: PhysicalDevice,
        p_external_semaphore_info: *const PhysicalDeviceExternalSemaphoreInfo,
        p_external_semaphore_properties: *mut ExternalSemaphoreProperties,
    ) {
        (self.get_physical_device_external_semaphore_properties_khr)(
            physical_device,
            p_external_semaphore_info,
            p_external_semaphore_properties,
        )
    }
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO_KHR: Self =
        StructureType::PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl StructureType {
    pub const EXTERNAL_SEMAPHORE_PROPERTIES_KHR: Self =
        StructureType::EXTERNAL_SEMAPHORE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const OPAQUE_FD_KHR: Self = ExternalSemaphoreHandleTypeFlags::OPAQUE_FD;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const OPAQUE_WIN32_KHR: Self = ExternalSemaphoreHandleTypeFlags::OPAQUE_WIN32;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const OPAQUE_WIN32_KMT_KHR: Self = ExternalSemaphoreHandleTypeFlags::OPAQUE_WIN32_KMT;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const D3D12_FENCE_KHR: Self = ExternalSemaphoreHandleTypeFlags::D3D12_FENCE;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const SYNC_FD_KHR: Self = ExternalSemaphoreHandleTypeFlags::SYNC_FD;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreFeatureFlags {
    pub const EXPORTABLE_KHR: Self = ExternalSemaphoreFeatureFlags::EXPORTABLE;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreFeatureFlags {
    pub const IMPORTABLE_KHR: Self = ExternalSemaphoreFeatureFlags::IMPORTABLE;
}
impl KhrExternalSemaphoreFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_semaphore\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrExternalSemaphoreFn {}
unsafe impl Send for KhrExternalSemaphoreFn {}
unsafe impl Sync for KhrExternalSemaphoreFn {}
impl ::std::clone::Clone for KhrExternalSemaphoreFn {
    fn clone(&self) -> Self {
        KhrExternalSemaphoreFn {}
    }
}
impl KhrExternalSemaphoreFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalSemaphoreFn {}
    }
}
#[doc = "Generated from 'VK_KHR_external_semaphore'"]
impl StructureType {
    pub const EXPORT_SEMAPHORE_CREATE_INFO_KHR: Self = StructureType::EXPORT_SEMAPHORE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_semaphore'"]
impl SemaphoreImportFlags {
    pub const TEMPORARY_KHR: Self = SemaphoreImportFlags::TEMPORARY;
}
impl KhrExternalSemaphoreWin32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_semaphore_win32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkImportSemaphoreWin32HandleKHR = extern "system" fn(
    device: Device,
    p_import_semaphore_win32_handle_info: *const ImportSemaphoreWin32HandleInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSemaphoreWin32HandleKHR = extern "system" fn(
    device: Device,
    p_get_win32_handle_info: *const SemaphoreGetWin32HandleInfoKHR,
    p_handle: *mut HANDLE,
) -> Result;
pub struct KhrExternalSemaphoreWin32Fn {
    pub import_semaphore_win32_handle_khr: extern "system" fn(
        device: Device,
        p_import_semaphore_win32_handle_info: *const ImportSemaphoreWin32HandleInfoKHR,
    ) -> Result,
    pub get_semaphore_win32_handle_khr: extern "system" fn(
        device: Device,
        p_get_win32_handle_info: *const SemaphoreGetWin32HandleInfoKHR,
        p_handle: *mut HANDLE,
    ) -> Result,
}
unsafe impl Send for KhrExternalSemaphoreWin32Fn {}
unsafe impl Sync for KhrExternalSemaphoreWin32Fn {}
impl ::std::clone::Clone for KhrExternalSemaphoreWin32Fn {
    fn clone(&self) -> Self {
        KhrExternalSemaphoreWin32Fn {
            import_semaphore_win32_handle_khr: self.import_semaphore_win32_handle_khr,
            get_semaphore_win32_handle_khr: self.get_semaphore_win32_handle_khr,
        }
    }
}
impl KhrExternalSemaphoreWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalSemaphoreWin32Fn {
            import_semaphore_win32_handle_khr: unsafe {
                extern "system" fn import_semaphore_win32_handle_khr(
                    _device: Device,
                    _p_import_semaphore_win32_handle_info: *const ImportSemaphoreWin32HandleInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(import_semaphore_win32_handle_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkImportSemaphoreWin32HandleKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    import_semaphore_win32_handle_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_semaphore_win32_handle_khr: unsafe {
                extern "system" fn get_semaphore_win32_handle_khr(
                    _device: Device,
                    _p_get_win32_handle_info: *const SemaphoreGetWin32HandleInfoKHR,
                    _p_handle: *mut HANDLE,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_semaphore_win32_handle_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetSemaphoreWin32HandleKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_semaphore_win32_handle_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkImportSemaphoreWin32HandleKHR.html>"]
    pub unsafe fn import_semaphore_win32_handle_khr(
        &self,
        device: Device,
        p_import_semaphore_win32_handle_info: *const ImportSemaphoreWin32HandleInfoKHR,
    ) -> Result {
        (self.import_semaphore_win32_handle_khr)(device, p_import_semaphore_win32_handle_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreWin32HandleKHR.html>"]
    pub unsafe fn get_semaphore_win32_handle_khr(
        &self,
        device: Device,
        p_get_win32_handle_info: *const SemaphoreGetWin32HandleInfoKHR,
        p_handle: *mut HANDLE,
    ) -> Result {
        (self.get_semaphore_win32_handle_khr)(device, p_get_win32_handle_info, p_handle)
    }
}
#[doc = "Generated from 'VK_KHR_external_semaphore_win32'"]
impl StructureType {
    pub const IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_078_000);
}
#[doc = "Generated from 'VK_KHR_external_semaphore_win32'"]
impl StructureType {
    pub const EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_078_001);
}
#[doc = "Generated from 'VK_KHR_external_semaphore_win32'"]
impl StructureType {
    pub const D3D12_FENCE_SUBMIT_INFO_KHR: Self = Self(1_000_078_002);
}
#[doc = "Generated from 'VK_KHR_external_semaphore_win32'"]
impl StructureType {
    pub const SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_078_003);
}
impl KhrExternalSemaphoreFdFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_semaphore_fd\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkImportSemaphoreFdKHR = extern "system" fn(
    device: Device,
    p_import_semaphore_fd_info: *const ImportSemaphoreFdInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSemaphoreFdKHR = extern "system" fn(
    device: Device,
    p_get_fd_info: *const SemaphoreGetFdInfoKHR,
    p_fd: *mut c_int,
) -> Result;
pub struct KhrExternalSemaphoreFdFn {
    pub import_semaphore_fd_khr: extern "system" fn(
        device: Device,
        p_import_semaphore_fd_info: *const ImportSemaphoreFdInfoKHR,
    ) -> Result,
    pub get_semaphore_fd_khr: extern "system" fn(
        device: Device,
        p_get_fd_info: *const SemaphoreGetFdInfoKHR,
        p_fd: *mut c_int,
    ) -> Result,
}
unsafe impl Send for KhrExternalSemaphoreFdFn {}
unsafe impl Sync for KhrExternalSemaphoreFdFn {}
impl ::std::clone::Clone for KhrExternalSemaphoreFdFn {
    fn clone(&self) -> Self {
        KhrExternalSemaphoreFdFn {
            import_semaphore_fd_khr: self.import_semaphore_fd_khr,
            get_semaphore_fd_khr: self.get_semaphore_fd_khr,
        }
    }
}
impl KhrExternalSemaphoreFdFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalSemaphoreFdFn {
            import_semaphore_fd_khr: unsafe {
                extern "system" fn import_semaphore_fd_khr(
                    _device: Device,
                    _p_import_semaphore_fd_info: *const ImportSemaphoreFdInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(import_semaphore_fd_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkImportSemaphoreFdKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    import_semaphore_fd_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_semaphore_fd_khr: unsafe {
                extern "system" fn get_semaphore_fd_khr(
                    _device: Device,
                    _p_get_fd_info: *const SemaphoreGetFdInfoKHR,
                    _p_fd: *mut c_int,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_semaphore_fd_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetSemaphoreFdKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    get_semaphore_fd_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkImportSemaphoreFdKHR.html>"]
    pub unsafe fn import_semaphore_fd_khr(
        &self,
        device: Device,
        p_import_semaphore_fd_info: *const ImportSemaphoreFdInfoKHR,
    ) -> Result {
        (self.import_semaphore_fd_khr)(device, p_import_semaphore_fd_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreFdKHR.html>"]
    pub unsafe fn get_semaphore_fd_khr(
        &self,
        device: Device,
        p_get_fd_info: *const SemaphoreGetFdInfoKHR,
        p_fd: *mut c_int,
    ) -> Result {
        (self.get_semaphore_fd_khr)(device, p_get_fd_info, p_fd)
    }
}
#[doc = "Generated from 'VK_KHR_external_semaphore_fd'"]
impl StructureType {
    pub const IMPORT_SEMAPHORE_FD_INFO_KHR: Self = Self(1_000_079_000);
}
#[doc = "Generated from 'VK_KHR_external_semaphore_fd'"]
impl StructureType {
    pub const SEMAPHORE_GET_FD_INFO_KHR: Self = Self(1_000_079_001);
}
impl KhrPushDescriptorFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_push_descriptor\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPushDescriptorSetKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_bind_point: PipelineBindPoint,
    layout: PipelineLayout,
    set: u32,
    descriptor_write_count: u32,
    p_descriptor_writes: *const WriteDescriptorSet,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPushDescriptorSetWithTemplateKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    descriptor_update_template: DescriptorUpdateTemplate,
    layout: PipelineLayout,
    set: u32,
    p_data: *const c_void,
);
pub struct KhrPushDescriptorFn {
    pub cmd_push_descriptor_set_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        pipeline_bind_point: PipelineBindPoint,
        layout: PipelineLayout,
        set: u32,
        descriptor_write_count: u32,
        p_descriptor_writes: *const WriteDescriptorSet,
    ),
    pub cmd_push_descriptor_set_with_template_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        descriptor_update_template: DescriptorUpdateTemplate,
        layout: PipelineLayout,
        set: u32,
        p_data: *const c_void,
    ),
}
unsafe impl Send for KhrPushDescriptorFn {}
unsafe impl Sync for KhrPushDescriptorFn {}
impl ::std::clone::Clone for KhrPushDescriptorFn {
    fn clone(&self) -> Self {
        KhrPushDescriptorFn {
            cmd_push_descriptor_set_khr: self.cmd_push_descriptor_set_khr,
            cmd_push_descriptor_set_with_template_khr: self
                .cmd_push_descriptor_set_with_template_khr,
        }
    }
}
impl KhrPushDescriptorFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPushDescriptorFn {
            cmd_push_descriptor_set_khr: unsafe {
                extern "system" fn cmd_push_descriptor_set_khr(
                    _command_buffer: CommandBuffer,
                    _pipeline_bind_point: PipelineBindPoint,
                    _layout: PipelineLayout,
                    _set: u32,
                    _descriptor_write_count: u32,
                    _p_descriptor_writes: *const WriteDescriptorSet,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_push_descriptor_set_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdPushDescriptorSetKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_push_descriptor_set_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_push_descriptor_set_with_template_khr: unsafe {
                extern "system" fn cmd_push_descriptor_set_with_template_khr(
                    _command_buffer: CommandBuffer,
                    _descriptor_update_template: DescriptorUpdateTemplate,
                    _layout: PipelineLayout,
                    _set: u32,
                    _p_data: *const c_void,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_push_descriptor_set_with_template_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdPushDescriptorSetWithTemplateKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_push_descriptor_set_with_template_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushDescriptorSetKHR.html>"]
    pub unsafe fn cmd_push_descriptor_set_khr(
        &self,
        command_buffer: CommandBuffer,
        pipeline_bind_point: PipelineBindPoint,
        layout: PipelineLayout,
        set: u32,
        descriptor_write_count: u32,
        p_descriptor_writes: *const WriteDescriptorSet,
    ) {
        (self.cmd_push_descriptor_set_khr)(
            command_buffer,
            pipeline_bind_point,
            layout,
            set,
            descriptor_write_count,
            p_descriptor_writes,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushDescriptorSetWithTemplateKHR.html>"]
    pub unsafe fn cmd_push_descriptor_set_with_template_khr(
        &self,
        command_buffer: CommandBuffer,
        descriptor_update_template: DescriptorUpdateTemplate,
        layout: PipelineLayout,
        set: u32,
        p_data: *const c_void,
    ) {
        (self.cmd_push_descriptor_set_with_template_khr)(
            command_buffer,
            descriptor_update_template,
            layout,
            set,
            p_data,
        )
    }
}
#[doc = "Generated from 'VK_KHR_push_descriptor'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR: Self = Self(1_000_080_000);
}
#[doc = "Generated from 'VK_KHR_push_descriptor'"]
impl DescriptorSetLayoutCreateFlags {
    pub const PUSH_DESCRIPTOR_KHR: Self = Self(0b1);
}
#[doc = "Generated from 'VK_KHR_push_descriptor'"]
impl DescriptorUpdateTemplateType {
    pub const PUSH_DESCRIPTORS_KHR: Self = Self(1);
}
impl ExtConditionalRenderingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_conditional_rendering\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginConditionalRenderingEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    p_conditional_rendering_begin: *const ConditionalRenderingBeginInfoEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndConditionalRenderingEXT = extern "system" fn(command_buffer: CommandBuffer);
pub struct ExtConditionalRenderingFn {
    pub cmd_begin_conditional_rendering_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        p_conditional_rendering_begin: *const ConditionalRenderingBeginInfoEXT,
    ),
    pub cmd_end_conditional_rendering_ext: extern "system" fn(command_buffer: CommandBuffer),
}
unsafe impl Send for ExtConditionalRenderingFn {}
unsafe impl Sync for ExtConditionalRenderingFn {}
impl ::std::clone::Clone for ExtConditionalRenderingFn {
    fn clone(&self) -> Self {
        ExtConditionalRenderingFn {
            cmd_begin_conditional_rendering_ext: self.cmd_begin_conditional_rendering_ext,
            cmd_end_conditional_rendering_ext: self.cmd_end_conditional_rendering_ext,
        }
    }
}
impl ExtConditionalRenderingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtConditionalRenderingFn {
            cmd_begin_conditional_rendering_ext: unsafe {
                extern "system" fn cmd_begin_conditional_rendering_ext(
                    _command_buffer: CommandBuffer,
                    _p_conditional_rendering_begin: *const ConditionalRenderingBeginInfoEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_conditional_rendering_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBeginConditionalRenderingEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_conditional_rendering_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_conditional_rendering_ext: unsafe {
                extern "system" fn cmd_end_conditional_rendering_ext(
                    _command_buffer: CommandBuffer,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_conditional_rendering_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdEndConditionalRenderingEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_conditional_rendering_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginConditionalRenderingEXT.html>"]
    pub unsafe fn cmd_begin_conditional_rendering_ext(
        &self,
        command_buffer: CommandBuffer,
        p_conditional_rendering_begin: *const ConditionalRenderingBeginInfoEXT,
    ) {
        (self.cmd_begin_conditional_rendering_ext)(command_buffer, p_conditional_rendering_begin)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndConditionalRenderingEXT.html>"]
    pub unsafe fn cmd_end_conditional_rendering_ext(&self, command_buffer: CommandBuffer) {
        (self.cmd_end_conditional_rendering_ext)(command_buffer)
    }
}
#[doc = "Generated from 'VK_EXT_conditional_rendering'"]
impl StructureType {
    pub const COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT: Self = Self(1_000_081_000);
}
#[doc = "Generated from 'VK_EXT_conditional_rendering'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT: Self = Self(1_000_081_001);
}
#[doc = "Generated from 'VK_EXT_conditional_rendering'"]
impl StructureType {
    pub const CONDITIONAL_RENDERING_BEGIN_INFO_EXT: Self = Self(1_000_081_002);
}
#[doc = "Generated from 'VK_EXT_conditional_rendering'"]
impl AccessFlags {
    pub const CONDITIONAL_RENDERING_READ_EXT: Self = Self(0b1_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_conditional_rendering'"]
impl BufferUsageFlags {
    pub const CONDITIONAL_RENDERING_EXT: Self = Self(0b10_0000_0000);
}
#[doc = "Generated from 'VK_EXT_conditional_rendering'"]
impl PipelineStageFlags {
    pub const CONDITIONAL_RENDERING_EXT: Self = Self(0b100_0000_0000_0000_0000);
}
impl KhrShaderFloat16Int8Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_float16_int8\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrShaderFloat16Int8Fn {}
unsafe impl Send for KhrShaderFloat16Int8Fn {}
unsafe impl Sync for KhrShaderFloat16Int8Fn {}
impl ::std::clone::Clone for KhrShaderFloat16Int8Fn {
    fn clone(&self) -> Self {
        KhrShaderFloat16Int8Fn {}
    }
}
impl KhrShaderFloat16Int8Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderFloat16Int8Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_float16_int8'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
}
#[doc = "Generated from 'VK_KHR_shader_float16_int8'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
}
impl Khr16bitStorageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_16bit_storage\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct Khr16bitStorageFn {}
unsafe impl Send for Khr16bitStorageFn {}
unsafe impl Sync for Khr16bitStorageFn {}
impl ::std::clone::Clone for Khr16bitStorageFn {
    fn clone(&self) -> Self {
        Khr16bitStorageFn {}
    }
}
impl Khr16bitStorageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Khr16bitStorageFn {}
    }
}
#[doc = "Generated from 'VK_KHR_16bit_storage'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
}
impl KhrIncrementalPresentFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_incremental_present\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrIncrementalPresentFn {}
unsafe impl Send for KhrIncrementalPresentFn {}
unsafe impl Sync for KhrIncrementalPresentFn {}
impl ::std::clone::Clone for KhrIncrementalPresentFn {
    fn clone(&self) -> Self {
        KhrIncrementalPresentFn {}
    }
}
impl KhrIncrementalPresentFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrIncrementalPresentFn {}
    }
}
#[doc = "Generated from 'VK_KHR_incremental_present'"]
impl StructureType {
    pub const PRESENT_REGIONS_KHR: Self = Self(1_000_084_000);
}
impl KhrDescriptorUpdateTemplateFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_descriptor_update_template\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDescriptorUpdateTemplate = extern "system" fn(
    device: Device,
    p_create_info: *const DescriptorUpdateTemplateCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_descriptor_update_template: *mut DescriptorUpdateTemplate,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDescriptorUpdateTemplate = extern "system" fn(
    device: Device,
    descriptor_update_template: DescriptorUpdateTemplate,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkUpdateDescriptorSetWithTemplate = extern "system" fn(
    device: Device,
    descriptor_set: DescriptorSet,
    descriptor_update_template: DescriptorUpdateTemplate,
    p_data: *const c_void,
);
pub struct KhrDescriptorUpdateTemplateFn {
    pub create_descriptor_update_template_khr: extern "system" fn(
        device: Device,
        p_create_info: *const DescriptorUpdateTemplateCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_descriptor_update_template: *mut DescriptorUpdateTemplate,
    ) -> Result,
    pub destroy_descriptor_update_template_khr: extern "system" fn(
        device: Device,
        descriptor_update_template: DescriptorUpdateTemplate,
        p_allocator: *const AllocationCallbacks,
    ),
    pub update_descriptor_set_with_template_khr: extern "system" fn(
        device: Device,
        descriptor_set: DescriptorSet,
        descriptor_update_template: DescriptorUpdateTemplate,
        p_data: *const c_void,
    ),
    pub cmd_push_descriptor_set_with_template_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        descriptor_update_template: DescriptorUpdateTemplate,
        layout: PipelineLayout,
        set: u32,
        p_data: *const c_void,
    ),
}
unsafe impl Send for KhrDescriptorUpdateTemplateFn {}
unsafe impl Sync for KhrDescriptorUpdateTemplateFn {}
impl ::std::clone::Clone for KhrDescriptorUpdateTemplateFn {
    fn clone(&self) -> Self {
        KhrDescriptorUpdateTemplateFn {
            create_descriptor_update_template_khr: self.create_descriptor_update_template_khr,
            destroy_descriptor_update_template_khr: self.destroy_descriptor_update_template_khr,
            update_descriptor_set_with_template_khr: self.update_descriptor_set_with_template_khr,
            cmd_push_descriptor_set_with_template_khr: self
                .cmd_push_descriptor_set_with_template_khr,
        }
    }
}
impl KhrDescriptorUpdateTemplateFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDescriptorUpdateTemplateFn {
            create_descriptor_update_template_khr: unsafe {
                extern "system" fn create_descriptor_update_template_khr(
                    _device: Device,
                    _p_create_info: *const DescriptorUpdateTemplateCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_descriptor_update_template: *mut DescriptorUpdateTemplate,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_descriptor_update_template_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDescriptorUpdateTemplateKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_descriptor_update_template_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_descriptor_update_template_khr: unsafe {
                extern "system" fn destroy_descriptor_update_template_khr(
                    _device: Device,
                    _descriptor_update_template: DescriptorUpdateTemplate,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_descriptor_update_template_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyDescriptorUpdateTemplateKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_descriptor_update_template_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            update_descriptor_set_with_template_khr: unsafe {
                extern "system" fn update_descriptor_set_with_template_khr(
                    _device: Device,
                    _descriptor_set: DescriptorSet,
                    _descriptor_update_template: DescriptorUpdateTemplate,
                    _p_data: *const c_void,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(update_descriptor_set_with_template_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkUpdateDescriptorSetWithTemplateKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    update_descriptor_set_with_template_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_push_descriptor_set_with_template_khr: unsafe {
                extern "system" fn cmd_push_descriptor_set_with_template_khr(
                    _command_buffer: CommandBuffer,
                    _descriptor_update_template: DescriptorUpdateTemplate,
                    _layout: PipelineLayout,
                    _set: u32,
                    _p_data: *const c_void,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_push_descriptor_set_with_template_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdPushDescriptorSetWithTemplateKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_push_descriptor_set_with_template_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDescriptorUpdateTemplateKHR.html>"]
    pub unsafe fn create_descriptor_update_template_khr(
        &self,
        device: Device,
        p_create_info: *const DescriptorUpdateTemplateCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_descriptor_update_template: *mut DescriptorUpdateTemplate,
    ) -> Result {
        (self.create_descriptor_update_template_khr)(
            device,
            p_create_info,
            p_allocator,
            p_descriptor_update_template,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDescriptorUpdateTemplateKHR.html>"]
    pub unsafe fn destroy_descriptor_update_template_khr(
        &self,
        device: Device,
        descriptor_update_template: DescriptorUpdateTemplate,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_descriptor_update_template_khr)(
            device,
            descriptor_update_template,
            p_allocator,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUpdateDescriptorSetWithTemplateKHR.html>"]
    pub unsafe fn update_descriptor_set_with_template_khr(
        &self,
        device: Device,
        descriptor_set: DescriptorSet,
        descriptor_update_template: DescriptorUpdateTemplate,
        p_data: *const c_void,
    ) {
        (self.update_descriptor_set_with_template_khr)(
            device,
            descriptor_set,
            descriptor_update_template,
            p_data,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushDescriptorSetWithTemplateKHR.html>"]
    pub unsafe fn cmd_push_descriptor_set_with_template_khr(
        &self,
        command_buffer: CommandBuffer,
        descriptor_update_template: DescriptorUpdateTemplate,
        layout: PipelineLayout,
        set: u32,
        p_data: *const c_void,
    ) {
        (self.cmd_push_descriptor_set_with_template_khr)(
            command_buffer,
            descriptor_update_template,
            layout,
            set,
            p_data,
        )
    }
}
#[doc = "Generated from 'VK_KHR_descriptor_update_template'"]
impl StructureType {
    pub const DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR: Self =
        StructureType::DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_descriptor_update_template'"]
impl ObjectType {
    pub const DESCRIPTOR_UPDATE_TEMPLATE_KHR: Self = ObjectType::DESCRIPTOR_UPDATE_TEMPLATE;
}
#[doc = "Generated from 'VK_KHR_descriptor_update_template'"]
impl DescriptorUpdateTemplateType {
    pub const DESCRIPTOR_SET_KHR: Self = DescriptorUpdateTemplateType::DESCRIPTOR_SET;
}
#[doc = "Generated from 'VK_KHR_descriptor_update_template'"]
impl DebugReportObjectTypeEXT {
    pub const DESCRIPTOR_UPDATE_TEMPLATE_KHR: Self =
        DebugReportObjectTypeEXT::DESCRIPTOR_UPDATE_TEMPLATE;
}
impl NvxDeviceGeneratedCommandsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NVX_device_generated_commands\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
pub struct NvxDeviceGeneratedCommandsFn {}
unsafe impl Send for NvxDeviceGeneratedCommandsFn {}
unsafe impl Sync for NvxDeviceGeneratedCommandsFn {}
impl ::std::clone::Clone for NvxDeviceGeneratedCommandsFn {
    fn clone(&self) -> Self {
        NvxDeviceGeneratedCommandsFn {}
    }
}
impl NvxDeviceGeneratedCommandsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvxDeviceGeneratedCommandsFn {}
    }
}
impl NvClipSpaceWScalingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_clip_space_w_scaling\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetViewportWScalingNV = extern "system" fn(
    command_buffer: CommandBuffer,
    first_viewport: u32,
    viewport_count: u32,
    p_viewport_w_scalings: *const ViewportWScalingNV,
);
pub struct NvClipSpaceWScalingFn {
    pub cmd_set_viewport_w_scaling_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        first_viewport: u32,
        viewport_count: u32,
        p_viewport_w_scalings: *const ViewportWScalingNV,
    ),
}
unsafe impl Send for NvClipSpaceWScalingFn {}
unsafe impl Sync for NvClipSpaceWScalingFn {}
impl ::std::clone::Clone for NvClipSpaceWScalingFn {
    fn clone(&self) -> Self {
        NvClipSpaceWScalingFn {
            cmd_set_viewport_w_scaling_nv: self.cmd_set_viewport_w_scaling_nv,
        }
    }
}
impl NvClipSpaceWScalingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvClipSpaceWScalingFn {
            cmd_set_viewport_w_scaling_nv: unsafe {
                extern "system" fn cmd_set_viewport_w_scaling_nv(
                    _command_buffer: CommandBuffer,
                    _first_viewport: u32,
                    _viewport_count: u32,
                    _p_viewport_w_scalings: *const ViewportWScalingNV,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_viewport_w_scaling_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetViewportWScalingNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_viewport_w_scaling_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetViewportWScalingNV.html>"]
    pub unsafe fn cmd_set_viewport_w_scaling_nv(
        &self,
        command_buffer: CommandBuffer,
        first_viewport: u32,
        viewport_count: u32,
        p_viewport_w_scalings: *const ViewportWScalingNV,
    ) {
        (self.cmd_set_viewport_w_scaling_nv)(
            command_buffer,
            first_viewport,
            viewport_count,
            p_viewport_w_scalings,
        )
    }
}
#[doc = "Generated from 'VK_NV_clip_space_w_scaling'"]
impl StructureType {
    pub const PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV: Self = Self(1_000_087_000);
}
#[doc = "Generated from 'VK_NV_clip_space_w_scaling'"]
impl DynamicState {
    pub const VIEWPORT_W_SCALING_NV: Self = Self(1_000_087_000);
}
impl ExtDirectModeDisplayFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_direct_mode_display\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkReleaseDisplayEXT =
    extern "system" fn(physical_device: PhysicalDevice, display: DisplayKHR) -> Result;
pub struct ExtDirectModeDisplayFn {
    pub release_display_ext:
        extern "system" fn(physical_device: PhysicalDevice, display: DisplayKHR) -> Result,
}
unsafe impl Send for ExtDirectModeDisplayFn {}
unsafe impl Sync for ExtDirectModeDisplayFn {}
impl ::std::clone::Clone for ExtDirectModeDisplayFn {
    fn clone(&self) -> Self {
        ExtDirectModeDisplayFn {
            release_display_ext: self.release_display_ext,
        }
    }
}
impl ExtDirectModeDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDirectModeDisplayFn {
            release_display_ext: unsafe {
                extern "system" fn release_display_ext(
                    _physical_device: PhysicalDevice,
                    _display: DisplayKHR,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(release_display_ext)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkReleaseDisplayEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    release_display_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkReleaseDisplayEXT.html>"]
    pub unsafe fn release_display_ext(
        &self,
        physical_device: PhysicalDevice,
        display: DisplayKHR,
    ) -> Result {
        (self.release_display_ext)(physical_device, display)
    }
}
impl ExtAcquireXlibDisplayFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_acquire_xlib_display\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireXlibDisplayEXT = extern "system" fn(
    physical_device: PhysicalDevice,
    dpy: *mut Display,
    display: DisplayKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetRandROutputDisplayEXT = extern "system" fn(
    physical_device: PhysicalDevice,
    dpy: *mut Display,
    rr_output: RROutput,
    p_display: *mut DisplayKHR,
) -> Result;
pub struct ExtAcquireXlibDisplayFn {
    pub acquire_xlib_display_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        dpy: *mut Display,
        display: DisplayKHR,
    ) -> Result,
    pub get_rand_r_output_display_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        dpy: *mut Display,
        rr_output: RROutput,
        p_display: *mut DisplayKHR,
    ) -> Result,
}
unsafe impl Send for ExtAcquireXlibDisplayFn {}
unsafe impl Sync for ExtAcquireXlibDisplayFn {}
impl ::std::clone::Clone for ExtAcquireXlibDisplayFn {
    fn clone(&self) -> Self {
        ExtAcquireXlibDisplayFn {
            acquire_xlib_display_ext: self.acquire_xlib_display_ext,
            get_rand_r_output_display_ext: self.get_rand_r_output_display_ext,
        }
    }
}
impl ExtAcquireXlibDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtAcquireXlibDisplayFn {
            acquire_xlib_display_ext: unsafe {
                extern "system" fn acquire_xlib_display_ext(
                    _physical_device: PhysicalDevice,
                    _dpy: *mut Display,
                    _display: DisplayKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_xlib_display_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireXlibDisplayEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_xlib_display_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_rand_r_output_display_ext: unsafe {
                extern "system" fn get_rand_r_output_display_ext(
                    _physical_device: PhysicalDevice,
                    _dpy: *mut Display,
                    _rr_output: RROutput,
                    _p_display: *mut DisplayKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_rand_r_output_display_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetRandROutputDisplayEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_rand_r_output_display_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireXlibDisplayEXT.html>"]
    pub unsafe fn acquire_xlib_display_ext(
        &self,
        physical_device: PhysicalDevice,
        dpy: *mut Display,
        display: DisplayKHR,
    ) -> Result {
        (self.acquire_xlib_display_ext)(physical_device, dpy, display)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRandROutputDisplayEXT.html>"]
    pub unsafe fn get_rand_r_output_display_ext(
        &self,
        physical_device: PhysicalDevice,
        dpy: *mut Display,
        rr_output: RROutput,
        p_display: *mut DisplayKHR,
    ) -> Result {
        (self.get_rand_r_output_display_ext)(physical_device, dpy, rr_output, p_display)
    }
}
impl ExtDisplaySurfaceCounterFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_display_surface_counter\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT = extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_surface_capabilities: *mut SurfaceCapabilities2EXT,
) -> Result;
pub struct ExtDisplaySurfaceCounterFn {
    pub get_physical_device_surface_capabilities2_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_surface_capabilities: *mut SurfaceCapabilities2EXT,
    ) -> Result,
}
unsafe impl Send for ExtDisplaySurfaceCounterFn {}
unsafe impl Sync for ExtDisplaySurfaceCounterFn {}
impl ::std::clone::Clone for ExtDisplaySurfaceCounterFn {
    fn clone(&self) -> Self {
        ExtDisplaySurfaceCounterFn {
            get_physical_device_surface_capabilities2_ext: self
                .get_physical_device_surface_capabilities2_ext,
        }
    }
}
impl ExtDisplaySurfaceCounterFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDisplaySurfaceCounterFn {
            get_physical_device_surface_capabilities2_ext: unsafe {
                extern "system" fn get_physical_device_surface_capabilities2_ext(
                    _physical_device: PhysicalDevice,
                    _surface: SurfaceKHR,
                    _p_surface_capabilities: *mut SurfaceCapabilities2EXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_capabilities2_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfaceCapabilities2EXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_capabilities2_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceCapabilities2EXT.html>"]
    pub unsafe fn get_physical_device_surface_capabilities2_ext(
        &self,
        physical_device: PhysicalDevice,
        surface: SurfaceKHR,
        p_surface_capabilities: *mut SurfaceCapabilities2EXT,
    ) -> Result {
        (self.get_physical_device_surface_capabilities2_ext)(
            physical_device,
            surface,
            p_surface_capabilities,
        )
    }
}
#[doc = "Generated from 'VK_EXT_display_surface_counter'"]
impl StructureType {
    pub const SURFACE_CAPABILITIES_2_EXT: Self = Self(1_000_090_000);
}
#[doc = "Generated from 'VK_EXT_display_surface_counter'"]
impl StructureType {
    pub const SURFACE_CAPABILITIES2_EXT: Self = StructureType::SURFACE_CAPABILITIES_2_EXT;
}
impl ExtDisplayControlFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_display_control\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkDisplayPowerControlEXT = extern "system" fn(
    device: Device,
    display: DisplayKHR,
    p_display_power_info: *const DisplayPowerInfoEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkRegisterDeviceEventEXT = extern "system" fn(
    device: Device,
    p_device_event_info: *const DeviceEventInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_fence: *mut Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkRegisterDisplayEventEXT = extern "system" fn(
    device: Device,
    display: DisplayKHR,
    p_display_event_info: *const DisplayEventInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_fence: *mut Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainCounterEXT = extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    counter: SurfaceCounterFlagsEXT,
    p_counter_value: *mut u64,
) -> Result;
pub struct ExtDisplayControlFn {
    pub display_power_control_ext: extern "system" fn(
        device: Device,
        display: DisplayKHR,
        p_display_power_info: *const DisplayPowerInfoEXT,
    ) -> Result,
    pub register_device_event_ext: extern "system" fn(
        device: Device,
        p_device_event_info: *const DeviceEventInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_fence: *mut Fence,
    ) -> Result,
    pub register_display_event_ext: extern "system" fn(
        device: Device,
        display: DisplayKHR,
        p_display_event_info: *const DisplayEventInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_fence: *mut Fence,
    ) -> Result,
    pub get_swapchain_counter_ext: extern "system" fn(
        device: Device,
        swapchain: SwapchainKHR,
        counter: SurfaceCounterFlagsEXT,
        p_counter_value: *mut u64,
    ) -> Result,
}
unsafe impl Send for ExtDisplayControlFn {}
unsafe impl Sync for ExtDisplayControlFn {}
impl ::std::clone::Clone for ExtDisplayControlFn {
    fn clone(&self) -> Self {
        ExtDisplayControlFn {
            display_power_control_ext: self.display_power_control_ext,
            register_device_event_ext: self.register_device_event_ext,
            register_display_event_ext: self.register_display_event_ext,
            get_swapchain_counter_ext: self.get_swapchain_counter_ext,
        }
    }
}
impl ExtDisplayControlFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDisplayControlFn {
            display_power_control_ext: unsafe {
                extern "system" fn display_power_control_ext(
                    _device: Device,
                    _display: DisplayKHR,
                    _p_display_power_info: *const DisplayPowerInfoEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(display_power_control_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDisplayPowerControlEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    display_power_control_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            register_device_event_ext: unsafe {
                extern "system" fn register_device_event_ext(
                    _device: Device,
                    _p_device_event_info: *const DeviceEventInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_fence: *mut Fence,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(register_device_event_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkRegisterDeviceEventEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    register_device_event_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            register_display_event_ext: unsafe {
                extern "system" fn register_display_event_ext(
                    _device: Device,
                    _display: DisplayKHR,
                    _p_display_event_info: *const DisplayEventInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_fence: *mut Fence,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(register_display_event_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkRegisterDisplayEventEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    register_display_event_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_swapchain_counter_ext: unsafe {
                extern "system" fn get_swapchain_counter_ext(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                    _counter: SurfaceCounterFlagsEXT,
                    _p_counter_value: *mut u64,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_swapchain_counter_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetSwapchainCounterEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    get_swapchain_counter_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDisplayPowerControlEXT.html>"]
    pub unsafe fn display_power_control_ext(
        &self,
        device: Device,
        display: DisplayKHR,
        p_display_power_info: *const DisplayPowerInfoEXT,
    ) -> Result {
        (self.display_power_control_ext)(device, display, p_display_power_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkRegisterDeviceEventEXT.html>"]
    pub unsafe fn register_device_event_ext(
        &self,
        device: Device,
        p_device_event_info: *const DeviceEventInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_fence: *mut Fence,
    ) -> Result {
        (self.register_device_event_ext)(device, p_device_event_info, p_allocator, p_fence)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkRegisterDisplayEventEXT.html>"]
    pub unsafe fn register_display_event_ext(
        &self,
        device: Device,
        display: DisplayKHR,
        p_display_event_info: *const DisplayEventInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_fence: *mut Fence,
    ) -> Result {
        (self.register_display_event_ext)(
            device,
            display,
            p_display_event_info,
            p_allocator,
            p_fence,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSwapchainCounterEXT.html>"]
    pub unsafe fn get_swapchain_counter_ext(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
        counter: SurfaceCounterFlagsEXT,
        p_counter_value: *mut u64,
    ) -> Result {
        (self.get_swapchain_counter_ext)(device, swapchain, counter, p_counter_value)
    }
}
#[doc = "Generated from 'VK_EXT_display_control'"]
impl StructureType {
    pub const DISPLAY_POWER_INFO_EXT: Self = Self(1_000_091_000);
}
#[doc = "Generated from 'VK_EXT_display_control'"]
impl StructureType {
    pub const DEVICE_EVENT_INFO_EXT: Self = Self(1_000_091_001);
}
#[doc = "Generated from 'VK_EXT_display_control'"]
impl StructureType {
    pub const DISPLAY_EVENT_INFO_EXT: Self = Self(1_000_091_002);
}
#[doc = "Generated from 'VK_EXT_display_control'"]
impl StructureType {
    pub const SWAPCHAIN_COUNTER_CREATE_INFO_EXT: Self = Self(1_000_091_003);
}
impl GoogleDisplayTimingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_display_timing\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetRefreshCycleDurationGOOGLE = extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_display_timing_properties: *mut RefreshCycleDurationGOOGLE,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPastPresentationTimingGOOGLE = extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_presentation_timing_count: *mut u32,
    p_presentation_timings: *mut PastPresentationTimingGOOGLE,
) -> Result;
pub struct GoogleDisplayTimingFn {
    pub get_refresh_cycle_duration_google: extern "system" fn(
        device: Device,
        swapchain: SwapchainKHR,
        p_display_timing_properties: *mut RefreshCycleDurationGOOGLE,
    ) -> Result,
    pub get_past_presentation_timing_google: extern "system" fn(
        device: Device,
        swapchain: SwapchainKHR,
        p_presentation_timing_count: *mut u32,
        p_presentation_timings: *mut PastPresentationTimingGOOGLE,
    ) -> Result,
}
unsafe impl Send for GoogleDisplayTimingFn {}
unsafe impl Sync for GoogleDisplayTimingFn {}
impl ::std::clone::Clone for GoogleDisplayTimingFn {
    fn clone(&self) -> Self {
        GoogleDisplayTimingFn {
            get_refresh_cycle_duration_google: self.get_refresh_cycle_duration_google,
            get_past_presentation_timing_google: self.get_past_presentation_timing_google,
        }
    }
}
impl GoogleDisplayTimingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleDisplayTimingFn {
            get_refresh_cycle_duration_google: unsafe {
                extern "system" fn get_refresh_cycle_duration_google(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                    _p_display_timing_properties: *mut RefreshCycleDurationGOOGLE,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_refresh_cycle_duration_google)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetRefreshCycleDurationGOOGLE\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_refresh_cycle_duration_google
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_past_presentation_timing_google: unsafe {
                extern "system" fn get_past_presentation_timing_google(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                    _p_presentation_timing_count: *mut u32,
                    _p_presentation_timings: *mut PastPresentationTimingGOOGLE,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_past_presentation_timing_google)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPastPresentationTimingGOOGLE\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_past_presentation_timing_google
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRefreshCycleDurationGOOGLE.html>"]
    pub unsafe fn get_refresh_cycle_duration_google(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
        p_display_timing_properties: *mut RefreshCycleDurationGOOGLE,
    ) -> Result {
        (self.get_refresh_cycle_duration_google)(device, swapchain, p_display_timing_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPastPresentationTimingGOOGLE.html>"]
    pub unsafe fn get_past_presentation_timing_google(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
        p_presentation_timing_count: *mut u32,
        p_presentation_timings: *mut PastPresentationTimingGOOGLE,
    ) -> Result {
        (self.get_past_presentation_timing_google)(
            device,
            swapchain,
            p_presentation_timing_count,
            p_presentation_timings,
        )
    }
}
#[doc = "Generated from 'VK_GOOGLE_display_timing'"]
impl StructureType {
    pub const PRESENT_TIMES_INFO_GOOGLE: Self = Self(1_000_092_000);
}
impl NvSampleMaskOverrideCoverageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_sample_mask_override_coverage\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvSampleMaskOverrideCoverageFn {}
unsafe impl Send for NvSampleMaskOverrideCoverageFn {}
unsafe impl Sync for NvSampleMaskOverrideCoverageFn {}
impl ::std::clone::Clone for NvSampleMaskOverrideCoverageFn {
    fn clone(&self) -> Self {
        NvSampleMaskOverrideCoverageFn {}
    }
}
impl NvSampleMaskOverrideCoverageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvSampleMaskOverrideCoverageFn {}
    }
}
impl NvGeometryShaderPassthroughFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_geometry_shader_passthrough\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvGeometryShaderPassthroughFn {}
unsafe impl Send for NvGeometryShaderPassthroughFn {}
unsafe impl Sync for NvGeometryShaderPassthroughFn {}
impl ::std::clone::Clone for NvGeometryShaderPassthroughFn {
    fn clone(&self) -> Self {
        NvGeometryShaderPassthroughFn {}
    }
}
impl NvGeometryShaderPassthroughFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvGeometryShaderPassthroughFn {}
    }
}
impl NvViewportArray2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_viewport_array2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvViewportArray2Fn {}
unsafe impl Send for NvViewportArray2Fn {}
unsafe impl Sync for NvViewportArray2Fn {}
impl ::std::clone::Clone for NvViewportArray2Fn {
    fn clone(&self) -> Self {
        NvViewportArray2Fn {}
    }
}
impl NvViewportArray2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvViewportArray2Fn {}
    }
}
impl NvxMultiviewPerViewAttributesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NVX_multiview_per_view_attributes\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvxMultiviewPerViewAttributesFn {}
unsafe impl Send for NvxMultiviewPerViewAttributesFn {}
unsafe impl Sync for NvxMultiviewPerViewAttributesFn {}
impl ::std::clone::Clone for NvxMultiviewPerViewAttributesFn {
    fn clone(&self) -> Self {
        NvxMultiviewPerViewAttributesFn {}
    }
}
impl NvxMultiviewPerViewAttributesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvxMultiviewPerViewAttributesFn {}
    }
}
#[doc = "Generated from 'VK_NVX_multiview_per_view_attributes'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX: Self =
        Self(1_000_097_000);
}
#[doc = "Generated from 'VK_NVX_multiview_per_view_attributes'"]
impl SubpassDescriptionFlags {
    pub const PER_VIEW_ATTRIBUTES_NVX: Self = Self(0b1);
}
#[doc = "Generated from 'VK_NVX_multiview_per_view_attributes'"]
impl SubpassDescriptionFlags {
    pub const PER_VIEW_POSITION_X_ONLY_NVX: Self = Self(0b10);
}
impl NvViewportSwizzleFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_viewport_swizzle\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvViewportSwizzleFn {}
unsafe impl Send for NvViewportSwizzleFn {}
unsafe impl Sync for NvViewportSwizzleFn {}
impl ::std::clone::Clone for NvViewportSwizzleFn {
    fn clone(&self) -> Self {
        NvViewportSwizzleFn {}
    }
}
impl NvViewportSwizzleFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvViewportSwizzleFn {}
    }
}
#[doc = "Generated from 'VK_NV_viewport_swizzle'"]
impl StructureType {
    pub const PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV: Self = Self(1_000_098_000);
}
impl ExtDiscardRectanglesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_discard_rectangles\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDiscardRectangleEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    first_discard_rectangle: u32,
    discard_rectangle_count: u32,
    p_discard_rectangles: *const Rect2D,
);
pub struct ExtDiscardRectanglesFn {
    pub cmd_set_discard_rectangle_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        first_discard_rectangle: u32,
        discard_rectangle_count: u32,
        p_discard_rectangles: *const Rect2D,
    ),
}
unsafe impl Send for ExtDiscardRectanglesFn {}
unsafe impl Sync for ExtDiscardRectanglesFn {}
impl ::std::clone::Clone for ExtDiscardRectanglesFn {
    fn clone(&self) -> Self {
        ExtDiscardRectanglesFn {
            cmd_set_discard_rectangle_ext: self.cmd_set_discard_rectangle_ext,
        }
    }
}
impl ExtDiscardRectanglesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDiscardRectanglesFn {
            cmd_set_discard_rectangle_ext: unsafe {
                extern "system" fn cmd_set_discard_rectangle_ext(
                    _command_buffer: CommandBuffer,
                    _first_discard_rectangle: u32,
                    _discard_rectangle_count: u32,
                    _p_discard_rectangles: *const Rect2D,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_discard_rectangle_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetDiscardRectangleEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_discard_rectangle_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDiscardRectangleEXT.html>"]
    pub unsafe fn cmd_set_discard_rectangle_ext(
        &self,
        command_buffer: CommandBuffer,
        first_discard_rectangle: u32,
        discard_rectangle_count: u32,
        p_discard_rectangles: *const Rect2D,
    ) {
        (self.cmd_set_discard_rectangle_ext)(
            command_buffer,
            first_discard_rectangle,
            discard_rectangle_count,
            p_discard_rectangles,
        )
    }
}
#[doc = "Generated from 'VK_EXT_discard_rectangles'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT: Self = Self(1_000_099_000);
}
#[doc = "Generated from 'VK_EXT_discard_rectangles'"]
impl StructureType {
    pub const PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT: Self = Self(1_000_099_001);
}
#[doc = "Generated from 'VK_EXT_discard_rectangles'"]
impl DynamicState {
    pub const DISCARD_RECTANGLE_EXT: Self = Self(1_000_099_000);
}
impl NvExtension101Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_101\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension101Fn {}
unsafe impl Send for NvExtension101Fn {}
unsafe impl Sync for NvExtension101Fn {}
impl ::std::clone::Clone for NvExtension101Fn {
    fn clone(&self) -> Self {
        NvExtension101Fn {}
    }
}
impl NvExtension101Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension101Fn {}
    }
}
impl ExtConservativeRasterizationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_conservative_rasterization\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtConservativeRasterizationFn {}
unsafe impl Send for ExtConservativeRasterizationFn {}
unsafe impl Sync for ExtConservativeRasterizationFn {}
impl ::std::clone::Clone for ExtConservativeRasterizationFn {
    fn clone(&self) -> Self {
        ExtConservativeRasterizationFn {}
    }
}
impl ExtConservativeRasterizationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtConservativeRasterizationFn {}
    }
}
#[doc = "Generated from 'VK_EXT_conservative_rasterization'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT: Self = Self(1_000_101_000);
}
#[doc = "Generated from 'VK_EXT_conservative_rasterization'"]
impl StructureType {
    pub const PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT: Self = Self(1_000_101_001);
}
impl ExtDepthClipEnableFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_depth_clip_enable\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtDepthClipEnableFn {}
unsafe impl Send for ExtDepthClipEnableFn {}
unsafe impl Sync for ExtDepthClipEnableFn {}
impl ::std::clone::Clone for ExtDepthClipEnableFn {
    fn clone(&self) -> Self {
        ExtDepthClipEnableFn {}
    }
}
impl ExtDepthClipEnableFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDepthClipEnableFn {}
    }
}
#[doc = "Generated from 'VK_EXT_depth_clip_enable'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT: Self = Self(1_000_102_000);
}
#[doc = "Generated from 'VK_EXT_depth_clip_enable'"]
impl StructureType {
    pub const PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT: Self = Self(1_000_102_001);
}
impl NvExtension104Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_104\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension104Fn {}
unsafe impl Send for NvExtension104Fn {}
unsafe impl Sync for NvExtension104Fn {}
impl ::std::clone::Clone for NvExtension104Fn {
    fn clone(&self) -> Self {
        NvExtension104Fn {}
    }
}
impl NvExtension104Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension104Fn {}
    }
}
impl ExtSwapchainColorspaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_swapchain_colorspace\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
pub struct ExtSwapchainColorspaceFn {}
unsafe impl Send for ExtSwapchainColorspaceFn {}
unsafe impl Sync for ExtSwapchainColorspaceFn {}
impl ::std::clone::Clone for ExtSwapchainColorspaceFn {
    fn clone(&self) -> Self {
        ExtSwapchainColorspaceFn {}
    }
}
impl ExtSwapchainColorspaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtSwapchainColorspaceFn {}
    }
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const DISPLAY_P3_NONLINEAR_EXT: Self = Self(1_000_104_001);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const EXTENDED_SRGB_LINEAR_EXT: Self = Self(1_000_104_002);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const DISPLAY_P3_LINEAR_EXT: Self = Self(1_000_104_003);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const DCI_P3_NONLINEAR_EXT: Self = Self(1_000_104_004);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const BT709_LINEAR_EXT: Self = Self(1_000_104_005);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const BT709_NONLINEAR_EXT: Self = Self(1_000_104_006);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const BT2020_LINEAR_EXT: Self = Self(1_000_104_007);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const HDR10_ST2084_EXT: Self = Self(1_000_104_008);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const DOLBYVISION_EXT: Self = Self(1_000_104_009);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const HDR10_HLG_EXT: Self = Self(1_000_104_010);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const ADOBERGB_LINEAR_EXT: Self = Self(1_000_104_011);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const ADOBERGB_NONLINEAR_EXT: Self = Self(1_000_104_012);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const PASS_THROUGH_EXT: Self = Self(1_000_104_013);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const EXTENDED_SRGB_NONLINEAR_EXT: Self = Self(1_000_104_014);
}
#[doc = "Generated from 'VK_EXT_swapchain_colorspace'"]
impl ColorSpaceKHR {
    pub const DCI_P3_LINEAR_EXT: Self = ColorSpaceKHR::DISPLAY_P3_LINEAR_EXT;
}
impl ExtHdrMetadataFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_hdr_metadata\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkSetHdrMetadataEXT = extern "system" fn(
    device: Device,
    swapchain_count: u32,
    p_swapchains: *const SwapchainKHR,
    p_metadata: *const HdrMetadataEXT,
);
pub struct ExtHdrMetadataFn {
    pub set_hdr_metadata_ext: extern "system" fn(
        device: Device,
        swapchain_count: u32,
        p_swapchains: *const SwapchainKHR,
        p_metadata: *const HdrMetadataEXT,
    ),
}
unsafe impl Send for ExtHdrMetadataFn {}
unsafe impl Sync for ExtHdrMetadataFn {}
impl ::std::clone::Clone for ExtHdrMetadataFn {
    fn clone(&self) -> Self {
        ExtHdrMetadataFn {
            set_hdr_metadata_ext: self.set_hdr_metadata_ext,
        }
    }
}
impl ExtHdrMetadataFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtHdrMetadataFn {
            set_hdr_metadata_ext: unsafe {
                extern "system" fn set_hdr_metadata_ext(
                    _device: Device,
                    _swapchain_count: u32,
                    _p_swapchains: *const SwapchainKHR,
                    _p_metadata: *const HdrMetadataEXT,
                ) {
                    panic!(concat!("Unable to load ", stringify!(set_hdr_metadata_ext)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkSetHdrMetadataEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    set_hdr_metadata_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetHdrMetadataEXT.html>"]
    pub unsafe fn set_hdr_metadata_ext(
        &self,
        device: Device,
        swapchain_count: u32,
        p_swapchains: *const SwapchainKHR,
        p_metadata: *const HdrMetadataEXT,
    ) {
        (self.set_hdr_metadata_ext)(device, swapchain_count, p_swapchains, p_metadata)
    }
}
#[doc = "Generated from 'VK_EXT_hdr_metadata'"]
impl StructureType {
    pub const HDR_METADATA_EXT: Self = Self(1_000_105_000);
}
impl ImgExtension107Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_extension_107\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ImgExtension107Fn {}
unsafe impl Send for ImgExtension107Fn {}
unsafe impl Sync for ImgExtension107Fn {}
impl ::std::clone::Clone for ImgExtension107Fn {
    fn clone(&self) -> Self {
        ImgExtension107Fn {}
    }
}
impl ImgExtension107Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ImgExtension107Fn {}
    }
}
impl ImgExtension108Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_extension_108\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ImgExtension108Fn {}
unsafe impl Send for ImgExtension108Fn {}
unsafe impl Sync for ImgExtension108Fn {}
impl ::std::clone::Clone for ImgExtension108Fn {
    fn clone(&self) -> Self {
        ImgExtension108Fn {}
    }
}
impl ImgExtension108Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ImgExtension108Fn {}
    }
}
impl KhrImagelessFramebufferFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_imageless_framebuffer\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrImagelessFramebufferFn {}
unsafe impl Send for KhrImagelessFramebufferFn {}
unsafe impl Sync for KhrImagelessFramebufferFn {}
impl ::std::clone::Clone for KhrImagelessFramebufferFn {
    fn clone(&self) -> Self {
        KhrImagelessFramebufferFn {}
    }
}
impl KhrImagelessFramebufferFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrImagelessFramebufferFn {}
    }
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl StructureType {
    pub const FRAMEBUFFER_ATTACHMENTS_CREATE_INFO_KHR: Self =
        StructureType::FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl StructureType {
    pub const FRAMEBUFFER_ATTACHMENT_IMAGE_INFO_KHR: Self =
        StructureType::FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl StructureType {
    pub const RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR: Self =
        StructureType::RENDER_PASS_ATTACHMENT_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl FramebufferCreateFlags {
    pub const IMAGELESS_KHR: Self = FramebufferCreateFlags::IMAGELESS;
}
impl KhrCreateRenderpass2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_create_renderpass2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateRenderPass2 = extern "system" fn(
    device: Device,
    p_create_info: *const RenderPassCreateInfo2,
    p_allocator: *const AllocationCallbacks,
    p_render_pass: *mut RenderPass,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginRenderPass2 = extern "system" fn(
    command_buffer: CommandBuffer,
    p_render_pass_begin: *const RenderPassBeginInfo,
    p_subpass_begin_info: *const SubpassBeginInfo,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdNextSubpass2 = extern "system" fn(
    command_buffer: CommandBuffer,
    p_subpass_begin_info: *const SubpassBeginInfo,
    p_subpass_end_info: *const SubpassEndInfo,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndRenderPass2 =
    extern "system" fn(command_buffer: CommandBuffer, p_subpass_end_info: *const SubpassEndInfo);
pub struct KhrCreateRenderpass2Fn {
    pub create_render_pass2_khr: extern "system" fn(
        device: Device,
        p_create_info: *const RenderPassCreateInfo2,
        p_allocator: *const AllocationCallbacks,
        p_render_pass: *mut RenderPass,
    ) -> Result,
    pub cmd_begin_render_pass2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_render_pass_begin: *const RenderPassBeginInfo,
        p_subpass_begin_info: *const SubpassBeginInfo,
    ),
    pub cmd_next_subpass2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_subpass_begin_info: *const SubpassBeginInfo,
        p_subpass_end_info: *const SubpassEndInfo,
    ),
    pub cmd_end_render_pass2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_subpass_end_info: *const SubpassEndInfo,
    ),
}
unsafe impl Send for KhrCreateRenderpass2Fn {}
unsafe impl Sync for KhrCreateRenderpass2Fn {}
impl ::std::clone::Clone for KhrCreateRenderpass2Fn {
    fn clone(&self) -> Self {
        KhrCreateRenderpass2Fn {
            create_render_pass2_khr: self.create_render_pass2_khr,
            cmd_begin_render_pass2_khr: self.cmd_begin_render_pass2_khr,
            cmd_next_subpass2_khr: self.cmd_next_subpass2_khr,
            cmd_end_render_pass2_khr: self.cmd_end_render_pass2_khr,
        }
    }
}
impl KhrCreateRenderpass2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrCreateRenderpass2Fn {
            create_render_pass2_khr: unsafe {
                extern "system" fn create_render_pass2_khr(
                    _device: Device,
                    _p_create_info: *const RenderPassCreateInfo2,
                    _p_allocator: *const AllocationCallbacks,
                    _p_render_pass: *mut RenderPass,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_render_pass2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateRenderPass2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_render_pass2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_render_pass2_khr: unsafe {
                extern "system" fn cmd_begin_render_pass2_khr(
                    _command_buffer: CommandBuffer,
                    _p_render_pass_begin: *const RenderPassBeginInfo,
                    _p_subpass_begin_info: *const SubpassBeginInfo,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_render_pass2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBeginRenderPass2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_render_pass2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_next_subpass2_khr: unsafe {
                extern "system" fn cmd_next_subpass2_khr(
                    _command_buffer: CommandBuffer,
                    _p_subpass_begin_info: *const SubpassBeginInfo,
                    _p_subpass_end_info: *const SubpassEndInfo,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_next_subpass2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdNextSubpass2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_next_subpass2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_render_pass2_khr: unsafe {
                extern "system" fn cmd_end_render_pass2_khr(
                    _command_buffer: CommandBuffer,
                    _p_subpass_end_info: *const SubpassEndInfo,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_render_pass2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdEndRenderPass2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_render_pass2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRenderPass2KHR.html>"]
    pub unsafe fn create_render_pass2_khr(
        &self,
        device: Device,
        p_create_info: *const RenderPassCreateInfo2,
        p_allocator: *const AllocationCallbacks,
        p_render_pass: *mut RenderPass,
    ) -> Result {
        (self.create_render_pass2_khr)(device, p_create_info, p_allocator, p_render_pass)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginRenderPass2KHR.html>"]
    pub unsafe fn cmd_begin_render_pass2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_render_pass_begin: *const RenderPassBeginInfo,
        p_subpass_begin_info: *const SubpassBeginInfo,
    ) {
        (self.cmd_begin_render_pass2_khr)(command_buffer, p_render_pass_begin, p_subpass_begin_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdNextSubpass2KHR.html>"]
    pub unsafe fn cmd_next_subpass2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_subpass_begin_info: *const SubpassBeginInfo,
        p_subpass_end_info: *const SubpassEndInfo,
    ) {
        (self.cmd_next_subpass2_khr)(command_buffer, p_subpass_begin_info, p_subpass_end_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndRenderPass2KHR.html>"]
    pub unsafe fn cmd_end_render_pass2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_subpass_end_info: *const SubpassEndInfo,
    ) {
        (self.cmd_end_render_pass2_khr)(command_buffer, p_subpass_end_info)
    }
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const ATTACHMENT_DESCRIPTION_2_KHR: Self = StructureType::ATTACHMENT_DESCRIPTION_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const ATTACHMENT_REFERENCE_2_KHR: Self = StructureType::ATTACHMENT_REFERENCE_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_DESCRIPTION_2_KHR: Self = StructureType::SUBPASS_DESCRIPTION_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_DEPENDENCY_2_KHR: Self = StructureType::SUBPASS_DEPENDENCY_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const RENDER_PASS_CREATE_INFO_2_KHR: Self = StructureType::RENDER_PASS_CREATE_INFO_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_BEGIN_INFO_KHR: Self = StructureType::SUBPASS_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_END_INFO_KHR: Self = StructureType::SUBPASS_END_INFO;
}
impl ImgExtension111Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_extension_111\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ImgExtension111Fn {}
unsafe impl Send for ImgExtension111Fn {}
unsafe impl Sync for ImgExtension111Fn {}
impl ::std::clone::Clone for ImgExtension111Fn {
    fn clone(&self) -> Self {
        ImgExtension111Fn {}
    }
}
impl ImgExtension111Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ImgExtension111Fn {}
    }
}
impl KhrSharedPresentableImageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shared_presentable_image\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainStatusKHR =
    extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result;
pub struct KhrSharedPresentableImageFn {
    pub get_swapchain_status_khr:
        extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result,
}
unsafe impl Send for KhrSharedPresentableImageFn {}
unsafe impl Sync for KhrSharedPresentableImageFn {}
impl ::std::clone::Clone for KhrSharedPresentableImageFn {
    fn clone(&self) -> Self {
        KhrSharedPresentableImageFn {
            get_swapchain_status_khr: self.get_swapchain_status_khr,
        }
    }
}
impl KhrSharedPresentableImageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSharedPresentableImageFn {
            get_swapchain_status_khr: unsafe {
                extern "system" fn get_swapchain_status_khr(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_swapchain_status_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetSwapchainStatusKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    get_swapchain_status_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSwapchainStatusKHR.html>"]
    pub unsafe fn get_swapchain_status_khr(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
    ) -> Result {
        (self.get_swapchain_status_khr)(device, swapchain)
    }
}
#[doc = "Generated from 'VK_KHR_shared_presentable_image'"]
impl StructureType {
    pub const SHARED_PRESENT_SURFACE_CAPABILITIES_KHR: Self = Self(1_000_111_000);
}
#[doc = "Generated from 'VK_KHR_shared_presentable_image'"]
impl PresentModeKHR {
    pub const SHARED_DEMAND_REFRESH: Self = Self(1_000_111_000);
}
#[doc = "Generated from 'VK_KHR_shared_presentable_image'"]
impl PresentModeKHR {
    pub const SHARED_CONTINUOUS_REFRESH: Self = Self(1_000_111_001);
}
#[doc = "Generated from 'VK_KHR_shared_presentable_image'"]
impl ImageLayout {
    pub const SHARED_PRESENT_KHR: Self = Self(1_000_111_000);
}
impl KhrExternalFenceCapabilitiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_fence_capabilities\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceExternalFenceProperties = extern "system" fn(
    physical_device: PhysicalDevice,
    p_external_fence_info: *const PhysicalDeviceExternalFenceInfo,
    p_external_fence_properties: *mut ExternalFenceProperties,
);
pub struct KhrExternalFenceCapabilitiesFn {
    pub get_physical_device_external_fence_properties_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_external_fence_info: *const PhysicalDeviceExternalFenceInfo,
        p_external_fence_properties: *mut ExternalFenceProperties,
    ),
}
unsafe impl Send for KhrExternalFenceCapabilitiesFn {}
unsafe impl Sync for KhrExternalFenceCapabilitiesFn {}
impl ::std::clone::Clone for KhrExternalFenceCapabilitiesFn {
    fn clone(&self) -> Self {
        KhrExternalFenceCapabilitiesFn {
            get_physical_device_external_fence_properties_khr: self
                .get_physical_device_external_fence_properties_khr,
        }
    }
}
impl KhrExternalFenceCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalFenceCapabilitiesFn {
            get_physical_device_external_fence_properties_khr: unsafe {
                extern "system" fn get_physical_device_external_fence_properties_khr(
                    _physical_device: PhysicalDevice,
                    _p_external_fence_info: *const PhysicalDeviceExternalFenceInfo,
                    _p_external_fence_properties: *mut ExternalFenceProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_external_fence_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceExternalFencePropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_external_fence_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalFencePropertiesKHR.html>"]
    pub unsafe fn get_physical_device_external_fence_properties_khr(
        &self,
        physical_device: PhysicalDevice,
        p_external_fence_info: *const PhysicalDeviceExternalFenceInfo,
        p_external_fence_properties: *mut ExternalFenceProperties,
    ) {
        (self.get_physical_device_external_fence_properties_khr)(
            physical_device,
            p_external_fence_info,
            p_external_fence_properties,
        )
    }
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO_KHR: Self =
        StructureType::PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl StructureType {
    pub const EXTERNAL_FENCE_PROPERTIES_KHR: Self = StructureType::EXTERNAL_FENCE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const OPAQUE_FD_KHR: Self = ExternalFenceHandleTypeFlags::OPAQUE_FD;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const OPAQUE_WIN32_KHR: Self = ExternalFenceHandleTypeFlags::OPAQUE_WIN32;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const OPAQUE_WIN32_KMT_KHR: Self = ExternalFenceHandleTypeFlags::OPAQUE_WIN32_KMT;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const SYNC_FD_KHR: Self = ExternalFenceHandleTypeFlags::SYNC_FD;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceFeatureFlags {
    pub const EXPORTABLE_KHR: Self = ExternalFenceFeatureFlags::EXPORTABLE;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceFeatureFlags {
    pub const IMPORTABLE_KHR: Self = ExternalFenceFeatureFlags::IMPORTABLE;
}
impl KhrExternalFenceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_fence\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrExternalFenceFn {}
unsafe impl Send for KhrExternalFenceFn {}
unsafe impl Sync for KhrExternalFenceFn {}
impl ::std::clone::Clone for KhrExternalFenceFn {
    fn clone(&self) -> Self {
        KhrExternalFenceFn {}
    }
}
impl KhrExternalFenceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalFenceFn {}
    }
}
#[doc = "Generated from 'VK_KHR_external_fence'"]
impl StructureType {
    pub const EXPORT_FENCE_CREATE_INFO_KHR: Self = StructureType::EXPORT_FENCE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_fence'"]
impl FenceImportFlags {
    pub const TEMPORARY_KHR: Self = FenceImportFlags::TEMPORARY;
}
impl KhrExternalFenceWin32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_fence_win32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkImportFenceWin32HandleKHR = extern "system" fn(
    device: Device,
    p_import_fence_win32_handle_info: *const ImportFenceWin32HandleInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetFenceWin32HandleKHR = extern "system" fn(
    device: Device,
    p_get_win32_handle_info: *const FenceGetWin32HandleInfoKHR,
    p_handle: *mut HANDLE,
) -> Result;
pub struct KhrExternalFenceWin32Fn {
    pub import_fence_win32_handle_khr: extern "system" fn(
        device: Device,
        p_import_fence_win32_handle_info: *const ImportFenceWin32HandleInfoKHR,
    ) -> Result,
    pub get_fence_win32_handle_khr: extern "system" fn(
        device: Device,
        p_get_win32_handle_info: *const FenceGetWin32HandleInfoKHR,
        p_handle: *mut HANDLE,
    ) -> Result,
}
unsafe impl Send for KhrExternalFenceWin32Fn {}
unsafe impl Sync for KhrExternalFenceWin32Fn {}
impl ::std::clone::Clone for KhrExternalFenceWin32Fn {
    fn clone(&self) -> Self {
        KhrExternalFenceWin32Fn {
            import_fence_win32_handle_khr: self.import_fence_win32_handle_khr,
            get_fence_win32_handle_khr: self.get_fence_win32_handle_khr,
        }
    }
}
impl KhrExternalFenceWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalFenceWin32Fn {
            import_fence_win32_handle_khr: unsafe {
                extern "system" fn import_fence_win32_handle_khr(
                    _device: Device,
                    _p_import_fence_win32_handle_info: *const ImportFenceWin32HandleInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(import_fence_win32_handle_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkImportFenceWin32HandleKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    import_fence_win32_handle_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_fence_win32_handle_khr: unsafe {
                extern "system" fn get_fence_win32_handle_khr(
                    _device: Device,
                    _p_get_win32_handle_info: *const FenceGetWin32HandleInfoKHR,
                    _p_handle: *mut HANDLE,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_fence_win32_handle_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetFenceWin32HandleKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    get_fence_win32_handle_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkImportFenceWin32HandleKHR.html>"]
    pub unsafe fn import_fence_win32_handle_khr(
        &self,
        device: Device,
        p_import_fence_win32_handle_info: *const ImportFenceWin32HandleInfoKHR,
    ) -> Result {
        (self.import_fence_win32_handle_khr)(device, p_import_fence_win32_handle_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetFenceWin32HandleKHR.html>"]
    pub unsafe fn get_fence_win32_handle_khr(
        &self,
        device: Device,
        p_get_win32_handle_info: *const FenceGetWin32HandleInfoKHR,
        p_handle: *mut HANDLE,
    ) -> Result {
        (self.get_fence_win32_handle_khr)(device, p_get_win32_handle_info, p_handle)
    }
}
#[doc = "Generated from 'VK_KHR_external_fence_win32'"]
impl StructureType {
    pub const IMPORT_FENCE_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_114_000);
}
#[doc = "Generated from 'VK_KHR_external_fence_win32'"]
impl StructureType {
    pub const EXPORT_FENCE_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_114_001);
}
#[doc = "Generated from 'VK_KHR_external_fence_win32'"]
impl StructureType {
    pub const FENCE_GET_WIN32_HANDLE_INFO_KHR: Self = Self(1_000_114_002);
}
impl KhrExternalFenceFdFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_fence_fd\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkImportFenceFdKHR = extern "system" fn(
    device: Device,
    p_import_fence_fd_info: *const ImportFenceFdInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetFenceFdKHR = extern "system" fn(
    device: Device,
    p_get_fd_info: *const FenceGetFdInfoKHR,
    p_fd: *mut c_int,
) -> Result;
pub struct KhrExternalFenceFdFn {
    pub import_fence_fd_khr: extern "system" fn(
        device: Device,
        p_import_fence_fd_info: *const ImportFenceFdInfoKHR,
    ) -> Result,
    pub get_fence_fd_khr: extern "system" fn(
        device: Device,
        p_get_fd_info: *const FenceGetFdInfoKHR,
        p_fd: *mut c_int,
    ) -> Result,
}
unsafe impl Send for KhrExternalFenceFdFn {}
unsafe impl Sync for KhrExternalFenceFdFn {}
impl ::std::clone::Clone for KhrExternalFenceFdFn {
    fn clone(&self) -> Self {
        KhrExternalFenceFdFn {
            import_fence_fd_khr: self.import_fence_fd_khr,
            get_fence_fd_khr: self.get_fence_fd_khr,
        }
    }
}
impl KhrExternalFenceFdFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalFenceFdFn {
            import_fence_fd_khr: unsafe {
                extern "system" fn import_fence_fd_khr(
                    _device: Device,
                    _p_import_fence_fd_info: *const ImportFenceFdInfoKHR,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(import_fence_fd_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkImportFenceFdKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    import_fence_fd_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_fence_fd_khr: unsafe {
                extern "system" fn get_fence_fd_khr(
                    _device: Device,
                    _p_get_fd_info: *const FenceGetFdInfoKHR,
                    _p_fd: *mut c_int,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_fence_fd_khr)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetFenceFdKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    get_fence_fd_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkImportFenceFdKHR.html>"]
    pub unsafe fn import_fence_fd_khr(
        &self,
        device: Device,
        p_import_fence_fd_info: *const ImportFenceFdInfoKHR,
    ) -> Result {
        (self.import_fence_fd_khr)(device, p_import_fence_fd_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetFenceFdKHR.html>"]
    pub unsafe fn get_fence_fd_khr(
        &self,
        device: Device,
        p_get_fd_info: *const FenceGetFdInfoKHR,
        p_fd: *mut c_int,
    ) -> Result {
        (self.get_fence_fd_khr)(device, p_get_fd_info, p_fd)
    }
}
#[doc = "Generated from 'VK_KHR_external_fence_fd'"]
impl StructureType {
    pub const IMPORT_FENCE_FD_INFO_KHR: Self = Self(1_000_115_000);
}
#[doc = "Generated from 'VK_KHR_external_fence_fd'"]
impl StructureType {
    pub const FENCE_GET_FD_INFO_KHR: Self = Self(1_000_115_001);
}
impl KhrPerformanceQueryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_performance_query\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR =
    extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        p_counter_count: *mut u32,
        p_counters: *mut PerformanceCounterKHR,
        p_counter_descriptions: *mut PerformanceCounterDescriptionKHR,
    ) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_performance_query_create_info: *const QueryPoolPerformanceCreateInfoKHR,
    p_num_passes: *mut u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireProfilingLockKHR =
    extern "system" fn(device: Device, p_info: *const AcquireProfilingLockInfoKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkReleaseProfilingLockKHR = extern "system" fn(device: Device);
pub struct KhrPerformanceQueryFn {
    pub enumerate_physical_device_queue_family_performance_query_counters_khr:
        extern "system" fn(
            physical_device: PhysicalDevice,
            queue_family_index: u32,
            p_counter_count: *mut u32,
            p_counters: *mut PerformanceCounterKHR,
            p_counter_descriptions: *mut PerformanceCounterDescriptionKHR,
        ) -> Result,
    pub get_physical_device_queue_family_performance_query_passes_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_performance_query_create_info: *const QueryPoolPerformanceCreateInfoKHR,
        p_num_passes: *mut u32,
    ),
    pub acquire_profiling_lock_khr:
        extern "system" fn(device: Device, p_info: *const AcquireProfilingLockInfoKHR) -> Result,
    pub release_profiling_lock_khr: extern "system" fn(device: Device),
}
unsafe impl Send for KhrPerformanceQueryFn {}
unsafe impl Sync for KhrPerformanceQueryFn {}
impl ::std::clone::Clone for KhrPerformanceQueryFn {
    fn clone(&self) -> Self {
        KhrPerformanceQueryFn {
            enumerate_physical_device_queue_family_performance_query_counters_khr: self
                .enumerate_physical_device_queue_family_performance_query_counters_khr,
            get_physical_device_queue_family_performance_query_passes_khr: self
                .get_physical_device_queue_family_performance_query_passes_khr,
            acquire_profiling_lock_khr: self.acquire_profiling_lock_khr,
            release_profiling_lock_khr: self.release_profiling_lock_khr,
        }
    }
}
impl KhrPerformanceQueryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPerformanceQueryFn {
            enumerate_physical_device_queue_family_performance_query_counters_khr: unsafe {
                extern "system" fn enumerate_physical_device_queue_family_performance_query_counters_khr(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                    _p_counter_count: *mut u32,
                    _p_counters: *mut PerformanceCounterKHR,
                    _p_counter_descriptions: *mut PerformanceCounterDescriptionKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(
                            enumerate_physical_device_queue_family_performance_query_counters_khr
                        )
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_physical_device_queue_family_performance_query_counters_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_queue_family_performance_query_passes_khr: unsafe {
                extern "system" fn get_physical_device_queue_family_performance_query_passes_khr(
                    _physical_device: PhysicalDevice,
                    _p_performance_query_create_info: *const QueryPoolPerformanceCreateInfoKHR,
                    _p_num_passes: *mut u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_queue_family_performance_query_passes_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_queue_family_performance_query_passes_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            acquire_profiling_lock_khr: unsafe {
                extern "system" fn acquire_profiling_lock_khr(
                    _device: Device,
                    _p_info: *const AcquireProfilingLockInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_profiling_lock_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireProfilingLockKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_profiling_lock_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            release_profiling_lock_khr: unsafe {
                extern "system" fn release_profiling_lock_khr(_device: Device) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(release_profiling_lock_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkReleaseProfilingLockKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    release_profiling_lock_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR.html>"]
    pub unsafe fn enumerate_physical_device_queue_family_performance_query_counters_khr(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        p_counter_count: *mut u32,
        p_counters: *mut PerformanceCounterKHR,
        p_counter_descriptions: *mut PerformanceCounterDescriptionKHR,
    ) -> Result {
        (self.enumerate_physical_device_queue_family_performance_query_counters_khr)(
            physical_device,
            queue_family_index,
            p_counter_count,
            p_counters,
            p_counter_descriptions,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR.html>"]
    pub unsafe fn get_physical_device_queue_family_performance_query_passes_khr(
        &self,
        physical_device: PhysicalDevice,
        p_performance_query_create_info: *const QueryPoolPerformanceCreateInfoKHR,
        p_num_passes: *mut u32,
    ) {
        (self.get_physical_device_queue_family_performance_query_passes_khr)(
            physical_device,
            p_performance_query_create_info,
            p_num_passes,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireProfilingLockKHR.html>"]
    pub unsafe fn acquire_profiling_lock_khr(
        &self,
        device: Device,
        p_info: *const AcquireProfilingLockInfoKHR,
    ) -> Result {
        (self.acquire_profiling_lock_khr)(device, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkReleaseProfilingLockKHR.html>"]
    pub unsafe fn release_profiling_lock_khr(&self, device: Device) {
        (self.release_profiling_lock_khr)(device)
    }
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl QueryType {
    pub const PERFORMANCE_QUERY_KHR: Self = Self(1_000_116_000);
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR: Self = Self(1_000_116_000);
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR: Self = Self(1_000_116_001);
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl StructureType {
    pub const QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR: Self = Self(1_000_116_002);
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl StructureType {
    pub const PERFORMANCE_QUERY_SUBMIT_INFO_KHR: Self = Self(1_000_116_003);
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl StructureType {
    pub const ACQUIRE_PROFILING_LOCK_INFO_KHR: Self = Self(1_000_116_004);
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl StructureType {
    pub const PERFORMANCE_COUNTER_KHR: Self = Self(1_000_116_005);
}
#[doc = "Generated from 'VK_KHR_performance_query'"]
impl StructureType {
    pub const PERFORMANCE_COUNTER_DESCRIPTION_KHR: Self = Self(1_000_116_006);
}
impl KhrMaintenance2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_maintenance2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrMaintenance2Fn {}
unsafe impl Send for KhrMaintenance2Fn {}
unsafe impl Sync for KhrMaintenance2Fn {}
impl ::std::clone::Clone for KhrMaintenance2Fn {
    fn clone(&self) -> Self {
        KhrMaintenance2Fn {}
    }
}
impl KhrMaintenance2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrMaintenance2Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl ImageCreateFlags {
    pub const BLOCK_TEXEL_VIEW_COMPATIBLE_KHR: Self = ImageCreateFlags::BLOCK_TEXEL_VIEW_COMPATIBLE;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl ImageCreateFlags {
    pub const EXTENDED_USAGE_KHR: Self = ImageCreateFlags::EXTENDED_USAGE;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO_KHR: Self =
        StructureType::RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const IMAGE_VIEW_USAGE_CREATE_INFO_KHR: Self = StructureType::IMAGE_VIEW_USAGE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO_KHR: Self =
        StructureType::PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl ImageLayout {
    pub const DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR: Self =
        ImageLayout::DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl ImageLayout {
    pub const DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR: Self =
        ImageLayout::DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl PointClippingBehavior {
    pub const ALL_CLIP_PLANES_KHR: Self = PointClippingBehavior::ALL_CLIP_PLANES;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl PointClippingBehavior {
    pub const USER_CLIP_PLANES_ONLY_KHR: Self = PointClippingBehavior::USER_CLIP_PLANES_ONLY;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl TessellationDomainOrigin {
    pub const UPPER_LEFT_KHR: Self = TessellationDomainOrigin::UPPER_LEFT;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl TessellationDomainOrigin {
    pub const LOWER_LEFT_KHR: Self = TessellationDomainOrigin::LOWER_LEFT;
}
impl KhrExtension119Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_119\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension119Fn {}
unsafe impl Send for KhrExtension119Fn {}
unsafe impl Sync for KhrExtension119Fn {}
impl ::std::clone::Clone for KhrExtension119Fn {
    fn clone(&self) -> Self {
        KhrExtension119Fn {}
    }
}
impl KhrExtension119Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension119Fn {}
    }
}
impl KhrGetSurfaceCapabilities2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_get_surface_capabilities2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_surface_capabilities: *mut SurfaceCapabilities2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceFormats2KHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_surface_format_count: *mut u32,
    p_surface_formats: *mut SurfaceFormat2KHR,
) -> Result;
pub struct KhrGetSurfaceCapabilities2Fn {
    pub get_physical_device_surface_capabilities2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_surface_capabilities: *mut SurfaceCapabilities2KHR,
    ) -> Result,
    pub get_physical_device_surface_formats2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_surface_format_count: *mut u32,
        p_surface_formats: *mut SurfaceFormat2KHR,
    ) -> Result,
}
unsafe impl Send for KhrGetSurfaceCapabilities2Fn {}
unsafe impl Sync for KhrGetSurfaceCapabilities2Fn {}
impl ::std::clone::Clone for KhrGetSurfaceCapabilities2Fn {
    fn clone(&self) -> Self {
        KhrGetSurfaceCapabilities2Fn {
            get_physical_device_surface_capabilities2_khr: self
                .get_physical_device_surface_capabilities2_khr,
            get_physical_device_surface_formats2_khr: self.get_physical_device_surface_formats2_khr,
        }
    }
}
impl KhrGetSurfaceCapabilities2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetSurfaceCapabilities2Fn {
            get_physical_device_surface_capabilities2_khr: unsafe {
                extern "system" fn get_physical_device_surface_capabilities2_khr(
                    _physical_device: PhysicalDevice,
                    _p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
                    _p_surface_capabilities: *mut SurfaceCapabilities2KHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_capabilities2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfaceCapabilities2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_capabilities2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_surface_formats2_khr: unsafe {
                extern "system" fn get_physical_device_surface_formats2_khr(
                    _physical_device: PhysicalDevice,
                    _p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
                    _p_surface_format_count: *mut u32,
                    _p_surface_formats: *mut SurfaceFormat2KHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_formats2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfaceFormats2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_formats2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceCapabilities2KHR.html>"]
    pub unsafe fn get_physical_device_surface_capabilities2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_surface_capabilities: *mut SurfaceCapabilities2KHR,
    ) -> Result {
        (self.get_physical_device_surface_capabilities2_khr)(
            physical_device,
            p_surface_info,
            p_surface_capabilities,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfaceFormats2KHR.html>"]
    pub unsafe fn get_physical_device_surface_formats2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_surface_format_count: *mut u32,
        p_surface_formats: *mut SurfaceFormat2KHR,
    ) -> Result {
        (self.get_physical_device_surface_formats2_khr)(
            physical_device,
            p_surface_info,
            p_surface_format_count,
            p_surface_formats,
        )
    }
}
#[doc = "Generated from 'VK_KHR_get_surface_capabilities2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SURFACE_INFO_2_KHR: Self = Self(1_000_119_000);
}
#[doc = "Generated from 'VK_KHR_get_surface_capabilities2'"]
impl StructureType {
    pub const SURFACE_CAPABILITIES_2_KHR: Self = Self(1_000_119_001);
}
#[doc = "Generated from 'VK_KHR_get_surface_capabilities2'"]
impl StructureType {
    pub const SURFACE_FORMAT_2_KHR: Self = Self(1_000_119_002);
}
impl KhrVariablePointersFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_variable_pointers\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrVariablePointersFn {}
unsafe impl Send for KhrVariablePointersFn {}
unsafe impl Sync for KhrVariablePointersFn {}
impl ::std::clone::Clone for KhrVariablePointersFn {
    fn clone(&self) -> Self {
        KhrVariablePointersFn {}
    }
}
impl KhrVariablePointersFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrVariablePointersFn {}
    }
}
#[doc = "Generated from 'VK_KHR_variable_pointers'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES;
}
#[doc = "Generated from 'VK_KHR_variable_pointers'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES_KHR;
}
impl KhrGetDisplayProperties2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_get_display_properties2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDisplayProperties2KHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayProperties2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayPlaneProperties2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayModeProperties2KHR = extern "system" fn(
    physical_device: PhysicalDevice,
    display: DisplayKHR,
    p_property_count: *mut u32,
    p_properties: *mut DisplayModeProperties2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayPlaneCapabilities2KHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_display_plane_info: *const DisplayPlaneInfo2KHR,
    p_capabilities: *mut DisplayPlaneCapabilities2KHR,
) -> Result;
pub struct KhrGetDisplayProperties2Fn {
    pub get_physical_device_display_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayProperties2KHR,
    ) -> Result,
    pub get_physical_device_display_plane_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayPlaneProperties2KHR,
    ) -> Result,
    pub get_display_mode_properties2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        display: DisplayKHR,
        p_property_count: *mut u32,
        p_properties: *mut DisplayModeProperties2KHR,
    ) -> Result,
    pub get_display_plane_capabilities2_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_display_plane_info: *const DisplayPlaneInfo2KHR,
        p_capabilities: *mut DisplayPlaneCapabilities2KHR,
    ) -> Result,
}
unsafe impl Send for KhrGetDisplayProperties2Fn {}
unsafe impl Sync for KhrGetDisplayProperties2Fn {}
impl ::std::clone::Clone for KhrGetDisplayProperties2Fn {
    fn clone(&self) -> Self {
        KhrGetDisplayProperties2Fn {
            get_physical_device_display_properties2_khr: self
                .get_physical_device_display_properties2_khr,
            get_physical_device_display_plane_properties2_khr: self
                .get_physical_device_display_plane_properties2_khr,
            get_display_mode_properties2_khr: self.get_display_mode_properties2_khr,
            get_display_plane_capabilities2_khr: self.get_display_plane_capabilities2_khr,
        }
    }
}
impl KhrGetDisplayProperties2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetDisplayProperties2Fn {
            get_physical_device_display_properties2_khr: unsafe {
                extern "system" fn get_physical_device_display_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _p_property_count: *mut u32,
                    _p_properties: *mut DisplayProperties2KHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_display_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceDisplayProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_display_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_display_plane_properties2_khr: unsafe {
                extern "system" fn get_physical_device_display_plane_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _p_property_count: *mut u32,
                    _p_properties: *mut DisplayPlaneProperties2KHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_display_plane_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceDisplayPlaneProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_display_plane_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_display_mode_properties2_khr: unsafe {
                extern "system" fn get_display_mode_properties2_khr(
                    _physical_device: PhysicalDevice,
                    _display: DisplayKHR,
                    _p_property_count: *mut u32,
                    _p_properties: *mut DisplayModeProperties2KHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_display_mode_properties2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDisplayModeProperties2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_display_mode_properties2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_display_plane_capabilities2_khr: unsafe {
                extern "system" fn get_display_plane_capabilities2_khr(
                    _physical_device: PhysicalDevice,
                    _p_display_plane_info: *const DisplayPlaneInfo2KHR,
                    _p_capabilities: *mut DisplayPlaneCapabilities2KHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_display_plane_capabilities2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDisplayPlaneCapabilities2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_display_plane_capabilities2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceDisplayProperties2KHR.html>"]
    pub unsafe fn get_physical_device_display_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayProperties2KHR,
    ) -> Result {
        (self.get_physical_device_display_properties2_khr)(
            physical_device,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceDisplayPlaneProperties2KHR.html>"]
    pub unsafe fn get_physical_device_display_plane_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut DisplayPlaneProperties2KHR,
    ) -> Result {
        (self.get_physical_device_display_plane_properties2_khr)(
            physical_device,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDisplayModeProperties2KHR.html>"]
    pub unsafe fn get_display_mode_properties2_khr(
        &self,
        physical_device: PhysicalDevice,
        display: DisplayKHR,
        p_property_count: *mut u32,
        p_properties: *mut DisplayModeProperties2KHR,
    ) -> Result {
        (self.get_display_mode_properties2_khr)(
            physical_device,
            display,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDisplayPlaneCapabilities2KHR.html>"]
    pub unsafe fn get_display_plane_capabilities2_khr(
        &self,
        physical_device: PhysicalDevice,
        p_display_plane_info: *const DisplayPlaneInfo2KHR,
        p_capabilities: *mut DisplayPlaneCapabilities2KHR,
    ) -> Result {
        (self.get_display_plane_capabilities2_khr)(
            physical_device,
            p_display_plane_info,
            p_capabilities,
        )
    }
}
#[doc = "Generated from 'VK_KHR_get_display_properties2'"]
impl StructureType {
    pub const DISPLAY_PROPERTIES_2_KHR: Self = Self(1_000_121_000);
}
#[doc = "Generated from 'VK_KHR_get_display_properties2'"]
impl StructureType {
    pub const DISPLAY_PLANE_PROPERTIES_2_KHR: Self = Self(1_000_121_001);
}
#[doc = "Generated from 'VK_KHR_get_display_properties2'"]
impl StructureType {
    pub const DISPLAY_MODE_PROPERTIES_2_KHR: Self = Self(1_000_121_002);
}
#[doc = "Generated from 'VK_KHR_get_display_properties2'"]
impl StructureType {
    pub const DISPLAY_PLANE_INFO_2_KHR: Self = Self(1_000_121_003);
}
#[doc = "Generated from 'VK_KHR_get_display_properties2'"]
impl StructureType {
    pub const DISPLAY_PLANE_CAPABILITIES_2_KHR: Self = Self(1_000_121_004);
}
impl MvkIosSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_MVK_ios_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateIOSSurfaceMVK = extern "system" fn(
    instance: Instance,
    p_create_info: *const IOSSurfaceCreateInfoMVK,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct MvkIosSurfaceFn {
    pub create_ios_surface_mvk: extern "system" fn(
        instance: Instance,
        p_create_info: *const IOSSurfaceCreateInfoMVK,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for MvkIosSurfaceFn {}
unsafe impl Sync for MvkIosSurfaceFn {}
impl ::std::clone::Clone for MvkIosSurfaceFn {
    fn clone(&self) -> Self {
        MvkIosSurfaceFn {
            create_ios_surface_mvk: self.create_ios_surface_mvk,
        }
    }
}
impl MvkIosSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        MvkIosSurfaceFn {
            create_ios_surface_mvk: unsafe {
                extern "system" fn create_ios_surface_mvk(
                    _instance: Instance,
                    _p_create_info: *const IOSSurfaceCreateInfoMVK,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_ios_surface_mvk)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateIOSSurfaceMVK\0");
                let val = _f(cname);
                if val.is_null() {
                    create_ios_surface_mvk
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateIOSSurfaceMVK.html>"]
    pub unsafe fn create_ios_surface_mvk(
        &self,
        instance: Instance,
        p_create_info: *const IOSSurfaceCreateInfoMVK,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_ios_surface_mvk)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_MVK_ios_surface'"]
impl StructureType {
    pub const IOS_SURFACE_CREATE_INFO_MVK: Self = Self(1_000_122_000);
}
impl MvkMacosSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_MVK_macos_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateMacOSSurfaceMVK = extern "system" fn(
    instance: Instance,
    p_create_info: *const MacOSSurfaceCreateInfoMVK,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct MvkMacosSurfaceFn {
    pub create_mac_os_surface_mvk: extern "system" fn(
        instance: Instance,
        p_create_info: *const MacOSSurfaceCreateInfoMVK,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for MvkMacosSurfaceFn {}
unsafe impl Sync for MvkMacosSurfaceFn {}
impl ::std::clone::Clone for MvkMacosSurfaceFn {
    fn clone(&self) -> Self {
        MvkMacosSurfaceFn {
            create_mac_os_surface_mvk: self.create_mac_os_surface_mvk,
        }
    }
}
impl MvkMacosSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        MvkMacosSurfaceFn {
            create_mac_os_surface_mvk: unsafe {
                extern "system" fn create_mac_os_surface_mvk(
                    _instance: Instance,
                    _p_create_info: *const MacOSSurfaceCreateInfoMVK,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_mac_os_surface_mvk)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateMacOSSurfaceMVK\0");
                let val = _f(cname);
                if val.is_null() {
                    create_mac_os_surface_mvk
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateMacOSSurfaceMVK.html>"]
    pub unsafe fn create_mac_os_surface_mvk(
        &self,
        instance: Instance,
        p_create_info: *const MacOSSurfaceCreateInfoMVK,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_mac_os_surface_mvk)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_MVK_macos_surface'"]
impl StructureType {
    pub const MACOS_SURFACE_CREATE_INFO_MVK: Self = Self(1_000_123_000);
}
impl MvkMoltenvkFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_MVK_moltenvk\0").expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct MvkMoltenvkFn {}
unsafe impl Send for MvkMoltenvkFn {}
unsafe impl Sync for MvkMoltenvkFn {}
impl ::std::clone::Clone for MvkMoltenvkFn {
    fn clone(&self) -> Self {
        MvkMoltenvkFn {}
    }
}
impl MvkMoltenvkFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        MvkMoltenvkFn {}
    }
}
impl ExtExternalMemoryDmaBufFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_external_memory_dma_buf\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtExternalMemoryDmaBufFn {}
unsafe impl Send for ExtExternalMemoryDmaBufFn {}
unsafe impl Sync for ExtExternalMemoryDmaBufFn {}
impl ::std::clone::Clone for ExtExternalMemoryDmaBufFn {
    fn clone(&self) -> Self {
        ExtExternalMemoryDmaBufFn {}
    }
}
impl ExtExternalMemoryDmaBufFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExternalMemoryDmaBufFn {}
    }
}
#[doc = "Generated from 'VK_EXT_external_memory_dma_buf'"]
impl ExternalMemoryHandleTypeFlags {
    pub const DMA_BUF_EXT: Self = Self(0b10_0000_0000);
}
impl ExtQueueFamilyForeignFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_queue_family_foreign\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtQueueFamilyForeignFn {}
unsafe impl Send for ExtQueueFamilyForeignFn {}
unsafe impl Sync for ExtQueueFamilyForeignFn {}
impl ::std::clone::Clone for ExtQueueFamilyForeignFn {
    fn clone(&self) -> Self {
        ExtQueueFamilyForeignFn {}
    }
}
impl ExtQueueFamilyForeignFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtQueueFamilyForeignFn {}
    }
}
impl KhrDedicatedAllocationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_dedicated_allocation\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
pub struct KhrDedicatedAllocationFn {}
unsafe impl Send for KhrDedicatedAllocationFn {}
unsafe impl Sync for KhrDedicatedAllocationFn {}
impl ::std::clone::Clone for KhrDedicatedAllocationFn {
    fn clone(&self) -> Self {
        KhrDedicatedAllocationFn {}
    }
}
impl KhrDedicatedAllocationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDedicatedAllocationFn {}
    }
}
#[doc = "Generated from 'VK_KHR_dedicated_allocation'"]
impl StructureType {
    pub const MEMORY_DEDICATED_REQUIREMENTS_KHR: Self =
        StructureType::MEMORY_DEDICATED_REQUIREMENTS;
}
#[doc = "Generated from 'VK_KHR_dedicated_allocation'"]
impl StructureType {
    pub const MEMORY_DEDICATED_ALLOCATE_INFO_KHR: Self =
        StructureType::MEMORY_DEDICATED_ALLOCATE_INFO;
}
impl ExtDebugUtilsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_debug_utils\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkSetDebugUtilsObjectNameEXT =
    extern "system" fn(device: Device, p_name_info: *const DebugUtilsObjectNameInfoEXT) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkSetDebugUtilsObjectTagEXT =
    extern "system" fn(device: Device, p_tag_info: *const DebugUtilsObjectTagInfoEXT) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueueBeginDebugUtilsLabelEXT =
    extern "system" fn(queue: Queue, p_label_info: *const DebugUtilsLabelEXT);
#[allow(non_camel_case_types)]
pub type PFN_vkQueueEndDebugUtilsLabelEXT = extern "system" fn(queue: Queue);
#[allow(non_camel_case_types)]
pub type PFN_vkQueueInsertDebugUtilsLabelEXT =
    extern "system" fn(queue: Queue, p_label_info: *const DebugUtilsLabelEXT);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginDebugUtilsLabelEXT =
    extern "system" fn(command_buffer: CommandBuffer, p_label_info: *const DebugUtilsLabelEXT);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndDebugUtilsLabelEXT = extern "system" fn(command_buffer: CommandBuffer);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdInsertDebugUtilsLabelEXT =
    extern "system" fn(command_buffer: CommandBuffer, p_label_info: *const DebugUtilsLabelEXT);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDebugUtilsMessengerEXT = extern "system" fn(
    instance: Instance,
    p_create_info: *const DebugUtilsMessengerCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_messenger: *mut DebugUtilsMessengerEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDebugUtilsMessengerEXT = extern "system" fn(
    instance: Instance,
    messenger: DebugUtilsMessengerEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkSubmitDebugUtilsMessageEXT = extern "system" fn(
    instance: Instance,
    message_severity: DebugUtilsMessageSeverityFlagsEXT,
    message_types: DebugUtilsMessageTypeFlagsEXT,
    p_callback_data: *const DebugUtilsMessengerCallbackDataEXT,
);
pub struct ExtDebugUtilsFn {
    pub set_debug_utils_object_name_ext: extern "system" fn(
        device: Device,
        p_name_info: *const DebugUtilsObjectNameInfoEXT,
    ) -> Result,
    pub set_debug_utils_object_tag_ext:
        extern "system" fn(device: Device, p_tag_info: *const DebugUtilsObjectTagInfoEXT) -> Result,
    pub queue_begin_debug_utils_label_ext:
        extern "system" fn(queue: Queue, p_label_info: *const DebugUtilsLabelEXT),
    pub queue_end_debug_utils_label_ext: extern "system" fn(queue: Queue),
    pub queue_insert_debug_utils_label_ext:
        extern "system" fn(queue: Queue, p_label_info: *const DebugUtilsLabelEXT),
    pub cmd_begin_debug_utils_label_ext:
        extern "system" fn(command_buffer: CommandBuffer, p_label_info: *const DebugUtilsLabelEXT),
    pub cmd_end_debug_utils_label_ext: extern "system" fn(command_buffer: CommandBuffer),
    pub cmd_insert_debug_utils_label_ext:
        extern "system" fn(command_buffer: CommandBuffer, p_label_info: *const DebugUtilsLabelEXT),
    pub create_debug_utils_messenger_ext: extern "system" fn(
        instance: Instance,
        p_create_info: *const DebugUtilsMessengerCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_messenger: *mut DebugUtilsMessengerEXT,
    ) -> Result,
    pub destroy_debug_utils_messenger_ext: extern "system" fn(
        instance: Instance,
        messenger: DebugUtilsMessengerEXT,
        p_allocator: *const AllocationCallbacks,
    ),
    pub submit_debug_utils_message_ext: extern "system" fn(
        instance: Instance,
        message_severity: DebugUtilsMessageSeverityFlagsEXT,
        message_types: DebugUtilsMessageTypeFlagsEXT,
        p_callback_data: *const DebugUtilsMessengerCallbackDataEXT,
    ),
}
unsafe impl Send for ExtDebugUtilsFn {}
unsafe impl Sync for ExtDebugUtilsFn {}
impl ::std::clone::Clone for ExtDebugUtilsFn {
    fn clone(&self) -> Self {
        ExtDebugUtilsFn {
            set_debug_utils_object_name_ext: self.set_debug_utils_object_name_ext,
            set_debug_utils_object_tag_ext: self.set_debug_utils_object_tag_ext,
            queue_begin_debug_utils_label_ext: self.queue_begin_debug_utils_label_ext,
            queue_end_debug_utils_label_ext: self.queue_end_debug_utils_label_ext,
            queue_insert_debug_utils_label_ext: self.queue_insert_debug_utils_label_ext,
            cmd_begin_debug_utils_label_ext: self.cmd_begin_debug_utils_label_ext,
            cmd_end_debug_utils_label_ext: self.cmd_end_debug_utils_label_ext,
            cmd_insert_debug_utils_label_ext: self.cmd_insert_debug_utils_label_ext,
            create_debug_utils_messenger_ext: self.create_debug_utils_messenger_ext,
            destroy_debug_utils_messenger_ext: self.destroy_debug_utils_messenger_ext,
            submit_debug_utils_message_ext: self.submit_debug_utils_message_ext,
        }
    }
}
impl ExtDebugUtilsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDebugUtilsFn {
            set_debug_utils_object_name_ext: unsafe {
                extern "system" fn set_debug_utils_object_name_ext(
                    _device: Device,
                    _p_name_info: *const DebugUtilsObjectNameInfoEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(set_debug_utils_object_name_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkSetDebugUtilsObjectNameEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    set_debug_utils_object_name_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            set_debug_utils_object_tag_ext: unsafe {
                extern "system" fn set_debug_utils_object_tag_ext(
                    _device: Device,
                    _p_tag_info: *const DebugUtilsObjectTagInfoEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(set_debug_utils_object_tag_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkSetDebugUtilsObjectTagEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    set_debug_utils_object_tag_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_begin_debug_utils_label_ext: unsafe {
                extern "system" fn queue_begin_debug_utils_label_ext(
                    _queue: Queue,
                    _p_label_info: *const DebugUtilsLabelEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(queue_begin_debug_utils_label_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkQueueBeginDebugUtilsLabelEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    queue_begin_debug_utils_label_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_end_debug_utils_label_ext: unsafe {
                extern "system" fn queue_end_debug_utils_label_ext(_queue: Queue) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(queue_end_debug_utils_label_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkQueueEndDebugUtilsLabelEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    queue_end_debug_utils_label_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_insert_debug_utils_label_ext: unsafe {
                extern "system" fn queue_insert_debug_utils_label_ext(
                    _queue: Queue,
                    _p_label_info: *const DebugUtilsLabelEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(queue_insert_debug_utils_label_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkQueueInsertDebugUtilsLabelEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    queue_insert_debug_utils_label_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_debug_utils_label_ext: unsafe {
                extern "system" fn cmd_begin_debug_utils_label_ext(
                    _command_buffer: CommandBuffer,
                    _p_label_info: *const DebugUtilsLabelEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_debug_utils_label_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBeginDebugUtilsLabelEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_debug_utils_label_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_debug_utils_label_ext: unsafe {
                extern "system" fn cmd_end_debug_utils_label_ext(_command_buffer: CommandBuffer) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_debug_utils_label_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdEndDebugUtilsLabelEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_debug_utils_label_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_insert_debug_utils_label_ext: unsafe {
                extern "system" fn cmd_insert_debug_utils_label_ext(
                    _command_buffer: CommandBuffer,
                    _p_label_info: *const DebugUtilsLabelEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_insert_debug_utils_label_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdInsertDebugUtilsLabelEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_insert_debug_utils_label_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_debug_utils_messenger_ext: unsafe {
                extern "system" fn create_debug_utils_messenger_ext(
                    _instance: Instance,
                    _p_create_info: *const DebugUtilsMessengerCreateInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_messenger: *mut DebugUtilsMessengerEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_debug_utils_messenger_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDebugUtilsMessengerEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_debug_utils_messenger_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_debug_utils_messenger_ext: unsafe {
                extern "system" fn destroy_debug_utils_messenger_ext(
                    _instance: Instance,
                    _messenger: DebugUtilsMessengerEXT,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_debug_utils_messenger_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyDebugUtilsMessengerEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_debug_utils_messenger_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            submit_debug_utils_message_ext: unsafe {
                extern "system" fn submit_debug_utils_message_ext(
                    _instance: Instance,
                    _message_severity: DebugUtilsMessageSeverityFlagsEXT,
                    _message_types: DebugUtilsMessageTypeFlagsEXT,
                    _p_callback_data: *const DebugUtilsMessengerCallbackDataEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(submit_debug_utils_message_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkSubmitDebugUtilsMessageEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    submit_debug_utils_message_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetDebugUtilsObjectNameEXT.html>"]
    pub unsafe fn set_debug_utils_object_name_ext(
        &self,
        device: Device,
        p_name_info: *const DebugUtilsObjectNameInfoEXT,
    ) -> Result {
        (self.set_debug_utils_object_name_ext)(device, p_name_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetDebugUtilsObjectTagEXT.html>"]
    pub unsafe fn set_debug_utils_object_tag_ext(
        &self,
        device: Device,
        p_tag_info: *const DebugUtilsObjectTagInfoEXT,
    ) -> Result {
        (self.set_debug_utils_object_tag_ext)(device, p_tag_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueBeginDebugUtilsLabelEXT.html>"]
    pub unsafe fn queue_begin_debug_utils_label_ext(
        &self,
        queue: Queue,
        p_label_info: *const DebugUtilsLabelEXT,
    ) {
        (self.queue_begin_debug_utils_label_ext)(queue, p_label_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueEndDebugUtilsLabelEXT.html>"]
    pub unsafe fn queue_end_debug_utils_label_ext(&self, queue: Queue) {
        (self.queue_end_debug_utils_label_ext)(queue)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueInsertDebugUtilsLabelEXT.html>"]
    pub unsafe fn queue_insert_debug_utils_label_ext(
        &self,
        queue: Queue,
        p_label_info: *const DebugUtilsLabelEXT,
    ) {
        (self.queue_insert_debug_utils_label_ext)(queue, p_label_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginDebugUtilsLabelEXT.html>"]
    pub unsafe fn cmd_begin_debug_utils_label_ext(
        &self,
        command_buffer: CommandBuffer,
        p_label_info: *const DebugUtilsLabelEXT,
    ) {
        (self.cmd_begin_debug_utils_label_ext)(command_buffer, p_label_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndDebugUtilsLabelEXT.html>"]
    pub unsafe fn cmd_end_debug_utils_label_ext(&self, command_buffer: CommandBuffer) {
        (self.cmd_end_debug_utils_label_ext)(command_buffer)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdInsertDebugUtilsLabelEXT.html>"]
    pub unsafe fn cmd_insert_debug_utils_label_ext(
        &self,
        command_buffer: CommandBuffer,
        p_label_info: *const DebugUtilsLabelEXT,
    ) {
        (self.cmd_insert_debug_utils_label_ext)(command_buffer, p_label_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDebugUtilsMessengerEXT.html>"]
    pub unsafe fn create_debug_utils_messenger_ext(
        &self,
        instance: Instance,
        p_create_info: *const DebugUtilsMessengerCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_messenger: *mut DebugUtilsMessengerEXT,
    ) -> Result {
        (self.create_debug_utils_messenger_ext)(instance, p_create_info, p_allocator, p_messenger)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDebugUtilsMessengerEXT.html>"]
    pub unsafe fn destroy_debug_utils_messenger_ext(
        &self,
        instance: Instance,
        messenger: DebugUtilsMessengerEXT,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_debug_utils_messenger_ext)(instance, messenger, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSubmitDebugUtilsMessageEXT.html>"]
    pub unsafe fn submit_debug_utils_message_ext(
        &self,
        instance: Instance,
        message_severity: DebugUtilsMessageSeverityFlagsEXT,
        message_types: DebugUtilsMessageTypeFlagsEXT,
        p_callback_data: *const DebugUtilsMessengerCallbackDataEXT,
    ) {
        (self.submit_debug_utils_message_ext)(
            instance,
            message_severity,
            message_types,
            p_callback_data,
        )
    }
}
#[doc = "Generated from 'VK_EXT_debug_utils'"]
impl StructureType {
    pub const DEBUG_UTILS_OBJECT_NAME_INFO_EXT: Self = Self(1_000_128_000);
}
#[doc = "Generated from 'VK_EXT_debug_utils'"]
impl StructureType {
    pub const DEBUG_UTILS_OBJECT_TAG_INFO_EXT: Self = Self(1_000_128_001);
}
#[doc = "Generated from 'VK_EXT_debug_utils'"]
impl StructureType {
    pub const DEBUG_UTILS_LABEL_EXT: Self = Self(1_000_128_002);
}
#[doc = "Generated from 'VK_EXT_debug_utils'"]
impl StructureType {
    pub const DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT: Self = Self(1_000_128_003);
}
#[doc = "Generated from 'VK_EXT_debug_utils'"]
impl StructureType {
    pub const DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT: Self = Self(1_000_128_004);
}
#[doc = "Generated from 'VK_EXT_debug_utils'"]
impl ObjectType {
    pub const DEBUG_UTILS_MESSENGER_EXT: Self = Self(1_000_128_000);
}
impl AndroidExternalMemoryAndroidHardwareBufferFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(
            b"VK_ANDROID_external_memory_android_hardware_buffer\0",
        )
        .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetAndroidHardwareBufferPropertiesANDROID = extern "system" fn(
    device: Device,
    buffer: *const AHardwareBuffer,
    p_properties: *mut AndroidHardwareBufferPropertiesANDROID,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryAndroidHardwareBufferANDROID = extern "system" fn(
    device: Device,
    p_info: *const MemoryGetAndroidHardwareBufferInfoANDROID,
    p_buffer: *mut *mut AHardwareBuffer,
) -> Result;
pub struct AndroidExternalMemoryAndroidHardwareBufferFn {
    pub get_android_hardware_buffer_properties_android: extern "system" fn(
        device: Device,
        buffer: *const AHardwareBuffer,
        p_properties: *mut AndroidHardwareBufferPropertiesANDROID,
    ) -> Result,
    pub get_memory_android_hardware_buffer_android: extern "system" fn(
        device: Device,
        p_info: *const MemoryGetAndroidHardwareBufferInfoANDROID,
        p_buffer: *mut *mut AHardwareBuffer,
    ) -> Result,
}
unsafe impl Send for AndroidExternalMemoryAndroidHardwareBufferFn {}
unsafe impl Sync for AndroidExternalMemoryAndroidHardwareBufferFn {}
impl ::std::clone::Clone for AndroidExternalMemoryAndroidHardwareBufferFn {
    fn clone(&self) -> Self {
        AndroidExternalMemoryAndroidHardwareBufferFn {
            get_android_hardware_buffer_properties_android: self
                .get_android_hardware_buffer_properties_android,
            get_memory_android_hardware_buffer_android: self
                .get_memory_android_hardware_buffer_android,
        }
    }
}
impl AndroidExternalMemoryAndroidHardwareBufferFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AndroidExternalMemoryAndroidHardwareBufferFn {
            get_android_hardware_buffer_properties_android: unsafe {
                extern "system" fn get_android_hardware_buffer_properties_android(
                    _device: Device,
                    _buffer: *const AHardwareBuffer,
                    _p_properties: *mut AndroidHardwareBufferPropertiesANDROID,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_android_hardware_buffer_properties_android)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetAndroidHardwareBufferPropertiesANDROID\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_android_hardware_buffer_properties_android
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_memory_android_hardware_buffer_android: unsafe {
                extern "system" fn get_memory_android_hardware_buffer_android(
                    _device: Device,
                    _p_info: *const MemoryGetAndroidHardwareBufferInfoANDROID,
                    _p_buffer: *mut *mut AHardwareBuffer,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_android_hardware_buffer_android)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetMemoryAndroidHardwareBufferANDROID\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_memory_android_hardware_buffer_android
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAndroidHardwareBufferPropertiesANDROID.html>"]
    pub unsafe fn get_android_hardware_buffer_properties_android(
        &self,
        device: Device,
        buffer: *const AHardwareBuffer,
        p_properties: *mut AndroidHardwareBufferPropertiesANDROID,
    ) -> Result {
        (self.get_android_hardware_buffer_properties_android)(device, buffer, p_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryAndroidHardwareBufferANDROID.html>"]
    pub unsafe fn get_memory_android_hardware_buffer_android(
        &self,
        device: Device,
        p_info: *const MemoryGetAndroidHardwareBufferInfoANDROID,
        p_buffer: *mut *mut AHardwareBuffer,
    ) -> Result {
        (self.get_memory_android_hardware_buffer_android)(device, p_info, p_buffer)
    }
}
#[doc = "Generated from 'VK_ANDROID_external_memory_android_hardware_buffer'"]
impl ExternalMemoryHandleTypeFlags {
    pub const ANDROID_HARDWARE_BUFFER_ANDROID: Self = Self(0b100_0000_0000);
}
#[doc = "Generated from 'VK_ANDROID_external_memory_android_hardware_buffer'"]
impl StructureType {
    pub const ANDROID_HARDWARE_BUFFER_USAGE_ANDROID: Self = Self(1_000_129_000);
}
#[doc = "Generated from 'VK_ANDROID_external_memory_android_hardware_buffer'"]
impl StructureType {
    pub const ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID: Self = Self(1_000_129_001);
}
#[doc = "Generated from 'VK_ANDROID_external_memory_android_hardware_buffer'"]
impl StructureType {
    pub const ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID: Self = Self(1_000_129_002);
}
#[doc = "Generated from 'VK_ANDROID_external_memory_android_hardware_buffer'"]
impl StructureType {
    pub const IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID: Self = Self(1_000_129_003);
}
#[doc = "Generated from 'VK_ANDROID_external_memory_android_hardware_buffer'"]
impl StructureType {
    pub const MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID: Self = Self(1_000_129_004);
}
#[doc = "Generated from 'VK_ANDROID_external_memory_android_hardware_buffer'"]
impl StructureType {
    pub const EXTERNAL_FORMAT_ANDROID: Self = Self(1_000_129_005);
}
impl ExtSamplerFilterMinmaxFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_sampler_filter_minmax\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtSamplerFilterMinmaxFn {}
unsafe impl Send for ExtSamplerFilterMinmaxFn {}
unsafe impl Sync for ExtSamplerFilterMinmaxFn {}
impl ::std::clone::Clone for ExtSamplerFilterMinmaxFn {
    fn clone(&self) -> Self {
        ExtSamplerFilterMinmaxFn {}
    }
}
impl ExtSamplerFilterMinmaxFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtSamplerFilterMinmaxFn {}
    }
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT: Self =
        StructureType::PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl StructureType {
    pub const SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT: Self =
        StructureType::SAMPLER_REDUCTION_MODE_CREATE_INFO;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_FILTER_MINMAX_EXT: Self =
        FormatFeatureFlags::SAMPLED_IMAGE_FILTER_MINMAX;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl SamplerReductionMode {
    pub const WEIGHTED_AVERAGE_EXT: Self = SamplerReductionMode::WEIGHTED_AVERAGE;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl SamplerReductionMode {
    pub const MIN_EXT: Self = SamplerReductionMode::MIN;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl SamplerReductionMode {
    pub const MAX_EXT: Self = SamplerReductionMode::MAX;
}
impl KhrStorageBufferStorageClassFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_storage_buffer_storage_class\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrStorageBufferStorageClassFn {}
unsafe impl Send for KhrStorageBufferStorageClassFn {}
unsafe impl Sync for KhrStorageBufferStorageClassFn {}
impl ::std::clone::Clone for KhrStorageBufferStorageClassFn {
    fn clone(&self) -> Self {
        KhrStorageBufferStorageClassFn {}
    }
}
impl KhrStorageBufferStorageClassFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrStorageBufferStorageClassFn {}
    }
}
impl AmdGpuShaderInt16Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_gpu_shader_int16\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct AmdGpuShaderInt16Fn {}
unsafe impl Send for AmdGpuShaderInt16Fn {}
unsafe impl Sync for AmdGpuShaderInt16Fn {}
impl ::std::clone::Clone for AmdGpuShaderInt16Fn {
    fn clone(&self) -> Self {
        AmdGpuShaderInt16Fn {}
    }
}
impl AmdGpuShaderInt16Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdGpuShaderInt16Fn {}
    }
}
impl AmdExtension134Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_134\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension134Fn {}
unsafe impl Send for AmdExtension134Fn {}
unsafe impl Sync for AmdExtension134Fn {}
impl ::std::clone::Clone for AmdExtension134Fn {
    fn clone(&self) -> Self {
        AmdExtension134Fn {}
    }
}
impl AmdExtension134Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension134Fn {}
    }
}
impl AmdExtension135Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_135\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension135Fn {}
unsafe impl Send for AmdExtension135Fn {}
unsafe impl Sync for AmdExtension135Fn {}
impl ::std::clone::Clone for AmdExtension135Fn {
    fn clone(&self) -> Self {
        AmdExtension135Fn {}
    }
}
impl AmdExtension135Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension135Fn {}
    }
}
impl AmdExtension136Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_136\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension136Fn {}
unsafe impl Send for AmdExtension136Fn {}
unsafe impl Sync for AmdExtension136Fn {}
impl ::std::clone::Clone for AmdExtension136Fn {
    fn clone(&self) -> Self {
        AmdExtension136Fn {}
    }
}
impl AmdExtension136Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension136Fn {}
    }
}
impl AmdMixedAttachmentSamplesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_mixed_attachment_samples\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdMixedAttachmentSamplesFn {}
unsafe impl Send for AmdMixedAttachmentSamplesFn {}
unsafe impl Sync for AmdMixedAttachmentSamplesFn {}
impl ::std::clone::Clone for AmdMixedAttachmentSamplesFn {
    fn clone(&self) -> Self {
        AmdMixedAttachmentSamplesFn {}
    }
}
impl AmdMixedAttachmentSamplesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdMixedAttachmentSamplesFn {}
    }
}
impl AmdShaderFragmentMaskFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_fragment_mask\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdShaderFragmentMaskFn {}
unsafe impl Send for AmdShaderFragmentMaskFn {}
unsafe impl Sync for AmdShaderFragmentMaskFn {}
impl ::std::clone::Clone for AmdShaderFragmentMaskFn {
    fn clone(&self) -> Self {
        AmdShaderFragmentMaskFn {}
    }
}
impl AmdShaderFragmentMaskFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderFragmentMaskFn {}
    }
}
impl ExtInlineUniformBlockFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_inline_uniform_block\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtInlineUniformBlockFn {}
unsafe impl Send for ExtInlineUniformBlockFn {}
unsafe impl Sync for ExtInlineUniformBlockFn {}
impl ::std::clone::Clone for ExtInlineUniformBlockFn {
    fn clone(&self) -> Self {
        ExtInlineUniformBlockFn {}
    }
}
impl ExtInlineUniformBlockFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtInlineUniformBlockFn {}
    }
}
#[doc = "Generated from 'VK_EXT_inline_uniform_block'"]
impl DescriptorType {
    pub const INLINE_UNIFORM_BLOCK_EXT: Self = Self(1_000_138_000);
}
#[doc = "Generated from 'VK_EXT_inline_uniform_block'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT: Self = Self(1_000_138_000);
}
#[doc = "Generated from 'VK_EXT_inline_uniform_block'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT: Self = Self(1_000_138_001);
}
#[doc = "Generated from 'VK_EXT_inline_uniform_block'"]
impl StructureType {
    pub const WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT: Self = Self(1_000_138_002);
}
#[doc = "Generated from 'VK_EXT_inline_uniform_block'"]
impl StructureType {
    pub const DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT: Self = Self(1_000_138_003);
}
impl AmdExtension140Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_140\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension140Fn {}
unsafe impl Send for AmdExtension140Fn {}
unsafe impl Sync for AmdExtension140Fn {}
impl ::std::clone::Clone for AmdExtension140Fn {
    fn clone(&self) -> Self {
        AmdExtension140Fn {}
    }
}
impl AmdExtension140Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension140Fn {}
    }
}
impl ExtShaderStencilExportFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_stencil_export\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtShaderStencilExportFn {}
unsafe impl Send for ExtShaderStencilExportFn {}
unsafe impl Sync for ExtShaderStencilExportFn {}
impl ::std::clone::Clone for ExtShaderStencilExportFn {
    fn clone(&self) -> Self {
        ExtShaderStencilExportFn {}
    }
}
impl ExtShaderStencilExportFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderStencilExportFn {}
    }
}
impl AmdExtension142Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_142\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension142Fn {}
unsafe impl Send for AmdExtension142Fn {}
unsafe impl Sync for AmdExtension142Fn {}
impl ::std::clone::Clone for AmdExtension142Fn {
    fn clone(&self) -> Self {
        AmdExtension142Fn {}
    }
}
impl AmdExtension142Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension142Fn {}
    }
}
impl AmdExtension143Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_143\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension143Fn {}
unsafe impl Send for AmdExtension143Fn {}
unsafe impl Sync for AmdExtension143Fn {}
impl ::std::clone::Clone for AmdExtension143Fn {
    fn clone(&self) -> Self {
        AmdExtension143Fn {}
    }
}
impl AmdExtension143Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension143Fn {}
    }
}
impl ExtSampleLocationsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_sample_locations\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetSampleLocationsEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    p_sample_locations_info: *const SampleLocationsInfoEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT = extern "system" fn(
    physical_device: PhysicalDevice,
    samples: SampleCountFlags,
    p_multisample_properties: *mut MultisamplePropertiesEXT,
);
pub struct ExtSampleLocationsFn {
    pub cmd_set_sample_locations_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        p_sample_locations_info: *const SampleLocationsInfoEXT,
    ),
    pub get_physical_device_multisample_properties_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        samples: SampleCountFlags,
        p_multisample_properties: *mut MultisamplePropertiesEXT,
    ),
}
unsafe impl Send for ExtSampleLocationsFn {}
unsafe impl Sync for ExtSampleLocationsFn {}
impl ::std::clone::Clone for ExtSampleLocationsFn {
    fn clone(&self) -> Self {
        ExtSampleLocationsFn {
            cmd_set_sample_locations_ext: self.cmd_set_sample_locations_ext,
            get_physical_device_multisample_properties_ext: self
                .get_physical_device_multisample_properties_ext,
        }
    }
}
impl ExtSampleLocationsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtSampleLocationsFn {
            cmd_set_sample_locations_ext: unsafe {
                extern "system" fn cmd_set_sample_locations_ext(
                    _command_buffer: CommandBuffer,
                    _p_sample_locations_info: *const SampleLocationsInfoEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_sample_locations_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetSampleLocationsEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_sample_locations_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_multisample_properties_ext: unsafe {
                extern "system" fn get_physical_device_multisample_properties_ext(
                    _physical_device: PhysicalDevice,
                    _samples: SampleCountFlags,
                    _p_multisample_properties: *mut MultisamplePropertiesEXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_multisample_properties_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceMultisamplePropertiesEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_multisample_properties_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetSampleLocationsEXT.html>"]
    pub unsafe fn cmd_set_sample_locations_ext(
        &self,
        command_buffer: CommandBuffer,
        p_sample_locations_info: *const SampleLocationsInfoEXT,
    ) {
        (self.cmd_set_sample_locations_ext)(command_buffer, p_sample_locations_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceMultisamplePropertiesEXT.html>"]
    pub unsafe fn get_physical_device_multisample_properties_ext(
        &self,
        physical_device: PhysicalDevice,
        samples: SampleCountFlags,
        p_multisample_properties: *mut MultisamplePropertiesEXT,
    ) {
        (self.get_physical_device_multisample_properties_ext)(
            physical_device,
            samples,
            p_multisample_properties,
        )
    }
}
#[doc = "Generated from 'VK_EXT_sample_locations'"]
impl ImageCreateFlags {
    pub const SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_EXT: Self = Self(0b1_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_sample_locations'"]
impl StructureType {
    pub const SAMPLE_LOCATIONS_INFO_EXT: Self = Self(1_000_143_000);
}
#[doc = "Generated from 'VK_EXT_sample_locations'"]
impl StructureType {
    pub const RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT: Self = Self(1_000_143_001);
}
#[doc = "Generated from 'VK_EXT_sample_locations'"]
impl StructureType {
    pub const PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT: Self = Self(1_000_143_002);
}
#[doc = "Generated from 'VK_EXT_sample_locations'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT: Self = Self(1_000_143_003);
}
#[doc = "Generated from 'VK_EXT_sample_locations'"]
impl StructureType {
    pub const MULTISAMPLE_PROPERTIES_EXT: Self = Self(1_000_143_004);
}
#[doc = "Generated from 'VK_EXT_sample_locations'"]
impl DynamicState {
    pub const SAMPLE_LOCATIONS_EXT: Self = Self(1_000_143_000);
}
impl KhrRelaxedBlockLayoutFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_relaxed_block_layout\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrRelaxedBlockLayoutFn {}
unsafe impl Send for KhrRelaxedBlockLayoutFn {}
unsafe impl Sync for KhrRelaxedBlockLayoutFn {}
impl ::std::clone::Clone for KhrRelaxedBlockLayoutFn {
    fn clone(&self) -> Self {
        KhrRelaxedBlockLayoutFn {}
    }
}
impl KhrRelaxedBlockLayoutFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrRelaxedBlockLayoutFn {}
    }
}
impl KhrGetMemoryRequirements2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_get_memory_requirements2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageMemoryRequirements2 = extern "system" fn(
    device: Device,
    p_info: *const ImageMemoryRequirementsInfo2,
    p_memory_requirements: *mut MemoryRequirements2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetBufferMemoryRequirements2 = extern "system" fn(
    device: Device,
    p_info: *const BufferMemoryRequirementsInfo2,
    p_memory_requirements: *mut MemoryRequirements2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageSparseMemoryRequirements2 = extern "system" fn(
    device: Device,
    p_info: *const ImageSparseMemoryRequirementsInfo2,
    p_sparse_memory_requirement_count: *mut u32,
    p_sparse_memory_requirements: *mut SparseImageMemoryRequirements2,
);
pub struct KhrGetMemoryRequirements2Fn {
    pub get_image_memory_requirements2_khr: extern "system" fn(
        device: Device,
        p_info: *const ImageMemoryRequirementsInfo2,
        p_memory_requirements: *mut MemoryRequirements2,
    ),
    pub get_buffer_memory_requirements2_khr: extern "system" fn(
        device: Device,
        p_info: *const BufferMemoryRequirementsInfo2,
        p_memory_requirements: *mut MemoryRequirements2,
    ),
    pub get_image_sparse_memory_requirements2_khr: extern "system" fn(
        device: Device,
        p_info: *const ImageSparseMemoryRequirementsInfo2,
        p_sparse_memory_requirement_count: *mut u32,
        p_sparse_memory_requirements: *mut SparseImageMemoryRequirements2,
    ),
}
unsafe impl Send for KhrGetMemoryRequirements2Fn {}
unsafe impl Sync for KhrGetMemoryRequirements2Fn {}
impl ::std::clone::Clone for KhrGetMemoryRequirements2Fn {
    fn clone(&self) -> Self {
        KhrGetMemoryRequirements2Fn {
            get_image_memory_requirements2_khr: self.get_image_memory_requirements2_khr,
            get_buffer_memory_requirements2_khr: self.get_buffer_memory_requirements2_khr,
            get_image_sparse_memory_requirements2_khr: self
                .get_image_sparse_memory_requirements2_khr,
        }
    }
}
impl KhrGetMemoryRequirements2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetMemoryRequirements2Fn {
            get_image_memory_requirements2_khr: unsafe {
                extern "system" fn get_image_memory_requirements2_khr(
                    _device: Device,
                    _p_info: *const ImageMemoryRequirementsInfo2,
                    _p_memory_requirements: *mut MemoryRequirements2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_memory_requirements2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageMemoryRequirements2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_memory_requirements2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_buffer_memory_requirements2_khr: unsafe {
                extern "system" fn get_buffer_memory_requirements2_khr(
                    _device: Device,
                    _p_info: *const BufferMemoryRequirementsInfo2,
                    _p_memory_requirements: *mut MemoryRequirements2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_memory_requirements2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetBufferMemoryRequirements2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_memory_requirements2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_image_sparse_memory_requirements2_khr: unsafe {
                extern "system" fn get_image_sparse_memory_requirements2_khr(
                    _device: Device,
                    _p_info: *const ImageSparseMemoryRequirementsInfo2,
                    _p_sparse_memory_requirement_count: *mut u32,
                    _p_sparse_memory_requirements: *mut SparseImageMemoryRequirements2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_sparse_memory_requirements2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageSparseMemoryRequirements2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_sparse_memory_requirements2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageMemoryRequirements2KHR.html>"]
    pub unsafe fn get_image_memory_requirements2_khr(
        &self,
        device: Device,
        p_info: *const ImageMemoryRequirementsInfo2,
        p_memory_requirements: *mut MemoryRequirements2,
    ) {
        (self.get_image_memory_requirements2_khr)(device, p_info, p_memory_requirements)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferMemoryRequirements2KHR.html>"]
    pub unsafe fn get_buffer_memory_requirements2_khr(
        &self,
        device: Device,
        p_info: *const BufferMemoryRequirementsInfo2,
        p_memory_requirements: *mut MemoryRequirements2,
    ) {
        (self.get_buffer_memory_requirements2_khr)(device, p_info, p_memory_requirements)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageSparseMemoryRequirements2KHR.html>"]
    pub unsafe fn get_image_sparse_memory_requirements2_khr(
        &self,
        device: Device,
        p_info: *const ImageSparseMemoryRequirementsInfo2,
        p_sparse_memory_requirement_count: *mut u32,
        p_sparse_memory_requirements: *mut SparseImageMemoryRequirements2,
    ) {
        (self.get_image_sparse_memory_requirements2_khr)(
            device,
            p_info,
            p_sparse_memory_requirement_count,
            p_sparse_memory_requirements,
        )
    }
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR: Self =
        StructureType::BUFFER_MEMORY_REQUIREMENTS_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR: Self =
        StructureType::IMAGE_MEMORY_REQUIREMENTS_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2_KHR: Self =
        StructureType::IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const MEMORY_REQUIREMENTS_2_KHR: Self = StructureType::MEMORY_REQUIREMENTS_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const SPARSE_IMAGE_MEMORY_REQUIREMENTS_2_KHR: Self =
        StructureType::SPARSE_IMAGE_MEMORY_REQUIREMENTS_2;
}
impl KhrImageFormatListFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_image_format_list\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrImageFormatListFn {}
unsafe impl Send for KhrImageFormatListFn {}
unsafe impl Sync for KhrImageFormatListFn {}
impl ::std::clone::Clone for KhrImageFormatListFn {
    fn clone(&self) -> Self {
        KhrImageFormatListFn {}
    }
}
impl KhrImageFormatListFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrImageFormatListFn {}
    }
}
#[doc = "Generated from 'VK_KHR_image_format_list'"]
impl StructureType {
    pub const IMAGE_FORMAT_LIST_CREATE_INFO_KHR: Self =
        StructureType::IMAGE_FORMAT_LIST_CREATE_INFO;
}
impl ExtBlendOperationAdvancedFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_blend_operation_advanced\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtBlendOperationAdvancedFn {}
unsafe impl Send for ExtBlendOperationAdvancedFn {}
unsafe impl Sync for ExtBlendOperationAdvancedFn {}
impl ::std::clone::Clone for ExtBlendOperationAdvancedFn {
    fn clone(&self) -> Self {
        ExtBlendOperationAdvancedFn {}
    }
}
impl ExtBlendOperationAdvancedFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtBlendOperationAdvancedFn {}
    }
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT: Self = Self(1_000_148_000);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT: Self = Self(1_000_148_001);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl StructureType {
    pub const PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT: Self = Self(1_000_148_002);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const ZERO_EXT: Self = Self(1_000_148_000);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const SRC_EXT: Self = Self(1_000_148_001);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const DST_EXT: Self = Self(1_000_148_002);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const SRC_OVER_EXT: Self = Self(1_000_148_003);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const DST_OVER_EXT: Self = Self(1_000_148_004);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const SRC_IN_EXT: Self = Self(1_000_148_005);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const DST_IN_EXT: Self = Self(1_000_148_006);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const SRC_OUT_EXT: Self = Self(1_000_148_007);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const DST_OUT_EXT: Self = Self(1_000_148_008);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const SRC_ATOP_EXT: Self = Self(1_000_148_009);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const DST_ATOP_EXT: Self = Self(1_000_148_010);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const XOR_EXT: Self = Self(1_000_148_011);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const MULTIPLY_EXT: Self = Self(1_000_148_012);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const SCREEN_EXT: Self = Self(1_000_148_013);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const OVERLAY_EXT: Self = Self(1_000_148_014);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const DARKEN_EXT: Self = Self(1_000_148_015);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const LIGHTEN_EXT: Self = Self(1_000_148_016);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const COLORDODGE_EXT: Self = Self(1_000_148_017);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const COLORBURN_EXT: Self = Self(1_000_148_018);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const HARDLIGHT_EXT: Self = Self(1_000_148_019);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const SOFTLIGHT_EXT: Self = Self(1_000_148_020);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const DIFFERENCE_EXT: Self = Self(1_000_148_021);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const EXCLUSION_EXT: Self = Self(1_000_148_022);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const INVERT_EXT: Self = Self(1_000_148_023);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const INVERT_RGB_EXT: Self = Self(1_000_148_024);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const LINEARDODGE_EXT: Self = Self(1_000_148_025);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const LINEARBURN_EXT: Self = Self(1_000_148_026);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const VIVIDLIGHT_EXT: Self = Self(1_000_148_027);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const LINEARLIGHT_EXT: Self = Self(1_000_148_028);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const PINLIGHT_EXT: Self = Self(1_000_148_029);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const HARDMIX_EXT: Self = Self(1_000_148_030);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const HSL_HUE_EXT: Self = Self(1_000_148_031);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const HSL_SATURATION_EXT: Self = Self(1_000_148_032);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const HSL_COLOR_EXT: Self = Self(1_000_148_033);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const HSL_LUMINOSITY_EXT: Self = Self(1_000_148_034);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const PLUS_EXT: Self = Self(1_000_148_035);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const PLUS_CLAMPED_EXT: Self = Self(1_000_148_036);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const PLUS_CLAMPED_ALPHA_EXT: Self = Self(1_000_148_037);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const PLUS_DARKER_EXT: Self = Self(1_000_148_038);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const MINUS_EXT: Self = Self(1_000_148_039);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const MINUS_CLAMPED_EXT: Self = Self(1_000_148_040);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const CONTRAST_EXT: Self = Self(1_000_148_041);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const INVERT_OVG_EXT: Self = Self(1_000_148_042);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const RED_EXT: Self = Self(1_000_148_043);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const GREEN_EXT: Self = Self(1_000_148_044);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl BlendOp {
    pub const BLUE_EXT: Self = Self(1_000_148_045);
}
#[doc = "Generated from 'VK_EXT_blend_operation_advanced'"]
impl AccessFlags {
    pub const COLOR_ATTACHMENT_READ_NONCOHERENT_EXT: Self = Self(0b1000_0000_0000_0000_0000);
}
impl NvFragmentCoverageToColorFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_fragment_coverage_to_color\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvFragmentCoverageToColorFn {}
unsafe impl Send for NvFragmentCoverageToColorFn {}
unsafe impl Sync for NvFragmentCoverageToColorFn {}
impl ::std::clone::Clone for NvFragmentCoverageToColorFn {
    fn clone(&self) -> Self {
        NvFragmentCoverageToColorFn {}
    }
}
impl NvFragmentCoverageToColorFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvFragmentCoverageToColorFn {}
    }
}
#[doc = "Generated from 'VK_NV_fragment_coverage_to_color'"]
impl StructureType {
    pub const PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV: Self = Self(1_000_149_000);
}
impl KhrAccelerationStructureFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_acceleration_structure\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 11u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateAccelerationStructureKHR = extern "system" fn(
    device: Device,
    p_create_info: *const AccelerationStructureCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_acceleration_structure: *mut AccelerationStructureKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyAccelerationStructureKHR = extern "system" fn(
    device: Device,
    acceleration_structure: AccelerationStructureKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBuildAccelerationStructuresKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    info_count: u32,
    p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
    pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBuildAccelerationStructuresIndirectKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    info_count: u32,
    p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
    p_indirect_device_addresses: *const DeviceAddress,
    p_indirect_strides: *const u32,
    pp_max_primitive_counts: *const *const u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkBuildAccelerationStructuresKHR = extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    info_count: u32,
    p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
    pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCopyAccelerationStructureKHR = extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    p_info: *const CopyAccelerationStructureInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCopyAccelerationStructureToMemoryKHR = extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCopyMemoryToAccelerationStructureKHR = extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkWriteAccelerationStructuresPropertiesKHR = extern "system" fn(
    device: Device,
    acceleration_structure_count: u32,
    p_acceleration_structures: *const AccelerationStructureKHR,
    query_type: QueryType,
    data_size: usize,
    p_data: *mut c_void,
    stride: usize,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyAccelerationStructureKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_info: *const CopyAccelerationStructureInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyAccelerationStructureToMemoryKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyMemoryToAccelerationStructureKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureDeviceAddressKHR = extern "system" fn(
    device: Device,
    p_info: *const AccelerationStructureDeviceAddressInfoKHR,
) -> DeviceAddress;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteAccelerationStructuresPropertiesKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    acceleration_structure_count: u32,
    p_acceleration_structures: *const AccelerationStructureKHR,
    query_type: QueryType,
    query_pool: QueryPool,
    first_query: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceAccelerationStructureCompatibilityKHR = extern "system" fn(
    device: Device,
    p_version_info: *const AccelerationStructureVersionInfoKHR,
    p_compatibility: *mut AccelerationStructureCompatibilityKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureBuildSizesKHR = extern "system" fn(
    device: Device,
    build_type: AccelerationStructureBuildTypeKHR,
    p_build_info: *const AccelerationStructureBuildGeometryInfoKHR,
    p_max_primitive_counts: *const u32,
    p_size_info: *mut AccelerationStructureBuildSizesInfoKHR,
);
pub struct KhrAccelerationStructureFn {
    pub create_acceleration_structure_khr: extern "system" fn(
        device: Device,
        p_create_info: *const AccelerationStructureCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_acceleration_structure: *mut AccelerationStructureKHR,
    ) -> Result,
    pub destroy_acceleration_structure_khr: extern "system" fn(
        device: Device,
        acceleration_structure: AccelerationStructureKHR,
        p_allocator: *const AllocationCallbacks,
    ),
    pub cmd_build_acceleration_structures_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        info_count: u32,
        p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
        pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
    ),
    pub cmd_build_acceleration_structures_indirect_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        info_count: u32,
        p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
        p_indirect_device_addresses: *const DeviceAddress,
        p_indirect_strides: *const u32,
        pp_max_primitive_counts: *const *const u32,
    ),
    pub build_acceleration_structures_khr: extern "system" fn(
        device: Device,
        deferred_operation: DeferredOperationKHR,
        info_count: u32,
        p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
        pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
    ) -> Result,
    pub copy_acceleration_structure_khr: extern "system" fn(
        device: Device,
        deferred_operation: DeferredOperationKHR,
        p_info: *const CopyAccelerationStructureInfoKHR,
    ) -> Result,
    pub copy_acceleration_structure_to_memory_khr: extern "system" fn(
        device: Device,
        deferred_operation: DeferredOperationKHR,
        p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
    ) -> Result,
    pub copy_memory_to_acceleration_structure_khr: extern "system" fn(
        device: Device,
        deferred_operation: DeferredOperationKHR,
        p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
    ) -> Result,
    pub write_acceleration_structures_properties_khr: extern "system" fn(
        device: Device,
        acceleration_structure_count: u32,
        p_acceleration_structures: *const AccelerationStructureKHR,
        query_type: QueryType,
        data_size: usize,
        p_data: *mut c_void,
        stride: usize,
    ) -> Result,
    pub cmd_copy_acceleration_structure_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_info: *const CopyAccelerationStructureInfoKHR,
    ),
    pub cmd_copy_acceleration_structure_to_memory_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
    ),
    pub cmd_copy_memory_to_acceleration_structure_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
    ),
    pub get_acceleration_structure_device_address_khr: extern "system" fn(
        device: Device,
        p_info: *const AccelerationStructureDeviceAddressInfoKHR,
    ) -> DeviceAddress,
    pub cmd_write_acceleration_structures_properties_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        acceleration_structure_count: u32,
        p_acceleration_structures: *const AccelerationStructureKHR,
        query_type: QueryType,
        query_pool: QueryPool,
        first_query: u32,
    ),
    pub get_device_acceleration_structure_compatibility_khr: extern "system" fn(
        device: Device,
        p_version_info: *const AccelerationStructureVersionInfoKHR,
        p_compatibility: *mut AccelerationStructureCompatibilityKHR,
    ),
    pub get_acceleration_structure_build_sizes_khr: extern "system" fn(
        device: Device,
        build_type: AccelerationStructureBuildTypeKHR,
        p_build_info: *const AccelerationStructureBuildGeometryInfoKHR,
        p_max_primitive_counts: *const u32,
        p_size_info: *mut AccelerationStructureBuildSizesInfoKHR,
    ),
}
unsafe impl Send for KhrAccelerationStructureFn {}
unsafe impl Sync for KhrAccelerationStructureFn {}
impl ::std::clone::Clone for KhrAccelerationStructureFn {
    fn clone(&self) -> Self {
        KhrAccelerationStructureFn {
            create_acceleration_structure_khr: self.create_acceleration_structure_khr,
            destroy_acceleration_structure_khr: self.destroy_acceleration_structure_khr,
            cmd_build_acceleration_structures_khr: self.cmd_build_acceleration_structures_khr,
            cmd_build_acceleration_structures_indirect_khr: self
                .cmd_build_acceleration_structures_indirect_khr,
            build_acceleration_structures_khr: self.build_acceleration_structures_khr,
            copy_acceleration_structure_khr: self.copy_acceleration_structure_khr,
            copy_acceleration_structure_to_memory_khr: self
                .copy_acceleration_structure_to_memory_khr,
            copy_memory_to_acceleration_structure_khr: self
                .copy_memory_to_acceleration_structure_khr,
            write_acceleration_structures_properties_khr: self
                .write_acceleration_structures_properties_khr,
            cmd_copy_acceleration_structure_khr: self.cmd_copy_acceleration_structure_khr,
            cmd_copy_acceleration_structure_to_memory_khr: self
                .cmd_copy_acceleration_structure_to_memory_khr,
            cmd_copy_memory_to_acceleration_structure_khr: self
                .cmd_copy_memory_to_acceleration_structure_khr,
            get_acceleration_structure_device_address_khr: self
                .get_acceleration_structure_device_address_khr,
            cmd_write_acceleration_structures_properties_khr: self
                .cmd_write_acceleration_structures_properties_khr,
            get_device_acceleration_structure_compatibility_khr: self
                .get_device_acceleration_structure_compatibility_khr,
            get_acceleration_structure_build_sizes_khr: self
                .get_acceleration_structure_build_sizes_khr,
        }
    }
}
impl KhrAccelerationStructureFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrAccelerationStructureFn {
            create_acceleration_structure_khr: unsafe {
                extern "system" fn create_acceleration_structure_khr(
                    _device: Device,
                    _p_create_info: *const AccelerationStructureCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_acceleration_structure: *mut AccelerationStructureKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_acceleration_structure_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateAccelerationStructureKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_acceleration_structure_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_acceleration_structure_khr: unsafe {
                extern "system" fn destroy_acceleration_structure_khr(
                    _device: Device,
                    _acceleration_structure: AccelerationStructureKHR,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_acceleration_structure_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyAccelerationStructureKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_acceleration_structure_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_build_acceleration_structures_khr: unsafe {
                extern "system" fn cmd_build_acceleration_structures_khr(
                    _command_buffer: CommandBuffer,
                    _info_count: u32,
                    _p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
                    _pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_build_acceleration_structures_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBuildAccelerationStructuresKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_build_acceleration_structures_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_build_acceleration_structures_indirect_khr: unsafe {
                extern "system" fn cmd_build_acceleration_structures_indirect_khr(
                    _command_buffer: CommandBuffer,
                    _info_count: u32,
                    _p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
                    _p_indirect_device_addresses: *const DeviceAddress,
                    _p_indirect_strides: *const u32,
                    _pp_max_primitive_counts: *const *const u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_build_acceleration_structures_indirect_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBuildAccelerationStructuresIndirectKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_build_acceleration_structures_indirect_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            build_acceleration_structures_khr: unsafe {
                extern "system" fn build_acceleration_structures_khr(
                    _device: Device,
                    _deferred_operation: DeferredOperationKHR,
                    _info_count: u32,
                    _p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
                    _pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(build_acceleration_structures_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkBuildAccelerationStructuresKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    build_acceleration_structures_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            copy_acceleration_structure_khr: unsafe {
                extern "system" fn copy_acceleration_structure_khr(
                    _device: Device,
                    _deferred_operation: DeferredOperationKHR,
                    _p_info: *const CopyAccelerationStructureInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(copy_acceleration_structure_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCopyAccelerationStructureKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    copy_acceleration_structure_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            copy_acceleration_structure_to_memory_khr: unsafe {
                extern "system" fn copy_acceleration_structure_to_memory_khr(
                    _device: Device,
                    _deferred_operation: DeferredOperationKHR,
                    _p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(copy_acceleration_structure_to_memory_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCopyAccelerationStructureToMemoryKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    copy_acceleration_structure_to_memory_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            copy_memory_to_acceleration_structure_khr: unsafe {
                extern "system" fn copy_memory_to_acceleration_structure_khr(
                    _device: Device,
                    _deferred_operation: DeferredOperationKHR,
                    _p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(copy_memory_to_acceleration_structure_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCopyMemoryToAccelerationStructureKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    copy_memory_to_acceleration_structure_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            write_acceleration_structures_properties_khr: unsafe {
                extern "system" fn write_acceleration_structures_properties_khr(
                    _device: Device,
                    _acceleration_structure_count: u32,
                    _p_acceleration_structures: *const AccelerationStructureKHR,
                    _query_type: QueryType,
                    _data_size: usize,
                    _p_data: *mut c_void,
                    _stride: usize,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(write_acceleration_structures_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkWriteAccelerationStructuresPropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    write_acceleration_structures_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_acceleration_structure_khr: unsafe {
                extern "system" fn cmd_copy_acceleration_structure_khr(
                    _command_buffer: CommandBuffer,
                    _p_info: *const CopyAccelerationStructureInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_acceleration_structure_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdCopyAccelerationStructureKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_acceleration_structure_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_acceleration_structure_to_memory_khr: unsafe {
                extern "system" fn cmd_copy_acceleration_structure_to_memory_khr(
                    _command_buffer: CommandBuffer,
                    _p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_acceleration_structure_to_memory_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdCopyAccelerationStructureToMemoryKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_acceleration_structure_to_memory_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_memory_to_acceleration_structure_khr: unsafe {
                extern "system" fn cmd_copy_memory_to_acceleration_structure_khr(
                    _command_buffer: CommandBuffer,
                    _p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_memory_to_acceleration_structure_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdCopyMemoryToAccelerationStructureKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_memory_to_acceleration_structure_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_acceleration_structure_device_address_khr: unsafe {
                extern "system" fn get_acceleration_structure_device_address_khr(
                    _device: Device,
                    _p_info: *const AccelerationStructureDeviceAddressInfoKHR,
                ) -> DeviceAddress {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_acceleration_structure_device_address_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetAccelerationStructureDeviceAddressKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_acceleration_structure_device_address_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_write_acceleration_structures_properties_khr: unsafe {
                extern "system" fn cmd_write_acceleration_structures_properties_khr(
                    _command_buffer: CommandBuffer,
                    _acceleration_structure_count: u32,
                    _p_acceleration_structures: *const AccelerationStructureKHR,
                    _query_type: QueryType,
                    _query_pool: QueryPool,
                    _first_query: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_write_acceleration_structures_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdWriteAccelerationStructuresPropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_write_acceleration_structures_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_acceleration_structure_compatibility_khr: unsafe {
                extern "system" fn get_device_acceleration_structure_compatibility_khr(
                    _device: Device,
                    _p_version_info: *const AccelerationStructureVersionInfoKHR,
                    _p_compatibility: *mut AccelerationStructureCompatibilityKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_acceleration_structure_compatibility_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceAccelerationStructureCompatibilityKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_acceleration_structure_compatibility_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_acceleration_structure_build_sizes_khr: unsafe {
                extern "system" fn get_acceleration_structure_build_sizes_khr(
                    _device: Device,
                    _build_type: AccelerationStructureBuildTypeKHR,
                    _p_build_info: *const AccelerationStructureBuildGeometryInfoKHR,
                    _p_max_primitive_counts: *const u32,
                    _p_size_info: *mut AccelerationStructureBuildSizesInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_acceleration_structure_build_sizes_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetAccelerationStructureBuildSizesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_acceleration_structure_build_sizes_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateAccelerationStructureKHR.html>"]
    pub unsafe fn create_acceleration_structure_khr(
        &self,
        device: Device,
        p_create_info: *const AccelerationStructureCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_acceleration_structure: *mut AccelerationStructureKHR,
    ) -> Result {
        (self.create_acceleration_structure_khr)(
            device,
            p_create_info,
            p_allocator,
            p_acceleration_structure,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyAccelerationStructureKHR.html>"]
    pub unsafe fn destroy_acceleration_structure_khr(
        &self,
        device: Device,
        acceleration_structure: AccelerationStructureKHR,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_acceleration_structure_khr)(device, acceleration_structure, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBuildAccelerationStructuresKHR.html>"]
    pub unsafe fn cmd_build_acceleration_structures_khr(
        &self,
        command_buffer: CommandBuffer,
        info_count: u32,
        p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
        pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
    ) {
        (self.cmd_build_acceleration_structures_khr)(
            command_buffer,
            info_count,
            p_infos,
            pp_build_range_infos,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBuildAccelerationStructuresIndirectKHR.html>"]
    pub unsafe fn cmd_build_acceleration_structures_indirect_khr(
        &self,
        command_buffer: CommandBuffer,
        info_count: u32,
        p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
        p_indirect_device_addresses: *const DeviceAddress,
        p_indirect_strides: *const u32,
        pp_max_primitive_counts: *const *const u32,
    ) {
        (self.cmd_build_acceleration_structures_indirect_khr)(
            command_buffer,
            info_count,
            p_infos,
            p_indirect_device_addresses,
            p_indirect_strides,
            pp_max_primitive_counts,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBuildAccelerationStructuresKHR.html>"]
    pub unsafe fn build_acceleration_structures_khr(
        &self,
        device: Device,
        deferred_operation: DeferredOperationKHR,
        info_count: u32,
        p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
        pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
    ) -> Result {
        (self.build_acceleration_structures_khr)(
            device,
            deferred_operation,
            info_count,
            p_infos,
            pp_build_range_infos,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCopyAccelerationStructureKHR.html>"]
    pub unsafe fn copy_acceleration_structure_khr(
        &self,
        device: Device,
        deferred_operation: DeferredOperationKHR,
        p_info: *const CopyAccelerationStructureInfoKHR,
    ) -> Result {
        (self.copy_acceleration_structure_khr)(device, deferred_operation, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCopyAccelerationStructureToMemoryKHR.html>"]
    pub unsafe fn copy_acceleration_structure_to_memory_khr(
        &self,
        device: Device,
        deferred_operation: DeferredOperationKHR,
        p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
    ) -> Result {
        (self.copy_acceleration_structure_to_memory_khr)(device, deferred_operation, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCopyMemoryToAccelerationStructureKHR.html>"]
    pub unsafe fn copy_memory_to_acceleration_structure_khr(
        &self,
        device: Device,
        deferred_operation: DeferredOperationKHR,
        p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
    ) -> Result {
        (self.copy_memory_to_acceleration_structure_khr)(device, deferred_operation, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWriteAccelerationStructuresPropertiesKHR.html>"]
    pub unsafe fn write_acceleration_structures_properties_khr(
        &self,
        device: Device,
        acceleration_structure_count: u32,
        p_acceleration_structures: *const AccelerationStructureKHR,
        query_type: QueryType,
        data_size: usize,
        p_data: *mut c_void,
        stride: usize,
    ) -> Result {
        (self.write_acceleration_structures_properties_khr)(
            device,
            acceleration_structure_count,
            p_acceleration_structures,
            query_type,
            data_size,
            p_data,
            stride,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyAccelerationStructureKHR.html>"]
    pub unsafe fn cmd_copy_acceleration_structure_khr(
        &self,
        command_buffer: CommandBuffer,
        p_info: *const CopyAccelerationStructureInfoKHR,
    ) {
        (self.cmd_copy_acceleration_structure_khr)(command_buffer, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyAccelerationStructureToMemoryKHR.html>"]
    pub unsafe fn cmd_copy_acceleration_structure_to_memory_khr(
        &self,
        command_buffer: CommandBuffer,
        p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
    ) {
        (self.cmd_copy_acceleration_structure_to_memory_khr)(command_buffer, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyMemoryToAccelerationStructureKHR.html>"]
    pub unsafe fn cmd_copy_memory_to_acceleration_structure_khr(
        &self,
        command_buffer: CommandBuffer,
        p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
    ) {
        (self.cmd_copy_memory_to_acceleration_structure_khr)(command_buffer, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureDeviceAddressKHR.html>"]
    pub unsafe fn get_acceleration_structure_device_address_khr(
        &self,
        device: Device,
        p_info: *const AccelerationStructureDeviceAddressInfoKHR,
    ) -> DeviceAddress {
        (self.get_acceleration_structure_device_address_khr)(device, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteAccelerationStructuresPropertiesKHR.html>"]
    pub unsafe fn cmd_write_acceleration_structures_properties_khr(
        &self,
        command_buffer: CommandBuffer,
        acceleration_structure_count: u32,
        p_acceleration_structures: *const AccelerationStructureKHR,
        query_type: QueryType,
        query_pool: QueryPool,
        first_query: u32,
    ) {
        (self.cmd_write_acceleration_structures_properties_khr)(
            command_buffer,
            acceleration_structure_count,
            p_acceleration_structures,
            query_type,
            query_pool,
            first_query,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceAccelerationStructureCompatibilityKHR.html>"]
    pub unsafe fn get_device_acceleration_structure_compatibility_khr(
        &self,
        device: Device,
        p_version_info: *const AccelerationStructureVersionInfoKHR,
        p_compatibility: *mut AccelerationStructureCompatibilityKHR,
    ) {
        (self.get_device_acceleration_structure_compatibility_khr)(
            device,
            p_version_info,
            p_compatibility,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureBuildSizesKHR.html>"]
    pub unsafe fn get_acceleration_structure_build_sizes_khr(
        &self,
        device: Device,
        build_type: AccelerationStructureBuildTypeKHR,
        p_build_info: *const AccelerationStructureBuildGeometryInfoKHR,
        p_max_primitive_counts: *const u32,
        p_size_info: *mut AccelerationStructureBuildSizesInfoKHR,
    ) {
        (self.get_acceleration_structure_build_sizes_khr)(
            device,
            build_type,
            p_build_info,
            p_max_primitive_counts,
            p_size_info,
        )
    }
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR: Self = Self(1_000_150_007);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR: Self = Self(1_000_150_000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR: Self = Self(1_000_150_002);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR: Self = Self(1_000_150_003);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR: Self = Self(1_000_150_004);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR: Self = Self(1_000_150_005);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_GEOMETRY_KHR: Self = Self(1_000_150_006);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_VERSION_INFO_KHR: Self = Self(1_000_150_009);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const COPY_ACCELERATION_STRUCTURE_INFO_KHR: Self = Self(1_000_150_010);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR: Self = Self(1_000_150_011);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR: Self = Self(1_000_150_012);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR: Self = Self(1_000_150_013);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR: Self = Self(1_000_150_014);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_CREATE_INFO_KHR: Self = Self(1_000_150_017);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR: Self = Self(1_000_150_020);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl PipelineStageFlags {
    pub const ACCELERATION_STRUCTURE_BUILD_KHR: Self = Self(0b10_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl DescriptorType {
    pub const ACCELERATION_STRUCTURE_KHR: Self = Self(1_000_150_000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl AccessFlags {
    pub const ACCELERATION_STRUCTURE_READ_KHR: Self = Self(0b10_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl AccessFlags {
    pub const ACCELERATION_STRUCTURE_WRITE_KHR: Self = Self(0b100_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl QueryType {
    pub const ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR: Self = Self(1_000_150_000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl QueryType {
    pub const ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR: Self = Self(1_000_150_001);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl ObjectType {
    pub const ACCELERATION_STRUCTURE_KHR: Self = Self(1_000_150_000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl DebugReportObjectTypeEXT {
    pub const ACCELERATION_STRUCTURE_KHR: Self = Self(1_000_150_000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl IndexType {
    pub const NONE_KHR: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl FormatFeatureFlags {
    pub const ACCELERATION_STRUCTURE_VERTEX_BUFFER_KHR: Self =
        Self(0b10_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl BufferUsageFlags {
    pub const ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_KHR: Self =
        Self(0b1000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_acceleration_structure'"]
impl BufferUsageFlags {
    pub const ACCELERATION_STRUCTURE_STORAGE_KHR: Self = Self(0b1_0000_0000_0000_0000_0000);
}
impl KhrRayTracingPipelineFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_ray_tracing_pipeline\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdTraceRaysKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    width: u32,
    height: u32,
    depth: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateRayTracingPipelinesKHR = extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    pipeline_cache: PipelineCache,
    create_info_count: u32,
    p_create_infos: *const RayTracingPipelineCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_pipelines: *mut Pipeline,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetRayTracingShaderGroupHandlesKHR = extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    first_group: u32,
    group_count: u32,
    data_size: usize,
    p_data: *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    first_group: u32,
    group_count: u32,
    data_size: usize,
    p_data: *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdTraceRaysIndirectKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    indirect_device_address: DeviceAddress,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetRayTracingShaderGroupStackSizeKHR = extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    group: u32,
    group_shader: ShaderGroupShaderKHR,
) -> DeviceSize;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetRayTracingPipelineStackSizeKHR =
    extern "system" fn(command_buffer: CommandBuffer, pipeline_stack_size: u32);
pub struct KhrRayTracingPipelineFn {
    pub cmd_trace_rays_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        width: u32,
        height: u32,
        depth: u32,
    ),
    pub create_ray_tracing_pipelines_khr: extern "system" fn(
        device: Device,
        deferred_operation: DeferredOperationKHR,
        pipeline_cache: PipelineCache,
        create_info_count: u32,
        p_create_infos: *const RayTracingPipelineCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_pipelines: *mut Pipeline,
    ) -> Result,
    pub get_ray_tracing_shader_group_handles_khr: extern "system" fn(
        device: Device,
        pipeline: Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result,
    pub get_ray_tracing_capture_replay_shader_group_handles_khr: extern "system" fn(
        device: Device,
        pipeline: Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result,
    pub cmd_trace_rays_indirect_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        indirect_device_address: DeviceAddress,
    ),
    pub get_ray_tracing_shader_group_stack_size_khr: extern "system" fn(
        device: Device,
        pipeline: Pipeline,
        group: u32,
        group_shader: ShaderGroupShaderKHR,
    ) -> DeviceSize,
    pub cmd_set_ray_tracing_pipeline_stack_size_khr:
        extern "system" fn(command_buffer: CommandBuffer, pipeline_stack_size: u32),
}
unsafe impl Send for KhrRayTracingPipelineFn {}
unsafe impl Sync for KhrRayTracingPipelineFn {}
impl ::std::clone::Clone for KhrRayTracingPipelineFn {
    fn clone(&self) -> Self {
        KhrRayTracingPipelineFn {
            cmd_trace_rays_khr: self.cmd_trace_rays_khr,
            create_ray_tracing_pipelines_khr: self.create_ray_tracing_pipelines_khr,
            get_ray_tracing_shader_group_handles_khr: self.get_ray_tracing_shader_group_handles_khr,
            get_ray_tracing_capture_replay_shader_group_handles_khr: self
                .get_ray_tracing_capture_replay_shader_group_handles_khr,
            cmd_trace_rays_indirect_khr: self.cmd_trace_rays_indirect_khr,
            get_ray_tracing_shader_group_stack_size_khr: self
                .get_ray_tracing_shader_group_stack_size_khr,
            cmd_set_ray_tracing_pipeline_stack_size_khr: self
                .cmd_set_ray_tracing_pipeline_stack_size_khr,
        }
    }
}
impl KhrRayTracingPipelineFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrRayTracingPipelineFn {
            cmd_trace_rays_khr: unsafe {
                extern "system" fn cmd_trace_rays_khr(
                    _command_buffer: CommandBuffer,
                    _p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _width: u32,
                    _height: u32,
                    _depth: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_trace_rays_khr)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdTraceRaysKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_trace_rays_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_ray_tracing_pipelines_khr: unsafe {
                extern "system" fn create_ray_tracing_pipelines_khr(
                    _device: Device,
                    _deferred_operation: DeferredOperationKHR,
                    _pipeline_cache: PipelineCache,
                    _create_info_count: u32,
                    _p_create_infos: *const RayTracingPipelineCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_pipelines: *mut Pipeline,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_ray_tracing_pipelines_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateRayTracingPipelinesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_ray_tracing_pipelines_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_ray_tracing_shader_group_handles_khr: unsafe {
                extern "system" fn get_ray_tracing_shader_group_handles_khr(
                    _device: Device,
                    _pipeline: Pipeline,
                    _first_group: u32,
                    _group_count: u32,
                    _data_size: usize,
                    _p_data: *mut c_void,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_ray_tracing_shader_group_handles_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetRayTracingShaderGroupHandlesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_ray_tracing_shader_group_handles_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_ray_tracing_capture_replay_shader_group_handles_khr: unsafe {
                extern "system" fn get_ray_tracing_capture_replay_shader_group_handles_khr(
                    _device: Device,
                    _pipeline: Pipeline,
                    _first_group: u32,
                    _group_count: u32,
                    _data_size: usize,
                    _p_data: *mut c_void,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_ray_tracing_capture_replay_shader_group_handles_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetRayTracingCaptureReplayShaderGroupHandlesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_ray_tracing_capture_replay_shader_group_handles_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_trace_rays_indirect_khr: unsafe {
                extern "system" fn cmd_trace_rays_indirect_khr(
                    _command_buffer: CommandBuffer,
                    _p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
                    _indirect_device_address: DeviceAddress,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_trace_rays_indirect_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdTraceRaysIndirectKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_trace_rays_indirect_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_ray_tracing_shader_group_stack_size_khr: unsafe {
                extern "system" fn get_ray_tracing_shader_group_stack_size_khr(
                    _device: Device,
                    _pipeline: Pipeline,
                    _group: u32,
                    _group_shader: ShaderGroupShaderKHR,
                ) -> DeviceSize {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_ray_tracing_shader_group_stack_size_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetRayTracingShaderGroupStackSizeKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_ray_tracing_shader_group_stack_size_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_ray_tracing_pipeline_stack_size_khr: unsafe {
                extern "system" fn cmd_set_ray_tracing_pipeline_stack_size_khr(
                    _command_buffer: CommandBuffer,
                    _pipeline_stack_size: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_ray_tracing_pipeline_stack_size_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetRayTracingPipelineStackSizeKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_ray_tracing_pipeline_stack_size_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdTraceRaysKHR.html>"]
    pub unsafe fn cmd_trace_rays_khr(
        &self,
        command_buffer: CommandBuffer,
        p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        width: u32,
        height: u32,
        depth: u32,
    ) {
        (self.cmd_trace_rays_khr)(
            command_buffer,
            p_raygen_shader_binding_table,
            p_miss_shader_binding_table,
            p_hit_shader_binding_table,
            p_callable_shader_binding_table,
            width,
            height,
            depth,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRayTracingPipelinesKHR.html>"]
    pub unsafe fn create_ray_tracing_pipelines_khr(
        &self,
        device: Device,
        deferred_operation: DeferredOperationKHR,
        pipeline_cache: PipelineCache,
        create_info_count: u32,
        p_create_infos: *const RayTracingPipelineCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_pipelines: *mut Pipeline,
    ) -> Result {
        (self.create_ray_tracing_pipelines_khr)(
            device,
            deferred_operation,
            pipeline_cache,
            create_info_count,
            p_create_infos,
            p_allocator,
            p_pipelines,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRayTracingShaderGroupHandlesKHR.html>"]
    pub unsafe fn get_ray_tracing_shader_group_handles_khr(
        &self,
        device: Device,
        pipeline: Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result {
        (self.get_ray_tracing_shader_group_handles_khr)(
            device,
            pipeline,
            first_group,
            group_count,
            data_size,
            p_data,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRayTracingCaptureReplayShaderGroupHandlesKHR.html>"]
    pub unsafe fn get_ray_tracing_capture_replay_shader_group_handles_khr(
        &self,
        device: Device,
        pipeline: Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result {
        (self.get_ray_tracing_capture_replay_shader_group_handles_khr)(
            device,
            pipeline,
            first_group,
            group_count,
            data_size,
            p_data,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdTraceRaysIndirectKHR.html>"]
    pub unsafe fn cmd_trace_rays_indirect_khr(
        &self,
        command_buffer: CommandBuffer,
        p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
        indirect_device_address: DeviceAddress,
    ) {
        (self.cmd_trace_rays_indirect_khr)(
            command_buffer,
            p_raygen_shader_binding_table,
            p_miss_shader_binding_table,
            p_hit_shader_binding_table,
            p_callable_shader_binding_table,
            indirect_device_address,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRayTracingShaderGroupStackSizeKHR.html>"]
    pub unsafe fn get_ray_tracing_shader_group_stack_size_khr(
        &self,
        device: Device,
        pipeline: Pipeline,
        group: u32,
        group_shader: ShaderGroupShaderKHR,
    ) -> DeviceSize {
        (self.get_ray_tracing_shader_group_stack_size_khr)(device, pipeline, group, group_shader)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetRayTracingPipelineStackSizeKHR.html>"]
    pub unsafe fn cmd_set_ray_tracing_pipeline_stack_size_khr(
        &self,
        command_buffer: CommandBuffer,
        pipeline_stack_size: u32,
    ) {
        (self.cmd_set_ray_tracing_pipeline_stack_size_khr)(command_buffer, pipeline_stack_size)
    }
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR: Self = Self(1_000_347_000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR: Self = Self(1_000_347_001);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl StructureType {
    pub const RAY_TRACING_PIPELINE_CREATE_INFO_KHR: Self = Self(1_000_150_015);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl StructureType {
    pub const RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR: Self = Self(1_000_150_016);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl StructureType {
    pub const RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR: Self = Self(1_000_150_018);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl ShaderStageFlags {
    pub const RAYGEN_KHR: Self = Self(0b1_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl ShaderStageFlags {
    pub const ANY_HIT_KHR: Self = Self(0b10_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl ShaderStageFlags {
    pub const CLOSEST_HIT_KHR: Self = Self(0b100_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl ShaderStageFlags {
    pub const MISS_KHR: Self = Self(0b1000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl ShaderStageFlags {
    pub const INTERSECTION_KHR: Self = Self(0b1_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl ShaderStageFlags {
    pub const CALLABLE_KHR: Self = Self(0b10_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineStageFlags {
    pub const RAY_TRACING_SHADER_KHR: Self = Self(0b10_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl BufferUsageFlags {
    pub const SHADER_BINDING_TABLE_KHR: Self = Self(0b100_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineBindPoint {
    pub const RAY_TRACING_KHR: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_KHR: Self = Self(0b100_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_KHR: Self = Self(0b1000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_NO_NULL_MISS_SHADERS_KHR: Self = Self(0b1_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_KHR: Self = Self(0b10_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_SKIP_TRIANGLES_KHR: Self = Self(0b1_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_SKIP_AABBS_KHR: Self = Self(0b10_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_KHR: Self =
        Self(0b1000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_ray_tracing_pipeline'"]
impl DynamicState {
    pub const RAY_TRACING_PIPELINE_STACK_SIZE_KHR: Self = Self(1_000_347_000);
}
impl KhrRayQueryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_ray_query\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrRayQueryFn {}
unsafe impl Send for KhrRayQueryFn {}
unsafe impl Sync for KhrRayQueryFn {}
impl ::std::clone::Clone for KhrRayQueryFn {
    fn clone(&self) -> Self {
        KhrRayQueryFn {}
    }
}
impl KhrRayQueryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrRayQueryFn {}
    }
}
#[doc = "Generated from 'VK_KHR_ray_query'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR: Self = Self(1_000_348_013);
}
impl NvExtension152Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_152\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension152Fn {}
unsafe impl Send for NvExtension152Fn {}
unsafe impl Sync for NvExtension152Fn {}
impl ::std::clone::Clone for NvExtension152Fn {
    fn clone(&self) -> Self {
        NvExtension152Fn {}
    }
}
impl NvExtension152Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension152Fn {}
    }
}
impl NvFramebufferMixedSamplesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_framebuffer_mixed_samples\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvFramebufferMixedSamplesFn {}
unsafe impl Send for NvFramebufferMixedSamplesFn {}
unsafe impl Sync for NvFramebufferMixedSamplesFn {}
impl ::std::clone::Clone for NvFramebufferMixedSamplesFn {
    fn clone(&self) -> Self {
        NvFramebufferMixedSamplesFn {}
    }
}
impl NvFramebufferMixedSamplesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvFramebufferMixedSamplesFn {}
    }
}
#[doc = "Generated from 'VK_NV_framebuffer_mixed_samples'"]
impl StructureType {
    pub const PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV: Self = Self(1_000_152_000);
}
impl NvFillRectangleFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_fill_rectangle\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvFillRectangleFn {}
unsafe impl Send for NvFillRectangleFn {}
unsafe impl Sync for NvFillRectangleFn {}
impl ::std::clone::Clone for NvFillRectangleFn {
    fn clone(&self) -> Self {
        NvFillRectangleFn {}
    }
}
impl NvFillRectangleFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvFillRectangleFn {}
    }
}
#[doc = "Generated from 'VK_NV_fill_rectangle'"]
impl PolygonMode {
    pub const FILL_RECTANGLE_NV: Self = Self(1_000_153_000);
}
impl NvShaderSmBuiltinsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_shader_sm_builtins\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvShaderSmBuiltinsFn {}
unsafe impl Send for NvShaderSmBuiltinsFn {}
unsafe impl Sync for NvShaderSmBuiltinsFn {}
impl ::std::clone::Clone for NvShaderSmBuiltinsFn {
    fn clone(&self) -> Self {
        NvShaderSmBuiltinsFn {}
    }
}
impl NvShaderSmBuiltinsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvShaderSmBuiltinsFn {}
    }
}
#[doc = "Generated from 'VK_NV_shader_sm_builtins'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV: Self = Self(1_000_154_000);
}
#[doc = "Generated from 'VK_NV_shader_sm_builtins'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV: Self = Self(1_000_154_001);
}
impl ExtPostDepthCoverageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_post_depth_coverage\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtPostDepthCoverageFn {}
unsafe impl Send for ExtPostDepthCoverageFn {}
unsafe impl Sync for ExtPostDepthCoverageFn {}
impl ::std::clone::Clone for ExtPostDepthCoverageFn {
    fn clone(&self) -> Self {
        ExtPostDepthCoverageFn {}
    }
}
impl ExtPostDepthCoverageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPostDepthCoverageFn {}
    }
}
impl KhrSamplerYcbcrConversionFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_sampler_ycbcr_conversion\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 14u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateSamplerYcbcrConversion = extern "system" fn(
    device: Device,
    p_create_info: *const SamplerYcbcrConversionCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_ycbcr_conversion: *mut SamplerYcbcrConversion,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroySamplerYcbcrConversion = extern "system" fn(
    device: Device,
    ycbcr_conversion: SamplerYcbcrConversion,
    p_allocator: *const AllocationCallbacks,
);
pub struct KhrSamplerYcbcrConversionFn {
    pub create_sampler_ycbcr_conversion_khr: extern "system" fn(
        device: Device,
        p_create_info: *const SamplerYcbcrConversionCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_ycbcr_conversion: *mut SamplerYcbcrConversion,
    ) -> Result,
    pub destroy_sampler_ycbcr_conversion_khr: extern "system" fn(
        device: Device,
        ycbcr_conversion: SamplerYcbcrConversion,
        p_allocator: *const AllocationCallbacks,
    ),
}
unsafe impl Send for KhrSamplerYcbcrConversionFn {}
unsafe impl Sync for KhrSamplerYcbcrConversionFn {}
impl ::std::clone::Clone for KhrSamplerYcbcrConversionFn {
    fn clone(&self) -> Self {
        KhrSamplerYcbcrConversionFn {
            create_sampler_ycbcr_conversion_khr: self.create_sampler_ycbcr_conversion_khr,
            destroy_sampler_ycbcr_conversion_khr: self.destroy_sampler_ycbcr_conversion_khr,
        }
    }
}
impl KhrSamplerYcbcrConversionFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSamplerYcbcrConversionFn {
            create_sampler_ycbcr_conversion_khr: unsafe {
                extern "system" fn create_sampler_ycbcr_conversion_khr(
                    _device: Device,
                    _p_create_info: *const SamplerYcbcrConversionCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_ycbcr_conversion: *mut SamplerYcbcrConversion,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_sampler_ycbcr_conversion_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateSamplerYcbcrConversionKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_sampler_ycbcr_conversion_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_sampler_ycbcr_conversion_khr: unsafe {
                extern "system" fn destroy_sampler_ycbcr_conversion_khr(
                    _device: Device,
                    _ycbcr_conversion: SamplerYcbcrConversion,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_sampler_ycbcr_conversion_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroySamplerYcbcrConversionKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_sampler_ycbcr_conversion_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSamplerYcbcrConversionKHR.html>"]
    pub unsafe fn create_sampler_ycbcr_conversion_khr(
        &self,
        device: Device,
        p_create_info: *const SamplerYcbcrConversionCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_ycbcr_conversion: *mut SamplerYcbcrConversion,
    ) -> Result {
        (self.create_sampler_ycbcr_conversion_khr)(
            device,
            p_create_info,
            p_allocator,
            p_ycbcr_conversion,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySamplerYcbcrConversionKHR.html>"]
    pub unsafe fn destroy_sampler_ycbcr_conversion_khr(
        &self,
        device: Device,
        ycbcr_conversion: SamplerYcbcrConversion,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_sampler_ycbcr_conversion_khr)(device, ycbcr_conversion, p_allocator)
    }
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const SAMPLER_YCBCR_CONVERSION_CREATE_INFO_KHR: Self =
        StructureType::SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const SAMPLER_YCBCR_CONVERSION_INFO_KHR: Self =
        StructureType::SAMPLER_YCBCR_CONVERSION_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const BIND_IMAGE_PLANE_MEMORY_INFO_KHR: Self = StructureType::BIND_IMAGE_PLANE_MEMORY_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO_KHR: Self =
        StructureType::IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES_KHR: Self =
        StructureType::SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl DebugReportObjectTypeEXT {
    pub const SAMPLER_YCBCR_CONVERSION_KHR: Self =
        DebugReportObjectTypeEXT::SAMPLER_YCBCR_CONVERSION;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ObjectType {
    pub const SAMPLER_YCBCR_CONVERSION_KHR: Self = ObjectType::SAMPLER_YCBCR_CONVERSION;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8B8G8R8_422_UNORM_KHR: Self = Format::G8B8G8R8_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B8G8R8G8_422_UNORM_KHR: Self = Format::B8G8R8G8_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8_R8_3PLANE_420_UNORM_KHR: Self = Format::G8_B8_R8_3PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8R8_2PLANE_420_UNORM_KHR: Self = Format::G8_B8R8_2PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8_R8_3PLANE_422_UNORM_KHR: Self = Format::G8_B8_R8_3PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8R8_2PLANE_422_UNORM_KHR: Self = Format::G8_B8R8_2PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8_R8_3PLANE_444_UNORM_KHR: Self = Format::G8_B8_R8_3PLANE_444_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R10X6_UNORM_PACK16_KHR: Self = Format::R10X6_UNORM_PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R10X6G10X6_UNORM_2PACK16_KHR: Self = Format::R10X6G10X6_UNORM_2PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR: Self =
        Format::R10X6G10X6B10X6A10X6_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR: Self =
        Format::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR: Self =
        Format::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR: Self =
        Format::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR: Self =
        Format::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR: Self =
        Format::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR: Self =
        Format::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR: Self =
        Format::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R12X4_UNORM_PACK16_KHR: Self = Format::R12X4_UNORM_PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R12X4G12X4_UNORM_2PACK16_KHR: Self = Format::R12X4G12X4_UNORM_2PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR: Self =
        Format::R12X4G12X4B12X4A12X4_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR: Self =
        Format::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR: Self =
        Format::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR: Self =
        Format::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR: Self =
        Format::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR: Self =
        Format::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR: Self =
        Format::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR: Self =
        Format::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16B16G16R16_422_UNORM_KHR: Self = Format::G16B16G16R16_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B16G16R16G16_422_UNORM_KHR: Self = Format::B16G16R16G16_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16_R16_3PLANE_420_UNORM_KHR: Self = Format::G16_B16_R16_3PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16R16_2PLANE_420_UNORM_KHR: Self = Format::G16_B16R16_2PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16_R16_3PLANE_422_UNORM_KHR: Self = Format::G16_B16_R16_3PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16R16_2PLANE_422_UNORM_KHR: Self = Format::G16_B16R16_2PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16_R16_3PLANE_444_UNORM_KHR: Self = Format::G16_B16_R16_3PLANE_444_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageAspectFlags {
    pub const PLANE_0_KHR: Self = ImageAspectFlags::PLANE_0;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageAspectFlags {
    pub const PLANE_1_KHR: Self = ImageAspectFlags::PLANE_1;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageAspectFlags {
    pub const PLANE_2_KHR: Self = ImageAspectFlags::PLANE_2;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageCreateFlags {
    pub const DISJOINT_KHR: Self = ImageCreateFlags::DISJOINT;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const MIDPOINT_CHROMA_SAMPLES_KHR: Self = FormatFeatureFlags::MIDPOINT_CHROMA_SAMPLES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_KHR: Self =
        FormatFeatureFlags::SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_KHR: Self =
        FormatFeatureFlags::SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_KHR: Self =
        FormatFeatureFlags::SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_KHR: Self =
        FormatFeatureFlags::SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const DISJOINT_KHR: Self = FormatFeatureFlags::DISJOINT;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const COSITED_CHROMA_SAMPLES_KHR: Self = FormatFeatureFlags::COSITED_CHROMA_SAMPLES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const RGB_IDENTITY_KHR: Self = SamplerYcbcrModelConversion::RGB_IDENTITY;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_IDENTITY_KHR: Self = SamplerYcbcrModelConversion::YCBCR_IDENTITY;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_709_KHR: Self = SamplerYcbcrModelConversion::YCBCR_709;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_601_KHR: Self = SamplerYcbcrModelConversion::YCBCR_601;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_2020_KHR: Self = SamplerYcbcrModelConversion::YCBCR_2020;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrRange {
    pub const ITU_FULL_KHR: Self = SamplerYcbcrRange::ITU_FULL;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrRange {
    pub const ITU_NARROW_KHR: Self = SamplerYcbcrRange::ITU_NARROW;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ChromaLocation {
    pub const COSITED_EVEN_KHR: Self = ChromaLocation::COSITED_EVEN;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ChromaLocation {
    pub const MIDPOINT_KHR: Self = ChromaLocation::MIDPOINT;
}
impl KhrBindMemory2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_bind_memory2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkBindBufferMemory2 = extern "system" fn(
    device: Device,
    bind_info_count: u32,
    p_bind_infos: *const BindBufferMemoryInfo,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkBindImageMemory2 = extern "system" fn(
    device: Device,
    bind_info_count: u32,
    p_bind_infos: *const BindImageMemoryInfo,
) -> Result;
pub struct KhrBindMemory2Fn {
    pub bind_buffer_memory2_khr: extern "system" fn(
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindBufferMemoryInfo,
    ) -> Result,
    pub bind_image_memory2_khr: extern "system" fn(
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindImageMemoryInfo,
    ) -> Result,
}
unsafe impl Send for KhrBindMemory2Fn {}
unsafe impl Sync for KhrBindMemory2Fn {}
impl ::std::clone::Clone for KhrBindMemory2Fn {
    fn clone(&self) -> Self {
        KhrBindMemory2Fn {
            bind_buffer_memory2_khr: self.bind_buffer_memory2_khr,
            bind_image_memory2_khr: self.bind_image_memory2_khr,
        }
    }
}
impl KhrBindMemory2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrBindMemory2Fn {
            bind_buffer_memory2_khr: unsafe {
                extern "system" fn bind_buffer_memory2_khr(
                    _device: Device,
                    _bind_info_count: u32,
                    _p_bind_infos: *const BindBufferMemoryInfo,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(bind_buffer_memory2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkBindBufferMemory2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    bind_buffer_memory2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            bind_image_memory2_khr: unsafe {
                extern "system" fn bind_image_memory2_khr(
                    _device: Device,
                    _bind_info_count: u32,
                    _p_bind_infos: *const BindImageMemoryInfo,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(bind_image_memory2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkBindImageMemory2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    bind_image_memory2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindBufferMemory2KHR.html>"]
    pub unsafe fn bind_buffer_memory2_khr(
        &self,
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindBufferMemoryInfo,
    ) -> Result {
        (self.bind_buffer_memory2_khr)(device, bind_info_count, p_bind_infos)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindImageMemory2KHR.html>"]
    pub unsafe fn bind_image_memory2_khr(
        &self,
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindImageMemoryInfo,
    ) -> Result {
        (self.bind_image_memory2_khr)(device, bind_info_count, p_bind_infos)
    }
}
#[doc = "Generated from 'VK_KHR_bind_memory2'"]
impl StructureType {
    pub const BIND_BUFFER_MEMORY_INFO_KHR: Self = StructureType::BIND_BUFFER_MEMORY_INFO;
}
#[doc = "Generated from 'VK_KHR_bind_memory2'"]
impl StructureType {
    pub const BIND_IMAGE_MEMORY_INFO_KHR: Self = StructureType::BIND_IMAGE_MEMORY_INFO;
}
#[doc = "Generated from 'VK_KHR_bind_memory2'"]
impl ImageCreateFlags {
    pub const ALIAS_KHR: Self = ImageCreateFlags::ALIAS;
}
impl ExtImageDrmFormatModifierFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_image_drm_format_modifier\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageDrmFormatModifierPropertiesEXT = extern "system" fn(
    device: Device,
    image: Image,
    p_properties: *mut ImageDrmFormatModifierPropertiesEXT,
) -> Result;
pub struct ExtImageDrmFormatModifierFn {
    pub get_image_drm_format_modifier_properties_ext: extern "system" fn(
        device: Device,
        image: Image,
        p_properties: *mut ImageDrmFormatModifierPropertiesEXT,
    ) -> Result,
}
unsafe impl Send for ExtImageDrmFormatModifierFn {}
unsafe impl Sync for ExtImageDrmFormatModifierFn {}
impl ::std::clone::Clone for ExtImageDrmFormatModifierFn {
    fn clone(&self) -> Self {
        ExtImageDrmFormatModifierFn {
            get_image_drm_format_modifier_properties_ext: self
                .get_image_drm_format_modifier_properties_ext,
        }
    }
}
impl ExtImageDrmFormatModifierFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtImageDrmFormatModifierFn {
            get_image_drm_format_modifier_properties_ext: unsafe {
                extern "system" fn get_image_drm_format_modifier_properties_ext(
                    _device: Device,
                    _image: Image,
                    _p_properties: *mut ImageDrmFormatModifierPropertiesEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_drm_format_modifier_properties_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageDrmFormatModifierPropertiesEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_drm_format_modifier_properties_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageDrmFormatModifierPropertiesEXT.html>"]
    pub unsafe fn get_image_drm_format_modifier_properties_ext(
        &self,
        device: Device,
        image: Image,
        p_properties: *mut ImageDrmFormatModifierPropertiesEXT,
    ) -> Result {
        (self.get_image_drm_format_modifier_properties_ext)(device, image, p_properties)
    }
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl Result {
    pub const ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: Self = Self(-1000158000);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl StructureType {
    pub const DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT: Self = Self(1_000_158_000);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT: Self = Self(1_000_158_002);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl StructureType {
    pub const IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT: Self = Self(1_000_158_003);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl StructureType {
    pub const IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT: Self = Self(1_000_158_004);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl StructureType {
    pub const IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT: Self = Self(1_000_158_005);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl ImageTiling {
    pub const DRM_FORMAT_MODIFIER_EXT: Self = Self(1_000_158_000);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl ImageAspectFlags {
    pub const MEMORY_PLANE_0_EXT: Self = Self(0b1000_0000);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl ImageAspectFlags {
    pub const MEMORY_PLANE_1_EXT: Self = Self(0b1_0000_0000);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl ImageAspectFlags {
    pub const MEMORY_PLANE_2_EXT: Self = Self(0b10_0000_0000);
}
#[doc = "Generated from 'VK_EXT_image_drm_format_modifier'"]
impl ImageAspectFlags {
    pub const MEMORY_PLANE_3_EXT: Self = Self(0b100_0000_0000);
}
impl ExtExtension160Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_160\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension160Fn {}
unsafe impl Send for ExtExtension160Fn {}
unsafe impl Sync for ExtExtension160Fn {}
impl ::std::clone::Clone for ExtExtension160Fn {
    fn clone(&self) -> Self {
        ExtExtension160Fn {}
    }
}
impl ExtExtension160Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension160Fn {}
    }
}
impl ExtValidationCacheFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_validation_cache\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateValidationCacheEXT = extern "system" fn(
    device: Device,
    p_create_info: *const ValidationCacheCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_validation_cache: *mut ValidationCacheEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyValidationCacheEXT = extern "system" fn(
    device: Device,
    validation_cache: ValidationCacheEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkMergeValidationCachesEXT = extern "system" fn(
    device: Device,
    dst_cache: ValidationCacheEXT,
    src_cache_count: u32,
    p_src_caches: *const ValidationCacheEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetValidationCacheDataEXT = extern "system" fn(
    device: Device,
    validation_cache: ValidationCacheEXT,
    p_data_size: *mut usize,
    p_data: *mut c_void,
) -> Result;
pub struct ExtValidationCacheFn {
    pub create_validation_cache_ext: extern "system" fn(
        device: Device,
        p_create_info: *const ValidationCacheCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_validation_cache: *mut ValidationCacheEXT,
    ) -> Result,
    pub destroy_validation_cache_ext: extern "system" fn(
        device: Device,
        validation_cache: ValidationCacheEXT,
        p_allocator: *const AllocationCallbacks,
    ),
    pub merge_validation_caches_ext: extern "system" fn(
        device: Device,
        dst_cache: ValidationCacheEXT,
        src_cache_count: u32,
        p_src_caches: *const ValidationCacheEXT,
    ) -> Result,
    pub get_validation_cache_data_ext: extern "system" fn(
        device: Device,
        validation_cache: ValidationCacheEXT,
        p_data_size: *mut usize,
        p_data: *mut c_void,
    ) -> Result,
}
unsafe impl Send for ExtValidationCacheFn {}
unsafe impl Sync for ExtValidationCacheFn {}
impl ::std::clone::Clone for ExtValidationCacheFn {
    fn clone(&self) -> Self {
        ExtValidationCacheFn {
            create_validation_cache_ext: self.create_validation_cache_ext,
            destroy_validation_cache_ext: self.destroy_validation_cache_ext,
            merge_validation_caches_ext: self.merge_validation_caches_ext,
            get_validation_cache_data_ext: self.get_validation_cache_data_ext,
        }
    }
}
impl ExtValidationCacheFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtValidationCacheFn {
            create_validation_cache_ext: unsafe {
                extern "system" fn create_validation_cache_ext(
                    _device: Device,
                    _p_create_info: *const ValidationCacheCreateInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_validation_cache: *mut ValidationCacheEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_validation_cache_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateValidationCacheEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_validation_cache_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_validation_cache_ext: unsafe {
                extern "system" fn destroy_validation_cache_ext(
                    _device: Device,
                    _validation_cache: ValidationCacheEXT,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_validation_cache_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyValidationCacheEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_validation_cache_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            merge_validation_caches_ext: unsafe {
                extern "system" fn merge_validation_caches_ext(
                    _device: Device,
                    _dst_cache: ValidationCacheEXT,
                    _src_cache_count: u32,
                    _p_src_caches: *const ValidationCacheEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(merge_validation_caches_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkMergeValidationCachesEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    merge_validation_caches_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_validation_cache_data_ext: unsafe {
                extern "system" fn get_validation_cache_data_ext(
                    _device: Device,
                    _validation_cache: ValidationCacheEXT,
                    _p_data_size: *mut usize,
                    _p_data: *mut c_void,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_validation_cache_data_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetValidationCacheDataEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_validation_cache_data_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateValidationCacheEXT.html>"]
    pub unsafe fn create_validation_cache_ext(
        &self,
        device: Device,
        p_create_info: *const ValidationCacheCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_validation_cache: *mut ValidationCacheEXT,
    ) -> Result {
        (self.create_validation_cache_ext)(device, p_create_info, p_allocator, p_validation_cache)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyValidationCacheEXT.html>"]
    pub unsafe fn destroy_validation_cache_ext(
        &self,
        device: Device,
        validation_cache: ValidationCacheEXT,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_validation_cache_ext)(device, validation_cache, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkMergeValidationCachesEXT.html>"]
    pub unsafe fn merge_validation_caches_ext(
        &self,
        device: Device,
        dst_cache: ValidationCacheEXT,
        src_cache_count: u32,
        p_src_caches: *const ValidationCacheEXT,
    ) -> Result {
        (self.merge_validation_caches_ext)(device, dst_cache, src_cache_count, p_src_caches)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetValidationCacheDataEXT.html>"]
    pub unsafe fn get_validation_cache_data_ext(
        &self,
        device: Device,
        validation_cache: ValidationCacheEXT,
        p_data_size: *mut usize,
        p_data: *mut c_void,
    ) -> Result {
        (self.get_validation_cache_data_ext)(device, validation_cache, p_data_size, p_data)
    }
}
#[doc = "Generated from 'VK_EXT_validation_cache'"]
impl StructureType {
    pub const VALIDATION_CACHE_CREATE_INFO_EXT: Self = Self(1_000_160_000);
}
#[doc = "Generated from 'VK_EXT_validation_cache'"]
impl StructureType {
    pub const SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT: Self = Self(1_000_160_001);
}
#[doc = "Generated from 'VK_EXT_validation_cache'"]
impl ObjectType {
    pub const VALIDATION_CACHE_EXT: Self = Self(1_000_160_000);
}
impl ExtDescriptorIndexingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_descriptor_indexing\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtDescriptorIndexingFn {}
unsafe impl Send for ExtDescriptorIndexingFn {}
unsafe impl Sync for ExtDescriptorIndexingFn {}
impl ::std::clone::Clone for ExtDescriptorIndexingFn {
    fn clone(&self) -> Self {
        ExtDescriptorIndexingFn {}
    }
}
impl ExtDescriptorIndexingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDescriptorIndexingFn {}
    }
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT: Self =
        StructureType::DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: Self =
        StructureType::PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT: Self =
        StructureType::PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT: Self =
        StructureType::DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT: Self =
        StructureType::DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const UPDATE_AFTER_BIND_EXT: Self = DescriptorBindingFlags::UPDATE_AFTER_BIND;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const UPDATE_UNUSED_WHILE_PENDING_EXT: Self =
        DescriptorBindingFlags::UPDATE_UNUSED_WHILE_PENDING;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const PARTIALLY_BOUND_EXT: Self = DescriptorBindingFlags::PARTIALLY_BOUND;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const VARIABLE_DESCRIPTOR_COUNT_EXT: Self =
        DescriptorBindingFlags::VARIABLE_DESCRIPTOR_COUNT;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorPoolCreateFlags {
    pub const UPDATE_AFTER_BIND_EXT: Self = DescriptorPoolCreateFlags::UPDATE_AFTER_BIND;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorSetLayoutCreateFlags {
    pub const UPDATE_AFTER_BIND_POOL_EXT: Self =
        DescriptorSetLayoutCreateFlags::UPDATE_AFTER_BIND_POOL;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl Result {
    pub const ERROR_FRAGMENTATION_EXT: Self = Result::ERROR_FRAGMENTATION;
}
impl ExtShaderViewportIndexLayerFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_viewport_index_layer\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtShaderViewportIndexLayerFn {}
unsafe impl Send for ExtShaderViewportIndexLayerFn {}
unsafe impl Sync for ExtShaderViewportIndexLayerFn {}
impl ::std::clone::Clone for ExtShaderViewportIndexLayerFn {
    fn clone(&self) -> Self {
        ExtShaderViewportIndexLayerFn {}
    }
}
impl ExtShaderViewportIndexLayerFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderViewportIndexLayerFn {}
    }
}
impl KhrPortabilitySubsetFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_portability_subset\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrPortabilitySubsetFn {}
unsafe impl Send for KhrPortabilitySubsetFn {}
unsafe impl Sync for KhrPortabilitySubsetFn {}
impl ::std::clone::Clone for KhrPortabilitySubsetFn {
    fn clone(&self) -> Self {
        KhrPortabilitySubsetFn {}
    }
}
impl KhrPortabilitySubsetFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPortabilitySubsetFn {}
    }
}
#[doc = "Generated from 'VK_KHR_portability_subset'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR: Self = Self(1_000_163_000);
}
#[doc = "Generated from 'VK_KHR_portability_subset'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR: Self = Self(1_000_163_001);
}
impl NvShadingRateImageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_shading_rate_image\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindShadingRateImageNV = extern "system" fn(
    command_buffer: CommandBuffer,
    image_view: ImageView,
    image_layout: ImageLayout,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetViewportShadingRatePaletteNV = extern "system" fn(
    command_buffer: CommandBuffer,
    first_viewport: u32,
    viewport_count: u32,
    p_shading_rate_palettes: *const ShadingRatePaletteNV,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetCoarseSampleOrderNV = extern "system" fn(
    command_buffer: CommandBuffer,
    sample_order_type: CoarseSampleOrderTypeNV,
    custom_sample_order_count: u32,
    p_custom_sample_orders: *const CoarseSampleOrderCustomNV,
);
pub struct NvShadingRateImageFn {
    pub cmd_bind_shading_rate_image_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        image_view: ImageView,
        image_layout: ImageLayout,
    ),
    pub cmd_set_viewport_shading_rate_palette_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        first_viewport: u32,
        viewport_count: u32,
        p_shading_rate_palettes: *const ShadingRatePaletteNV,
    ),
    pub cmd_set_coarse_sample_order_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        sample_order_type: CoarseSampleOrderTypeNV,
        custom_sample_order_count: u32,
        p_custom_sample_orders: *const CoarseSampleOrderCustomNV,
    ),
}
unsafe impl Send for NvShadingRateImageFn {}
unsafe impl Sync for NvShadingRateImageFn {}
impl ::std::clone::Clone for NvShadingRateImageFn {
    fn clone(&self) -> Self {
        NvShadingRateImageFn {
            cmd_bind_shading_rate_image_nv: self.cmd_bind_shading_rate_image_nv,
            cmd_set_viewport_shading_rate_palette_nv: self.cmd_set_viewport_shading_rate_palette_nv,
            cmd_set_coarse_sample_order_nv: self.cmd_set_coarse_sample_order_nv,
        }
    }
}
impl NvShadingRateImageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvShadingRateImageFn {
            cmd_bind_shading_rate_image_nv: unsafe {
                extern "system" fn cmd_bind_shading_rate_image_nv(
                    _command_buffer: CommandBuffer,
                    _image_view: ImageView,
                    _image_layout: ImageLayout,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_shading_rate_image_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBindShadingRateImageNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_shading_rate_image_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_viewport_shading_rate_palette_nv: unsafe {
                extern "system" fn cmd_set_viewport_shading_rate_palette_nv(
                    _command_buffer: CommandBuffer,
                    _first_viewport: u32,
                    _viewport_count: u32,
                    _p_shading_rate_palettes: *const ShadingRatePaletteNV,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_viewport_shading_rate_palette_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetViewportShadingRatePaletteNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_viewport_shading_rate_palette_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_coarse_sample_order_nv: unsafe {
                extern "system" fn cmd_set_coarse_sample_order_nv(
                    _command_buffer: CommandBuffer,
                    _sample_order_type: CoarseSampleOrderTypeNV,
                    _custom_sample_order_count: u32,
                    _p_custom_sample_orders: *const CoarseSampleOrderCustomNV,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_coarse_sample_order_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetCoarseSampleOrderNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_coarse_sample_order_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindShadingRateImageNV.html>"]
    pub unsafe fn cmd_bind_shading_rate_image_nv(
        &self,
        command_buffer: CommandBuffer,
        image_view: ImageView,
        image_layout: ImageLayout,
    ) {
        (self.cmd_bind_shading_rate_image_nv)(command_buffer, image_view, image_layout)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetViewportShadingRatePaletteNV.html>"]
    pub unsafe fn cmd_set_viewport_shading_rate_palette_nv(
        &self,
        command_buffer: CommandBuffer,
        first_viewport: u32,
        viewport_count: u32,
        p_shading_rate_palettes: *const ShadingRatePaletteNV,
    ) {
        (self.cmd_set_viewport_shading_rate_palette_nv)(
            command_buffer,
            first_viewport,
            viewport_count,
            p_shading_rate_palettes,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetCoarseSampleOrderNV.html>"]
    pub unsafe fn cmd_set_coarse_sample_order_nv(
        &self,
        command_buffer: CommandBuffer,
        sample_order_type: CoarseSampleOrderTypeNV,
        custom_sample_order_count: u32,
        p_custom_sample_orders: *const CoarseSampleOrderCustomNV,
    ) {
        (self.cmd_set_coarse_sample_order_nv)(
            command_buffer,
            sample_order_type,
            custom_sample_order_count,
            p_custom_sample_orders,
        )
    }
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl StructureType {
    pub const PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV: Self = Self(1_000_164_000);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV: Self = Self(1_000_164_001);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV: Self = Self(1_000_164_002);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl ImageLayout {
    pub const SHADING_RATE_OPTIMAL_NV: Self = Self(1_000_164_003);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl DynamicState {
    pub const VIEWPORT_SHADING_RATE_PALETTE_NV: Self = Self(1_000_164_004);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl AccessFlags {
    pub const SHADING_RATE_IMAGE_READ_NV: Self = Self(0b1000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl ImageUsageFlags {
    pub const SHADING_RATE_IMAGE_NV: Self = Self(0b1_0000_0000);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl PipelineStageFlags {
    pub const SHADING_RATE_IMAGE_NV: Self = Self(0b100_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl StructureType {
    pub const PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV: Self =
        Self(1_000_164_005);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl DynamicState {
    pub const VIEWPORT_COARSE_SAMPLE_ORDER_NV: Self = Self(1_000_164_006);
}
impl NvRayTracingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_ray_tracing\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateAccelerationStructureNV = extern "system" fn(
    device: Device,
    p_create_info: *const AccelerationStructureCreateInfoNV,
    p_allocator: *const AllocationCallbacks,
    p_acceleration_structure: *mut AccelerationStructureNV,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyAccelerationStructureNV = extern "system" fn(
    device: Device,
    acceleration_structure: AccelerationStructureNV,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureMemoryRequirementsNV = extern "system" fn(
    device: Device,
    p_info: *const AccelerationStructureMemoryRequirementsInfoNV,
    p_memory_requirements: *mut MemoryRequirements2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkBindAccelerationStructureMemoryNV = extern "system" fn(
    device: Device,
    bind_info_count: u32,
    p_bind_infos: *const BindAccelerationStructureMemoryInfoNV,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBuildAccelerationStructureNV = extern "system" fn(
    command_buffer: CommandBuffer,
    p_info: *const AccelerationStructureInfoNV,
    instance_data: Buffer,
    instance_offset: DeviceSize,
    update: Bool32,
    dst: AccelerationStructureNV,
    src: AccelerationStructureNV,
    scratch: Buffer,
    scratch_offset: DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyAccelerationStructureNV = extern "system" fn(
    command_buffer: CommandBuffer,
    dst: AccelerationStructureNV,
    src: AccelerationStructureNV,
    mode: CopyAccelerationStructureModeKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdTraceRaysNV = extern "system" fn(
    command_buffer: CommandBuffer,
    raygen_shader_binding_table_buffer: Buffer,
    raygen_shader_binding_offset: DeviceSize,
    miss_shader_binding_table_buffer: Buffer,
    miss_shader_binding_offset: DeviceSize,
    miss_shader_binding_stride: DeviceSize,
    hit_shader_binding_table_buffer: Buffer,
    hit_shader_binding_offset: DeviceSize,
    hit_shader_binding_stride: DeviceSize,
    callable_shader_binding_table_buffer: Buffer,
    callable_shader_binding_offset: DeviceSize,
    callable_shader_binding_stride: DeviceSize,
    width: u32,
    height: u32,
    depth: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateRayTracingPipelinesNV = extern "system" fn(
    device: Device,
    pipeline_cache: PipelineCache,
    create_info_count: u32,
    p_create_infos: *const RayTracingPipelineCreateInfoNV,
    p_allocator: *const AllocationCallbacks,
    p_pipelines: *mut Pipeline,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureHandleNV = extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    first_group: u32,
    group_count: u32,
    data_size: usize,
    p_data: *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteAccelerationStructuresPropertiesNV = extern "system" fn(
    device: Device,
    acceleration_structure: AccelerationStructureNV,
    data_size: usize,
    p_data: *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCompileDeferredNV = extern "system" fn(
    command_buffer: CommandBuffer,
    acceleration_structure_count: u32,
    p_acceleration_structures: *const AccelerationStructureNV,
    query_type: QueryType,
    query_pool: QueryPool,
    first_query: u32,
);
pub struct NvRayTracingFn {
    pub create_acceleration_structure_nv: extern "system" fn(
        device: Device,
        p_create_info: *const AccelerationStructureCreateInfoNV,
        p_allocator: *const AllocationCallbacks,
        p_acceleration_structure: *mut AccelerationStructureNV,
    ) -> Result,
    pub destroy_acceleration_structure_nv: extern "system" fn(
        device: Device,
        acceleration_structure: AccelerationStructureNV,
        p_allocator: *const AllocationCallbacks,
    ),
    pub get_acceleration_structure_memory_requirements_nv: extern "system" fn(
        device: Device,
        p_info: *const AccelerationStructureMemoryRequirementsInfoNV,
        p_memory_requirements: *mut MemoryRequirements2KHR,
    ),
    pub bind_acceleration_structure_memory_nv: extern "system" fn(
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindAccelerationStructureMemoryInfoNV,
    ) -> Result,
    pub cmd_build_acceleration_structure_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        p_info: *const AccelerationStructureInfoNV,
        instance_data: Buffer,
        instance_offset: DeviceSize,
        update: Bool32,
        dst: AccelerationStructureNV,
        src: AccelerationStructureNV,
        scratch: Buffer,
        scratch_offset: DeviceSize,
    ),
    pub cmd_copy_acceleration_structure_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        dst: AccelerationStructureNV,
        src: AccelerationStructureNV,
        mode: CopyAccelerationStructureModeKHR,
    ),
    pub cmd_trace_rays_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        raygen_shader_binding_table_buffer: Buffer,
        raygen_shader_binding_offset: DeviceSize,
        miss_shader_binding_table_buffer: Buffer,
        miss_shader_binding_offset: DeviceSize,
        miss_shader_binding_stride: DeviceSize,
        hit_shader_binding_table_buffer: Buffer,
        hit_shader_binding_offset: DeviceSize,
        hit_shader_binding_stride: DeviceSize,
        callable_shader_binding_table_buffer: Buffer,
        callable_shader_binding_offset: DeviceSize,
        callable_shader_binding_stride: DeviceSize,
        width: u32,
        height: u32,
        depth: u32,
    ),
    pub create_ray_tracing_pipelines_nv: extern "system" fn(
        device: Device,
        pipeline_cache: PipelineCache,
        create_info_count: u32,
        p_create_infos: *const RayTracingPipelineCreateInfoNV,
        p_allocator: *const AllocationCallbacks,
        p_pipelines: *mut Pipeline,
    ) -> Result,
    pub get_ray_tracing_shader_group_handles_nv: extern "system" fn(
        device: Device,
        pipeline: Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result,
    pub get_acceleration_structure_handle_nv: extern "system" fn(
        device: Device,
        acceleration_structure: AccelerationStructureNV,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result,
    pub cmd_write_acceleration_structures_properties_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        acceleration_structure_count: u32,
        p_acceleration_structures: *const AccelerationStructureNV,
        query_type: QueryType,
        query_pool: QueryPool,
        first_query: u32,
    ),
    pub compile_deferred_nv:
        extern "system" fn(device: Device, pipeline: Pipeline, shader: u32) -> Result,
}
unsafe impl Send for NvRayTracingFn {}
unsafe impl Sync for NvRayTracingFn {}
impl ::std::clone::Clone for NvRayTracingFn {
    fn clone(&self) -> Self {
        NvRayTracingFn {
            create_acceleration_structure_nv: self.create_acceleration_structure_nv,
            destroy_acceleration_structure_nv: self.destroy_acceleration_structure_nv,
            get_acceleration_structure_memory_requirements_nv: self
                .get_acceleration_structure_memory_requirements_nv,
            bind_acceleration_structure_memory_nv: self.bind_acceleration_structure_memory_nv,
            cmd_build_acceleration_structure_nv: self.cmd_build_acceleration_structure_nv,
            cmd_copy_acceleration_structure_nv: self.cmd_copy_acceleration_structure_nv,
            cmd_trace_rays_nv: self.cmd_trace_rays_nv,
            create_ray_tracing_pipelines_nv: self.create_ray_tracing_pipelines_nv,
            get_ray_tracing_shader_group_handles_nv: self.get_ray_tracing_shader_group_handles_nv,
            get_acceleration_structure_handle_nv: self.get_acceleration_structure_handle_nv,
            cmd_write_acceleration_structures_properties_nv: self
                .cmd_write_acceleration_structures_properties_nv,
            compile_deferred_nv: self.compile_deferred_nv,
        }
    }
}
impl NvRayTracingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvRayTracingFn {
            create_acceleration_structure_nv: unsafe {
                extern "system" fn create_acceleration_structure_nv(
                    _device: Device,
                    _p_create_info: *const AccelerationStructureCreateInfoNV,
                    _p_allocator: *const AllocationCallbacks,
                    _p_acceleration_structure: *mut AccelerationStructureNV,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_acceleration_structure_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateAccelerationStructureNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_acceleration_structure_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_acceleration_structure_nv: unsafe {
                extern "system" fn destroy_acceleration_structure_nv(
                    _device: Device,
                    _acceleration_structure: AccelerationStructureNV,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_acceleration_structure_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyAccelerationStructureNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_acceleration_structure_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_acceleration_structure_memory_requirements_nv: unsafe {
                extern "system" fn get_acceleration_structure_memory_requirements_nv(
                    _device: Device,
                    _p_info: *const AccelerationStructureMemoryRequirementsInfoNV,
                    _p_memory_requirements: *mut MemoryRequirements2KHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_acceleration_structure_memory_requirements_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetAccelerationStructureMemoryRequirementsNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_acceleration_structure_memory_requirements_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            bind_acceleration_structure_memory_nv: unsafe {
                extern "system" fn bind_acceleration_structure_memory_nv(
                    _device: Device,
                    _bind_info_count: u32,
                    _p_bind_infos: *const BindAccelerationStructureMemoryInfoNV,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(bind_acceleration_structure_memory_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkBindAccelerationStructureMemoryNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    bind_acceleration_structure_memory_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_build_acceleration_structure_nv: unsafe {
                extern "system" fn cmd_build_acceleration_structure_nv(
                    _command_buffer: CommandBuffer,
                    _p_info: *const AccelerationStructureInfoNV,
                    _instance_data: Buffer,
                    _instance_offset: DeviceSize,
                    _update: Bool32,
                    _dst: AccelerationStructureNV,
                    _src: AccelerationStructureNV,
                    _scratch: Buffer,
                    _scratch_offset: DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_build_acceleration_structure_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBuildAccelerationStructureNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_build_acceleration_structure_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_acceleration_structure_nv: unsafe {
                extern "system" fn cmd_copy_acceleration_structure_nv(
                    _command_buffer: CommandBuffer,
                    _dst: AccelerationStructureNV,
                    _src: AccelerationStructureNV,
                    _mode: CopyAccelerationStructureModeKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_acceleration_structure_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdCopyAccelerationStructureNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_acceleration_structure_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_trace_rays_nv: unsafe {
                extern "system" fn cmd_trace_rays_nv(
                    _command_buffer: CommandBuffer,
                    _raygen_shader_binding_table_buffer: Buffer,
                    _raygen_shader_binding_offset: DeviceSize,
                    _miss_shader_binding_table_buffer: Buffer,
                    _miss_shader_binding_offset: DeviceSize,
                    _miss_shader_binding_stride: DeviceSize,
                    _hit_shader_binding_table_buffer: Buffer,
                    _hit_shader_binding_offset: DeviceSize,
                    _hit_shader_binding_stride: DeviceSize,
                    _callable_shader_binding_table_buffer: Buffer,
                    _callable_shader_binding_offset: DeviceSize,
                    _callable_shader_binding_stride: DeviceSize,
                    _width: u32,
                    _height: u32,
                    _depth: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_trace_rays_nv)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdTraceRaysNV\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_trace_rays_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_ray_tracing_pipelines_nv: unsafe {
                extern "system" fn create_ray_tracing_pipelines_nv(
                    _device: Device,
                    _pipeline_cache: PipelineCache,
                    _create_info_count: u32,
                    _p_create_infos: *const RayTracingPipelineCreateInfoNV,
                    _p_allocator: *const AllocationCallbacks,
                    _p_pipelines: *mut Pipeline,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_ray_tracing_pipelines_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateRayTracingPipelinesNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_ray_tracing_pipelines_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_ray_tracing_shader_group_handles_nv: unsafe {
                extern "system" fn get_ray_tracing_shader_group_handles_nv(
                    _device: Device,
                    _pipeline: Pipeline,
                    _first_group: u32,
                    _group_count: u32,
                    _data_size: usize,
                    _p_data: *mut c_void,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_ray_tracing_shader_group_handles_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetRayTracingShaderGroupHandlesNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_ray_tracing_shader_group_handles_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_acceleration_structure_handle_nv: unsafe {
                extern "system" fn get_acceleration_structure_handle_nv(
                    _device: Device,
                    _acceleration_structure: AccelerationStructureNV,
                    _data_size: usize,
                    _p_data: *mut c_void,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_acceleration_structure_handle_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetAccelerationStructureHandleNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_acceleration_structure_handle_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_write_acceleration_structures_properties_nv: unsafe {
                extern "system" fn cmd_write_acceleration_structures_properties_nv(
                    _command_buffer: CommandBuffer,
                    _acceleration_structure_count: u32,
                    _p_acceleration_structures: *const AccelerationStructureNV,
                    _query_type: QueryType,
                    _query_pool: QueryPool,
                    _first_query: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_write_acceleration_structures_properties_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdWriteAccelerationStructuresPropertiesNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_write_acceleration_structures_properties_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            compile_deferred_nv: unsafe {
                extern "system" fn compile_deferred_nv(
                    _device: Device,
                    _pipeline: Pipeline,
                    _shader: u32,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(compile_deferred_nv)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCompileDeferredNV\0");
                let val = _f(cname);
                if val.is_null() {
                    compile_deferred_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateAccelerationStructureNV.html>"]
    pub unsafe fn create_acceleration_structure_nv(
        &self,
        device: Device,
        p_create_info: *const AccelerationStructureCreateInfoNV,
        p_allocator: *const AllocationCallbacks,
        p_acceleration_structure: *mut AccelerationStructureNV,
    ) -> Result {
        (self.create_acceleration_structure_nv)(
            device,
            p_create_info,
            p_allocator,
            p_acceleration_structure,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyAccelerationStructureNV.html>"]
    pub unsafe fn destroy_acceleration_structure_nv(
        &self,
        device: Device,
        acceleration_structure: AccelerationStructureNV,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_acceleration_structure_nv)(device, acceleration_structure, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureMemoryRequirementsNV.html>"]
    pub unsafe fn get_acceleration_structure_memory_requirements_nv(
        &self,
        device: Device,
        p_info: *const AccelerationStructureMemoryRequirementsInfoNV,
        p_memory_requirements: *mut MemoryRequirements2KHR,
    ) {
        (self.get_acceleration_structure_memory_requirements_nv)(
            device,
            p_info,
            p_memory_requirements,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindAccelerationStructureMemoryNV.html>"]
    pub unsafe fn bind_acceleration_structure_memory_nv(
        &self,
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindAccelerationStructureMemoryInfoNV,
    ) -> Result {
        (self.bind_acceleration_structure_memory_nv)(device, bind_info_count, p_bind_infos)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBuildAccelerationStructureNV.html>"]
    pub unsafe fn cmd_build_acceleration_structure_nv(
        &self,
        command_buffer: CommandBuffer,
        p_info: *const AccelerationStructureInfoNV,
        instance_data: Buffer,
        instance_offset: DeviceSize,
        update: Bool32,
        dst: AccelerationStructureNV,
        src: AccelerationStructureNV,
        scratch: Buffer,
        scratch_offset: DeviceSize,
    ) {
        (self.cmd_build_acceleration_structure_nv)(
            command_buffer,
            p_info,
            instance_data,
            instance_offset,
            update,
            dst,
            src,
            scratch,
            scratch_offset,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyAccelerationStructureNV.html>"]
    pub unsafe fn cmd_copy_acceleration_structure_nv(
        &self,
        command_buffer: CommandBuffer,
        dst: AccelerationStructureNV,
        src: AccelerationStructureNV,
        mode: CopyAccelerationStructureModeKHR,
    ) {
        (self.cmd_copy_acceleration_structure_nv)(command_buffer, dst, src, mode)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdTraceRaysNV.html>"]
    pub unsafe fn cmd_trace_rays_nv(
        &self,
        command_buffer: CommandBuffer,
        raygen_shader_binding_table_buffer: Buffer,
        raygen_shader_binding_offset: DeviceSize,
        miss_shader_binding_table_buffer: Buffer,
        miss_shader_binding_offset: DeviceSize,
        miss_shader_binding_stride: DeviceSize,
        hit_shader_binding_table_buffer: Buffer,
        hit_shader_binding_offset: DeviceSize,
        hit_shader_binding_stride: DeviceSize,
        callable_shader_binding_table_buffer: Buffer,
        callable_shader_binding_offset: DeviceSize,
        callable_shader_binding_stride: DeviceSize,
        width: u32,
        height: u32,
        depth: u32,
    ) {
        (self.cmd_trace_rays_nv)(
            command_buffer,
            raygen_shader_binding_table_buffer,
            raygen_shader_binding_offset,
            miss_shader_binding_table_buffer,
            miss_shader_binding_offset,
            miss_shader_binding_stride,
            hit_shader_binding_table_buffer,
            hit_shader_binding_offset,
            hit_shader_binding_stride,
            callable_shader_binding_table_buffer,
            callable_shader_binding_offset,
            callable_shader_binding_stride,
            width,
            height,
            depth,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRayTracingPipelinesNV.html>"]
    pub unsafe fn create_ray_tracing_pipelines_nv(
        &self,
        device: Device,
        pipeline_cache: PipelineCache,
        create_info_count: u32,
        p_create_infos: *const RayTracingPipelineCreateInfoNV,
        p_allocator: *const AllocationCallbacks,
        p_pipelines: *mut Pipeline,
    ) -> Result {
        (self.create_ray_tracing_pipelines_nv)(
            device,
            pipeline_cache,
            create_info_count,
            p_create_infos,
            p_allocator,
            p_pipelines,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRayTracingShaderGroupHandlesNV.html>"]
    pub unsafe fn get_ray_tracing_shader_group_handles_nv(
        &self,
        device: Device,
        pipeline: Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result {
        (self.get_ray_tracing_shader_group_handles_nv)(
            device,
            pipeline,
            first_group,
            group_count,
            data_size,
            p_data,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetAccelerationStructureHandleNV.html>"]
    pub unsafe fn get_acceleration_structure_handle_nv(
        &self,
        device: Device,
        acceleration_structure: AccelerationStructureNV,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result {
        (self.get_acceleration_structure_handle_nv)(
            device,
            acceleration_structure,
            data_size,
            p_data,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteAccelerationStructuresPropertiesNV.html>"]
    pub unsafe fn cmd_write_acceleration_structures_properties_nv(
        &self,
        command_buffer: CommandBuffer,
        acceleration_structure_count: u32,
        p_acceleration_structures: *const AccelerationStructureNV,
        query_type: QueryType,
        query_pool: QueryPool,
        first_query: u32,
    ) {
        (self.cmd_write_acceleration_structures_properties_nv)(
            command_buffer,
            acceleration_structure_count,
            p_acceleration_structures,
            query_type,
            query_pool,
            first_query,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCompileDeferredNV.html>"]
    pub unsafe fn compile_deferred_nv(
        &self,
        device: Device,
        pipeline: Pipeline,
        shader: u32,
    ) -> Result {
        (self.compile_deferred_nv)(device, pipeline, shader)
    }
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const RAY_TRACING_PIPELINE_CREATE_INFO_NV: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_CREATE_INFO_NV: Self = Self(1_000_165_001);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const GEOMETRY_NV: Self = Self(1_000_165_003);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const GEOMETRY_TRIANGLES_NV: Self = Self(1_000_165_004);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const GEOMETRY_AABB_NV: Self = Self(1_000_165_005);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV: Self = Self(1_000_165_006);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV: Self = Self(1_000_165_007);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV: Self = Self(1_000_165_008);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV: Self = Self(1_000_165_009);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV: Self = Self(1_000_165_011);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_INFO_NV: Self = Self(1_000_165_012);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const RAYGEN_NV: Self = ShaderStageFlags::RAYGEN_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const ANY_HIT_NV: Self = ShaderStageFlags::ANY_HIT_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const CLOSEST_HIT_NV: Self = ShaderStageFlags::CLOSEST_HIT_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const MISS_NV: Self = ShaderStageFlags::MISS_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const INTERSECTION_NV: Self = ShaderStageFlags::INTERSECTION_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const CALLABLE_NV: Self = ShaderStageFlags::CALLABLE_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl PipelineStageFlags {
    pub const RAY_TRACING_SHADER_NV: Self = PipelineStageFlags::RAY_TRACING_SHADER_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl PipelineStageFlags {
    pub const ACCELERATION_STRUCTURE_BUILD_NV: Self =
        PipelineStageFlags::ACCELERATION_STRUCTURE_BUILD_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BufferUsageFlags {
    pub const RAY_TRACING_NV: Self = BufferUsageFlags::SHADER_BINDING_TABLE_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl PipelineBindPoint {
    pub const RAY_TRACING_NV: Self = PipelineBindPoint::RAY_TRACING_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl DescriptorType {
    pub const ACCELERATION_STRUCTURE_NV: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccessFlags {
    pub const ACCELERATION_STRUCTURE_READ_NV: Self = AccessFlags::ACCELERATION_STRUCTURE_READ_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccessFlags {
    pub const ACCELERATION_STRUCTURE_WRITE_NV: Self = AccessFlags::ACCELERATION_STRUCTURE_WRITE_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl QueryType {
    pub const ACCELERATION_STRUCTURE_COMPACTED_SIZE_NV: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl PipelineCreateFlags {
    pub const DEFER_COMPILE_NV: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ObjectType {
    pub const ACCELERATION_STRUCTURE_NV: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl DebugReportObjectTypeEXT {
    pub const ACCELERATION_STRUCTURE_NV: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl IndexType {
    pub const NONE_NV: Self = IndexType::NONE_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl RayTracingShaderGroupTypeKHR {
    pub const GENERAL_NV: Self = RayTracingShaderGroupTypeKHR::GENERAL;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl RayTracingShaderGroupTypeKHR {
    pub const TRIANGLES_HIT_GROUP_NV: Self = RayTracingShaderGroupTypeKHR::TRIANGLES_HIT_GROUP;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl RayTracingShaderGroupTypeKHR {
    pub const PROCEDURAL_HIT_GROUP_NV: Self = RayTracingShaderGroupTypeKHR::PROCEDURAL_HIT_GROUP;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryTypeKHR {
    pub const TRIANGLES_NV: Self = GeometryTypeKHR::TRIANGLES;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryTypeKHR {
    pub const AABBS_NV: Self = GeometryTypeKHR::AABBS;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccelerationStructureTypeKHR {
    pub const TOP_LEVEL_NV: Self = AccelerationStructureTypeKHR::TOP_LEVEL;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccelerationStructureTypeKHR {
    pub const BOTTOM_LEVEL_NV: Self = AccelerationStructureTypeKHR::BOTTOM_LEVEL;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryFlagsKHR {
    pub const OPAQUE_NV: Self = GeometryFlagsKHR::OPAQUE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryFlagsKHR {
    pub const NO_DUPLICATE_ANY_HIT_INVOCATION_NV: Self =
        GeometryFlagsKHR::NO_DUPLICATE_ANY_HIT_INVOCATION;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const TRIANGLE_CULL_DISABLE_NV: Self =
        GeometryInstanceFlagsKHR::TRIANGLE_FACING_CULL_DISABLE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const TRIANGLE_FRONT_COUNTERCLOCKWISE_NV: Self =
        GeometryInstanceFlagsKHR::TRIANGLE_FRONT_COUNTERCLOCKWISE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const FORCE_OPAQUE_NV: Self = GeometryInstanceFlagsKHR::FORCE_OPAQUE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const FORCE_NO_OPAQUE_NV: Self = GeometryInstanceFlagsKHR::FORCE_NO_OPAQUE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const ALLOW_UPDATE_NV: Self = BuildAccelerationStructureFlagsKHR::ALLOW_UPDATE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const ALLOW_COMPACTION_NV: Self = BuildAccelerationStructureFlagsKHR::ALLOW_COMPACTION;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const PREFER_FAST_TRACE_NV: Self = BuildAccelerationStructureFlagsKHR::PREFER_FAST_TRACE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const PREFER_FAST_BUILD_NV: Self = BuildAccelerationStructureFlagsKHR::PREFER_FAST_BUILD;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const LOW_MEMORY_NV: Self = BuildAccelerationStructureFlagsKHR::LOW_MEMORY;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl CopyAccelerationStructureModeKHR {
    pub const CLONE_NV: Self = CopyAccelerationStructureModeKHR::CLONE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl CopyAccelerationStructureModeKHR {
    pub const COMPACT_NV: Self = CopyAccelerationStructureModeKHR::COMPACT;
}
impl NvRepresentativeFragmentTestFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_representative_fragment_test\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct NvRepresentativeFragmentTestFn {}
unsafe impl Send for NvRepresentativeFragmentTestFn {}
unsafe impl Sync for NvRepresentativeFragmentTestFn {}
impl ::std::clone::Clone for NvRepresentativeFragmentTestFn {
    fn clone(&self) -> Self {
        NvRepresentativeFragmentTestFn {}
    }
}
impl NvRepresentativeFragmentTestFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvRepresentativeFragmentTestFn {}
    }
}
#[doc = "Generated from 'VK_NV_representative_fragment_test'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV: Self = Self(1_000_166_000);
}
#[doc = "Generated from 'VK_NV_representative_fragment_test'"]
impl StructureType {
    pub const PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV: Self =
        Self(1_000_166_001);
}
impl NvExtension168Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_168\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension168Fn {}
unsafe impl Send for NvExtension168Fn {}
unsafe impl Sync for NvExtension168Fn {}
impl ::std::clone::Clone for NvExtension168Fn {
    fn clone(&self) -> Self {
        NvExtension168Fn {}
    }
}
impl NvExtension168Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension168Fn {}
    }
}
impl KhrMaintenance3Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_maintenance3\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetDescriptorSetLayoutSupport = extern "system" fn(
    device: Device,
    p_create_info: *const DescriptorSetLayoutCreateInfo,
    p_support: *mut DescriptorSetLayoutSupport,
);
pub struct KhrMaintenance3Fn {
    pub get_descriptor_set_layout_support_khr: extern "system" fn(
        device: Device,
        p_create_info: *const DescriptorSetLayoutCreateInfo,
        p_support: *mut DescriptorSetLayoutSupport,
    ),
}
unsafe impl Send for KhrMaintenance3Fn {}
unsafe impl Sync for KhrMaintenance3Fn {}
impl ::std::clone::Clone for KhrMaintenance3Fn {
    fn clone(&self) -> Self {
        KhrMaintenance3Fn {
            get_descriptor_set_layout_support_khr: self.get_descriptor_set_layout_support_khr,
        }
    }
}
impl KhrMaintenance3Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrMaintenance3Fn {
            get_descriptor_set_layout_support_khr: unsafe {
                extern "system" fn get_descriptor_set_layout_support_khr(
                    _device: Device,
                    _p_create_info: *const DescriptorSetLayoutCreateInfo,
                    _p_support: *mut DescriptorSetLayoutSupport,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_descriptor_set_layout_support_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDescriptorSetLayoutSupportKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_descriptor_set_layout_support_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDescriptorSetLayoutSupportKHR.html>"]
    pub unsafe fn get_descriptor_set_layout_support_khr(
        &self,
        device: Device,
        p_create_info: *const DescriptorSetLayoutCreateInfo,
        p_support: *mut DescriptorSetLayoutSupport,
    ) {
        (self.get_descriptor_set_layout_support_khr)(device, p_create_info, p_support)
    }
}
#[doc = "Generated from 'VK_KHR_maintenance3'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_maintenance3'"]
impl StructureType {
    pub const DESCRIPTOR_SET_LAYOUT_SUPPORT_KHR: Self =
        StructureType::DESCRIPTOR_SET_LAYOUT_SUPPORT;
}
impl KhrDrawIndirectCountFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_draw_indirect_count\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrDrawIndirectCountFn {
    pub cmd_draw_indirect_count_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ),
    pub cmd_draw_indexed_indirect_count_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ),
}
unsafe impl Send for KhrDrawIndirectCountFn {}
unsafe impl Sync for KhrDrawIndirectCountFn {}
impl ::std::clone::Clone for KhrDrawIndirectCountFn {
    fn clone(&self) -> Self {
        KhrDrawIndirectCountFn {
            cmd_draw_indirect_count_khr: self.cmd_draw_indirect_count_khr,
            cmd_draw_indexed_indirect_count_khr: self.cmd_draw_indexed_indirect_count_khr,
        }
    }
}
impl KhrDrawIndirectCountFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDrawIndirectCountFn {
            cmd_draw_indirect_count_khr: unsafe {
                extern "system" fn cmd_draw_indirect_count_khr(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _count_buffer: Buffer,
                    _count_buffer_offset: DeviceSize,
                    _max_draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_indirect_count_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawIndirectCountKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indirect_count_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_indexed_indirect_count_khr: unsafe {
                extern "system" fn cmd_draw_indexed_indirect_count_khr(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _count_buffer: Buffer,
                    _count_buffer_offset: DeviceSize,
                    _max_draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_indexed_indirect_count_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdDrawIndexedIndirectCountKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indexed_indirect_count_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirectCountKHR.html>"]
    pub unsafe fn cmd_draw_indirect_count_khr(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indirect_count_khr)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexedIndirectCountKHR.html>"]
    pub unsafe fn cmd_draw_indexed_indirect_count_khr(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indexed_indirect_count_khr)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        )
    }
}
impl ExtFilterCubicFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_filter_cubic\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
pub struct ExtFilterCubicFn {}
unsafe impl Send for ExtFilterCubicFn {}
unsafe impl Sync for ExtFilterCubicFn {}
impl ::std::clone::Clone for ExtFilterCubicFn {
    fn clone(&self) -> Self {
        ExtFilterCubicFn {}
    }
}
impl ExtFilterCubicFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtFilterCubicFn {}
    }
}
#[doc = "Generated from 'VK_EXT_filter_cubic'"]
impl Filter {
    pub const CUBIC_EXT: Self = Filter::CUBIC_IMG;
}
#[doc = "Generated from 'VK_EXT_filter_cubic'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_FILTER_CUBIC_EXT: Self =
        FormatFeatureFlags::SAMPLED_IMAGE_FILTER_CUBIC_IMG;
}
#[doc = "Generated from 'VK_EXT_filter_cubic'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT: Self = Self(1_000_170_000);
}
#[doc = "Generated from 'VK_EXT_filter_cubic'"]
impl StructureType {
    pub const FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT: Self = Self(1_000_170_001);
}
impl QcomRenderPassShaderResolveFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_render_pass_shader_resolve\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
pub struct QcomRenderPassShaderResolveFn {}
unsafe impl Send for QcomRenderPassShaderResolveFn {}
unsafe impl Sync for QcomRenderPassShaderResolveFn {}
impl ::std::clone::Clone for QcomRenderPassShaderResolveFn {
    fn clone(&self) -> Self {
        QcomRenderPassShaderResolveFn {}
    }
}
impl QcomRenderPassShaderResolveFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomRenderPassShaderResolveFn {}
    }
}
#[doc = "Generated from 'VK_QCOM_render_pass_shader_resolve'"]
impl SubpassDescriptionFlags {
    pub const FRAGMENT_REGION_QCOM: Self = Self(0b100);
}
#[doc = "Generated from 'VK_QCOM_render_pass_shader_resolve'"]
impl SubpassDescriptionFlags {
    pub const SHADER_RESOLVE_QCOM: Self = Self(0b1000);
}
impl QcomExtension173Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_173\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension173Fn {}
unsafe impl Send for QcomExtension173Fn {}
unsafe impl Sync for QcomExtension173Fn {}
impl ::std::clone::Clone for QcomExtension173Fn {
    fn clone(&self) -> Self {
        QcomExtension173Fn {}
    }
}
impl QcomExtension173Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension173Fn {}
    }
}
#[doc = "Generated from 'VK_QCOM_extension_173'"]
impl BufferUsageFlags {
    pub const RESERVED_18_QCOM: Self = Self(0b100_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_QCOM_extension_173'"]
impl ImageUsageFlags {
    pub const RESERVED_16_QCOM: Self = Self(0b1_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_QCOM_extension_173'"]
impl ImageUsageFlags {
    pub const RESERVED_17_QCOM: Self = Self(0b10_0000_0000_0000_0000);
}
impl QcomExtension174Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_174\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension174Fn {}
unsafe impl Send for QcomExtension174Fn {}
unsafe impl Sync for QcomExtension174Fn {}
impl ::std::clone::Clone for QcomExtension174Fn {
    fn clone(&self) -> Self {
        QcomExtension174Fn {}
    }
}
impl QcomExtension174Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension174Fn {}
    }
}
impl ExtGlobalPriorityFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_global_priority\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtGlobalPriorityFn {}
unsafe impl Send for ExtGlobalPriorityFn {}
unsafe impl Sync for ExtGlobalPriorityFn {}
impl ::std::clone::Clone for ExtGlobalPriorityFn {
    fn clone(&self) -> Self {
        ExtGlobalPriorityFn {}
    }
}
impl ExtGlobalPriorityFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtGlobalPriorityFn {}
    }
}
#[doc = "Generated from 'VK_EXT_global_priority'"]
impl StructureType {
    pub const DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT: Self = Self(1_000_174_000);
}
#[doc = "Generated from 'VK_EXT_global_priority'"]
impl Result {
    pub const ERROR_NOT_PERMITTED_EXT: Self = Self(-1000174001);
}
impl KhrShaderSubgroupExtendedTypesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_subgroup_extended_types\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrShaderSubgroupExtendedTypesFn {}
unsafe impl Send for KhrShaderSubgroupExtendedTypesFn {}
unsafe impl Sync for KhrShaderSubgroupExtendedTypesFn {}
impl ::std::clone::Clone for KhrShaderSubgroupExtendedTypesFn {
    fn clone(&self) -> Self {
        KhrShaderSubgroupExtendedTypesFn {}
    }
}
impl KhrShaderSubgroupExtendedTypesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderSubgroupExtendedTypesFn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_subgroup_extended_types'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES;
}
impl ExtExtension177Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_177\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension177Fn {}
unsafe impl Send for ExtExtension177Fn {}
unsafe impl Sync for ExtExtension177Fn {}
impl ::std::clone::Clone for ExtExtension177Fn {
    fn clone(&self) -> Self {
        ExtExtension177Fn {}
    }
}
impl ExtExtension177Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension177Fn {}
    }
}
impl Khr8bitStorageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_8bit_storage\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct Khr8bitStorageFn {}
unsafe impl Send for Khr8bitStorageFn {}
unsafe impl Sync for Khr8bitStorageFn {}
impl ::std::clone::Clone for Khr8bitStorageFn {
    fn clone(&self) -> Self {
        Khr8bitStorageFn {}
    }
}
impl Khr8bitStorageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Khr8bitStorageFn {}
    }
}
#[doc = "Generated from 'VK_KHR_8bit_storage'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
}
impl ExtExternalMemoryHostFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_external_memory_host\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryHostPointerPropertiesEXT = extern "system" fn(
    device: Device,
    handle_type: ExternalMemoryHandleTypeFlags,
    p_host_pointer: *const c_void,
    p_memory_host_pointer_properties: *mut MemoryHostPointerPropertiesEXT,
) -> Result;
pub struct ExtExternalMemoryHostFn {
    pub get_memory_host_pointer_properties_ext: extern "system" fn(
        device: Device,
        handle_type: ExternalMemoryHandleTypeFlags,
        p_host_pointer: *const c_void,
        p_memory_host_pointer_properties: *mut MemoryHostPointerPropertiesEXT,
    ) -> Result,
}
unsafe impl Send for ExtExternalMemoryHostFn {}
unsafe impl Sync for ExtExternalMemoryHostFn {}
impl ::std::clone::Clone for ExtExternalMemoryHostFn {
    fn clone(&self) -> Self {
        ExtExternalMemoryHostFn {
            get_memory_host_pointer_properties_ext: self.get_memory_host_pointer_properties_ext,
        }
    }
}
impl ExtExternalMemoryHostFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExternalMemoryHostFn {
            get_memory_host_pointer_properties_ext: unsafe {
                extern "system" fn get_memory_host_pointer_properties_ext(
                    _device: Device,
                    _handle_type: ExternalMemoryHandleTypeFlags,
                    _p_host_pointer: *const c_void,
                    _p_memory_host_pointer_properties: *mut MemoryHostPointerPropertiesEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_host_pointer_properties_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetMemoryHostPointerPropertiesEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_memory_host_pointer_properties_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryHostPointerPropertiesEXT.html>"]
    pub unsafe fn get_memory_host_pointer_properties_ext(
        &self,
        device: Device,
        handle_type: ExternalMemoryHandleTypeFlags,
        p_host_pointer: *const c_void,
        p_memory_host_pointer_properties: *mut MemoryHostPointerPropertiesEXT,
    ) -> Result {
        (self.get_memory_host_pointer_properties_ext)(
            device,
            handle_type,
            p_host_pointer,
            p_memory_host_pointer_properties,
        )
    }
}
#[doc = "Generated from 'VK_EXT_external_memory_host'"]
impl StructureType {
    pub const IMPORT_MEMORY_HOST_POINTER_INFO_EXT: Self = Self(1_000_178_000);
}
#[doc = "Generated from 'VK_EXT_external_memory_host'"]
impl StructureType {
    pub const MEMORY_HOST_POINTER_PROPERTIES_EXT: Self = Self(1_000_178_001);
}
#[doc = "Generated from 'VK_EXT_external_memory_host'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT: Self = Self(1_000_178_002);
}
#[doc = "Generated from 'VK_EXT_external_memory_host'"]
impl ExternalMemoryHandleTypeFlags {
    pub const HOST_ALLOCATION_EXT: Self = Self(0b1000_0000);
}
#[doc = "Generated from 'VK_EXT_external_memory_host'"]
impl ExternalMemoryHandleTypeFlags {
    pub const HOST_MAPPED_FOREIGN_MEMORY_EXT: Self = Self(0b1_0000_0000);
}
impl AmdBufferMarkerFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_buffer_marker\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteBufferMarkerAMD = extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_stage: PipelineStageFlags,
    dst_buffer: Buffer,
    dst_offset: DeviceSize,
    marker: u32,
);
pub struct AmdBufferMarkerFn {
    pub cmd_write_buffer_marker_amd: extern "system" fn(
        command_buffer: CommandBuffer,
        pipeline_stage: PipelineStageFlags,
        dst_buffer: Buffer,
        dst_offset: DeviceSize,
        marker: u32,
    ),
}
unsafe impl Send for AmdBufferMarkerFn {}
unsafe impl Sync for AmdBufferMarkerFn {}
impl ::std::clone::Clone for AmdBufferMarkerFn {
    fn clone(&self) -> Self {
        AmdBufferMarkerFn {
            cmd_write_buffer_marker_amd: self.cmd_write_buffer_marker_amd,
        }
    }
}
impl AmdBufferMarkerFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdBufferMarkerFn {
            cmd_write_buffer_marker_amd: unsafe {
                extern "system" fn cmd_write_buffer_marker_amd(
                    _command_buffer: CommandBuffer,
                    _pipeline_stage: PipelineStageFlags,
                    _dst_buffer: Buffer,
                    _dst_offset: DeviceSize,
                    _marker: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_write_buffer_marker_amd)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdWriteBufferMarkerAMD\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_write_buffer_marker_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteBufferMarkerAMD.html>"]
    pub unsafe fn cmd_write_buffer_marker_amd(
        &self,
        command_buffer: CommandBuffer,
        pipeline_stage: PipelineStageFlags,
        dst_buffer: Buffer,
        dst_offset: DeviceSize,
        marker: u32,
    ) {
        (self.cmd_write_buffer_marker_amd)(
            command_buffer,
            pipeline_stage,
            dst_buffer,
            dst_offset,
            marker,
        )
    }
}
impl KhrShaderAtomicInt64Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_atomic_int64\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrShaderAtomicInt64Fn {}
unsafe impl Send for KhrShaderAtomicInt64Fn {}
unsafe impl Sync for KhrShaderAtomicInt64Fn {}
impl ::std::clone::Clone for KhrShaderAtomicInt64Fn {
    fn clone(&self) -> Self {
        KhrShaderAtomicInt64Fn {}
    }
}
impl KhrShaderAtomicInt64Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderAtomicInt64Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_atomic_int64'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES;
}
impl KhrShaderClockFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_clock\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrShaderClockFn {}
unsafe impl Send for KhrShaderClockFn {}
unsafe impl Sync for KhrShaderClockFn {}
impl ::std::clone::Clone for KhrShaderClockFn {
    fn clone(&self) -> Self {
        KhrShaderClockFn {}
    }
}
impl KhrShaderClockFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderClockFn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_clock'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR: Self = Self(1_000_181_000);
}
impl AmdExtension183Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_183\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension183Fn {}
unsafe impl Send for AmdExtension183Fn {}
unsafe impl Sync for AmdExtension183Fn {}
impl ::std::clone::Clone for AmdExtension183Fn {
    fn clone(&self) -> Self {
        AmdExtension183Fn {}
    }
}
impl AmdExtension183Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension183Fn {}
    }
}
impl AmdPipelineCompilerControlFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_pipeline_compiler_control\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdPipelineCompilerControlFn {}
unsafe impl Send for AmdPipelineCompilerControlFn {}
unsafe impl Sync for AmdPipelineCompilerControlFn {}
impl ::std::clone::Clone for AmdPipelineCompilerControlFn {
    fn clone(&self) -> Self {
        AmdPipelineCompilerControlFn {}
    }
}
impl AmdPipelineCompilerControlFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdPipelineCompilerControlFn {}
    }
}
#[doc = "Generated from 'VK_AMD_pipeline_compiler_control'"]
impl StructureType {
    pub const PIPELINE_COMPILER_CONTROL_CREATE_INFO_AMD: Self = Self(1_000_183_000);
}
impl ExtCalibratedTimestampsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_calibrated_timestamps\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = extern "system" fn(
    physical_device: PhysicalDevice,
    p_time_domain_count: *mut u32,
    p_time_domains: *mut TimeDomainEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetCalibratedTimestampsEXT = extern "system" fn(
    device: Device,
    timestamp_count: u32,
    p_timestamp_infos: *const CalibratedTimestampInfoEXT,
    p_timestamps: *mut u64,
    p_max_deviation: *mut u64,
) -> Result;
pub struct ExtCalibratedTimestampsFn {
    pub get_physical_device_calibrateable_time_domains_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        p_time_domain_count: *mut u32,
        p_time_domains: *mut TimeDomainEXT,
    ) -> Result,
    pub get_calibrated_timestamps_ext: extern "system" fn(
        device: Device,
        timestamp_count: u32,
        p_timestamp_infos: *const CalibratedTimestampInfoEXT,
        p_timestamps: *mut u64,
        p_max_deviation: *mut u64,
    ) -> Result,
}
unsafe impl Send for ExtCalibratedTimestampsFn {}
unsafe impl Sync for ExtCalibratedTimestampsFn {}
impl ::std::clone::Clone for ExtCalibratedTimestampsFn {
    fn clone(&self) -> Self {
        ExtCalibratedTimestampsFn {
            get_physical_device_calibrateable_time_domains_ext: self
                .get_physical_device_calibrateable_time_domains_ext,
            get_calibrated_timestamps_ext: self.get_calibrated_timestamps_ext,
        }
    }
}
impl ExtCalibratedTimestampsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtCalibratedTimestampsFn {
            get_physical_device_calibrateable_time_domains_ext: unsafe {
                extern "system" fn get_physical_device_calibrateable_time_domains_ext(
                    _physical_device: PhysicalDevice,
                    _p_time_domain_count: *mut u32,
                    _p_time_domains: *mut TimeDomainEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_calibrateable_time_domains_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceCalibrateableTimeDomainsEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_calibrateable_time_domains_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_calibrated_timestamps_ext: unsafe {
                extern "system" fn get_calibrated_timestamps_ext(
                    _device: Device,
                    _timestamp_count: u32,
                    _p_timestamp_infos: *const CalibratedTimestampInfoEXT,
                    _p_timestamps: *mut u64,
                    _p_max_deviation: *mut u64,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_calibrated_timestamps_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetCalibratedTimestampsEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_calibrated_timestamps_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceCalibrateableTimeDomainsEXT.html>"]
    pub unsafe fn get_physical_device_calibrateable_time_domains_ext(
        &self,
        physical_device: PhysicalDevice,
        p_time_domain_count: *mut u32,
        p_time_domains: *mut TimeDomainEXT,
    ) -> Result {
        (self.get_physical_device_calibrateable_time_domains_ext)(
            physical_device,
            p_time_domain_count,
            p_time_domains,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetCalibratedTimestampsEXT.html>"]
    pub unsafe fn get_calibrated_timestamps_ext(
        &self,
        device: Device,
        timestamp_count: u32,
        p_timestamp_infos: *const CalibratedTimestampInfoEXT,
        p_timestamps: *mut u64,
        p_max_deviation: *mut u64,
    ) -> Result {
        (self.get_calibrated_timestamps_ext)(
            device,
            timestamp_count,
            p_timestamp_infos,
            p_timestamps,
            p_max_deviation,
        )
    }
}
#[doc = "Generated from 'VK_EXT_calibrated_timestamps'"]
impl StructureType {
    pub const CALIBRATED_TIMESTAMP_INFO_EXT: Self = Self(1_000_184_000);
}
impl AmdShaderCorePropertiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_core_properties\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct AmdShaderCorePropertiesFn {}
unsafe impl Send for AmdShaderCorePropertiesFn {}
unsafe impl Sync for AmdShaderCorePropertiesFn {}
impl ::std::clone::Clone for AmdShaderCorePropertiesFn {
    fn clone(&self) -> Self {
        AmdShaderCorePropertiesFn {}
    }
}
impl AmdShaderCorePropertiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderCorePropertiesFn {}
    }
}
#[doc = "Generated from 'VK_AMD_shader_core_properties'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD: Self = Self(1_000_185_000);
}
impl AmdExtension187Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_187\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension187Fn {}
unsafe impl Send for AmdExtension187Fn {}
unsafe impl Sync for AmdExtension187Fn {}
impl ::std::clone::Clone for AmdExtension187Fn {
    fn clone(&self) -> Self {
        AmdExtension187Fn {}
    }
}
impl AmdExtension187Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension187Fn {}
    }
}
impl AmdExtension188Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_188\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension188Fn {}
unsafe impl Send for AmdExtension188Fn {}
unsafe impl Sync for AmdExtension188Fn {}
impl ::std::clone::Clone for AmdExtension188Fn {
    fn clone(&self) -> Self {
        AmdExtension188Fn {}
    }
}
impl AmdExtension188Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension188Fn {}
    }
}
impl AmdExtension189Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_189\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension189Fn {}
unsafe impl Send for AmdExtension189Fn {}
unsafe impl Sync for AmdExtension189Fn {}
impl ::std::clone::Clone for AmdExtension189Fn {
    fn clone(&self) -> Self {
        AmdExtension189Fn {}
    }
}
impl AmdExtension189Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension189Fn {}
    }
}
impl AmdMemoryOverallocationBehaviorFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_memory_overallocation_behavior\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdMemoryOverallocationBehaviorFn {}
unsafe impl Send for AmdMemoryOverallocationBehaviorFn {}
unsafe impl Sync for AmdMemoryOverallocationBehaviorFn {}
impl ::std::clone::Clone for AmdMemoryOverallocationBehaviorFn {
    fn clone(&self) -> Self {
        AmdMemoryOverallocationBehaviorFn {}
    }
}
impl AmdMemoryOverallocationBehaviorFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdMemoryOverallocationBehaviorFn {}
    }
}
#[doc = "Generated from 'VK_AMD_memory_overallocation_behavior'"]
impl StructureType {
    pub const DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD: Self = Self(1_000_189_000);
}
impl ExtVertexAttributeDivisorFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_vertex_attribute_divisor\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
pub struct ExtVertexAttributeDivisorFn {}
unsafe impl Send for ExtVertexAttributeDivisorFn {}
unsafe impl Sync for ExtVertexAttributeDivisorFn {}
impl ::std::clone::Clone for ExtVertexAttributeDivisorFn {
    fn clone(&self) -> Self {
        ExtVertexAttributeDivisorFn {}
    }
}
impl ExtVertexAttributeDivisorFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVertexAttributeDivisorFn {}
    }
}
#[doc = "Generated from 'VK_EXT_vertex_attribute_divisor'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT: Self = Self(1_000_190_000);
}
#[doc = "Generated from 'VK_EXT_vertex_attribute_divisor'"]
impl StructureType {
    pub const PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT: Self = Self(1_000_190_001);
}
#[doc = "Generated from 'VK_EXT_vertex_attribute_divisor'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT: Self = Self(1_000_190_002);
}
impl GgpFrameTokenFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_frame_token\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct GgpFrameTokenFn {}
unsafe impl Send for GgpFrameTokenFn {}
unsafe impl Sync for GgpFrameTokenFn {}
impl ::std::clone::Clone for GgpFrameTokenFn {
    fn clone(&self) -> Self {
        GgpFrameTokenFn {}
    }
}
impl GgpFrameTokenFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpFrameTokenFn {}
    }
}
#[doc = "Generated from 'VK_GGP_frame_token'"]
impl StructureType {
    pub const PRESENT_FRAME_TOKEN_GGP: Self = Self(1_000_191_000);
}
impl ExtPipelineCreationFeedbackFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_pipeline_creation_feedback\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtPipelineCreationFeedbackFn {}
unsafe impl Send for ExtPipelineCreationFeedbackFn {}
unsafe impl Sync for ExtPipelineCreationFeedbackFn {}
impl ::std::clone::Clone for ExtPipelineCreationFeedbackFn {
    fn clone(&self) -> Self {
        ExtPipelineCreationFeedbackFn {}
    }
}
impl ExtPipelineCreationFeedbackFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPipelineCreationFeedbackFn {}
    }
}
#[doc = "Generated from 'VK_EXT_pipeline_creation_feedback'"]
impl StructureType {
    pub const PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT: Self = Self(1_000_192_000);
}
impl GoogleExtension194Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_extension_194\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct GoogleExtension194Fn {}
unsafe impl Send for GoogleExtension194Fn {}
unsafe impl Sync for GoogleExtension194Fn {}
impl ::std::clone::Clone for GoogleExtension194Fn {
    fn clone(&self) -> Self {
        GoogleExtension194Fn {}
    }
}
impl GoogleExtension194Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleExtension194Fn {}
    }
}
impl GoogleExtension195Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_extension_195\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct GoogleExtension195Fn {}
unsafe impl Send for GoogleExtension195Fn {}
unsafe impl Sync for GoogleExtension195Fn {}
impl ::std::clone::Clone for GoogleExtension195Fn {
    fn clone(&self) -> Self {
        GoogleExtension195Fn {}
    }
}
impl GoogleExtension195Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleExtension195Fn {}
    }
}
impl GoogleExtension196Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_extension_196\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct GoogleExtension196Fn {}
unsafe impl Send for GoogleExtension196Fn {}
unsafe impl Sync for GoogleExtension196Fn {}
impl ::std::clone::Clone for GoogleExtension196Fn {
    fn clone(&self) -> Self {
        GoogleExtension196Fn {}
    }
}
impl GoogleExtension196Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleExtension196Fn {}
    }
}
#[doc = "Generated from 'VK_GOOGLE_extension_196'"]
impl PipelineCacheCreateFlags {
    pub const RESERVED_1_EXT: Self = Self(0b10);
}
impl KhrDriverPropertiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_driver_properties\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrDriverPropertiesFn {}
unsafe impl Send for KhrDriverPropertiesFn {}
unsafe impl Sync for KhrDriverPropertiesFn {}
impl ::std::clone::Clone for KhrDriverPropertiesFn {
    fn clone(&self) -> Self {
        KhrDriverPropertiesFn {}
    }
}
impl KhrDriverPropertiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDriverPropertiesFn {}
    }
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_DRIVER_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const AMD_PROPRIETARY_KHR: Self = DriverId::AMD_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const AMD_OPEN_SOURCE_KHR: Self = DriverId::AMD_OPEN_SOURCE;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const MESA_RADV_KHR: Self = DriverId::MESA_RADV;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const NVIDIA_PROPRIETARY_KHR: Self = DriverId::NVIDIA_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const INTEL_PROPRIETARY_WINDOWS_KHR: Self = DriverId::INTEL_PROPRIETARY_WINDOWS;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const INTEL_OPEN_SOURCE_MESA_KHR: Self = DriverId::INTEL_OPEN_SOURCE_MESA;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const IMAGINATION_PROPRIETARY_KHR: Self = DriverId::IMAGINATION_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const QUALCOMM_PROPRIETARY_KHR: Self = DriverId::QUALCOMM_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const ARM_PROPRIETARY_KHR: Self = DriverId::ARM_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const GOOGLE_SWIFTSHADER_KHR: Self = DriverId::GOOGLE_SWIFTSHADER;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const GGP_PROPRIETARY_KHR: Self = DriverId::GGP_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const BROADCOM_PROPRIETARY_KHR: Self = DriverId::BROADCOM_PROPRIETARY;
}
impl KhrShaderFloatControlsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_float_controls\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
pub struct KhrShaderFloatControlsFn {}
unsafe impl Send for KhrShaderFloatControlsFn {}
unsafe impl Sync for KhrShaderFloatControlsFn {}
impl ::std::clone::Clone for KhrShaderFloatControlsFn {
    fn clone(&self) -> Self {
        KhrShaderFloatControlsFn {}
    }
}
impl KhrShaderFloatControlsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderFloatControlsFn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_float_controls'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_shader_float_controls'"]
impl ShaderFloatControlsIndependence {
    pub const TYPE_32_ONLY_KHR: Self = ShaderFloatControlsIndependence::TYPE_32_ONLY;
}
#[doc = "Generated from 'VK_KHR_shader_float_controls'"]
impl ShaderFloatControlsIndependence {
    pub const ALL_KHR: Self = ShaderFloatControlsIndependence::ALL;
}
#[doc = "Generated from 'VK_KHR_shader_float_controls'"]
impl ShaderFloatControlsIndependence {
    pub const NONE_KHR: Self = ShaderFloatControlsIndependence::NONE;
}
impl NvShaderSubgroupPartitionedFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_shader_subgroup_partitioned\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvShaderSubgroupPartitionedFn {}
unsafe impl Send for NvShaderSubgroupPartitionedFn {}
unsafe impl Sync for NvShaderSubgroupPartitionedFn {}
impl ::std::clone::Clone for NvShaderSubgroupPartitionedFn {
    fn clone(&self) -> Self {
        NvShaderSubgroupPartitionedFn {}
    }
}
impl NvShaderSubgroupPartitionedFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvShaderSubgroupPartitionedFn {}
    }
}
#[doc = "Generated from 'VK_NV_shader_subgroup_partitioned'"]
impl SubgroupFeatureFlags {
    pub const PARTITIONED_NV: Self = Self(0b1_0000_0000);
}
impl KhrDepthStencilResolveFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_depth_stencil_resolve\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrDepthStencilResolveFn {}
unsafe impl Send for KhrDepthStencilResolveFn {}
unsafe impl Sync for KhrDepthStencilResolveFn {}
impl ::std::clone::Clone for KhrDepthStencilResolveFn {
    fn clone(&self) -> Self {
        KhrDepthStencilResolveFn {}
    }
}
impl KhrDepthStencilResolveFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDepthStencilResolveFn {}
    }
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl StructureType {
    pub const SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR: Self =
        StructureType::SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const NONE_KHR: Self = ResolveModeFlags::NONE;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const SAMPLE_ZERO_KHR: Self = ResolveModeFlags::SAMPLE_ZERO;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const AVERAGE_KHR: Self = ResolveModeFlags::AVERAGE;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const MIN_KHR: Self = ResolveModeFlags::MIN;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const MAX_KHR: Self = ResolveModeFlags::MAX;
}
impl KhrSwapchainMutableFormatFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_swapchain_mutable_format\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrSwapchainMutableFormatFn {}
unsafe impl Send for KhrSwapchainMutableFormatFn {}
unsafe impl Sync for KhrSwapchainMutableFormatFn {}
impl ::std::clone::Clone for KhrSwapchainMutableFormatFn {
    fn clone(&self) -> Self {
        KhrSwapchainMutableFormatFn {}
    }
}
impl KhrSwapchainMutableFormatFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSwapchainMutableFormatFn {}
    }
}
#[doc = "Generated from 'VK_KHR_swapchain_mutable_format'"]
impl SwapchainCreateFlagsKHR {
    pub const MUTABLE_FORMAT: Self = Self(0b100);
}
impl NvComputeShaderDerivativesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_compute_shader_derivatives\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvComputeShaderDerivativesFn {}
unsafe impl Send for NvComputeShaderDerivativesFn {}
unsafe impl Sync for NvComputeShaderDerivativesFn {}
impl ::std::clone::Clone for NvComputeShaderDerivativesFn {
    fn clone(&self) -> Self {
        NvComputeShaderDerivativesFn {}
    }
}
impl NvComputeShaderDerivativesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvComputeShaderDerivativesFn {}
    }
}
#[doc = "Generated from 'VK_NV_compute_shader_derivatives'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV: Self = Self(1_000_201_000);
}
impl NvMeshShaderFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_mesh_shader\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawMeshTasksNV =
    extern "system" fn(command_buffer: CommandBuffer, task_count: u32, first_task: u32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawMeshTasksIndirectNV = extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    draw_count: u32,
    stride: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawMeshTasksIndirectCountNV = extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    count_buffer: Buffer,
    count_buffer_offset: DeviceSize,
    max_draw_count: u32,
    stride: u32,
);
pub struct NvMeshShaderFn {
    pub cmd_draw_mesh_tasks_nv:
        extern "system" fn(command_buffer: CommandBuffer, task_count: u32, first_task: u32),
    pub cmd_draw_mesh_tasks_indirect_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        draw_count: u32,
        stride: u32,
    ),
    pub cmd_draw_mesh_tasks_indirect_count_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ),
}
unsafe impl Send for NvMeshShaderFn {}
unsafe impl Sync for NvMeshShaderFn {}
impl ::std::clone::Clone for NvMeshShaderFn {
    fn clone(&self) -> Self {
        NvMeshShaderFn {
            cmd_draw_mesh_tasks_nv: self.cmd_draw_mesh_tasks_nv,
            cmd_draw_mesh_tasks_indirect_nv: self.cmd_draw_mesh_tasks_indirect_nv,
            cmd_draw_mesh_tasks_indirect_count_nv: self.cmd_draw_mesh_tasks_indirect_count_nv,
        }
    }
}
impl NvMeshShaderFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvMeshShaderFn {
            cmd_draw_mesh_tasks_nv: unsafe {
                extern "system" fn cmd_draw_mesh_tasks_nv(
                    _command_buffer: CommandBuffer,
                    _task_count: u32,
                    _first_task: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_mesh_tasks_nv)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawMeshTasksNV\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_mesh_tasks_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_mesh_tasks_indirect_nv: unsafe {
                extern "system" fn cmd_draw_mesh_tasks_indirect_nv(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_mesh_tasks_indirect_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdDrawMeshTasksIndirectNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_mesh_tasks_indirect_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_mesh_tasks_indirect_count_nv: unsafe {
                extern "system" fn cmd_draw_mesh_tasks_indirect_count_nv(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _count_buffer: Buffer,
                    _count_buffer_offset: DeviceSize,
                    _max_draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_mesh_tasks_indirect_count_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdDrawMeshTasksIndirectCountNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_mesh_tasks_indirect_count_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMeshTasksNV.html>"]
    pub unsafe fn cmd_draw_mesh_tasks_nv(
        &self,
        command_buffer: CommandBuffer,
        task_count: u32,
        first_task: u32,
    ) {
        (self.cmd_draw_mesh_tasks_nv)(command_buffer, task_count, first_task)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMeshTasksIndirectNV.html>"]
    pub unsafe fn cmd_draw_mesh_tasks_indirect_nv(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_mesh_tasks_indirect_nv)(command_buffer, buffer, offset, draw_count, stride)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMeshTasksIndirectCountNV.html>"]
    pub unsafe fn cmd_draw_mesh_tasks_indirect_count_nv(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_mesh_tasks_indirect_count_nv)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        )
    }
}
#[doc = "Generated from 'VK_NV_mesh_shader'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV: Self = Self(1_000_202_000);
}
#[doc = "Generated from 'VK_NV_mesh_shader'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV: Self = Self(1_000_202_001);
}
#[doc = "Generated from 'VK_NV_mesh_shader'"]
impl ShaderStageFlags {
    pub const TASK_NV: Self = Self(0b100_0000);
}
#[doc = "Generated from 'VK_NV_mesh_shader'"]
impl ShaderStageFlags {
    pub const MESH_NV: Self = Self(0b1000_0000);
}
#[doc = "Generated from 'VK_NV_mesh_shader'"]
impl PipelineStageFlags {
    pub const TASK_SHADER_NV: Self = Self(0b1000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_mesh_shader'"]
impl PipelineStageFlags {
    pub const MESH_SHADER_NV: Self = Self(0b1_0000_0000_0000_0000_0000);
}
impl NvFragmentShaderBarycentricFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_fragment_shader_barycentric\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvFragmentShaderBarycentricFn {}
unsafe impl Send for NvFragmentShaderBarycentricFn {}
unsafe impl Sync for NvFragmentShaderBarycentricFn {}
impl ::std::clone::Clone for NvFragmentShaderBarycentricFn {
    fn clone(&self) -> Self {
        NvFragmentShaderBarycentricFn {}
    }
}
impl NvFragmentShaderBarycentricFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvFragmentShaderBarycentricFn {}
    }
}
#[doc = "Generated from 'VK_NV_fragment_shader_barycentric'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV: Self = Self(1_000_203_000);
}
impl NvShaderImageFootprintFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_shader_image_footprint\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct NvShaderImageFootprintFn {}
unsafe impl Send for NvShaderImageFootprintFn {}
unsafe impl Sync for NvShaderImageFootprintFn {}
impl ::std::clone::Clone for NvShaderImageFootprintFn {
    fn clone(&self) -> Self {
        NvShaderImageFootprintFn {}
    }
}
impl NvShaderImageFootprintFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvShaderImageFootprintFn {}
    }
}
#[doc = "Generated from 'VK_NV_shader_image_footprint'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV: Self = Self(1_000_204_000);
}
impl NvScissorExclusiveFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_scissor_exclusive\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetExclusiveScissorNV = extern "system" fn(
    command_buffer: CommandBuffer,
    first_exclusive_scissor: u32,
    exclusive_scissor_count: u32,
    p_exclusive_scissors: *const Rect2D,
);
pub struct NvScissorExclusiveFn {
    pub cmd_set_exclusive_scissor_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        first_exclusive_scissor: u32,
        exclusive_scissor_count: u32,
        p_exclusive_scissors: *const Rect2D,
    ),
}
unsafe impl Send for NvScissorExclusiveFn {}
unsafe impl Sync for NvScissorExclusiveFn {}
impl ::std::clone::Clone for NvScissorExclusiveFn {
    fn clone(&self) -> Self {
        NvScissorExclusiveFn {
            cmd_set_exclusive_scissor_nv: self.cmd_set_exclusive_scissor_nv,
        }
    }
}
impl NvScissorExclusiveFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvScissorExclusiveFn {
            cmd_set_exclusive_scissor_nv: unsafe {
                extern "system" fn cmd_set_exclusive_scissor_nv(
                    _command_buffer: CommandBuffer,
                    _first_exclusive_scissor: u32,
                    _exclusive_scissor_count: u32,
                    _p_exclusive_scissors: *const Rect2D,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_exclusive_scissor_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetExclusiveScissorNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_exclusive_scissor_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetExclusiveScissorNV.html>"]
    pub unsafe fn cmd_set_exclusive_scissor_nv(
        &self,
        command_buffer: CommandBuffer,
        first_exclusive_scissor: u32,
        exclusive_scissor_count: u32,
        p_exclusive_scissors: *const Rect2D,
    ) {
        (self.cmd_set_exclusive_scissor_nv)(
            command_buffer,
            first_exclusive_scissor,
            exclusive_scissor_count,
            p_exclusive_scissors,
        )
    }
}
#[doc = "Generated from 'VK_NV_scissor_exclusive'"]
impl StructureType {
    pub const PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV: Self = Self(1_000_205_000);
}
#[doc = "Generated from 'VK_NV_scissor_exclusive'"]
impl DynamicState {
    pub const EXCLUSIVE_SCISSOR_NV: Self = Self(1_000_205_001);
}
#[doc = "Generated from 'VK_NV_scissor_exclusive'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV: Self = Self(1_000_205_002);
}
impl NvDeviceDiagnosticCheckpointsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_device_diagnostic_checkpoints\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetCheckpointNV =
    extern "system" fn(command_buffer: CommandBuffer, p_checkpoint_marker: *const c_void);
#[allow(non_camel_case_types)]
pub type PFN_vkGetQueueCheckpointDataNV = extern "system" fn(
    queue: Queue,
    p_checkpoint_data_count: *mut u32,
    p_checkpoint_data: *mut CheckpointDataNV,
);
pub struct NvDeviceDiagnosticCheckpointsFn {
    pub cmd_set_checkpoint_nv:
        extern "system" fn(command_buffer: CommandBuffer, p_checkpoint_marker: *const c_void),
    pub get_queue_checkpoint_data_nv: extern "system" fn(
        queue: Queue,
        p_checkpoint_data_count: *mut u32,
        p_checkpoint_data: *mut CheckpointDataNV,
    ),
}
unsafe impl Send for NvDeviceDiagnosticCheckpointsFn {}
unsafe impl Sync for NvDeviceDiagnosticCheckpointsFn {}
impl ::std::clone::Clone for NvDeviceDiagnosticCheckpointsFn {
    fn clone(&self) -> Self {
        NvDeviceDiagnosticCheckpointsFn {
            cmd_set_checkpoint_nv: self.cmd_set_checkpoint_nv,
            get_queue_checkpoint_data_nv: self.get_queue_checkpoint_data_nv,
        }
    }
}
impl NvDeviceDiagnosticCheckpointsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvDeviceDiagnosticCheckpointsFn {
            cmd_set_checkpoint_nv: unsafe {
                extern "system" fn cmd_set_checkpoint_nv(
                    _command_buffer: CommandBuffer,
                    _p_checkpoint_marker: *const c_void,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_checkpoint_nv)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetCheckpointNV\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_checkpoint_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_queue_checkpoint_data_nv: unsafe {
                extern "system" fn get_queue_checkpoint_data_nv(
                    _queue: Queue,
                    _p_checkpoint_data_count: *mut u32,
                    _p_checkpoint_data: *mut CheckpointDataNV,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_queue_checkpoint_data_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetQueueCheckpointDataNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_queue_checkpoint_data_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetCheckpointNV.html>"]
    pub unsafe fn cmd_set_checkpoint_nv(
        &self,
        command_buffer: CommandBuffer,
        p_checkpoint_marker: *const c_void,
    ) {
        (self.cmd_set_checkpoint_nv)(command_buffer, p_checkpoint_marker)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetQueueCheckpointDataNV.html>"]
    pub unsafe fn get_queue_checkpoint_data_nv(
        &self,
        queue: Queue,
        p_checkpoint_data_count: *mut u32,
        p_checkpoint_data: *mut CheckpointDataNV,
    ) {
        (self.get_queue_checkpoint_data_nv)(queue, p_checkpoint_data_count, p_checkpoint_data)
    }
}
#[doc = "Generated from 'VK_NV_device_diagnostic_checkpoints'"]
impl StructureType {
    pub const CHECKPOINT_DATA_NV: Self = Self(1_000_206_000);
}
#[doc = "Generated from 'VK_NV_device_diagnostic_checkpoints'"]
impl StructureType {
    pub const QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV: Self = Self(1_000_206_001);
}
impl KhrTimelineSemaphoreFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_timeline_semaphore\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetSemaphoreCounterValue =
    extern "system" fn(device: Device, semaphore: Semaphore, p_value: *mut u64) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkWaitSemaphores = extern "system" fn(
    device: Device,
    p_wait_info: *const SemaphoreWaitInfo,
    timeout: u64,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkSignalSemaphore =
    extern "system" fn(device: Device, p_signal_info: *const SemaphoreSignalInfo) -> Result;
pub struct KhrTimelineSemaphoreFn {
    pub get_semaphore_counter_value_khr:
        extern "system" fn(device: Device, semaphore: Semaphore, p_value: *mut u64) -> Result,
    pub wait_semaphores_khr: extern "system" fn(
        device: Device,
        p_wait_info: *const SemaphoreWaitInfo,
        timeout: u64,
    ) -> Result,
    pub signal_semaphore_khr:
        extern "system" fn(device: Device, p_signal_info: *const SemaphoreSignalInfo) -> Result,
}
unsafe impl Send for KhrTimelineSemaphoreFn {}
unsafe impl Sync for KhrTimelineSemaphoreFn {}
impl ::std::clone::Clone for KhrTimelineSemaphoreFn {
    fn clone(&self) -> Self {
        KhrTimelineSemaphoreFn {
            get_semaphore_counter_value_khr: self.get_semaphore_counter_value_khr,
            wait_semaphores_khr: self.wait_semaphores_khr,
            signal_semaphore_khr: self.signal_semaphore_khr,
        }
    }
}
impl KhrTimelineSemaphoreFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrTimelineSemaphoreFn {
            get_semaphore_counter_value_khr: unsafe {
                extern "system" fn get_semaphore_counter_value_khr(
                    _device: Device,
                    _semaphore: Semaphore,
                    _p_value: *mut u64,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_semaphore_counter_value_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetSemaphoreCounterValueKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_semaphore_counter_value_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            wait_semaphores_khr: unsafe {
                extern "system" fn wait_semaphores_khr(
                    _device: Device,
                    _p_wait_info: *const SemaphoreWaitInfo,
                    _timeout: u64,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(wait_semaphores_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkWaitSemaphoresKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    wait_semaphores_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            signal_semaphore_khr: unsafe {
                extern "system" fn signal_semaphore_khr(
                    _device: Device,
                    _p_signal_info: *const SemaphoreSignalInfo,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(signal_semaphore_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkSignalSemaphoreKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    signal_semaphore_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreCounterValueKHR.html>"]
    pub unsafe fn get_semaphore_counter_value_khr(
        &self,
        device: Device,
        semaphore: Semaphore,
        p_value: *mut u64,
    ) -> Result {
        (self.get_semaphore_counter_value_khr)(device, semaphore, p_value)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitSemaphoresKHR.html>"]
    pub unsafe fn wait_semaphores_khr(
        &self,
        device: Device,
        p_wait_info: *const SemaphoreWaitInfo,
        timeout: u64,
    ) -> Result {
        (self.wait_semaphores_khr)(device, p_wait_info, timeout)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSignalSemaphoreKHR.html>"]
    pub unsafe fn signal_semaphore_khr(
        &self,
        device: Device,
        p_signal_info: *const SemaphoreSignalInfo,
    ) -> Result {
        (self.signal_semaphore_khr)(device, p_signal_info)
    }
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const SEMAPHORE_TYPE_CREATE_INFO_KHR: Self = StructureType::SEMAPHORE_TYPE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR: Self =
        StructureType::TIMELINE_SEMAPHORE_SUBMIT_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const SEMAPHORE_WAIT_INFO_KHR: Self = StructureType::SEMAPHORE_WAIT_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const SEMAPHORE_SIGNAL_INFO_KHR: Self = StructureType::SEMAPHORE_SIGNAL_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl SemaphoreType {
    pub const BINARY_KHR: Self = SemaphoreType::BINARY;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl SemaphoreType {
    pub const TIMELINE_KHR: Self = SemaphoreType::TIMELINE;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl SemaphoreWaitFlags {
    pub const ANY_KHR: Self = SemaphoreWaitFlags::ANY;
}
impl KhrExtension209Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_209\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension209Fn {}
unsafe impl Send for KhrExtension209Fn {}
unsafe impl Sync for KhrExtension209Fn {}
impl ::std::clone::Clone for KhrExtension209Fn {
    fn clone(&self) -> Self {
        KhrExtension209Fn {}
    }
}
impl KhrExtension209Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension209Fn {}
    }
}
impl IntelShaderIntegerFunctions2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_shader_integer_functions2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct IntelShaderIntegerFunctions2Fn {}
unsafe impl Send for IntelShaderIntegerFunctions2Fn {}
unsafe impl Sync for IntelShaderIntegerFunctions2Fn {}
impl ::std::clone::Clone for IntelShaderIntegerFunctions2Fn {
    fn clone(&self) -> Self {
        IntelShaderIntegerFunctions2Fn {}
    }
}
impl IntelShaderIntegerFunctions2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelShaderIntegerFunctions2Fn {}
    }
}
#[doc = "Generated from 'VK_INTEL_shader_integer_functions2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL: Self = Self(1_000_209_000);
}
impl IntelPerformanceQueryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_performance_query\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkInitializePerformanceApiINTEL = extern "system" fn(
    device: Device,
    p_initialize_info: *const InitializePerformanceApiInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkUninitializePerformanceApiINTEL = extern "system" fn(device: Device);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPerformanceMarkerINTEL = extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const PerformanceMarkerInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPerformanceStreamMarkerINTEL = extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const PerformanceStreamMarkerInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPerformanceOverrideINTEL = extern "system" fn(
    command_buffer: CommandBuffer,
    p_override_info: *const PerformanceOverrideInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquirePerformanceConfigurationINTEL = extern "system" fn(
    device: Device,
    p_acquire_info: *const PerformanceConfigurationAcquireInfoINTEL,
    p_configuration: *mut PerformanceConfigurationINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkReleasePerformanceConfigurationINTEL =
    extern "system" fn(device: Device, configuration: PerformanceConfigurationINTEL) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueueSetPerformanceConfigurationINTEL =
    extern "system" fn(queue: Queue, configuration: PerformanceConfigurationINTEL) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPerformanceParameterINTEL = extern "system" fn(
    device: Device,
    parameter: PerformanceParameterTypeINTEL,
    p_value: *mut PerformanceValueINTEL,
) -> Result;
pub struct IntelPerformanceQueryFn {
    pub initialize_performance_api_intel: extern "system" fn(
        device: Device,
        p_initialize_info: *const InitializePerformanceApiInfoINTEL,
    ) -> Result,
    pub uninitialize_performance_api_intel: extern "system" fn(device: Device),
    pub cmd_set_performance_marker_intel: extern "system" fn(
        command_buffer: CommandBuffer,
        p_marker_info: *const PerformanceMarkerInfoINTEL,
    ) -> Result,
    pub cmd_set_performance_stream_marker_intel: extern "system" fn(
        command_buffer: CommandBuffer,
        p_marker_info: *const PerformanceStreamMarkerInfoINTEL,
    ) -> Result,
    pub cmd_set_performance_override_intel: extern "system" fn(
        command_buffer: CommandBuffer,
        p_override_info: *const PerformanceOverrideInfoINTEL,
    ) -> Result,
    pub acquire_performance_configuration_intel: extern "system" fn(
        device: Device,
        p_acquire_info: *const PerformanceConfigurationAcquireInfoINTEL,
        p_configuration: *mut PerformanceConfigurationINTEL,
    ) -> Result,
    pub release_performance_configuration_intel:
        extern "system" fn(device: Device, configuration: PerformanceConfigurationINTEL) -> Result,
    pub queue_set_performance_configuration_intel:
        extern "system" fn(queue: Queue, configuration: PerformanceConfigurationINTEL) -> Result,
    pub get_performance_parameter_intel: extern "system" fn(
        device: Device,
        parameter: PerformanceParameterTypeINTEL,
        p_value: *mut PerformanceValueINTEL,
    ) -> Result,
}
unsafe impl Send for IntelPerformanceQueryFn {}
unsafe impl Sync for IntelPerformanceQueryFn {}
impl ::std::clone::Clone for IntelPerformanceQueryFn {
    fn clone(&self) -> Self {
        IntelPerformanceQueryFn {
            initialize_performance_api_intel: self.initialize_performance_api_intel,
            uninitialize_performance_api_intel: self.uninitialize_performance_api_intel,
            cmd_set_performance_marker_intel: self.cmd_set_performance_marker_intel,
            cmd_set_performance_stream_marker_intel: self.cmd_set_performance_stream_marker_intel,
            cmd_set_performance_override_intel: self.cmd_set_performance_override_intel,
            acquire_performance_configuration_intel: self.acquire_performance_configuration_intel,
            release_performance_configuration_intel: self.release_performance_configuration_intel,
            queue_set_performance_configuration_intel: self
                .queue_set_performance_configuration_intel,
            get_performance_parameter_intel: self.get_performance_parameter_intel,
        }
    }
}
impl IntelPerformanceQueryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelPerformanceQueryFn {
            initialize_performance_api_intel: unsafe {
                extern "system" fn initialize_performance_api_intel(
                    _device: Device,
                    _p_initialize_info: *const InitializePerformanceApiInfoINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(initialize_performance_api_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkInitializePerformanceApiINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    initialize_performance_api_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            uninitialize_performance_api_intel: unsafe {
                extern "system" fn uninitialize_performance_api_intel(_device: Device) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(uninitialize_performance_api_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkUninitializePerformanceApiINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    uninitialize_performance_api_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_performance_marker_intel: unsafe {
                extern "system" fn cmd_set_performance_marker_intel(
                    _command_buffer: CommandBuffer,
                    _p_marker_info: *const PerformanceMarkerInfoINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_performance_marker_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetPerformanceMarkerINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_performance_marker_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_performance_stream_marker_intel: unsafe {
                extern "system" fn cmd_set_performance_stream_marker_intel(
                    _command_buffer: CommandBuffer,
                    _p_marker_info: *const PerformanceStreamMarkerInfoINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_performance_stream_marker_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetPerformanceStreamMarkerINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_performance_stream_marker_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_performance_override_intel: unsafe {
                extern "system" fn cmd_set_performance_override_intel(
                    _command_buffer: CommandBuffer,
                    _p_override_info: *const PerformanceOverrideInfoINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_performance_override_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetPerformanceOverrideINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_performance_override_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            acquire_performance_configuration_intel: unsafe {
                extern "system" fn acquire_performance_configuration_intel(
                    _device: Device,
                    _p_acquire_info: *const PerformanceConfigurationAcquireInfoINTEL,
                    _p_configuration: *mut PerformanceConfigurationINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_performance_configuration_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkAcquirePerformanceConfigurationINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    acquire_performance_configuration_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            release_performance_configuration_intel: unsafe {
                extern "system" fn release_performance_configuration_intel(
                    _device: Device,
                    _configuration: PerformanceConfigurationINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(release_performance_configuration_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkReleasePerformanceConfigurationINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    release_performance_configuration_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_set_performance_configuration_intel: unsafe {
                extern "system" fn queue_set_performance_configuration_intel(
                    _queue: Queue,
                    _configuration: PerformanceConfigurationINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(queue_set_performance_configuration_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkQueueSetPerformanceConfigurationINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    queue_set_performance_configuration_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_performance_parameter_intel: unsafe {
                extern "system" fn get_performance_parameter_intel(
                    _device: Device,
                    _parameter: PerformanceParameterTypeINTEL,
                    _p_value: *mut PerformanceValueINTEL,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_performance_parameter_intel)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPerformanceParameterINTEL\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_performance_parameter_intel
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkInitializePerformanceApiINTEL.html>"]
    pub unsafe fn initialize_performance_api_intel(
        &self,
        device: Device,
        p_initialize_info: *const InitializePerformanceApiInfoINTEL,
    ) -> Result {
        (self.initialize_performance_api_intel)(device, p_initialize_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUninitializePerformanceApiINTEL.html>"]
    pub unsafe fn uninitialize_performance_api_intel(&self, device: Device) {
        (self.uninitialize_performance_api_intel)(device)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetPerformanceMarkerINTEL.html>"]
    pub unsafe fn cmd_set_performance_marker_intel(
        &self,
        command_buffer: CommandBuffer,
        p_marker_info: *const PerformanceMarkerInfoINTEL,
    ) -> Result {
        (self.cmd_set_performance_marker_intel)(command_buffer, p_marker_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetPerformanceStreamMarkerINTEL.html>"]
    pub unsafe fn cmd_set_performance_stream_marker_intel(
        &self,
        command_buffer: CommandBuffer,
        p_marker_info: *const PerformanceStreamMarkerInfoINTEL,
    ) -> Result {
        (self.cmd_set_performance_stream_marker_intel)(command_buffer, p_marker_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetPerformanceOverrideINTEL.html>"]
    pub unsafe fn cmd_set_performance_override_intel(
        &self,
        command_buffer: CommandBuffer,
        p_override_info: *const PerformanceOverrideInfoINTEL,
    ) -> Result {
        (self.cmd_set_performance_override_intel)(command_buffer, p_override_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquirePerformanceConfigurationINTEL.html>"]
    pub unsafe fn acquire_performance_configuration_intel(
        &self,
        device: Device,
        p_acquire_info: *const PerformanceConfigurationAcquireInfoINTEL,
        p_configuration: *mut PerformanceConfigurationINTEL,
    ) -> Result {
        (self.acquire_performance_configuration_intel)(device, p_acquire_info, p_configuration)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkReleasePerformanceConfigurationINTEL.html>"]
    pub unsafe fn release_performance_configuration_intel(
        &self,
        device: Device,
        configuration: PerformanceConfigurationINTEL,
    ) -> Result {
        (self.release_performance_configuration_intel)(device, configuration)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueSetPerformanceConfigurationINTEL.html>"]
    pub unsafe fn queue_set_performance_configuration_intel(
        &self,
        queue: Queue,
        configuration: PerformanceConfigurationINTEL,
    ) -> Result {
        (self.queue_set_performance_configuration_intel)(queue, configuration)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPerformanceParameterINTEL.html>"]
    pub unsafe fn get_performance_parameter_intel(
        &self,
        device: Device,
        parameter: PerformanceParameterTypeINTEL,
        p_value: *mut PerformanceValueINTEL,
    ) -> Result {
        (self.get_performance_parameter_intel)(device, parameter, p_value)
    }
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl StructureType {
    pub const QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL: Self = Self(1_000_210_000);
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl StructureType {
    pub const QUERY_POOL_CREATE_INFO_INTEL: Self =
        StructureType::QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL;
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl StructureType {
    pub const INITIALIZE_PERFORMANCE_API_INFO_INTEL: Self = Self(1_000_210_001);
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl StructureType {
    pub const PERFORMANCE_MARKER_INFO_INTEL: Self = Self(1_000_210_002);
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl StructureType {
    pub const PERFORMANCE_STREAM_MARKER_INFO_INTEL: Self = Self(1_000_210_003);
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl StructureType {
    pub const PERFORMANCE_OVERRIDE_INFO_INTEL: Self = Self(1_000_210_004);
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl StructureType {
    pub const PERFORMANCE_CONFIGURATION_ACQUIRE_INFO_INTEL: Self = Self(1_000_210_005);
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl QueryType {
    pub const PERFORMANCE_QUERY_INTEL: Self = Self(1_000_210_000);
}
#[doc = "Generated from 'VK_INTEL_performance_query'"]
impl ObjectType {
    pub const PERFORMANCE_CONFIGURATION_INTEL: Self = Self(1_000_210_000);
}
impl KhrVulkanMemoryModelFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_vulkan_memory_model\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
pub struct KhrVulkanMemoryModelFn {}
unsafe impl Send for KhrVulkanMemoryModelFn {}
unsafe impl Sync for KhrVulkanMemoryModelFn {}
impl ::std::clone::Clone for KhrVulkanMemoryModelFn {
    fn clone(&self) -> Self {
        KhrVulkanMemoryModelFn {}
    }
}
impl KhrVulkanMemoryModelFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrVulkanMemoryModelFn {}
    }
}
#[doc = "Generated from 'VK_KHR_vulkan_memory_model'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
}
impl ExtPciBusInfoFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_pci_bus_info\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtPciBusInfoFn {}
unsafe impl Send for ExtPciBusInfoFn {}
unsafe impl Sync for ExtPciBusInfoFn {}
impl ::std::clone::Clone for ExtPciBusInfoFn {
    fn clone(&self) -> Self {
        ExtPciBusInfoFn {}
    }
}
impl ExtPciBusInfoFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPciBusInfoFn {}
    }
}
#[doc = "Generated from 'VK_EXT_pci_bus_info'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT: Self = Self(1_000_212_000);
}
impl AmdDisplayNativeHdrFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_display_native_hdr\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkSetLocalDimmingAMD =
    extern "system" fn(device: Device, swap_chain: SwapchainKHR, local_dimming_enable: Bool32);
pub struct AmdDisplayNativeHdrFn {
    pub set_local_dimming_amd:
        extern "system" fn(device: Device, swap_chain: SwapchainKHR, local_dimming_enable: Bool32),
}
unsafe impl Send for AmdDisplayNativeHdrFn {}
unsafe impl Sync for AmdDisplayNativeHdrFn {}
impl ::std::clone::Clone for AmdDisplayNativeHdrFn {
    fn clone(&self) -> Self {
        AmdDisplayNativeHdrFn {
            set_local_dimming_amd: self.set_local_dimming_amd,
        }
    }
}
impl AmdDisplayNativeHdrFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdDisplayNativeHdrFn {
            set_local_dimming_amd: unsafe {
                extern "system" fn set_local_dimming_amd(
                    _device: Device,
                    _swap_chain: SwapchainKHR,
                    _local_dimming_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(set_local_dimming_amd)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkSetLocalDimmingAMD\0");
                let val = _f(cname);
                if val.is_null() {
                    set_local_dimming_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetLocalDimmingAMD.html>"]
    pub unsafe fn set_local_dimming_amd(
        &self,
        device: Device,
        swap_chain: SwapchainKHR,
        local_dimming_enable: Bool32,
    ) {
        (self.set_local_dimming_amd)(device, swap_chain, local_dimming_enable)
    }
}
#[doc = "Generated from 'VK_AMD_display_native_hdr'"]
impl StructureType {
    pub const DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD: Self = Self(1_000_213_000);
}
#[doc = "Generated from 'VK_AMD_display_native_hdr'"]
impl StructureType {
    pub const SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD: Self = Self(1_000_213_001);
}
#[doc = "Generated from 'VK_AMD_display_native_hdr'"]
impl ColorSpaceKHR {
    pub const DISPLAY_NATIVE_AMD: Self = Self(1_000_213_000);
}
impl FuchsiaImagepipeSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_imagepipe_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateImagePipeSurfaceFUCHSIA = extern "system" fn(
    instance: Instance,
    p_create_info: *const ImagePipeSurfaceCreateInfoFUCHSIA,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct FuchsiaImagepipeSurfaceFn {
    pub create_image_pipe_surface_fuchsia: extern "system" fn(
        instance: Instance,
        p_create_info: *const ImagePipeSurfaceCreateInfoFUCHSIA,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for FuchsiaImagepipeSurfaceFn {}
unsafe impl Sync for FuchsiaImagepipeSurfaceFn {}
impl ::std::clone::Clone for FuchsiaImagepipeSurfaceFn {
    fn clone(&self) -> Self {
        FuchsiaImagepipeSurfaceFn {
            create_image_pipe_surface_fuchsia: self.create_image_pipe_surface_fuchsia,
        }
    }
}
impl FuchsiaImagepipeSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaImagepipeSurfaceFn {
            create_image_pipe_surface_fuchsia: unsafe {
                extern "system" fn create_image_pipe_surface_fuchsia(
                    _instance: Instance,
                    _p_create_info: *const ImagePipeSurfaceCreateInfoFUCHSIA,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_image_pipe_surface_fuchsia)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateImagePipeSurfaceFUCHSIA\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_image_pipe_surface_fuchsia
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateImagePipeSurfaceFUCHSIA.html>"]
    pub unsafe fn create_image_pipe_surface_fuchsia(
        &self,
        instance: Instance,
        p_create_info: *const ImagePipeSurfaceCreateInfoFUCHSIA,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_image_pipe_surface_fuchsia)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_FUCHSIA_imagepipe_surface'"]
impl StructureType {
    pub const IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA: Self = Self(1_000_214_000);
}
impl KhrShaderTerminateInvocationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_terminate_invocation\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrShaderTerminateInvocationFn {}
unsafe impl Send for KhrShaderTerminateInvocationFn {}
unsafe impl Sync for KhrShaderTerminateInvocationFn {}
impl ::std::clone::Clone for KhrShaderTerminateInvocationFn {
    fn clone(&self) -> Self {
        KhrShaderTerminateInvocationFn {}
    }
}
impl KhrShaderTerminateInvocationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderTerminateInvocationFn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_terminate_invocation'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES_KHR: Self = Self(1_000_215_000);
}
impl GoogleExtension217Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_extension_217\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct GoogleExtension217Fn {}
unsafe impl Send for GoogleExtension217Fn {}
unsafe impl Sync for GoogleExtension217Fn {}
impl ::std::clone::Clone for GoogleExtension217Fn {
    fn clone(&self) -> Self {
        GoogleExtension217Fn {}
    }
}
impl GoogleExtension217Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleExtension217Fn {}
    }
}
impl ExtMetalSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_metal_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateMetalSurfaceEXT = extern "system" fn(
    instance: Instance,
    p_create_info: *const MetalSurfaceCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct ExtMetalSurfaceFn {
    pub create_metal_surface_ext: extern "system" fn(
        instance: Instance,
        p_create_info: *const MetalSurfaceCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for ExtMetalSurfaceFn {}
unsafe impl Sync for ExtMetalSurfaceFn {}
impl ::std::clone::Clone for ExtMetalSurfaceFn {
    fn clone(&self) -> Self {
        ExtMetalSurfaceFn {
            create_metal_surface_ext: self.create_metal_surface_ext,
        }
    }
}
impl ExtMetalSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtMetalSurfaceFn {
            create_metal_surface_ext: unsafe {
                extern "system" fn create_metal_surface_ext(
                    _instance: Instance,
                    _p_create_info: *const MetalSurfaceCreateInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_metal_surface_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateMetalSurfaceEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    create_metal_surface_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateMetalSurfaceEXT.html>"]
    pub unsafe fn create_metal_surface_ext(
        &self,
        instance: Instance,
        p_create_info: *const MetalSurfaceCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_metal_surface_ext)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_EXT_metal_surface'"]
impl StructureType {
    pub const METAL_SURFACE_CREATE_INFO_EXT: Self = Self(1_000_217_000);
}
impl ExtFragmentDensityMapFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_fragment_density_map\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtFragmentDensityMapFn {}
unsafe impl Send for ExtFragmentDensityMapFn {}
unsafe impl Sync for ExtFragmentDensityMapFn {}
impl ::std::clone::Clone for ExtFragmentDensityMapFn {
    fn clone(&self) -> Self {
        ExtFragmentDensityMapFn {}
    }
}
impl ExtFragmentDensityMapFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtFragmentDensityMapFn {}
    }
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT: Self = Self(1_000_218_000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT: Self = Self(1_000_218_001);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl StructureType {
    pub const RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT: Self = Self(1_000_218_002);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl ImageCreateFlags {
    pub const SUBSAMPLED_EXT: Self = Self(0b100_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl ImageLayout {
    pub const FRAGMENT_DENSITY_MAP_OPTIMAL_EXT: Self = Self(1_000_218_000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl AccessFlags {
    pub const FRAGMENT_DENSITY_MAP_READ_EXT: Self = Self(0b1_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl FormatFeatureFlags {
    pub const FRAGMENT_DENSITY_MAP_EXT: Self = Self(0b1_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl ImageUsageFlags {
    pub const FRAGMENT_DENSITY_MAP_EXT: Self = Self(0b10_0000_0000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl ImageViewCreateFlags {
    pub const FRAGMENT_DENSITY_MAP_DYNAMIC_EXT: Self = Self(0b1);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl PipelineStageFlags {
    pub const FRAGMENT_DENSITY_PROCESS_EXT: Self = Self(0b1000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl SamplerCreateFlags {
    pub const SUBSAMPLED_EXT: Self = Self(0b1);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map'"]
impl SamplerCreateFlags {
    pub const SUBSAMPLED_COARSE_RECONSTRUCTION_EXT: Self = Self(0b10);
}
impl ExtExtension220Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_220\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension220Fn {}
unsafe impl Send for ExtExtension220Fn {}
unsafe impl Sync for ExtExtension220Fn {}
impl ::std::clone::Clone for ExtExtension220Fn {
    fn clone(&self) -> Self {
        ExtExtension220Fn {}
    }
}
impl ExtExtension220Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension220Fn {}
    }
}
impl KhrExtension221Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_221\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension221Fn {}
unsafe impl Send for KhrExtension221Fn {}
unsafe impl Sync for KhrExtension221Fn {}
impl ::std::clone::Clone for KhrExtension221Fn {
    fn clone(&self) -> Self {
        KhrExtension221Fn {}
    }
}
impl KhrExtension221Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension221Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_extension_221'"]
impl RenderPassCreateFlags {
    pub const RESERVED_0_KHR: Self = Self(0b1);
}
impl ExtScalarBlockLayoutFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_scalar_block_layout\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtScalarBlockLayoutFn {}
unsafe impl Send for ExtScalarBlockLayoutFn {}
unsafe impl Sync for ExtScalarBlockLayoutFn {}
impl ::std::clone::Clone for ExtScalarBlockLayoutFn {
    fn clone(&self) -> Self {
        ExtScalarBlockLayoutFn {}
    }
}
impl ExtScalarBlockLayoutFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtScalarBlockLayoutFn {}
    }
}
#[doc = "Generated from 'VK_EXT_scalar_block_layout'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: Self =
        StructureType::PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
}
impl ExtExtension223Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_223\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension223Fn {}
unsafe impl Send for ExtExtension223Fn {}
unsafe impl Sync for ExtExtension223Fn {}
impl ::std::clone::Clone for ExtExtension223Fn {
    fn clone(&self) -> Self {
        ExtExtension223Fn {}
    }
}
impl ExtExtension223Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension223Fn {}
    }
}
impl GoogleHlslFunctionality1Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_hlsl_functionality1\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct GoogleHlslFunctionality1Fn {}
unsafe impl Send for GoogleHlslFunctionality1Fn {}
unsafe impl Sync for GoogleHlslFunctionality1Fn {}
impl ::std::clone::Clone for GoogleHlslFunctionality1Fn {
    fn clone(&self) -> Self {
        GoogleHlslFunctionality1Fn {}
    }
}
impl GoogleHlslFunctionality1Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleHlslFunctionality1Fn {}
    }
}
impl GoogleDecorateStringFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_decorate_string\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct GoogleDecorateStringFn {}
unsafe impl Send for GoogleDecorateStringFn {}
unsafe impl Sync for GoogleDecorateStringFn {}
impl ::std::clone::Clone for GoogleDecorateStringFn {
    fn clone(&self) -> Self {
        GoogleDecorateStringFn {}
    }
}
impl GoogleDecorateStringFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleDecorateStringFn {}
    }
}
impl ExtSubgroupSizeControlFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_subgroup_size_control\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtSubgroupSizeControlFn {}
unsafe impl Send for ExtSubgroupSizeControlFn {}
unsafe impl Sync for ExtSubgroupSizeControlFn {}
impl ::std::clone::Clone for ExtSubgroupSizeControlFn {
    fn clone(&self) -> Self {
        ExtSubgroupSizeControlFn {}
    }
}
impl ExtSubgroupSizeControlFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtSubgroupSizeControlFn {}
    }
}
#[doc = "Generated from 'VK_EXT_subgroup_size_control'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT: Self = Self(1_000_225_000);
}
#[doc = "Generated from 'VK_EXT_subgroup_size_control'"]
impl StructureType {
    pub const PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT: Self =
        Self(1_000_225_001);
}
#[doc = "Generated from 'VK_EXT_subgroup_size_control'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT: Self = Self(1_000_225_002);
}
#[doc = "Generated from 'VK_EXT_subgroup_size_control'"]
impl PipelineShaderStageCreateFlags {
    pub const ALLOW_VARYING_SUBGROUP_SIZE_EXT: Self = Self(0b1);
}
#[doc = "Generated from 'VK_EXT_subgroup_size_control'"]
impl PipelineShaderStageCreateFlags {
    pub const REQUIRE_FULL_SUBGROUPS_EXT: Self = Self(0b10);
}
impl KhrFragmentShadingRateFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_fragment_shading_rate\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR = extern "system" fn(
    physical_device: PhysicalDevice,
    p_fragment_shading_rate_count: *mut u32,
    p_fragment_shading_rates: *mut PhysicalDeviceFragmentShadingRateKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetFragmentShadingRateKHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_fragment_size: *const Extent2D,
    combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
);
pub struct KhrFragmentShadingRateFn {
    pub get_physical_device_fragment_shading_rates_khr: extern "system" fn(
        physical_device: PhysicalDevice,
        p_fragment_shading_rate_count: *mut u32,
        p_fragment_shading_rates: *mut PhysicalDeviceFragmentShadingRateKHR,
    ) -> Result,
    pub cmd_set_fragment_shading_rate_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_fragment_size: *const Extent2D,
        combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
    ),
}
unsafe impl Send for KhrFragmentShadingRateFn {}
unsafe impl Sync for KhrFragmentShadingRateFn {}
impl ::std::clone::Clone for KhrFragmentShadingRateFn {
    fn clone(&self) -> Self {
        KhrFragmentShadingRateFn {
            get_physical_device_fragment_shading_rates_khr: self
                .get_physical_device_fragment_shading_rates_khr,
            cmd_set_fragment_shading_rate_khr: self.cmd_set_fragment_shading_rate_khr,
        }
    }
}
impl KhrFragmentShadingRateFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrFragmentShadingRateFn {
            get_physical_device_fragment_shading_rates_khr: unsafe {
                extern "system" fn get_physical_device_fragment_shading_rates_khr(
                    _physical_device: PhysicalDevice,
                    _p_fragment_shading_rate_count: *mut u32,
                    _p_fragment_shading_rates: *mut PhysicalDeviceFragmentShadingRateKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_fragment_shading_rates_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceFragmentShadingRatesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_fragment_shading_rates_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_fragment_shading_rate_khr: unsafe {
                extern "system" fn cmd_set_fragment_shading_rate_khr(
                    _command_buffer: CommandBuffer,
                    _p_fragment_size: *const Extent2D,
                    _combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_fragment_shading_rate_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetFragmentShadingRateKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_fragment_shading_rate_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFragmentShadingRatesKHR.html>"]
    pub unsafe fn get_physical_device_fragment_shading_rates_khr(
        &self,
        physical_device: PhysicalDevice,
        p_fragment_shading_rate_count: *mut u32,
        p_fragment_shading_rates: *mut PhysicalDeviceFragmentShadingRateKHR,
    ) -> Result {
        (self.get_physical_device_fragment_shading_rates_khr)(
            physical_device,
            p_fragment_shading_rate_count,
            p_fragment_shading_rates,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetFragmentShadingRateKHR.html>"]
    pub unsafe fn cmd_set_fragment_shading_rate_khr(
        &self,
        command_buffer: CommandBuffer,
        p_fragment_size: *const Extent2D,
        combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
    ) {
        (self.cmd_set_fragment_shading_rate_khr)(command_buffer, p_fragment_size, combiner_ops)
    }
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl ImageLayout {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR: Self =
        ImageLayout::SHADING_RATE_OPTIMAL_NV;
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl DynamicState {
    pub const FRAGMENT_SHADING_RATE_KHR: Self = Self(1_000_226_000);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl StructureType {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR: Self = Self(1_000_226_000);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl StructureType {
    pub const PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR: Self = Self(1_000_226_001);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR: Self = Self(1_000_226_002);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR: Self = Self(1_000_226_003);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR: Self = Self(1_000_226_004);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl AccessFlags {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_READ_KHR: Self =
        AccessFlags::SHADING_RATE_IMAGE_READ_NV;
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl ImageUsageFlags {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_KHR: Self = ImageUsageFlags::SHADING_RATE_IMAGE_NV;
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl PipelineStageFlags {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_KHR: Self =
        PipelineStageFlags::SHADING_RATE_IMAGE_NV;
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl FormatFeatureFlags {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_KHR: Self =
        Self(0b100_0000_0000_0000_0000_0000_0000_0000);
}
impl AmdShaderCoreProperties2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_shader_core_properties2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdShaderCoreProperties2Fn {}
unsafe impl Send for AmdShaderCoreProperties2Fn {}
unsafe impl Sync for AmdShaderCoreProperties2Fn {}
impl ::std::clone::Clone for AmdShaderCoreProperties2Fn {
    fn clone(&self) -> Self {
        AmdShaderCoreProperties2Fn {}
    }
}
impl AmdShaderCoreProperties2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderCoreProperties2Fn {}
    }
}
#[doc = "Generated from 'VK_AMD_shader_core_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD: Self = Self(1_000_227_000);
}
impl AmdExtension229Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_229\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension229Fn {}
unsafe impl Send for AmdExtension229Fn {}
unsafe impl Sync for AmdExtension229Fn {}
impl ::std::clone::Clone for AmdExtension229Fn {
    fn clone(&self) -> Self {
        AmdExtension229Fn {}
    }
}
impl AmdExtension229Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension229Fn {}
    }
}
impl AmdDeviceCoherentMemoryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_device_coherent_memory\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct AmdDeviceCoherentMemoryFn {}
unsafe impl Send for AmdDeviceCoherentMemoryFn {}
unsafe impl Sync for AmdDeviceCoherentMemoryFn {}
impl ::std::clone::Clone for AmdDeviceCoherentMemoryFn {
    fn clone(&self) -> Self {
        AmdDeviceCoherentMemoryFn {}
    }
}
impl AmdDeviceCoherentMemoryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdDeviceCoherentMemoryFn {}
    }
}
#[doc = "Generated from 'VK_AMD_device_coherent_memory'"]
impl MemoryPropertyFlags {
    pub const DEVICE_COHERENT_AMD: Self = Self(0b100_0000);
}
#[doc = "Generated from 'VK_AMD_device_coherent_memory'"]
impl MemoryPropertyFlags {
    pub const DEVICE_UNCACHED_AMD: Self = Self(0b1000_0000);
}
#[doc = "Generated from 'VK_AMD_device_coherent_memory'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD: Self = Self(1_000_229_000);
}
impl AmdExtension231Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_231\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension231Fn {}
unsafe impl Send for AmdExtension231Fn {}
unsafe impl Sync for AmdExtension231Fn {}
impl ::std::clone::Clone for AmdExtension231Fn {
    fn clone(&self) -> Self {
        AmdExtension231Fn {}
    }
}
impl AmdExtension231Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension231Fn {}
    }
}
impl AmdExtension232Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_232\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension232Fn {}
unsafe impl Send for AmdExtension232Fn {}
unsafe impl Sync for AmdExtension232Fn {}
impl ::std::clone::Clone for AmdExtension232Fn {
    fn clone(&self) -> Self {
        AmdExtension232Fn {}
    }
}
impl AmdExtension232Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension232Fn {}
    }
}
impl AmdExtension233Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_233\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension233Fn {}
unsafe impl Send for AmdExtension233Fn {}
unsafe impl Sync for AmdExtension233Fn {}
impl ::std::clone::Clone for AmdExtension233Fn {
    fn clone(&self) -> Self {
        AmdExtension233Fn {}
    }
}
impl AmdExtension233Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension233Fn {}
    }
}
impl AmdExtension234Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_234\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension234Fn {}
unsafe impl Send for AmdExtension234Fn {}
unsafe impl Sync for AmdExtension234Fn {}
impl ::std::clone::Clone for AmdExtension234Fn {
    fn clone(&self) -> Self {
        AmdExtension234Fn {}
    }
}
impl AmdExtension234Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension234Fn {}
    }
}
impl ExtShaderImageAtomicInt64Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_image_atomic_int64\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtShaderImageAtomicInt64Fn {}
unsafe impl Send for ExtShaderImageAtomicInt64Fn {}
unsafe impl Sync for ExtShaderImageAtomicInt64Fn {}
impl ::std::clone::Clone for ExtShaderImageAtomicInt64Fn {
    fn clone(&self) -> Self {
        ExtShaderImageAtomicInt64Fn {}
    }
}
impl ExtShaderImageAtomicInt64Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderImageAtomicInt64Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_shader_image_atomic_int64'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT: Self = Self(1_000_234_000);
}
impl AmdExtension236Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_236\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension236Fn {}
unsafe impl Send for AmdExtension236Fn {}
unsafe impl Sync for AmdExtension236Fn {}
impl ::std::clone::Clone for AmdExtension236Fn {
    fn clone(&self) -> Self {
        AmdExtension236Fn {}
    }
}
impl AmdExtension236Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension236Fn {}
    }
}
impl KhrSpirv14Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_spirv_1_4\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrSpirv14Fn {}
unsafe impl Send for KhrSpirv14Fn {}
unsafe impl Sync for KhrSpirv14Fn {}
impl ::std::clone::Clone for KhrSpirv14Fn {
    fn clone(&self) -> Self {
        KhrSpirv14Fn {}
    }
}
impl KhrSpirv14Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSpirv14Fn {}
    }
}
impl ExtMemoryBudgetFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_memory_budget\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtMemoryBudgetFn {}
unsafe impl Send for ExtMemoryBudgetFn {}
unsafe impl Sync for ExtMemoryBudgetFn {}
impl ::std::clone::Clone for ExtMemoryBudgetFn {
    fn clone(&self) -> Self {
        ExtMemoryBudgetFn {}
    }
}
impl ExtMemoryBudgetFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtMemoryBudgetFn {}
    }
}
#[doc = "Generated from 'VK_EXT_memory_budget'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT: Self = Self(1_000_237_000);
}
impl ExtMemoryPriorityFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_memory_priority\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtMemoryPriorityFn {}
unsafe impl Send for ExtMemoryPriorityFn {}
unsafe impl Sync for ExtMemoryPriorityFn {}
impl ::std::clone::Clone for ExtMemoryPriorityFn {
    fn clone(&self) -> Self {
        ExtMemoryPriorityFn {}
    }
}
impl ExtMemoryPriorityFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtMemoryPriorityFn {}
    }
}
#[doc = "Generated from 'VK_EXT_memory_priority'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT: Self = Self(1_000_238_000);
}
#[doc = "Generated from 'VK_EXT_memory_priority'"]
impl StructureType {
    pub const MEMORY_PRIORITY_ALLOCATE_INFO_EXT: Self = Self(1_000_238_001);
}
impl KhrSurfaceProtectedCapabilitiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_surface_protected_capabilities\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrSurfaceProtectedCapabilitiesFn {}
unsafe impl Send for KhrSurfaceProtectedCapabilitiesFn {}
unsafe impl Sync for KhrSurfaceProtectedCapabilitiesFn {}
impl ::std::clone::Clone for KhrSurfaceProtectedCapabilitiesFn {
    fn clone(&self) -> Self {
        KhrSurfaceProtectedCapabilitiesFn {}
    }
}
impl KhrSurfaceProtectedCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSurfaceProtectedCapabilitiesFn {}
    }
}
#[doc = "Generated from 'VK_KHR_surface_protected_capabilities'"]
impl StructureType {
    pub const SURFACE_PROTECTED_CAPABILITIES_KHR: Self = Self(1_000_239_000);
}
impl NvDedicatedAllocationImageAliasingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_dedicated_allocation_image_aliasing\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvDedicatedAllocationImageAliasingFn {}
unsafe impl Send for NvDedicatedAllocationImageAliasingFn {}
unsafe impl Sync for NvDedicatedAllocationImageAliasingFn {}
impl ::std::clone::Clone for NvDedicatedAllocationImageAliasingFn {
    fn clone(&self) -> Self {
        NvDedicatedAllocationImageAliasingFn {}
    }
}
impl NvDedicatedAllocationImageAliasingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvDedicatedAllocationImageAliasingFn {}
    }
}
#[doc = "Generated from 'VK_NV_dedicated_allocation_image_aliasing'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV: Self =
        Self(1_000_240_000);
}
impl KhrSeparateDepthStencilLayoutsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_separate_depth_stencil_layouts\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrSeparateDepthStencilLayoutsFn {}
unsafe impl Send for KhrSeparateDepthStencilLayoutsFn {}
unsafe impl Sync for KhrSeparateDepthStencilLayoutsFn {}
impl ::std::clone::Clone for KhrSeparateDepthStencilLayoutsFn {
    fn clone(&self) -> Self {
        KhrSeparateDepthStencilLayoutsFn {}
    }
}
impl KhrSeparateDepthStencilLayoutsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSeparateDepthStencilLayoutsFn {}
    }
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl StructureType {
    pub const ATTACHMENT_REFERENCE_STENCIL_LAYOUT_KHR: Self =
        StructureType::ATTACHMENT_REFERENCE_STENCIL_LAYOUT;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl StructureType {
    pub const ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT_KHR: Self =
        StructureType::ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const DEPTH_ATTACHMENT_OPTIMAL_KHR: Self = ImageLayout::DEPTH_ATTACHMENT_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const DEPTH_READ_ONLY_OPTIMAL_KHR: Self = ImageLayout::DEPTH_READ_ONLY_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const STENCIL_ATTACHMENT_OPTIMAL_KHR: Self = ImageLayout::STENCIL_ATTACHMENT_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const STENCIL_READ_ONLY_OPTIMAL_KHR: Self = ImageLayout::STENCIL_READ_ONLY_OPTIMAL;
}
impl IntelExtension243Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_extension_243\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct IntelExtension243Fn {}
unsafe impl Send for IntelExtension243Fn {}
unsafe impl Sync for IntelExtension243Fn {}
impl ::std::clone::Clone for IntelExtension243Fn {
    fn clone(&self) -> Self {
        IntelExtension243Fn {}
    }
}
impl IntelExtension243Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelExtension243Fn {}
    }
}
impl MesaExtension244Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_MESA_extension_244\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct MesaExtension244Fn {}
unsafe impl Send for MesaExtension244Fn {}
unsafe impl Sync for MesaExtension244Fn {}
impl ::std::clone::Clone for MesaExtension244Fn {
    fn clone(&self) -> Self {
        MesaExtension244Fn {}
    }
}
impl MesaExtension244Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        MesaExtension244Fn {}
    }
}
impl ExtBufferDeviceAddressFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_buffer_device_address\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetBufferDeviceAddress =
    extern "system" fn(device: Device, p_info: *const BufferDeviceAddressInfo) -> DeviceAddress;
pub struct ExtBufferDeviceAddressFn {
    pub get_buffer_device_address_ext:
        extern "system" fn(device: Device, p_info: *const BufferDeviceAddressInfo) -> DeviceAddress,
}
unsafe impl Send for ExtBufferDeviceAddressFn {}
unsafe impl Sync for ExtBufferDeviceAddressFn {}
impl ::std::clone::Clone for ExtBufferDeviceAddressFn {
    fn clone(&self) -> Self {
        ExtBufferDeviceAddressFn {
            get_buffer_device_address_ext: self.get_buffer_device_address_ext,
        }
    }
}
impl ExtBufferDeviceAddressFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtBufferDeviceAddressFn {
            get_buffer_device_address_ext: unsafe {
                extern "system" fn get_buffer_device_address_ext(
                    _device: Device,
                    _p_info: *const BufferDeviceAddressInfo,
                ) -> DeviceAddress {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_device_address_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetBufferDeviceAddressEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_device_address_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferDeviceAddressEXT.html>"]
    pub unsafe fn get_buffer_device_address_ext(
        &self,
        device: Device,
        p_info: *const BufferDeviceAddressInfo,
    ) -> DeviceAddress {
        (self.get_buffer_device_address_ext)(device, p_info)
    }
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT: Self = Self(1_000_244_000);
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT: Self =
        StructureType::PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_DEVICE_ADDRESS_INFO_EXT: Self = StructureType::BUFFER_DEVICE_ADDRESS_INFO;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT: Self = Self(1_000_244_002);
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl BufferUsageFlags {
    pub const SHADER_DEVICE_ADDRESS_EXT: Self = BufferUsageFlags::SHADER_DEVICE_ADDRESS;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl BufferCreateFlags {
    pub const DEVICE_ADDRESS_CAPTURE_REPLAY_EXT: Self =
        BufferCreateFlags::DEVICE_ADDRESS_CAPTURE_REPLAY;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl Result {
    pub const ERROR_INVALID_DEVICE_ADDRESS_EXT: Self = Result::ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
}
impl ExtToolingInfoFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_tooling_info\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceToolPropertiesEXT = extern "system" fn(
    physical_device: PhysicalDevice,
    p_tool_count: *mut u32,
    p_tool_properties: *mut PhysicalDeviceToolPropertiesEXT,
) -> Result;
pub struct ExtToolingInfoFn {
    pub get_physical_device_tool_properties_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        p_tool_count: *mut u32,
        p_tool_properties: *mut PhysicalDeviceToolPropertiesEXT,
    ) -> Result,
}
unsafe impl Send for ExtToolingInfoFn {}
unsafe impl Sync for ExtToolingInfoFn {}
impl ::std::clone::Clone for ExtToolingInfoFn {
    fn clone(&self) -> Self {
        ExtToolingInfoFn {
            get_physical_device_tool_properties_ext: self.get_physical_device_tool_properties_ext,
        }
    }
}
impl ExtToolingInfoFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtToolingInfoFn {
            get_physical_device_tool_properties_ext: unsafe {
                extern "system" fn get_physical_device_tool_properties_ext(
                    _physical_device: PhysicalDevice,
                    _p_tool_count: *mut u32,
                    _p_tool_properties: *mut PhysicalDeviceToolPropertiesEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_tool_properties_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceToolPropertiesEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_tool_properties_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceToolPropertiesEXT.html>"]
    pub unsafe fn get_physical_device_tool_properties_ext(
        &self,
        physical_device: PhysicalDevice,
        p_tool_count: *mut u32,
        p_tool_properties: *mut PhysicalDeviceToolPropertiesEXT,
    ) -> Result {
        (self.get_physical_device_tool_properties_ext)(
            physical_device,
            p_tool_count,
            p_tool_properties,
        )
    }
}
#[doc = "Generated from 'VK_EXT_tooling_info'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT: Self = Self(1_000_245_000);
}
#[doc = "Generated from 'VK_EXT_tooling_info'"]
impl ToolPurposeFlagsEXT {
    pub const DEBUG_REPORTING: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_EXT_tooling_info'"]
impl ToolPurposeFlagsEXT {
    pub const DEBUG_MARKERS: Self = Self(0b100_0000);
}
impl ExtSeparateStencilUsageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_separate_stencil_usage\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtSeparateStencilUsageFn {}
unsafe impl Send for ExtSeparateStencilUsageFn {}
unsafe impl Sync for ExtSeparateStencilUsageFn {}
impl ::std::clone::Clone for ExtSeparateStencilUsageFn {
    fn clone(&self) -> Self {
        ExtSeparateStencilUsageFn {}
    }
}
impl ExtSeparateStencilUsageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtSeparateStencilUsageFn {}
    }
}
#[doc = "Generated from 'VK_EXT_separate_stencil_usage'"]
impl StructureType {
    pub const IMAGE_STENCIL_USAGE_CREATE_INFO_EXT: Self =
        StructureType::IMAGE_STENCIL_USAGE_CREATE_INFO;
}
impl ExtValidationFeaturesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_validation_features\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
pub struct ExtValidationFeaturesFn {}
unsafe impl Send for ExtValidationFeaturesFn {}
unsafe impl Sync for ExtValidationFeaturesFn {}
impl ::std::clone::Clone for ExtValidationFeaturesFn {
    fn clone(&self) -> Self {
        ExtValidationFeaturesFn {}
    }
}
impl ExtValidationFeaturesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtValidationFeaturesFn {}
    }
}
#[doc = "Generated from 'VK_EXT_validation_features'"]
impl StructureType {
    pub const VALIDATION_FEATURES_EXT: Self = Self(1_000_247_000);
}
impl KhrExtension249Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_249\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension249Fn {}
unsafe impl Send for KhrExtension249Fn {}
unsafe impl Sync for KhrExtension249Fn {}
impl ::std::clone::Clone for KhrExtension249Fn {
    fn clone(&self) -> Self {
        KhrExtension249Fn {}
    }
}
impl KhrExtension249Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension249Fn {}
    }
}
impl NvCooperativeMatrixFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_cooperative_matrix\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut CooperativeMatrixPropertiesNV,
) -> Result;
pub struct NvCooperativeMatrixFn {
    pub get_physical_device_cooperative_matrix_properties_nv: extern "system" fn(
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut CooperativeMatrixPropertiesNV,
    ) -> Result,
}
unsafe impl Send for NvCooperativeMatrixFn {}
unsafe impl Sync for NvCooperativeMatrixFn {}
impl ::std::clone::Clone for NvCooperativeMatrixFn {
    fn clone(&self) -> Self {
        NvCooperativeMatrixFn {
            get_physical_device_cooperative_matrix_properties_nv: self
                .get_physical_device_cooperative_matrix_properties_nv,
        }
    }
}
impl NvCooperativeMatrixFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvCooperativeMatrixFn {
            get_physical_device_cooperative_matrix_properties_nv: unsafe {
                extern "system" fn get_physical_device_cooperative_matrix_properties_nv(
                    _physical_device: PhysicalDevice,
                    _p_property_count: *mut u32,
                    _p_properties: *mut CooperativeMatrixPropertiesNV,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_cooperative_matrix_properties_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceCooperativeMatrixPropertiesNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_cooperative_matrix_properties_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceCooperativeMatrixPropertiesNV.html>"]
    pub unsafe fn get_physical_device_cooperative_matrix_properties_nv(
        &self,
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut CooperativeMatrixPropertiesNV,
    ) -> Result {
        (self.get_physical_device_cooperative_matrix_properties_nv)(
            physical_device,
            p_property_count,
            p_properties,
        )
    }
}
#[doc = "Generated from 'VK_NV_cooperative_matrix'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV: Self = Self(1_000_249_000);
}
#[doc = "Generated from 'VK_NV_cooperative_matrix'"]
impl StructureType {
    pub const COOPERATIVE_MATRIX_PROPERTIES_NV: Self = Self(1_000_249_001);
}
#[doc = "Generated from 'VK_NV_cooperative_matrix'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV: Self = Self(1_000_249_002);
}
impl NvCoverageReductionModeFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_coverage_reduction_mode\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV =
    extern "system" fn(
        physical_device: PhysicalDevice,
        p_combination_count: *mut u32,
        p_combinations: *mut FramebufferMixedSamplesCombinationNV,
    ) -> Result;
pub struct NvCoverageReductionModeFn {
    pub get_physical_device_supported_framebuffer_mixed_samples_combinations_nv:
        extern "system" fn(
            physical_device: PhysicalDevice,
            p_combination_count: *mut u32,
            p_combinations: *mut FramebufferMixedSamplesCombinationNV,
        ) -> Result,
}
unsafe impl Send for NvCoverageReductionModeFn {}
unsafe impl Sync for NvCoverageReductionModeFn {}
impl ::std::clone::Clone for NvCoverageReductionModeFn {
    fn clone(&self) -> Self {
        NvCoverageReductionModeFn {
            get_physical_device_supported_framebuffer_mixed_samples_combinations_nv: self
                .get_physical_device_supported_framebuffer_mixed_samples_combinations_nv,
        }
    }
}
impl NvCoverageReductionModeFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvCoverageReductionModeFn {
            get_physical_device_supported_framebuffer_mixed_samples_combinations_nv: unsafe {
                extern "system" fn get_physical_device_supported_framebuffer_mixed_samples_combinations_nv(
                    _physical_device: PhysicalDevice,
                    _p_combination_count: *mut u32,
                    _p_combinations: *mut FramebufferMixedSamplesCombinationNV,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(
                            get_physical_device_supported_framebuffer_mixed_samples_combinations_nv
                        )
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_supported_framebuffer_mixed_samples_combinations_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV.html>"]
    pub unsafe fn get_physical_device_supported_framebuffer_mixed_samples_combinations_nv(
        &self,
        physical_device: PhysicalDevice,
        p_combination_count: *mut u32,
        p_combinations: *mut FramebufferMixedSamplesCombinationNV,
    ) -> Result {
        (self.get_physical_device_supported_framebuffer_mixed_samples_combinations_nv)(
            physical_device,
            p_combination_count,
            p_combinations,
        )
    }
}
#[doc = "Generated from 'VK_NV_coverage_reduction_mode'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV: Self = Self(1_000_250_000);
}
#[doc = "Generated from 'VK_NV_coverage_reduction_mode'"]
impl StructureType {
    pub const PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV: Self = Self(1_000_250_001);
}
#[doc = "Generated from 'VK_NV_coverage_reduction_mode'"]
impl StructureType {
    pub const FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV: Self = Self(1_000_250_002);
}
impl ExtFragmentShaderInterlockFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_fragment_shader_interlock\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtFragmentShaderInterlockFn {}
unsafe impl Send for ExtFragmentShaderInterlockFn {}
unsafe impl Sync for ExtFragmentShaderInterlockFn {}
impl ::std::clone::Clone for ExtFragmentShaderInterlockFn {
    fn clone(&self) -> Self {
        ExtFragmentShaderInterlockFn {}
    }
}
impl ExtFragmentShaderInterlockFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtFragmentShaderInterlockFn {}
    }
}
#[doc = "Generated from 'VK_EXT_fragment_shader_interlock'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT: Self = Self(1_000_251_000);
}
impl ExtYcbcrImageArraysFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_ycbcr_image_arrays\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtYcbcrImageArraysFn {}
unsafe impl Send for ExtYcbcrImageArraysFn {}
unsafe impl Sync for ExtYcbcrImageArraysFn {}
impl ::std::clone::Clone for ExtYcbcrImageArraysFn {
    fn clone(&self) -> Self {
        ExtYcbcrImageArraysFn {}
    }
}
impl ExtYcbcrImageArraysFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtYcbcrImageArraysFn {}
    }
}
#[doc = "Generated from 'VK_EXT_ycbcr_image_arrays'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT: Self = Self(1_000_252_000);
}
impl KhrUniformBufferStandardLayoutFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_uniform_buffer_standard_layout\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrUniformBufferStandardLayoutFn {}
unsafe impl Send for KhrUniformBufferStandardLayoutFn {}
unsafe impl Sync for KhrUniformBufferStandardLayoutFn {}
impl ::std::clone::Clone for KhrUniformBufferStandardLayoutFn {
    fn clone(&self) -> Self {
        KhrUniformBufferStandardLayoutFn {}
    }
}
impl KhrUniformBufferStandardLayoutFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrUniformBufferStandardLayoutFn {}
    }
}
#[doc = "Generated from 'VK_KHR_uniform_buffer_standard_layout'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES;
}
impl ExtExtension255Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_255\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension255Fn {}
unsafe impl Send for ExtExtension255Fn {}
unsafe impl Sync for ExtExtension255Fn {}
impl ::std::clone::Clone for ExtExtension255Fn {
    fn clone(&self) -> Self {
        ExtExtension255Fn {}
    }
}
impl ExtExtension255Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension255Fn {}
    }
}
impl ExtFullScreenExclusiveFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_full_screen_exclusive\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT = extern "system" fn(
    physical_device: PhysicalDevice,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_present_mode_count: *mut u32,
    p_present_modes: *mut PresentModeKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireFullScreenExclusiveModeEXT =
    extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkReleaseFullScreenExclusiveModeEXT =
    extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupSurfacePresentModes2EXT = extern "system" fn(
    device: Device,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_modes: *mut DeviceGroupPresentModeFlagsKHR,
) -> Result;
pub struct ExtFullScreenExclusiveFn {
    pub get_physical_device_surface_present_modes2_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_present_mode_count: *mut u32,
        p_present_modes: *mut PresentModeKHR,
    ) -> Result,
    pub acquire_full_screen_exclusive_mode_ext:
        extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result,
    pub release_full_screen_exclusive_mode_ext:
        extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result,
    pub get_device_group_surface_present_modes2_ext: extern "system" fn(
        device: Device,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_modes: *mut DeviceGroupPresentModeFlagsKHR,
    ) -> Result,
}
unsafe impl Send for ExtFullScreenExclusiveFn {}
unsafe impl Sync for ExtFullScreenExclusiveFn {}
impl ::std::clone::Clone for ExtFullScreenExclusiveFn {
    fn clone(&self) -> Self {
        ExtFullScreenExclusiveFn {
            get_physical_device_surface_present_modes2_ext: self
                .get_physical_device_surface_present_modes2_ext,
            acquire_full_screen_exclusive_mode_ext: self.acquire_full_screen_exclusive_mode_ext,
            release_full_screen_exclusive_mode_ext: self.release_full_screen_exclusive_mode_ext,
            get_device_group_surface_present_modes2_ext: self
                .get_device_group_surface_present_modes2_ext,
        }
    }
}
impl ExtFullScreenExclusiveFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtFullScreenExclusiveFn {
            get_physical_device_surface_present_modes2_ext: unsafe {
                extern "system" fn get_physical_device_surface_present_modes2_ext(
                    _physical_device: PhysicalDevice,
                    _p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
                    _p_present_mode_count: *mut u32,
                    _p_present_modes: *mut PresentModeKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_surface_present_modes2_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSurfacePresentModes2EXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_surface_present_modes2_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            acquire_full_screen_exclusive_mode_ext: unsafe {
                extern "system" fn acquire_full_screen_exclusive_mode_ext(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_full_screen_exclusive_mode_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkAcquireFullScreenExclusiveModeEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    acquire_full_screen_exclusive_mode_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            release_full_screen_exclusive_mode_ext: unsafe {
                extern "system" fn release_full_screen_exclusive_mode_ext(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(release_full_screen_exclusive_mode_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkReleaseFullScreenExclusiveModeEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    release_full_screen_exclusive_mode_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_group_surface_present_modes2_ext: unsafe {
                extern "system" fn get_device_group_surface_present_modes2_ext(
                    _device: Device,
                    _p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
                    _p_modes: *mut DeviceGroupPresentModeFlagsKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_group_surface_present_modes2_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceGroupSurfacePresentModes2EXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_group_surface_present_modes2_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSurfacePresentModes2EXT.html>"]
    pub unsafe fn get_physical_device_surface_present_modes2_ext(
        &self,
        physical_device: PhysicalDevice,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_present_mode_count: *mut u32,
        p_present_modes: *mut PresentModeKHR,
    ) -> Result {
        (self.get_physical_device_surface_present_modes2_ext)(
            physical_device,
            p_surface_info,
            p_present_mode_count,
            p_present_modes,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireFullScreenExclusiveModeEXT.html>"]
    pub unsafe fn acquire_full_screen_exclusive_mode_ext(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
    ) -> Result {
        (self.acquire_full_screen_exclusive_mode_ext)(device, swapchain)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkReleaseFullScreenExclusiveModeEXT.html>"]
    pub unsafe fn release_full_screen_exclusive_mode_ext(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
    ) -> Result {
        (self.release_full_screen_exclusive_mode_ext)(device, swapchain)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupSurfacePresentModes2EXT.html>"]
    pub unsafe fn get_device_group_surface_present_modes2_ext(
        &self,
        device: Device,
        p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
        p_modes: *mut DeviceGroupPresentModeFlagsKHR,
    ) -> Result {
        (self.get_device_group_surface_present_modes2_ext)(device, p_surface_info, p_modes)
    }
}
#[doc = "Generated from 'VK_EXT_full_screen_exclusive'"]
impl StructureType {
    pub const SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT: Self = Self(1_000_255_000);
}
#[doc = "Generated from 'VK_EXT_full_screen_exclusive'"]
impl StructureType {
    pub const SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT: Self = Self(1_000_255_002);
}
#[doc = "Generated from 'VK_EXT_full_screen_exclusive'"]
impl Result {
    pub const ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: Self = Self(-1000255000);
}
#[doc = "Generated from 'VK_EXT_full_screen_exclusive'"]
impl StructureType {
    pub const SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT: Self = Self(1_000_255_001);
}
impl ExtHeadlessSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_headless_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateHeadlessSurfaceEXT = extern "system" fn(
    instance: Instance,
    p_create_info: *const HeadlessSurfaceCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
pub struct ExtHeadlessSurfaceFn {
    pub create_headless_surface_ext: extern "system" fn(
        instance: Instance,
        p_create_info: *const HeadlessSurfaceCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
}
unsafe impl Send for ExtHeadlessSurfaceFn {}
unsafe impl Sync for ExtHeadlessSurfaceFn {}
impl ::std::clone::Clone for ExtHeadlessSurfaceFn {
    fn clone(&self) -> Self {
        ExtHeadlessSurfaceFn {
            create_headless_surface_ext: self.create_headless_surface_ext,
        }
    }
}
impl ExtHeadlessSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtHeadlessSurfaceFn {
            create_headless_surface_ext: unsafe {
                extern "system" fn create_headless_surface_ext(
                    _instance: Instance,
                    _p_create_info: *const HeadlessSurfaceCreateInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_headless_surface_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateHeadlessSurfaceEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_headless_surface_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateHeadlessSurfaceEXT.html>"]
    pub unsafe fn create_headless_surface_ext(
        &self,
        instance: Instance,
        p_create_info: *const HeadlessSurfaceCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_headless_surface_ext)(instance, p_create_info, p_allocator, p_surface)
    }
}
#[doc = "Generated from 'VK_EXT_headless_surface'"]
impl StructureType {
    pub const HEADLESS_SURFACE_CREATE_INFO_EXT: Self = Self(1_000_256_000);
}
impl KhrBufferDeviceAddressFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_buffer_device_address\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetBufferOpaqueCaptureAddress =
    extern "system" fn(device: Device, p_info: *const BufferDeviceAddressInfo) -> DeviceAddress;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceMemoryOpaqueCaptureAddress =
    extern "system" fn(device: Device, p_info: *const BufferDeviceAddressInfo) -> u64;
pub struct KhrBufferDeviceAddressFn {
    pub get_buffer_device_address_khr:
        extern "system" fn(device: Device, p_info: *const BufferDeviceAddressInfo) -> DeviceAddress,
    pub get_buffer_opaque_capture_address_khr:
        extern "system" fn(device: Device, p_info: *const BufferDeviceAddressInfo) -> u64,
    pub get_device_memory_opaque_capture_address_khr: extern "system" fn(
        device: Device,
        p_info: *const DeviceMemoryOpaqueCaptureAddressInfo,
    ) -> u64,
}
unsafe impl Send for KhrBufferDeviceAddressFn {}
unsafe impl Sync for KhrBufferDeviceAddressFn {}
impl ::std::clone::Clone for KhrBufferDeviceAddressFn {
    fn clone(&self) -> Self {
        KhrBufferDeviceAddressFn {
            get_buffer_device_address_khr: self.get_buffer_device_address_khr,
            get_buffer_opaque_capture_address_khr: self.get_buffer_opaque_capture_address_khr,
            get_device_memory_opaque_capture_address_khr: self
                .get_device_memory_opaque_capture_address_khr,
        }
    }
}
impl KhrBufferDeviceAddressFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrBufferDeviceAddressFn {
            get_buffer_device_address_khr: unsafe {
                extern "system" fn get_buffer_device_address_khr(
                    _device: Device,
                    _p_info: *const BufferDeviceAddressInfo,
                ) -> DeviceAddress {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_device_address_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetBufferDeviceAddressKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_device_address_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_buffer_opaque_capture_address_khr: unsafe {
                extern "system" fn get_buffer_opaque_capture_address_khr(
                    _device: Device,
                    _p_info: *const BufferDeviceAddressInfo,
                ) -> u64 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_opaque_capture_address_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetBufferOpaqueCaptureAddressKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_opaque_capture_address_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_memory_opaque_capture_address_khr: unsafe {
                extern "system" fn get_device_memory_opaque_capture_address_khr(
                    _device: Device,
                    _p_info: *const DeviceMemoryOpaqueCaptureAddressInfo,
                ) -> u64 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_memory_opaque_capture_address_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceMemoryOpaqueCaptureAddressKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_memory_opaque_capture_address_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferDeviceAddressKHR.html>"]
    pub unsafe fn get_buffer_device_address_khr(
        &self,
        device: Device,
        p_info: *const BufferDeviceAddressInfo,
    ) -> DeviceAddress {
        (self.get_buffer_device_address_khr)(device, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferOpaqueCaptureAddressKHR.html>"]
    pub unsafe fn get_buffer_opaque_capture_address_khr(
        &self,
        device: Device,
        p_info: *const BufferDeviceAddressInfo,
    ) -> u64 {
        (self.get_buffer_opaque_capture_address_khr)(device, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceMemoryOpaqueCaptureAddressKHR.html>"]
    pub unsafe fn get_device_memory_opaque_capture_address_khr(
        &self,
        device: Device,
        p_info: *const DeviceMemoryOpaqueCaptureAddressInfo,
    ) -> u64 {
        (self.get_device_memory_opaque_capture_address_khr)(device, p_info)
    }
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR: Self =
        StructureType::PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_DEVICE_ADDRESS_INFO_KHR: Self = StructureType::BUFFER_DEVICE_ADDRESS_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO_KHR: Self =
        StructureType::BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO_KHR: Self =
        StructureType::MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO_KHR: Self =
        StructureType::DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl BufferUsageFlags {
    pub const SHADER_DEVICE_ADDRESS_KHR: Self = BufferUsageFlags::SHADER_DEVICE_ADDRESS;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl BufferCreateFlags {
    pub const DEVICE_ADDRESS_CAPTURE_REPLAY_KHR: Self =
        BufferCreateFlags::DEVICE_ADDRESS_CAPTURE_REPLAY;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl MemoryAllocateFlags {
    pub const DEVICE_ADDRESS_KHR: Self = MemoryAllocateFlags::DEVICE_ADDRESS;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl MemoryAllocateFlags {
    pub const DEVICE_ADDRESS_CAPTURE_REPLAY_KHR: Self =
        MemoryAllocateFlags::DEVICE_ADDRESS_CAPTURE_REPLAY;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl Result {
    pub const ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR: Self =
        Result::ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
}
impl ExtExtension259Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_259\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension259Fn {}
unsafe impl Send for ExtExtension259Fn {}
unsafe impl Sync for ExtExtension259Fn {}
impl ::std::clone::Clone for ExtExtension259Fn {
    fn clone(&self) -> Self {
        ExtExtension259Fn {}
    }
}
impl ExtExtension259Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension259Fn {}
    }
}
impl ExtLineRasterizationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_line_rasterization\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetLineStippleEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    line_stipple_factor: u32,
    line_stipple_pattern: u16,
);
pub struct ExtLineRasterizationFn {
    pub cmd_set_line_stipple_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        line_stipple_factor: u32,
        line_stipple_pattern: u16,
    ),
}
unsafe impl Send for ExtLineRasterizationFn {}
unsafe impl Sync for ExtLineRasterizationFn {}
impl ::std::clone::Clone for ExtLineRasterizationFn {
    fn clone(&self) -> Self {
        ExtLineRasterizationFn {
            cmd_set_line_stipple_ext: self.cmd_set_line_stipple_ext,
        }
    }
}
impl ExtLineRasterizationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtLineRasterizationFn {
            cmd_set_line_stipple_ext: unsafe {
                extern "system" fn cmd_set_line_stipple_ext(
                    _command_buffer: CommandBuffer,
                    _line_stipple_factor: u32,
                    _line_stipple_pattern: u16,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_line_stipple_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetLineStippleEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_line_stipple_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetLineStippleEXT.html>"]
    pub unsafe fn cmd_set_line_stipple_ext(
        &self,
        command_buffer: CommandBuffer,
        line_stipple_factor: u32,
        line_stipple_pattern: u16,
    ) {
        (self.cmd_set_line_stipple_ext)(command_buffer, line_stipple_factor, line_stipple_pattern)
    }
}
#[doc = "Generated from 'VK_EXT_line_rasterization'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT: Self = Self(1_000_259_000);
}
#[doc = "Generated from 'VK_EXT_line_rasterization'"]
impl StructureType {
    pub const PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT: Self = Self(1_000_259_001);
}
#[doc = "Generated from 'VK_EXT_line_rasterization'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT: Self = Self(1_000_259_002);
}
#[doc = "Generated from 'VK_EXT_line_rasterization'"]
impl DynamicState {
    pub const LINE_STIPPLE_EXT: Self = Self(1_000_259_000);
}
impl ExtShaderAtomicFloatFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_atomic_float\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtShaderAtomicFloatFn {}
unsafe impl Send for ExtShaderAtomicFloatFn {}
unsafe impl Sync for ExtShaderAtomicFloatFn {}
impl ::std::clone::Clone for ExtShaderAtomicFloatFn {
    fn clone(&self) -> Self {
        ExtShaderAtomicFloatFn {}
    }
}
impl ExtShaderAtomicFloatFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderAtomicFloatFn {}
    }
}
#[doc = "Generated from 'VK_EXT_shader_atomic_float'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT: Self = Self(1_000_260_000);
}
impl ExtHostQueryResetFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_host_query_reset\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkResetQueryPool =
    extern "system" fn(device: Device, query_pool: QueryPool, first_query: u32, query_count: u32);
pub struct ExtHostQueryResetFn {
    pub reset_query_pool_ext: extern "system" fn(
        device: Device,
        query_pool: QueryPool,
        first_query: u32,
        query_count: u32,
    ),
}
unsafe impl Send for ExtHostQueryResetFn {}
unsafe impl Sync for ExtHostQueryResetFn {}
impl ::std::clone::Clone for ExtHostQueryResetFn {
    fn clone(&self) -> Self {
        ExtHostQueryResetFn {
            reset_query_pool_ext: self.reset_query_pool_ext,
        }
    }
}
impl ExtHostQueryResetFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtHostQueryResetFn {
            reset_query_pool_ext: unsafe {
                extern "system" fn reset_query_pool_ext(
                    _device: Device,
                    _query_pool: QueryPool,
                    _first_query: u32,
                    _query_count: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(reset_query_pool_ext)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkResetQueryPoolEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    reset_query_pool_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetQueryPoolEXT.html>"]
    pub unsafe fn reset_query_pool_ext(
        &self,
        device: Device,
        query_pool: QueryPool,
        first_query: u32,
        query_count: u32,
    ) {
        (self.reset_query_pool_ext)(device, query_pool, first_query, query_count)
    }
}
#[doc = "Generated from 'VK_EXT_host_query_reset'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT: Self =
        StructureType::PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
}
impl GgpExtension263Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_extension_263\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct GgpExtension263Fn {}
unsafe impl Send for GgpExtension263Fn {}
unsafe impl Sync for GgpExtension263Fn {}
impl ::std::clone::Clone for GgpExtension263Fn {
    fn clone(&self) -> Self {
        GgpExtension263Fn {}
    }
}
impl GgpExtension263Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpExtension263Fn {}
    }
}
impl BrcmExtension264Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_BRCM_extension_264\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct BrcmExtension264Fn {}
unsafe impl Send for BrcmExtension264Fn {}
unsafe impl Sync for BrcmExtension264Fn {}
impl ::std::clone::Clone for BrcmExtension264Fn {
    fn clone(&self) -> Self {
        BrcmExtension264Fn {}
    }
}
impl BrcmExtension264Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        BrcmExtension264Fn {}
    }
}
impl BrcmExtension265Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_BRCM_extension_265\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct BrcmExtension265Fn {}
unsafe impl Send for BrcmExtension265Fn {}
unsafe impl Sync for BrcmExtension265Fn {}
impl ::std::clone::Clone for BrcmExtension265Fn {
    fn clone(&self) -> Self {
        BrcmExtension265Fn {}
    }
}
impl BrcmExtension265Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        BrcmExtension265Fn {}
    }
}
impl ExtIndexTypeUint8Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_index_type_uint8\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtIndexTypeUint8Fn {}
unsafe impl Send for ExtIndexTypeUint8Fn {}
unsafe impl Sync for ExtIndexTypeUint8Fn {}
impl ::std::clone::Clone for ExtIndexTypeUint8Fn {
    fn clone(&self) -> Self {
        ExtIndexTypeUint8Fn {}
    }
}
impl ExtIndexTypeUint8Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtIndexTypeUint8Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_index_type_uint8'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT: Self = Self(1_000_265_000);
}
#[doc = "Generated from 'VK_EXT_index_type_uint8'"]
impl IndexType {
    pub const UINT8_EXT: Self = Self(1_000_265_000);
}
impl ExtExtension267Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_267\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension267Fn {}
unsafe impl Send for ExtExtension267Fn {}
unsafe impl Sync for ExtExtension267Fn {}
impl ::std::clone::Clone for ExtExtension267Fn {
    fn clone(&self) -> Self {
        ExtExtension267Fn {}
    }
}
impl ExtExtension267Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension267Fn {}
    }
}
impl ExtExtendedDynamicStateFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extended_dynamic_state\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetCullModeEXT =
    extern "system" fn(command_buffer: CommandBuffer, cull_mode: CullModeFlags);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetFrontFaceEXT =
    extern "system" fn(command_buffer: CommandBuffer, front_face: FrontFace);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPrimitiveTopologyEXT =
    extern "system" fn(command_buffer: CommandBuffer, primitive_topology: PrimitiveTopology);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetViewportWithCountEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    viewport_count: u32,
    p_viewports: *const Viewport,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetScissorWithCountEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    scissor_count: u32,
    p_scissors: *const Rect2D,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindVertexBuffers2EXT = extern "system" fn(
    command_buffer: CommandBuffer,
    first_binding: u32,
    binding_count: u32,
    p_buffers: *const Buffer,
    p_offsets: *const DeviceSize,
    p_sizes: *const DeviceSize,
    p_strides: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthTestEnableEXT =
    extern "system" fn(command_buffer: CommandBuffer, depth_test_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthWriteEnableEXT =
    extern "system" fn(command_buffer: CommandBuffer, depth_write_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthCompareOpEXT =
    extern "system" fn(command_buffer: CommandBuffer, depth_compare_op: CompareOp);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthBoundsTestEnableEXT =
    extern "system" fn(command_buffer: CommandBuffer, depth_bounds_test_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetStencilTestEnableEXT =
    extern "system" fn(command_buffer: CommandBuffer, stencil_test_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetStencilOpEXT = extern "system" fn(
    command_buffer: CommandBuffer,
    face_mask: StencilFaceFlags,
    fail_op: StencilOp,
    pass_op: StencilOp,
    depth_fail_op: StencilOp,
    compare_op: CompareOp,
);
pub struct ExtExtendedDynamicStateFn {
    pub cmd_set_cull_mode_ext:
        extern "system" fn(command_buffer: CommandBuffer, cull_mode: CullModeFlags),
    pub cmd_set_front_face_ext:
        extern "system" fn(command_buffer: CommandBuffer, front_face: FrontFace),
    pub cmd_set_primitive_topology_ext:
        extern "system" fn(command_buffer: CommandBuffer, primitive_topology: PrimitiveTopology),
    pub cmd_set_viewport_with_count_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        viewport_count: u32,
        p_viewports: *const Viewport,
    ),
    pub cmd_set_scissor_with_count_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        scissor_count: u32,
        p_scissors: *const Rect2D,
    ),
    pub cmd_bind_vertex_buffers2_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        first_binding: u32,
        binding_count: u32,
        p_buffers: *const Buffer,
        p_offsets: *const DeviceSize,
        p_sizes: *const DeviceSize,
        p_strides: *const DeviceSize,
    ),
    pub cmd_set_depth_test_enable_ext:
        extern "system" fn(command_buffer: CommandBuffer, depth_test_enable: Bool32),
    pub cmd_set_depth_write_enable_ext:
        extern "system" fn(command_buffer: CommandBuffer, depth_write_enable: Bool32),
    pub cmd_set_depth_compare_op_ext:
        extern "system" fn(command_buffer: CommandBuffer, depth_compare_op: CompareOp),
    pub cmd_set_depth_bounds_test_enable_ext:
        extern "system" fn(command_buffer: CommandBuffer, depth_bounds_test_enable: Bool32),
    pub cmd_set_stencil_test_enable_ext:
        extern "system" fn(command_buffer: CommandBuffer, stencil_test_enable: Bool32),
    pub cmd_set_stencil_op_ext: extern "system" fn(
        command_buffer: CommandBuffer,
        face_mask: StencilFaceFlags,
        fail_op: StencilOp,
        pass_op: StencilOp,
        depth_fail_op: StencilOp,
        compare_op: CompareOp,
    ),
}
unsafe impl Send for ExtExtendedDynamicStateFn {}
unsafe impl Sync for ExtExtendedDynamicStateFn {}
impl ::std::clone::Clone for ExtExtendedDynamicStateFn {
    fn clone(&self) -> Self {
        ExtExtendedDynamicStateFn {
            cmd_set_cull_mode_ext: self.cmd_set_cull_mode_ext,
            cmd_set_front_face_ext: self.cmd_set_front_face_ext,
            cmd_set_primitive_topology_ext: self.cmd_set_primitive_topology_ext,
            cmd_set_viewport_with_count_ext: self.cmd_set_viewport_with_count_ext,
            cmd_set_scissor_with_count_ext: self.cmd_set_scissor_with_count_ext,
            cmd_bind_vertex_buffers2_ext: self.cmd_bind_vertex_buffers2_ext,
            cmd_set_depth_test_enable_ext: self.cmd_set_depth_test_enable_ext,
            cmd_set_depth_write_enable_ext: self.cmd_set_depth_write_enable_ext,
            cmd_set_depth_compare_op_ext: self.cmd_set_depth_compare_op_ext,
            cmd_set_depth_bounds_test_enable_ext: self.cmd_set_depth_bounds_test_enable_ext,
            cmd_set_stencil_test_enable_ext: self.cmd_set_stencil_test_enable_ext,
            cmd_set_stencil_op_ext: self.cmd_set_stencil_op_ext,
        }
    }
}
impl ExtExtendedDynamicStateFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtendedDynamicStateFn {
            cmd_set_cull_mode_ext: unsafe {
                extern "system" fn cmd_set_cull_mode_ext(
                    _command_buffer: CommandBuffer,
                    _cull_mode: CullModeFlags,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_cull_mode_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetCullModeEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_cull_mode_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_front_face_ext: unsafe {
                extern "system" fn cmd_set_front_face_ext(
                    _command_buffer: CommandBuffer,
                    _front_face: FrontFace,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_front_face_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetFrontFaceEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_front_face_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_primitive_topology_ext: unsafe {
                extern "system" fn cmd_set_primitive_topology_ext(
                    _command_buffer: CommandBuffer,
                    _primitive_topology: PrimitiveTopology,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_primitive_topology_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetPrimitiveTopologyEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_primitive_topology_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_viewport_with_count_ext: unsafe {
                extern "system" fn cmd_set_viewport_with_count_ext(
                    _command_buffer: CommandBuffer,
                    _viewport_count: u32,
                    _p_viewports: *const Viewport,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_viewport_with_count_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetViewportWithCountEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_viewport_with_count_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_scissor_with_count_ext: unsafe {
                extern "system" fn cmd_set_scissor_with_count_ext(
                    _command_buffer: CommandBuffer,
                    _scissor_count: u32,
                    _p_scissors: *const Rect2D,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_scissor_with_count_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetScissorWithCountEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_scissor_with_count_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_bind_vertex_buffers2_ext: unsafe {
                extern "system" fn cmd_bind_vertex_buffers2_ext(
                    _command_buffer: CommandBuffer,
                    _first_binding: u32,
                    _binding_count: u32,
                    _p_buffers: *const Buffer,
                    _p_offsets: *const DeviceSize,
                    _p_sizes: *const DeviceSize,
                    _p_strides: *const DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_vertex_buffers2_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBindVertexBuffers2EXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_vertex_buffers2_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_depth_test_enable_ext: unsafe {
                extern "system" fn cmd_set_depth_test_enable_ext(
                    _command_buffer: CommandBuffer,
                    _depth_test_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_depth_test_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetDepthTestEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_depth_test_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_depth_write_enable_ext: unsafe {
                extern "system" fn cmd_set_depth_write_enable_ext(
                    _command_buffer: CommandBuffer,
                    _depth_write_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_depth_write_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetDepthWriteEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_depth_write_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_depth_compare_op_ext: unsafe {
                extern "system" fn cmd_set_depth_compare_op_ext(
                    _command_buffer: CommandBuffer,
                    _depth_compare_op: CompareOp,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_depth_compare_op_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetDepthCompareOpEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_depth_compare_op_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_depth_bounds_test_enable_ext: unsafe {
                extern "system" fn cmd_set_depth_bounds_test_enable_ext(
                    _command_buffer: CommandBuffer,
                    _depth_bounds_test_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_depth_bounds_test_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetDepthBoundsTestEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_depth_bounds_test_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_stencil_test_enable_ext: unsafe {
                extern "system" fn cmd_set_stencil_test_enable_ext(
                    _command_buffer: CommandBuffer,
                    _stencil_test_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_stencil_test_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetStencilTestEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_stencil_test_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_stencil_op_ext: unsafe {
                extern "system" fn cmd_set_stencil_op_ext(
                    _command_buffer: CommandBuffer,
                    _face_mask: StencilFaceFlags,
                    _fail_op: StencilOp,
                    _pass_op: StencilOp,
                    _depth_fail_op: StencilOp,
                    _compare_op: CompareOp,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_stencil_op_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetStencilOpEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_stencil_op_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetCullModeEXT.html>"]
    pub unsafe fn cmd_set_cull_mode_ext(
        &self,
        command_buffer: CommandBuffer,
        cull_mode: CullModeFlags,
    ) {
        (self.cmd_set_cull_mode_ext)(command_buffer, cull_mode)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetFrontFaceEXT.html>"]
    pub unsafe fn cmd_set_front_face_ext(
        &self,
        command_buffer: CommandBuffer,
        front_face: FrontFace,
    ) {
        (self.cmd_set_front_face_ext)(command_buffer, front_face)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetPrimitiveTopologyEXT.html>"]
    pub unsafe fn cmd_set_primitive_topology_ext(
        &self,
        command_buffer: CommandBuffer,
        primitive_topology: PrimitiveTopology,
    ) {
        (self.cmd_set_primitive_topology_ext)(command_buffer, primitive_topology)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetViewportWithCountEXT.html>"]
    pub unsafe fn cmd_set_viewport_with_count_ext(
        &self,
        command_buffer: CommandBuffer,
        viewport_count: u32,
        p_viewports: *const Viewport,
    ) {
        (self.cmd_set_viewport_with_count_ext)(command_buffer, viewport_count, p_viewports)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetScissorWithCountEXT.html>"]
    pub unsafe fn cmd_set_scissor_with_count_ext(
        &self,
        command_buffer: CommandBuffer,
        scissor_count: u32,
        p_scissors: *const Rect2D,
    ) {
        (self.cmd_set_scissor_with_count_ext)(command_buffer, scissor_count, p_scissors)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindVertexBuffers2EXT.html>"]
    pub unsafe fn cmd_bind_vertex_buffers2_ext(
        &self,
        command_buffer: CommandBuffer,
        first_binding: u32,
        binding_count: u32,
        p_buffers: *const Buffer,
        p_offsets: *const DeviceSize,
        p_sizes: *const DeviceSize,
        p_strides: *const DeviceSize,
    ) {
        (self.cmd_bind_vertex_buffers2_ext)(
            command_buffer,
            first_binding,
            binding_count,
            p_buffers,
            p_offsets,
            p_sizes,
            p_strides,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthTestEnableEXT.html>"]
    pub unsafe fn cmd_set_depth_test_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        depth_test_enable: Bool32,
    ) {
        (self.cmd_set_depth_test_enable_ext)(command_buffer, depth_test_enable)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthWriteEnableEXT.html>"]
    pub unsafe fn cmd_set_depth_write_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        depth_write_enable: Bool32,
    ) {
        (self.cmd_set_depth_write_enable_ext)(command_buffer, depth_write_enable)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthCompareOpEXT.html>"]
    pub unsafe fn cmd_set_depth_compare_op_ext(
        &self,
        command_buffer: CommandBuffer,
        depth_compare_op: CompareOp,
    ) {
        (self.cmd_set_depth_compare_op_ext)(command_buffer, depth_compare_op)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBoundsTestEnableEXT.html>"]
    pub unsafe fn cmd_set_depth_bounds_test_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        depth_bounds_test_enable: Bool32,
    ) {
        (self.cmd_set_depth_bounds_test_enable_ext)(command_buffer, depth_bounds_test_enable)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilTestEnableEXT.html>"]
    pub unsafe fn cmd_set_stencil_test_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        stencil_test_enable: Bool32,
    ) {
        (self.cmd_set_stencil_test_enable_ext)(command_buffer, stencil_test_enable)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilOpEXT.html>"]
    pub unsafe fn cmd_set_stencil_op_ext(
        &self,
        command_buffer: CommandBuffer,
        face_mask: StencilFaceFlags,
        fail_op: StencilOp,
        pass_op: StencilOp,
        depth_fail_op: StencilOp,
        compare_op: CompareOp,
    ) {
        (self.cmd_set_stencil_op_ext)(
            command_buffer,
            face_mask,
            fail_op,
            pass_op,
            depth_fail_op,
            compare_op,
        )
    }
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT: Self = Self(1_000_267_000);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const CULL_MODE_EXT: Self = Self(1_000_267_000);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const FRONT_FACE_EXT: Self = Self(1_000_267_001);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const PRIMITIVE_TOPOLOGY_EXT: Self = Self(1_000_267_002);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const VIEWPORT_WITH_COUNT_EXT: Self = Self(1_000_267_003);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const SCISSOR_WITH_COUNT_EXT: Self = Self(1_000_267_004);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const VERTEX_INPUT_BINDING_STRIDE_EXT: Self = Self(1_000_267_005);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const DEPTH_TEST_ENABLE_EXT: Self = Self(1_000_267_006);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const DEPTH_WRITE_ENABLE_EXT: Self = Self(1_000_267_007);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const DEPTH_COMPARE_OP_EXT: Self = Self(1_000_267_008);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const DEPTH_BOUNDS_TEST_ENABLE_EXT: Self = Self(1_000_267_009);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const STENCIL_TEST_ENABLE_EXT: Self = Self(1_000_267_010);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state'"]
impl DynamicState {
    pub const STENCIL_OP_EXT: Self = Self(1_000_267_011);
}
impl KhrDeferredHostOperationsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_deferred_host_operations\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDeferredOperationKHR = extern "system" fn(
    device: Device,
    p_allocator: *const AllocationCallbacks,
    p_deferred_operation: *mut DeferredOperationKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDeferredOperationKHR = extern "system" fn(
    device: Device,
    operation: DeferredOperationKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeferredOperationMaxConcurrencyKHR =
    extern "system" fn(device: Device, operation: DeferredOperationKHR) -> u32;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeferredOperationResultKHR =
    extern "system" fn(device: Device, operation: DeferredOperationKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDeferredOperationJoinKHR =
    extern "system" fn(device: Device, operation: DeferredOperationKHR) -> Result;
pub struct KhrDeferredHostOperationsFn {
    pub create_deferred_operation_khr: extern "system" fn(
        device: Device,
        p_allocator: *const AllocationCallbacks,
        p_deferred_operation: *mut DeferredOperationKHR,
    ) -> Result,
    pub destroy_deferred_operation_khr: extern "system" fn(
        device: Device,
        operation: DeferredOperationKHR,
        p_allocator: *const AllocationCallbacks,
    ),
    pub get_deferred_operation_max_concurrency_khr:
        extern "system" fn(device: Device, operation: DeferredOperationKHR) -> u32,
    pub get_deferred_operation_result_khr:
        extern "system" fn(device: Device, operation: DeferredOperationKHR) -> Result,
    pub deferred_operation_join_khr:
        extern "system" fn(device: Device, operation: DeferredOperationKHR) -> Result,
}
unsafe impl Send for KhrDeferredHostOperationsFn {}
unsafe impl Sync for KhrDeferredHostOperationsFn {}
impl ::std::clone::Clone for KhrDeferredHostOperationsFn {
    fn clone(&self) -> Self {
        KhrDeferredHostOperationsFn {
            create_deferred_operation_khr: self.create_deferred_operation_khr,
            destroy_deferred_operation_khr: self.destroy_deferred_operation_khr,
            get_deferred_operation_max_concurrency_khr: self
                .get_deferred_operation_max_concurrency_khr,
            get_deferred_operation_result_khr: self.get_deferred_operation_result_khr,
            deferred_operation_join_khr: self.deferred_operation_join_khr,
        }
    }
}
impl KhrDeferredHostOperationsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDeferredHostOperationsFn {
            create_deferred_operation_khr: unsafe {
                extern "system" fn create_deferred_operation_khr(
                    _device: Device,
                    _p_allocator: *const AllocationCallbacks,
                    _p_deferred_operation: *mut DeferredOperationKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_deferred_operation_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDeferredOperationKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_deferred_operation_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_deferred_operation_khr: unsafe {
                extern "system" fn destroy_deferred_operation_khr(
                    _device: Device,
                    _operation: DeferredOperationKHR,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_deferred_operation_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyDeferredOperationKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_deferred_operation_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_deferred_operation_max_concurrency_khr: unsafe {
                extern "system" fn get_deferred_operation_max_concurrency_khr(
                    _device: Device,
                    _operation: DeferredOperationKHR,
                ) -> u32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_deferred_operation_max_concurrency_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeferredOperationMaxConcurrencyKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_deferred_operation_max_concurrency_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_deferred_operation_result_khr: unsafe {
                extern "system" fn get_deferred_operation_result_khr(
                    _device: Device,
                    _operation: DeferredOperationKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_deferred_operation_result_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeferredOperationResultKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_deferred_operation_result_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            deferred_operation_join_khr: unsafe {
                extern "system" fn deferred_operation_join_khr(
                    _device: Device,
                    _operation: DeferredOperationKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(deferred_operation_join_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDeferredOperationJoinKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    deferred_operation_join_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDeferredOperationKHR.html>"]
    pub unsafe fn create_deferred_operation_khr(
        &self,
        device: Device,
        p_allocator: *const AllocationCallbacks,
        p_deferred_operation: *mut DeferredOperationKHR,
    ) -> Result {
        (self.create_deferred_operation_khr)(device, p_allocator, p_deferred_operation)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDeferredOperationKHR.html>"]
    pub unsafe fn destroy_deferred_operation_khr(
        &self,
        device: Device,
        operation: DeferredOperationKHR,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_deferred_operation_khr)(device, operation, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeferredOperationMaxConcurrencyKHR.html>"]
    pub unsafe fn get_deferred_operation_max_concurrency_khr(
        &self,
        device: Device,
        operation: DeferredOperationKHR,
    ) -> u32 {
        (self.get_deferred_operation_max_concurrency_khr)(device, operation)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeferredOperationResultKHR.html>"]
    pub unsafe fn get_deferred_operation_result_khr(
        &self,
        device: Device,
        operation: DeferredOperationKHR,
    ) -> Result {
        (self.get_deferred_operation_result_khr)(device, operation)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDeferredOperationJoinKHR.html>"]
    pub unsafe fn deferred_operation_join_khr(
        &self,
        device: Device,
        operation: DeferredOperationKHR,
    ) -> Result {
        (self.deferred_operation_join_khr)(device, operation)
    }
}
#[doc = "Generated from 'VK_KHR_deferred_host_operations'"]
impl ObjectType {
    pub const DEFERRED_OPERATION_KHR: Self = Self(1_000_268_000);
}
#[doc = "Generated from 'VK_KHR_deferred_host_operations'"]
impl Result {
    pub const THREAD_IDLE_KHR: Self = Self(1_000_268_000);
}
#[doc = "Generated from 'VK_KHR_deferred_host_operations'"]
impl Result {
    pub const THREAD_DONE_KHR: Self = Self(1_000_268_001);
}
#[doc = "Generated from 'VK_KHR_deferred_host_operations'"]
impl Result {
    pub const OPERATION_DEFERRED_KHR: Self = Self(1_000_268_002);
}
#[doc = "Generated from 'VK_KHR_deferred_host_operations'"]
impl Result {
    pub const OPERATION_NOT_DEFERRED_KHR: Self = Self(1_000_268_003);
}
impl KhrPipelineExecutablePropertiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_pipeline_executable_properties\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPipelineExecutablePropertiesKHR = extern "system" fn(
    device: Device,
    p_pipeline_info: *const PipelineInfoKHR,
    p_executable_count: *mut u32,
    p_properties: *mut PipelineExecutablePropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPipelineExecutableStatisticsKHR = extern "system" fn(
    device: Device,
    p_executable_info: *const PipelineExecutableInfoKHR,
    p_statistic_count: *mut u32,
    p_statistics: *mut PipelineExecutableStatisticKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPipelineExecutableInternalRepresentationsKHR = extern "system" fn(
    device: Device,
    p_executable_info: *const PipelineExecutableInfoKHR,
    p_internal_representation_count: *mut u32,
    p_internal_representations: *mut PipelineExecutableInternalRepresentationKHR,
) -> Result;
pub struct KhrPipelineExecutablePropertiesFn {
    pub get_pipeline_executable_properties_khr: extern "system" fn(
        device: Device,
        p_pipeline_info: *const PipelineInfoKHR,
        p_executable_count: *mut u32,
        p_properties: *mut PipelineExecutablePropertiesKHR,
    ) -> Result,
    pub get_pipeline_executable_statistics_khr: extern "system" fn(
        device: Device,
        p_executable_info: *const PipelineExecutableInfoKHR,
        p_statistic_count: *mut u32,
        p_statistics: *mut PipelineExecutableStatisticKHR,
    ) -> Result,
    pub get_pipeline_executable_internal_representations_khr: extern "system" fn(
        device: Device,
        p_executable_info: *const PipelineExecutableInfoKHR,
        p_internal_representation_count: *mut u32,
        p_internal_representations: *mut PipelineExecutableInternalRepresentationKHR,
    ) -> Result,
}
unsafe impl Send for KhrPipelineExecutablePropertiesFn {}
unsafe impl Sync for KhrPipelineExecutablePropertiesFn {}
impl ::std::clone::Clone for KhrPipelineExecutablePropertiesFn {
    fn clone(&self) -> Self {
        KhrPipelineExecutablePropertiesFn {
            get_pipeline_executable_properties_khr: self.get_pipeline_executable_properties_khr,
            get_pipeline_executable_statistics_khr: self.get_pipeline_executable_statistics_khr,
            get_pipeline_executable_internal_representations_khr: self
                .get_pipeline_executable_internal_representations_khr,
        }
    }
}
impl KhrPipelineExecutablePropertiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPipelineExecutablePropertiesFn {
            get_pipeline_executable_properties_khr: unsafe {
                extern "system" fn get_pipeline_executable_properties_khr(
                    _device: Device,
                    _p_pipeline_info: *const PipelineInfoKHR,
                    _p_executable_count: *mut u32,
                    _p_properties: *mut PipelineExecutablePropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_pipeline_executable_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPipelineExecutablePropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_pipeline_executable_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_pipeline_executable_statistics_khr: unsafe {
                extern "system" fn get_pipeline_executable_statistics_khr(
                    _device: Device,
                    _p_executable_info: *const PipelineExecutableInfoKHR,
                    _p_statistic_count: *mut u32,
                    _p_statistics: *mut PipelineExecutableStatisticKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_pipeline_executable_statistics_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPipelineExecutableStatisticsKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_pipeline_executable_statistics_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_pipeline_executable_internal_representations_khr: unsafe {
                extern "system" fn get_pipeline_executable_internal_representations_khr(
                    _device: Device,
                    _p_executable_info: *const PipelineExecutableInfoKHR,
                    _p_internal_representation_count: *mut u32,
                    _p_internal_representations: *mut PipelineExecutableInternalRepresentationKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_pipeline_executable_internal_representations_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPipelineExecutableInternalRepresentationsKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_pipeline_executable_internal_representations_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutablePropertiesKHR.html>"]
    pub unsafe fn get_pipeline_executable_properties_khr(
        &self,
        device: Device,
        p_pipeline_info: *const PipelineInfoKHR,
        p_executable_count: *mut u32,
        p_properties: *mut PipelineExecutablePropertiesKHR,
    ) -> Result {
        (self.get_pipeline_executable_properties_khr)(
            device,
            p_pipeline_info,
            p_executable_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutableStatisticsKHR.html>"]
    pub unsafe fn get_pipeline_executable_statistics_khr(
        &self,
        device: Device,
        p_executable_info: *const PipelineExecutableInfoKHR,
        p_statistic_count: *mut u32,
        p_statistics: *mut PipelineExecutableStatisticKHR,
    ) -> Result {
        (self.get_pipeline_executable_statistics_khr)(
            device,
            p_executable_info,
            p_statistic_count,
            p_statistics,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineExecutableInternalRepresentationsKHR.html>"]
    pub unsafe fn get_pipeline_executable_internal_representations_khr(
        &self,
        device: Device,
        p_executable_info: *const PipelineExecutableInfoKHR,
        p_internal_representation_count: *mut u32,
        p_internal_representations: *mut PipelineExecutableInternalRepresentationKHR,
    ) -> Result {
        (self.get_pipeline_executable_internal_representations_khr)(
            device,
            p_executable_info,
            p_internal_representation_count,
            p_internal_representations,
        )
    }
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR: Self =
        Self(1_000_269_000);
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl StructureType {
    pub const PIPELINE_INFO_KHR: Self = Self(1_000_269_001);
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl StructureType {
    pub const PIPELINE_EXECUTABLE_PROPERTIES_KHR: Self = Self(1_000_269_002);
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl StructureType {
    pub const PIPELINE_EXECUTABLE_INFO_KHR: Self = Self(1_000_269_003);
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl StructureType {
    pub const PIPELINE_EXECUTABLE_STATISTIC_KHR: Self = Self(1_000_269_004);
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl StructureType {
    pub const PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR: Self = Self(1_000_269_005);
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl PipelineCreateFlags {
    pub const CAPTURE_STATISTICS_KHR: Self = Self(0b100_0000);
}
#[doc = "Generated from 'VK_KHR_pipeline_executable_properties'"]
impl PipelineCreateFlags {
    pub const CAPTURE_INTERNAL_REPRESENTATIONS_KHR: Self = Self(0b1000_0000);
}
impl IntelExtension271Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_extension_271\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct IntelExtension271Fn {}
unsafe impl Send for IntelExtension271Fn {}
unsafe impl Sync for IntelExtension271Fn {}
impl ::std::clone::Clone for IntelExtension271Fn {
    fn clone(&self) -> Self {
        IntelExtension271Fn {}
    }
}
impl IntelExtension271Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelExtension271Fn {}
    }
}
impl IntelExtension272Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_extension_272\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct IntelExtension272Fn {}
unsafe impl Send for IntelExtension272Fn {}
unsafe impl Sync for IntelExtension272Fn {}
impl ::std::clone::Clone for IntelExtension272Fn {
    fn clone(&self) -> Self {
        IntelExtension272Fn {}
    }
}
impl IntelExtension272Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelExtension272Fn {}
    }
}
impl IntelExtension273Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_extension_273\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct IntelExtension273Fn {}
unsafe impl Send for IntelExtension273Fn {}
unsafe impl Sync for IntelExtension273Fn {}
impl ::std::clone::Clone for IntelExtension273Fn {
    fn clone(&self) -> Self {
        IntelExtension273Fn {}
    }
}
impl IntelExtension273Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelExtension273Fn {}
    }
}
impl IntelExtension274Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_extension_274\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct IntelExtension274Fn {}
unsafe impl Send for IntelExtension274Fn {}
unsafe impl Sync for IntelExtension274Fn {}
impl ::std::clone::Clone for IntelExtension274Fn {
    fn clone(&self) -> Self {
        IntelExtension274Fn {}
    }
}
impl IntelExtension274Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelExtension274Fn {}
    }
}
impl KhrExtension275Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_275\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension275Fn {}
unsafe impl Send for KhrExtension275Fn {}
unsafe impl Sync for KhrExtension275Fn {}
impl ::std::clone::Clone for KhrExtension275Fn {
    fn clone(&self) -> Self {
        KhrExtension275Fn {}
    }
}
impl KhrExtension275Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension275Fn {}
    }
}
impl KhrExtension276Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_276\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension276Fn {}
unsafe impl Send for KhrExtension276Fn {}
unsafe impl Sync for KhrExtension276Fn {}
impl ::std::clone::Clone for KhrExtension276Fn {
    fn clone(&self) -> Self {
        KhrExtension276Fn {}
    }
}
impl KhrExtension276Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension276Fn {}
    }
}
impl ExtShaderDemoteToHelperInvocationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_demote_to_helper_invocation\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtShaderDemoteToHelperInvocationFn {}
unsafe impl Send for ExtShaderDemoteToHelperInvocationFn {}
unsafe impl Sync for ExtShaderDemoteToHelperInvocationFn {}
impl ::std::clone::Clone for ExtShaderDemoteToHelperInvocationFn {
    fn clone(&self) -> Self {
        ExtShaderDemoteToHelperInvocationFn {}
    }
}
impl ExtShaderDemoteToHelperInvocationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderDemoteToHelperInvocationFn {}
    }
}
#[doc = "Generated from 'VK_EXT_shader_demote_to_helper_invocation'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT: Self =
        Self(1_000_276_000);
}
impl NvDeviceGeneratedCommandsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_device_generated_commands\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetGeneratedCommandsMemoryRequirementsNV = extern "system" fn(
    device: Device,
    p_info: *const GeneratedCommandsMemoryRequirementsInfoNV,
    p_memory_requirements: *mut MemoryRequirements2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPreprocessGeneratedCommandsNV = extern "system" fn(
    command_buffer: CommandBuffer,
    p_generated_commands_info: *const GeneratedCommandsInfoNV,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdExecuteGeneratedCommandsNV = extern "system" fn(
    command_buffer: CommandBuffer,
    is_preprocessed: Bool32,
    p_generated_commands_info: *const GeneratedCommandsInfoNV,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindPipelineShaderGroupNV = extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_bind_point: PipelineBindPoint,
    pipeline: Pipeline,
    group_index: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateIndirectCommandsLayoutNV = extern "system" fn(
    device: Device,
    p_create_info: *const IndirectCommandsLayoutCreateInfoNV,
    p_allocator: *const AllocationCallbacks,
    p_indirect_commands_layout: *mut IndirectCommandsLayoutNV,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyIndirectCommandsLayoutNV = extern "system" fn(
    device: Device,
    indirect_commands_layout: IndirectCommandsLayoutNV,
    p_allocator: *const AllocationCallbacks,
);
pub struct NvDeviceGeneratedCommandsFn {
    pub get_generated_commands_memory_requirements_nv: extern "system" fn(
        device: Device,
        p_info: *const GeneratedCommandsMemoryRequirementsInfoNV,
        p_memory_requirements: *mut MemoryRequirements2,
    ),
    pub cmd_preprocess_generated_commands_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        p_generated_commands_info: *const GeneratedCommandsInfoNV,
    ),
    pub cmd_execute_generated_commands_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        is_preprocessed: Bool32,
        p_generated_commands_info: *const GeneratedCommandsInfoNV,
    ),
    pub cmd_bind_pipeline_shader_group_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        pipeline_bind_point: PipelineBindPoint,
        pipeline: Pipeline,
        group_index: u32,
    ),
    pub create_indirect_commands_layout_nv: extern "system" fn(
        device: Device,
        p_create_info: *const IndirectCommandsLayoutCreateInfoNV,
        p_allocator: *const AllocationCallbacks,
        p_indirect_commands_layout: *mut IndirectCommandsLayoutNV,
    ) -> Result,
    pub destroy_indirect_commands_layout_nv: extern "system" fn(
        device: Device,
        indirect_commands_layout: IndirectCommandsLayoutNV,
        p_allocator: *const AllocationCallbacks,
    ),
}
unsafe impl Send for NvDeviceGeneratedCommandsFn {}
unsafe impl Sync for NvDeviceGeneratedCommandsFn {}
impl ::std::clone::Clone for NvDeviceGeneratedCommandsFn {
    fn clone(&self) -> Self {
        NvDeviceGeneratedCommandsFn {
            get_generated_commands_memory_requirements_nv: self
                .get_generated_commands_memory_requirements_nv,
            cmd_preprocess_generated_commands_nv: self.cmd_preprocess_generated_commands_nv,
            cmd_execute_generated_commands_nv: self.cmd_execute_generated_commands_nv,
            cmd_bind_pipeline_shader_group_nv: self.cmd_bind_pipeline_shader_group_nv,
            create_indirect_commands_layout_nv: self.create_indirect_commands_layout_nv,
            destroy_indirect_commands_layout_nv: self.destroy_indirect_commands_layout_nv,
        }
    }
}
impl NvDeviceGeneratedCommandsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvDeviceGeneratedCommandsFn {
            get_generated_commands_memory_requirements_nv: unsafe {
                extern "system" fn get_generated_commands_memory_requirements_nv(
                    _device: Device,
                    _p_info: *const GeneratedCommandsMemoryRequirementsInfoNV,
                    _p_memory_requirements: *mut MemoryRequirements2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_generated_commands_memory_requirements_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetGeneratedCommandsMemoryRequirementsNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_generated_commands_memory_requirements_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_preprocess_generated_commands_nv: unsafe {
                extern "system" fn cmd_preprocess_generated_commands_nv(
                    _command_buffer: CommandBuffer,
                    _p_generated_commands_info: *const GeneratedCommandsInfoNV,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_preprocess_generated_commands_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdPreprocessGeneratedCommandsNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_preprocess_generated_commands_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_execute_generated_commands_nv: unsafe {
                extern "system" fn cmd_execute_generated_commands_nv(
                    _command_buffer: CommandBuffer,
                    _is_preprocessed: Bool32,
                    _p_generated_commands_info: *const GeneratedCommandsInfoNV,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_execute_generated_commands_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdExecuteGeneratedCommandsNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_execute_generated_commands_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_bind_pipeline_shader_group_nv: unsafe {
                extern "system" fn cmd_bind_pipeline_shader_group_nv(
                    _command_buffer: CommandBuffer,
                    _pipeline_bind_point: PipelineBindPoint,
                    _pipeline: Pipeline,
                    _group_index: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_pipeline_shader_group_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBindPipelineShaderGroupNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_pipeline_shader_group_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_indirect_commands_layout_nv: unsafe {
                extern "system" fn create_indirect_commands_layout_nv(
                    _device: Device,
                    _p_create_info: *const IndirectCommandsLayoutCreateInfoNV,
                    _p_allocator: *const AllocationCallbacks,
                    _p_indirect_commands_layout: *mut IndirectCommandsLayoutNV,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_indirect_commands_layout_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateIndirectCommandsLayoutNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_indirect_commands_layout_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_indirect_commands_layout_nv: unsafe {
                extern "system" fn destroy_indirect_commands_layout_nv(
                    _device: Device,
                    _indirect_commands_layout: IndirectCommandsLayoutNV,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_indirect_commands_layout_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyIndirectCommandsLayoutNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_indirect_commands_layout_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetGeneratedCommandsMemoryRequirementsNV.html>"]
    pub unsafe fn get_generated_commands_memory_requirements_nv(
        &self,
        device: Device,
        p_info: *const GeneratedCommandsMemoryRequirementsInfoNV,
        p_memory_requirements: *mut MemoryRequirements2,
    ) {
        (self.get_generated_commands_memory_requirements_nv)(device, p_info, p_memory_requirements)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPreprocessGeneratedCommandsNV.html>"]
    pub unsafe fn cmd_preprocess_generated_commands_nv(
        &self,
        command_buffer: CommandBuffer,
        p_generated_commands_info: *const GeneratedCommandsInfoNV,
    ) {
        (self.cmd_preprocess_generated_commands_nv)(command_buffer, p_generated_commands_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdExecuteGeneratedCommandsNV.html>"]
    pub unsafe fn cmd_execute_generated_commands_nv(
        &self,
        command_buffer: CommandBuffer,
        is_preprocessed: Bool32,
        p_generated_commands_info: *const GeneratedCommandsInfoNV,
    ) {
        (self.cmd_execute_generated_commands_nv)(
            command_buffer,
            is_preprocessed,
            p_generated_commands_info,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindPipelineShaderGroupNV.html>"]
    pub unsafe fn cmd_bind_pipeline_shader_group_nv(
        &self,
        command_buffer: CommandBuffer,
        pipeline_bind_point: PipelineBindPoint,
        pipeline: Pipeline,
        group_index: u32,
    ) {
        (self.cmd_bind_pipeline_shader_group_nv)(
            command_buffer,
            pipeline_bind_point,
            pipeline,
            group_index,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateIndirectCommandsLayoutNV.html>"]
    pub unsafe fn create_indirect_commands_layout_nv(
        &self,
        device: Device,
        p_create_info: *const IndirectCommandsLayoutCreateInfoNV,
        p_allocator: *const AllocationCallbacks,
        p_indirect_commands_layout: *mut IndirectCommandsLayoutNV,
    ) -> Result {
        (self.create_indirect_commands_layout_nv)(
            device,
            p_create_info,
            p_allocator,
            p_indirect_commands_layout,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyIndirectCommandsLayoutNV.html>"]
    pub unsafe fn destroy_indirect_commands_layout_nv(
        &self,
        device: Device,
        indirect_commands_layout: IndirectCommandsLayoutNV,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_indirect_commands_layout_nv)(device, indirect_commands_layout, p_allocator)
    }
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV: Self = Self(1_000_277_000);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const GRAPHICS_SHADER_GROUP_CREATE_INFO_NV: Self = Self(1_000_277_001);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV: Self = Self(1_000_277_002);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const INDIRECT_COMMANDS_LAYOUT_TOKEN_NV: Self = Self(1_000_277_003);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV: Self = Self(1_000_277_004);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const GENERATED_COMMANDS_INFO_NV: Self = Self(1_000_277_005);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV: Self = Self(1_000_277_006);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV: Self = Self(1_000_277_007);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl PipelineCreateFlags {
    pub const INDIRECT_BINDABLE_NV: Self = Self(0b100_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl PipelineStageFlags {
    pub const COMMAND_PREPROCESS_NV: Self = Self(0b10_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl AccessFlags {
    pub const COMMAND_PREPROCESS_READ_NV: Self = Self(0b10_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl AccessFlags {
    pub const COMMAND_PREPROCESS_WRITE_NV: Self = Self(0b100_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_NV_device_generated_commands'"]
impl ObjectType {
    pub const INDIRECT_COMMANDS_LAYOUT_NV: Self = Self(1_000_277_000);
}
impl NvExtension279Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_279\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension279Fn {}
unsafe impl Send for NvExtension279Fn {}
unsafe impl Sync for NvExtension279Fn {}
impl ::std::clone::Clone for NvExtension279Fn {
    fn clone(&self) -> Self {
        NvExtension279Fn {}
    }
}
impl NvExtension279Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension279Fn {}
    }
}
impl KhrExtension280Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_280\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension280Fn {}
unsafe impl Send for KhrExtension280Fn {}
unsafe impl Sync for KhrExtension280Fn {}
impl ::std::clone::Clone for KhrExtension280Fn {
    fn clone(&self) -> Self {
        KhrExtension280Fn {}
    }
}
impl KhrExtension280Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension280Fn {}
    }
}
impl ArmExtension281Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ARM_extension_281\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ArmExtension281Fn {}
unsafe impl Send for ArmExtension281Fn {}
unsafe impl Sync for ArmExtension281Fn {}
impl ::std::clone::Clone for ArmExtension281Fn {
    fn clone(&self) -> Self {
        ArmExtension281Fn {}
    }
}
impl ArmExtension281Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ArmExtension281Fn {}
    }
}
impl ExtTexelBufferAlignmentFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_texel_buffer_alignment\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtTexelBufferAlignmentFn {}
unsafe impl Send for ExtTexelBufferAlignmentFn {}
unsafe impl Sync for ExtTexelBufferAlignmentFn {}
impl ::std::clone::Clone for ExtTexelBufferAlignmentFn {
    fn clone(&self) -> Self {
        ExtTexelBufferAlignmentFn {}
    }
}
impl ExtTexelBufferAlignmentFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtTexelBufferAlignmentFn {}
    }
}
#[doc = "Generated from 'VK_EXT_texel_buffer_alignment'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT: Self = Self(1_000_281_000);
}
#[doc = "Generated from 'VK_EXT_texel_buffer_alignment'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES_EXT: Self = Self(1_000_281_001);
}
impl QcomRenderPassTransformFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_render_pass_transform\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct QcomRenderPassTransformFn {}
unsafe impl Send for QcomRenderPassTransformFn {}
unsafe impl Sync for QcomRenderPassTransformFn {}
impl ::std::clone::Clone for QcomRenderPassTransformFn {
    fn clone(&self) -> Self {
        QcomRenderPassTransformFn {}
    }
}
impl QcomRenderPassTransformFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomRenderPassTransformFn {}
    }
}
#[doc = "Generated from 'VK_QCOM_render_pass_transform'"]
impl StructureType {
    pub const COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM: Self =
        Self(1_000_282_000);
}
#[doc = "Generated from 'VK_QCOM_render_pass_transform'"]
impl StructureType {
    pub const RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM: Self = Self(1_000_282_001);
}
#[doc = "Generated from 'VK_QCOM_render_pass_transform'"]
impl RenderPassCreateFlags {
    pub const TRANSFORM_QCOM: Self = Self(0b10);
}
impl ExtExtension284Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_284\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension284Fn {}
unsafe impl Send for ExtExtension284Fn {}
unsafe impl Sync for ExtExtension284Fn {}
impl ::std::clone::Clone for ExtExtension284Fn {
    fn clone(&self) -> Self {
        ExtExtension284Fn {}
    }
}
impl ExtExtension284Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension284Fn {}
    }
}
impl ExtDeviceMemoryReportFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_device_memory_report\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct ExtDeviceMemoryReportFn {}
unsafe impl Send for ExtDeviceMemoryReportFn {}
unsafe impl Sync for ExtDeviceMemoryReportFn {}
impl ::std::clone::Clone for ExtDeviceMemoryReportFn {
    fn clone(&self) -> Self {
        ExtDeviceMemoryReportFn {}
    }
}
impl ExtDeviceMemoryReportFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDeviceMemoryReportFn {}
    }
}
#[doc = "Generated from 'VK_EXT_device_memory_report'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT: Self = Self(1_000_284_000);
}
#[doc = "Generated from 'VK_EXT_device_memory_report'"]
impl StructureType {
    pub const DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT: Self = Self(1_000_284_001);
}
#[doc = "Generated from 'VK_EXT_device_memory_report'"]
impl StructureType {
    pub const DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT: Self = Self(1_000_284_002);
}
impl ExtExtension286Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_286\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension286Fn {}
unsafe impl Send for ExtExtension286Fn {}
unsafe impl Sync for ExtExtension286Fn {}
impl ::std::clone::Clone for ExtExtension286Fn {
    fn clone(&self) -> Self {
        ExtExtension286Fn {}
    }
}
impl ExtExtension286Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension286Fn {}
    }
}
impl ExtRobustness2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_robustness2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtRobustness2Fn {}
unsafe impl Send for ExtRobustness2Fn {}
unsafe impl Sync for ExtRobustness2Fn {}
impl ::std::clone::Clone for ExtRobustness2Fn {
    fn clone(&self) -> Self {
        ExtRobustness2Fn {}
    }
}
impl ExtRobustness2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtRobustness2Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_robustness2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT: Self = Self(1_000_286_000);
}
#[doc = "Generated from 'VK_EXT_robustness2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT: Self = Self(1_000_286_001);
}
impl ExtCustomBorderColorFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_custom_border_color\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 12u32;
}
pub struct ExtCustomBorderColorFn {}
unsafe impl Send for ExtCustomBorderColorFn {}
unsafe impl Sync for ExtCustomBorderColorFn {}
impl ::std::clone::Clone for ExtCustomBorderColorFn {
    fn clone(&self) -> Self {
        ExtCustomBorderColorFn {}
    }
}
impl ExtCustomBorderColorFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtCustomBorderColorFn {}
    }
}
#[doc = "Generated from 'VK_EXT_custom_border_color'"]
impl StructureType {
    pub const SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT: Self = Self(1_000_287_000);
}
#[doc = "Generated from 'VK_EXT_custom_border_color'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT: Self = Self(1_000_287_001);
}
#[doc = "Generated from 'VK_EXT_custom_border_color'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT: Self = Self(1_000_287_002);
}
#[doc = "Generated from 'VK_EXT_custom_border_color'"]
impl BorderColor {
    pub const FLOAT_CUSTOM_EXT: Self = Self(1_000_287_003);
}
#[doc = "Generated from 'VK_EXT_custom_border_color'"]
impl BorderColor {
    pub const INT_CUSTOM_EXT: Self = Self(1_000_287_004);
}
impl ExtExtension289Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_289\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension289Fn {}
unsafe impl Send for ExtExtension289Fn {}
unsafe impl Sync for ExtExtension289Fn {}
impl ::std::clone::Clone for ExtExtension289Fn {
    fn clone(&self) -> Self {
        ExtExtension289Fn {}
    }
}
impl ExtExtension289Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension289Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_3X3X3_UNORM_BLOCK_EXT: Self = Self(1_000_288_000);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_3X3X3_SRGB_BLOCK_EXT: Self = Self(1_000_288_001);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_3X3X3_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_002);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X3X3_UNORM_BLOCK_EXT: Self = Self(1_000_288_003);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X3X3_SRGB_BLOCK_EXT: Self = Self(1_000_288_004);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X3X3_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_005);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X4X3_UNORM_BLOCK_EXT: Self = Self(1_000_288_006);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X4X3_SRGB_BLOCK_EXT: Self = Self(1_000_288_007);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X4X3_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_008);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X4X4_UNORM_BLOCK_EXT: Self = Self(1_000_288_009);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X4X4_SRGB_BLOCK_EXT: Self = Self(1_000_288_010);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_4X4X4_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_011);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X4X4_UNORM_BLOCK_EXT: Self = Self(1_000_288_012);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X4X4_SRGB_BLOCK_EXT: Self = Self(1_000_288_013);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X4X4_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_014);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X5X4_UNORM_BLOCK_EXT: Self = Self(1_000_288_015);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X5X4_SRGB_BLOCK_EXT: Self = Self(1_000_288_016);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X5X4_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_017);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X5X5_UNORM_BLOCK_EXT: Self = Self(1_000_288_018);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X5X5_SRGB_BLOCK_EXT: Self = Self(1_000_288_019);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_5X5X5_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_020);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X5X5_UNORM_BLOCK_EXT: Self = Self(1_000_288_021);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X5X5_SRGB_BLOCK_EXT: Self = Self(1_000_288_022);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X5X5_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_023);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X6X5_UNORM_BLOCK_EXT: Self = Self(1_000_288_024);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X6X5_SRGB_BLOCK_EXT: Self = Self(1_000_288_025);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X6X5_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_026);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X6X6_UNORM_BLOCK_EXT: Self = Self(1_000_288_027);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X6X6_SRGB_BLOCK_EXT: Self = Self(1_000_288_028);
}
#[doc = "Generated from 'VK_EXT_extension_289'"]
impl Format {
    pub const ASTC_6X6X6_SFLOAT_BLOCK_EXT: Self = Self(1_000_288_029);
}
impl GoogleUserTypeFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_user_type\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct GoogleUserTypeFn {}
unsafe impl Send for GoogleUserTypeFn {}
unsafe impl Sync for GoogleUserTypeFn {}
impl ::std::clone::Clone for GoogleUserTypeFn {
    fn clone(&self) -> Self {
        GoogleUserTypeFn {}
    }
}
impl GoogleUserTypeFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleUserTypeFn {}
    }
}
impl KhrPipelineLibraryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_pipeline_library\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrPipelineLibraryFn {}
unsafe impl Send for KhrPipelineLibraryFn {}
unsafe impl Sync for KhrPipelineLibraryFn {}
impl ::std::clone::Clone for KhrPipelineLibraryFn {
    fn clone(&self) -> Self {
        KhrPipelineLibraryFn {}
    }
}
impl KhrPipelineLibraryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPipelineLibraryFn {}
    }
}
#[doc = "Generated from 'VK_KHR_pipeline_library'"]
impl PipelineCreateFlags {
    pub const LIBRARY_KHR: Self = Self(0b1000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_pipeline_library'"]
impl StructureType {
    pub const PIPELINE_LIBRARY_CREATE_INFO_KHR: Self = Self(1_000_290_000);
}
impl NvExtension292Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_292\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension292Fn {}
unsafe impl Send for NvExtension292Fn {}
unsafe impl Sync for NvExtension292Fn {}
impl ::std::clone::Clone for NvExtension292Fn {
    fn clone(&self) -> Self {
        NvExtension292Fn {}
    }
}
impl NvExtension292Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension292Fn {}
    }
}
impl NvExtension293Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_293\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension293Fn {}
unsafe impl Send for NvExtension293Fn {}
unsafe impl Sync for NvExtension293Fn {}
impl ::std::clone::Clone for NvExtension293Fn {
    fn clone(&self) -> Self {
        NvExtension293Fn {}
    }
}
impl NvExtension293Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension293Fn {}
    }
}
impl KhrShaderNonSemanticInfoFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_non_semantic_info\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrShaderNonSemanticInfoFn {}
unsafe impl Send for KhrShaderNonSemanticInfoFn {}
unsafe impl Sync for KhrShaderNonSemanticInfoFn {}
impl ::std::clone::Clone for KhrShaderNonSemanticInfoFn {
    fn clone(&self) -> Self {
        KhrShaderNonSemanticInfoFn {}
    }
}
impl KhrShaderNonSemanticInfoFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderNonSemanticInfoFn {}
    }
}
impl KhrExtension295Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_295\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension295Fn {}
unsafe impl Send for KhrExtension295Fn {}
unsafe impl Sync for KhrExtension295Fn {}
impl ::std::clone::Clone for KhrExtension295Fn {
    fn clone(&self) -> Self {
        KhrExtension295Fn {}
    }
}
impl KhrExtension295Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension295Fn {}
    }
}
impl ExtPrivateDataFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_private_data\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreatePrivateDataSlotEXT = extern "system" fn(
    device: Device,
    p_create_info: *const PrivateDataSlotCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_private_data_slot: *mut PrivateDataSlotEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyPrivateDataSlotEXT = extern "system" fn(
    device: Device,
    private_data_slot: PrivateDataSlotEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkSetPrivateDataEXT = extern "system" fn(
    device: Device,
    object_type: ObjectType,
    object_handle: u64,
    private_data_slot: PrivateDataSlotEXT,
    data: u64,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPrivateDataEXT = extern "system" fn(
    device: Device,
    object_type: ObjectType,
    object_handle: u64,
    private_data_slot: PrivateDataSlotEXT,
    p_data: *mut u64,
);
pub struct ExtPrivateDataFn {
    pub create_private_data_slot_ext: extern "system" fn(
        device: Device,
        p_create_info: *const PrivateDataSlotCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_private_data_slot: *mut PrivateDataSlotEXT,
    ) -> Result,
    pub destroy_private_data_slot_ext: extern "system" fn(
        device: Device,
        private_data_slot: PrivateDataSlotEXT,
        p_allocator: *const AllocationCallbacks,
    ),
    pub set_private_data_ext: extern "system" fn(
        device: Device,
        object_type: ObjectType,
        object_handle: u64,
        private_data_slot: PrivateDataSlotEXT,
        data: u64,
    ) -> Result,
    pub get_private_data_ext: extern "system" fn(
        device: Device,
        object_type: ObjectType,
        object_handle: u64,
        private_data_slot: PrivateDataSlotEXT,
        p_data: *mut u64,
    ),
}
unsafe impl Send for ExtPrivateDataFn {}
unsafe impl Sync for ExtPrivateDataFn {}
impl ::std::clone::Clone for ExtPrivateDataFn {
    fn clone(&self) -> Self {
        ExtPrivateDataFn {
            create_private_data_slot_ext: self.create_private_data_slot_ext,
            destroy_private_data_slot_ext: self.destroy_private_data_slot_ext,
            set_private_data_ext: self.set_private_data_ext,
            get_private_data_ext: self.get_private_data_ext,
        }
    }
}
impl ExtPrivateDataFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPrivateDataFn {
            create_private_data_slot_ext: unsafe {
                extern "system" fn create_private_data_slot_ext(
                    _device: Device,
                    _p_create_info: *const PrivateDataSlotCreateInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_private_data_slot: *mut PrivateDataSlotEXT,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_private_data_slot_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreatePrivateDataSlotEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_private_data_slot_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_private_data_slot_ext: unsafe {
                extern "system" fn destroy_private_data_slot_ext(
                    _device: Device,
                    _private_data_slot: PrivateDataSlotEXT,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_private_data_slot_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyPrivateDataSlotEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_private_data_slot_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            set_private_data_ext: unsafe {
                extern "system" fn set_private_data_ext(
                    _device: Device,
                    _object_type: ObjectType,
                    _object_handle: u64,
                    _private_data_slot: PrivateDataSlotEXT,
                    _data: u64,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(set_private_data_ext)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkSetPrivateDataEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    set_private_data_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_private_data_ext: unsafe {
                extern "system" fn get_private_data_ext(
                    _device: Device,
                    _object_type: ObjectType,
                    _object_handle: u64,
                    _private_data_slot: PrivateDataSlotEXT,
                    _p_data: *mut u64,
                ) {
                    panic!(concat!("Unable to load ", stringify!(get_private_data_ext)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetPrivateDataEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    get_private_data_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreatePrivateDataSlotEXT.html>"]
    pub unsafe fn create_private_data_slot_ext(
        &self,
        device: Device,
        p_create_info: *const PrivateDataSlotCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_private_data_slot: *mut PrivateDataSlotEXT,
    ) -> Result {
        (self.create_private_data_slot_ext)(device, p_create_info, p_allocator, p_private_data_slot)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyPrivateDataSlotEXT.html>"]
    pub unsafe fn destroy_private_data_slot_ext(
        &self,
        device: Device,
        private_data_slot: PrivateDataSlotEXT,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_private_data_slot_ext)(device, private_data_slot, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetPrivateDataEXT.html>"]
    pub unsafe fn set_private_data_ext(
        &self,
        device: Device,
        object_type: ObjectType,
        object_handle: u64,
        private_data_slot: PrivateDataSlotEXT,
        data: u64,
    ) -> Result {
        (self.set_private_data_ext)(device, object_type, object_handle, private_data_slot, data)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPrivateDataEXT.html>"]
    pub unsafe fn get_private_data_ext(
        &self,
        device: Device,
        object_type: ObjectType,
        object_handle: u64,
        private_data_slot: PrivateDataSlotEXT,
        p_data: *mut u64,
    ) {
        (self.get_private_data_ext)(
            device,
            object_type,
            object_handle,
            private_data_slot,
            p_data,
        )
    }
}
#[doc = "Generated from 'VK_EXT_private_data'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT: Self = Self(1_000_295_000);
}
#[doc = "Generated from 'VK_EXT_private_data'"]
impl StructureType {
    pub const DEVICE_PRIVATE_DATA_CREATE_INFO_EXT: Self = Self(1_000_295_001);
}
#[doc = "Generated from 'VK_EXT_private_data'"]
impl StructureType {
    pub const PRIVATE_DATA_SLOT_CREATE_INFO_EXT: Self = Self(1_000_295_002);
}
#[doc = "Generated from 'VK_EXT_private_data'"]
impl ObjectType {
    pub const PRIVATE_DATA_SLOT_EXT: Self = Self(1_000_295_000);
}
impl KhrExtension297Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_297\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension297Fn {}
unsafe impl Send for KhrExtension297Fn {}
unsafe impl Sync for KhrExtension297Fn {}
impl ::std::clone::Clone for KhrExtension297Fn {
    fn clone(&self) -> Self {
        KhrExtension297Fn {}
    }
}
impl KhrExtension297Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension297Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_extension_297'"]
impl PipelineShaderStageCreateFlags {
    pub const RESERVED_3_KHR: Self = Self(0b1000);
}
impl ExtPipelineCreationCacheControlFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_pipeline_creation_cache_control\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
pub struct ExtPipelineCreationCacheControlFn {}
unsafe impl Send for ExtPipelineCreationCacheControlFn {}
unsafe impl Sync for ExtPipelineCreationCacheControlFn {}
impl ::std::clone::Clone for ExtPipelineCreationCacheControlFn {
    fn clone(&self) -> Self {
        ExtPipelineCreationCacheControlFn {}
    }
}
impl ExtPipelineCreationCacheControlFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPipelineCreationCacheControlFn {}
    }
}
#[doc = "Generated from 'VK_EXT_pipeline_creation_cache_control'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT: Self =
        Self(1_000_297_000);
}
#[doc = "Generated from 'VK_EXT_pipeline_creation_cache_control'"]
impl PipelineCreateFlags {
    pub const FAIL_ON_PIPELINE_COMPILE_REQUIRED_EXT: Self = Self(0b1_0000_0000);
}
#[doc = "Generated from 'VK_EXT_pipeline_creation_cache_control'"]
impl PipelineCreateFlags {
    pub const EARLY_RETURN_ON_FAILURE_EXT: Self = Self(0b10_0000_0000);
}
#[doc = "Generated from 'VK_EXT_pipeline_creation_cache_control'"]
impl Result {
    pub const PIPELINE_COMPILE_REQUIRED_EXT: Self = Self(1_000_297_000);
}
#[doc = "Generated from 'VK_EXT_pipeline_creation_cache_control'"]
impl Result {
    pub const ERROR_PIPELINE_COMPILE_REQUIRED_EXT: Self = Result::PIPELINE_COMPILE_REQUIRED_EXT;
}
#[doc = "Generated from 'VK_EXT_pipeline_creation_cache_control'"]
impl PipelineCacheCreateFlags {
    pub const EXTERNALLY_SYNCHRONIZED_EXT: Self = Self(0b1);
}
impl KhrExtension299Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_299\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension299Fn {}
unsafe impl Send for KhrExtension299Fn {}
unsafe impl Sync for KhrExtension299Fn {}
impl ::std::clone::Clone for KhrExtension299Fn {
    fn clone(&self) -> Self {
        KhrExtension299Fn {}
    }
}
impl KhrExtension299Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension299Fn {}
    }
}
impl KhrExtension300Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_300\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension300Fn {}
unsafe impl Send for KhrExtension300Fn {}
unsafe impl Sync for KhrExtension300Fn {}
impl ::std::clone::Clone for KhrExtension300Fn {
    fn clone(&self) -> Self {
        KhrExtension300Fn {}
    }
}
impl KhrExtension300Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension300Fn {}
    }
}
impl NvDeviceDiagnosticsConfigFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_device_diagnostics_config\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct NvDeviceDiagnosticsConfigFn {}
unsafe impl Send for NvDeviceDiagnosticsConfigFn {}
unsafe impl Sync for NvDeviceDiagnosticsConfigFn {}
impl ::std::clone::Clone for NvDeviceDiagnosticsConfigFn {
    fn clone(&self) -> Self {
        NvDeviceDiagnosticsConfigFn {}
    }
}
impl NvDeviceDiagnosticsConfigFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvDeviceDiagnosticsConfigFn {}
    }
}
#[doc = "Generated from 'VK_NV_device_diagnostics_config'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV: Self = Self(1_000_300_000);
}
#[doc = "Generated from 'VK_NV_device_diagnostics_config'"]
impl StructureType {
    pub const DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV: Self = Self(1_000_300_001);
}
impl QcomRenderPassStoreOpsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_render_pass_store_ops\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
pub struct QcomRenderPassStoreOpsFn {}
unsafe impl Send for QcomRenderPassStoreOpsFn {}
unsafe impl Sync for QcomRenderPassStoreOpsFn {}
impl ::std::clone::Clone for QcomRenderPassStoreOpsFn {
    fn clone(&self) -> Self {
        QcomRenderPassStoreOpsFn {}
    }
}
impl QcomRenderPassStoreOpsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomRenderPassStoreOpsFn {}
    }
}
#[doc = "Generated from 'VK_QCOM_render_pass_store_ops'"]
impl AttachmentStoreOp {
    pub const NONE_QCOM: Self = Self(1_000_301_000);
}
impl QcomExtension303Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_303\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension303Fn {}
unsafe impl Send for QcomExtension303Fn {}
unsafe impl Sync for QcomExtension303Fn {}
impl ::std::clone::Clone for QcomExtension303Fn {
    fn clone(&self) -> Self {
        QcomExtension303Fn {}
    }
}
impl QcomExtension303Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension303Fn {}
    }
}
impl QcomExtension304Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_304\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension304Fn {}
unsafe impl Send for QcomExtension304Fn {}
unsafe impl Sync for QcomExtension304Fn {}
impl ::std::clone::Clone for QcomExtension304Fn {
    fn clone(&self) -> Self {
        QcomExtension304Fn {}
    }
}
impl QcomExtension304Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension304Fn {}
    }
}
impl QcomExtension305Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_305\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension305Fn {}
unsafe impl Send for QcomExtension305Fn {}
unsafe impl Sync for QcomExtension305Fn {}
impl ::std::clone::Clone for QcomExtension305Fn {
    fn clone(&self) -> Self {
        QcomExtension305Fn {}
    }
}
impl QcomExtension305Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension305Fn {}
    }
}
impl QcomExtension306Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_306\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension306Fn {}
unsafe impl Send for QcomExtension306Fn {}
unsafe impl Sync for QcomExtension306Fn {}
impl ::std::clone::Clone for QcomExtension306Fn {
    fn clone(&self) -> Self {
        QcomExtension306Fn {}
    }
}
impl QcomExtension306Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension306Fn {}
    }
}
impl QcomExtension307Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_307\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension307Fn {}
unsafe impl Send for QcomExtension307Fn {}
unsafe impl Sync for QcomExtension307Fn {}
impl ::std::clone::Clone for QcomExtension307Fn {
    fn clone(&self) -> Self {
        QcomExtension307Fn {}
    }
}
impl QcomExtension307Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension307Fn {}
    }
}
impl NvExtension308Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_308\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension308Fn {}
unsafe impl Send for NvExtension308Fn {}
unsafe impl Sync for NvExtension308Fn {}
impl ::std::clone::Clone for NvExtension308Fn {
    fn clone(&self) -> Self {
        NvExtension308Fn {}
    }
}
impl NvExtension308Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension308Fn {}
    }
}
impl KhrExtension309Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_309\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension309Fn {}
unsafe impl Send for KhrExtension309Fn {}
unsafe impl Sync for KhrExtension309Fn {}
impl ::std::clone::Clone for KhrExtension309Fn {
    fn clone(&self) -> Self {
        KhrExtension309Fn {}
    }
}
impl KhrExtension309Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension309Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_extension_309'"]
impl MemoryHeapFlags {
    pub const RESERVED_2_KHR: Self = Self(0b100);
}
impl QcomExtension310Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_310\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension310Fn {}
unsafe impl Send for QcomExtension310Fn {}
unsafe impl Sync for QcomExtension310Fn {}
impl ::std::clone::Clone for QcomExtension310Fn {
    fn clone(&self) -> Self {
        QcomExtension310Fn {}
    }
}
impl QcomExtension310Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension310Fn {}
    }
}
#[doc = "Generated from 'VK_QCOM_extension_310'"]
impl StructureType {
    pub const RESERVED_QCOM: Self = Self(1_000_309_000);
}
impl NvExtension311Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_311\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension311Fn {}
unsafe impl Send for NvExtension311Fn {}
unsafe impl Sync for NvExtension311Fn {}
impl ::std::clone::Clone for NvExtension311Fn {
    fn clone(&self) -> Self {
        NvExtension311Fn {}
    }
}
impl NvExtension311Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension311Fn {}
    }
}
impl ExtExtension312Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_312\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension312Fn {}
unsafe impl Send for ExtExtension312Fn {}
unsafe impl Sync for ExtExtension312Fn {}
impl ::std::clone::Clone for ExtExtension312Fn {
    fn clone(&self) -> Self {
        ExtExtension312Fn {}
    }
}
impl ExtExtension312Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension312Fn {}
    }
}
impl ExtExtension313Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_313\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension313Fn {}
unsafe impl Send for ExtExtension313Fn {}
unsafe impl Sync for ExtExtension313Fn {}
impl ::std::clone::Clone for ExtExtension313Fn {
    fn clone(&self) -> Self {
        ExtExtension313Fn {}
    }
}
impl ExtExtension313Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension313Fn {}
    }
}
impl AmdExtension314Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_314\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension314Fn {}
unsafe impl Send for AmdExtension314Fn {}
unsafe impl Sync for AmdExtension314Fn {}
impl ::std::clone::Clone for AmdExtension314Fn {
    fn clone(&self) -> Self {
        AmdExtension314Fn {}
    }
}
impl AmdExtension314Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension314Fn {}
    }
}
impl AmdExtension315Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_315\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension315Fn {}
unsafe impl Send for AmdExtension315Fn {}
unsafe impl Sync for AmdExtension315Fn {}
impl ::std::clone::Clone for AmdExtension315Fn {
    fn clone(&self) -> Self {
        AmdExtension315Fn {}
    }
}
impl AmdExtension315Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension315Fn {}
    }
}
impl AmdExtension316Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_316\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension316Fn {}
unsafe impl Send for AmdExtension316Fn {}
unsafe impl Sync for AmdExtension316Fn {}
impl ::std::clone::Clone for AmdExtension316Fn {
    fn clone(&self) -> Self {
        AmdExtension316Fn {}
    }
}
impl AmdExtension316Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension316Fn {}
    }
}
impl AmdExtension317Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_317\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension317Fn {}
unsafe impl Send for AmdExtension317Fn {}
unsafe impl Sync for AmdExtension317Fn {}
impl ::std::clone::Clone for AmdExtension317Fn {
    fn clone(&self) -> Self {
        AmdExtension317Fn {}
    }
}
impl AmdExtension317Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension317Fn {}
    }
}
impl AmdExtension318Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_318\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension318Fn {}
unsafe impl Send for AmdExtension318Fn {}
unsafe impl Sync for AmdExtension318Fn {}
impl ::std::clone::Clone for AmdExtension318Fn {
    fn clone(&self) -> Self {
        AmdExtension318Fn {}
    }
}
impl AmdExtension318Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension318Fn {}
    }
}
impl AmdExtension319Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_319\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension319Fn {}
unsafe impl Send for AmdExtension319Fn {}
unsafe impl Sync for AmdExtension319Fn {}
impl ::std::clone::Clone for AmdExtension319Fn {
    fn clone(&self) -> Self {
        AmdExtension319Fn {}
    }
}
impl AmdExtension319Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension319Fn {}
    }
}
impl AmdExtension320Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_320\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension320Fn {}
unsafe impl Send for AmdExtension320Fn {}
unsafe impl Sync for AmdExtension320Fn {}
impl ::std::clone::Clone for AmdExtension320Fn {
    fn clone(&self) -> Self {
        AmdExtension320Fn {}
    }
}
impl AmdExtension320Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension320Fn {}
    }
}
impl AmdExtension321Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_321\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension321Fn {}
unsafe impl Send for AmdExtension321Fn {}
unsafe impl Sync for AmdExtension321Fn {}
impl ::std::clone::Clone for AmdExtension321Fn {
    fn clone(&self) -> Self {
        AmdExtension321Fn {}
    }
}
impl AmdExtension321Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension321Fn {}
    }
}
impl AmdExtension322Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_322\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension322Fn {}
unsafe impl Send for AmdExtension322Fn {}
unsafe impl Sync for AmdExtension322Fn {}
impl ::std::clone::Clone for AmdExtension322Fn {
    fn clone(&self) -> Self {
        AmdExtension322Fn {}
    }
}
impl AmdExtension322Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension322Fn {}
    }
}
impl AmdExtension323Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_323\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct AmdExtension323Fn {}
unsafe impl Send for AmdExtension323Fn {}
unsafe impl Sync for AmdExtension323Fn {}
impl ::std::clone::Clone for AmdExtension323Fn {
    fn clone(&self) -> Self {
        AmdExtension323Fn {}
    }
}
impl AmdExtension323Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension323Fn {}
    }
}
impl KhrExtension324Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_324\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension324Fn {}
unsafe impl Send for KhrExtension324Fn {}
unsafe impl Sync for KhrExtension324Fn {}
impl ::std::clone::Clone for KhrExtension324Fn {
    fn clone(&self) -> Self {
        KhrExtension324Fn {}
    }
}
impl KhrExtension324Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension324Fn {}
    }
}
impl KhrExtension325Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_325\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension325Fn {}
unsafe impl Send for KhrExtension325Fn {}
unsafe impl Sync for KhrExtension325Fn {}
impl ::std::clone::Clone for KhrExtension325Fn {
    fn clone(&self) -> Self {
        KhrExtension325Fn {}
    }
}
impl KhrExtension325Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension325Fn {}
    }
}
impl KhrZeroInitializeWorkgroupMemoryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_zero_initialize_workgroup_memory\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrZeroInitializeWorkgroupMemoryFn {}
unsafe impl Send for KhrZeroInitializeWorkgroupMemoryFn {}
unsafe impl Sync for KhrZeroInitializeWorkgroupMemoryFn {}
impl ::std::clone::Clone for KhrZeroInitializeWorkgroupMemoryFn {
    fn clone(&self) -> Self {
        KhrZeroInitializeWorkgroupMemoryFn {}
    }
}
impl KhrZeroInitializeWorkgroupMemoryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrZeroInitializeWorkgroupMemoryFn {}
    }
}
#[doc = "Generated from 'VK_KHR_zero_initialize_workgroup_memory'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES_KHR: Self =
        Self(1_000_325_000);
}
impl NvFragmentShadingRateEnumsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_fragment_shading_rate_enums\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetFragmentShadingRateEnumNV = extern "system" fn(
    command_buffer: CommandBuffer,
    shading_rate: FragmentShadingRateNV,
    combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
);
pub struct NvFragmentShadingRateEnumsFn {
    pub cmd_set_fragment_shading_rate_enum_nv: extern "system" fn(
        command_buffer: CommandBuffer,
        shading_rate: FragmentShadingRateNV,
        combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
    ),
}
unsafe impl Send for NvFragmentShadingRateEnumsFn {}
unsafe impl Sync for NvFragmentShadingRateEnumsFn {}
impl ::std::clone::Clone for NvFragmentShadingRateEnumsFn {
    fn clone(&self) -> Self {
        NvFragmentShadingRateEnumsFn {
            cmd_set_fragment_shading_rate_enum_nv: self.cmd_set_fragment_shading_rate_enum_nv,
        }
    }
}
impl NvFragmentShadingRateEnumsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvFragmentShadingRateEnumsFn {
            cmd_set_fragment_shading_rate_enum_nv: unsafe {
                extern "system" fn cmd_set_fragment_shading_rate_enum_nv(
                    _command_buffer: CommandBuffer,
                    _shading_rate: FragmentShadingRateNV,
                    _combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_fragment_shading_rate_enum_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetFragmentShadingRateEnumNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_fragment_shading_rate_enum_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetFragmentShadingRateEnumNV.html>"]
    pub unsafe fn cmd_set_fragment_shading_rate_enum_nv(
        &self,
        command_buffer: CommandBuffer,
        shading_rate: FragmentShadingRateNV,
        combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
    ) {
        (self.cmd_set_fragment_shading_rate_enum_nv)(command_buffer, shading_rate, combiner_ops)
    }
}
#[doc = "Generated from 'VK_NV_fragment_shading_rate_enums'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_PROPERTIES_NV: Self = Self(1_000_326_000);
}
#[doc = "Generated from 'VK_NV_fragment_shading_rate_enums'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV: Self = Self(1_000_326_001);
}
#[doc = "Generated from 'VK_NV_fragment_shading_rate_enums'"]
impl StructureType {
    pub const PIPELINE_FRAGMENT_SHADING_RATE_ENUM_STATE_CREATE_INFO_NV: Self = Self(1_000_326_002);
}
impl NvExtension328Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_328\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension328Fn {}
unsafe impl Send for NvExtension328Fn {}
unsafe impl Sync for NvExtension328Fn {}
impl ::std::clone::Clone for NvExtension328Fn {
    fn clone(&self) -> Self {
        NvExtension328Fn {}
    }
}
impl NvExtension328Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension328Fn {}
    }
}
#[doc = "Generated from 'VK_NV_extension_328'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const RESERVED_5_NV: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_NV_extension_328'"]
impl AccelerationStructureCreateFlagsKHR {
    pub const RESERVED_2_NV: Self = Self(0b100);
}
impl NvExtension329Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_329\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension329Fn {}
unsafe impl Send for NvExtension329Fn {}
unsafe impl Sync for NvExtension329Fn {}
impl ::std::clone::Clone for NvExtension329Fn {
    fn clone(&self) -> Self {
        NvExtension329Fn {}
    }
}
impl NvExtension329Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension329Fn {}
    }
}
impl NvExtension330Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_330\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension330Fn {}
unsafe impl Send for NvExtension330Fn {}
unsafe impl Sync for NvExtension330Fn {}
impl ::std::clone::Clone for NvExtension330Fn {
    fn clone(&self) -> Self {
        NvExtension330Fn {}
    }
}
impl NvExtension330Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension330Fn {}
    }
}
impl NvExtension331Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_331\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension331Fn {}
unsafe impl Send for NvExtension331Fn {}
unsafe impl Sync for NvExtension331Fn {}
impl ::std::clone::Clone for NvExtension331Fn {
    fn clone(&self) -> Self {
        NvExtension331Fn {}
    }
}
impl NvExtension331Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension331Fn {}
    }
}
impl NvExtension332Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_332\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension332Fn {}
unsafe impl Send for NvExtension332Fn {}
unsafe impl Sync for NvExtension332Fn {}
impl ::std::clone::Clone for NvExtension332Fn {
    fn clone(&self) -> Self {
        NvExtension332Fn {}
    }
}
impl NvExtension332Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension332Fn {}
    }
}
impl ExtFragmentDensityMap2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_fragment_density_map2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtFragmentDensityMap2Fn {}
unsafe impl Send for ExtFragmentDensityMap2Fn {}
unsafe impl Sync for ExtFragmentDensityMap2Fn {}
impl ::std::clone::Clone for ExtFragmentDensityMap2Fn {
    fn clone(&self) -> Self {
        ExtFragmentDensityMap2Fn {}
    }
}
impl ExtFragmentDensityMap2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtFragmentDensityMap2Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_fragment_density_map2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT: Self = Self(1_000_332_000);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_PROPERTIES_EXT: Self = Self(1_000_332_001);
}
#[doc = "Generated from 'VK_EXT_fragment_density_map2'"]
impl ImageViewCreateFlags {
    pub const FRAGMENT_DENSITY_MAP_DEFERRED_EXT: Self = Self(0b10);
}
impl QcomRotatedCopyCommandsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_rotated_copy_commands\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomRotatedCopyCommandsFn {}
unsafe impl Send for QcomRotatedCopyCommandsFn {}
unsafe impl Sync for QcomRotatedCopyCommandsFn {}
impl ::std::clone::Clone for QcomRotatedCopyCommandsFn {
    fn clone(&self) -> Self {
        QcomRotatedCopyCommandsFn {}
    }
}
impl QcomRotatedCopyCommandsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomRotatedCopyCommandsFn {}
    }
}
#[doc = "Generated from 'VK_QCOM_rotated_copy_commands'"]
impl StructureType {
    pub const COPY_COMMAND_TRANSFORM_INFO_QCOM: Self = Self(1_000_333_000);
}
impl KhrExtension335Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_335\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension335Fn {}
unsafe impl Send for KhrExtension335Fn {}
unsafe impl Sync for KhrExtension335Fn {}
impl ::std::clone::Clone for KhrExtension335Fn {
    fn clone(&self) -> Self {
        KhrExtension335Fn {}
    }
}
impl KhrExtension335Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension335Fn {}
    }
}
impl ExtImageRobustnessFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_image_robustness\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ExtImageRobustnessFn {}
unsafe impl Send for ExtImageRobustnessFn {}
unsafe impl Sync for ExtImageRobustnessFn {}
impl ::std::clone::Clone for ExtImageRobustnessFn {
    fn clone(&self) -> Self {
        ExtImageRobustnessFn {}
    }
}
impl ExtImageRobustnessFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtImageRobustnessFn {}
    }
}
#[doc = "Generated from 'VK_EXT_image_robustness'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT: Self = Self(1_000_335_000);
}
impl KhrWorkgroupMemoryExplicitLayoutFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_workgroup_memory_explicit_layout\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct KhrWorkgroupMemoryExplicitLayoutFn {}
unsafe impl Send for KhrWorkgroupMemoryExplicitLayoutFn {}
unsafe impl Sync for KhrWorkgroupMemoryExplicitLayoutFn {}
impl ::std::clone::Clone for KhrWorkgroupMemoryExplicitLayoutFn {
    fn clone(&self) -> Self {
        KhrWorkgroupMemoryExplicitLayoutFn {}
    }
}
impl KhrWorkgroupMemoryExplicitLayoutFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrWorkgroupMemoryExplicitLayoutFn {}
    }
}
#[doc = "Generated from 'VK_KHR_workgroup_memory_explicit_layout'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR: Self =
        Self(1_000_336_000);
}
impl KhrCopyCommands2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_copy_commands2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyBuffer2KHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_copy_buffer_info: *const CopyBufferInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyImage2KHR =
    extern "system" fn(command_buffer: CommandBuffer, p_copy_image_info: *const CopyImageInfo2KHR);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyBufferToImage2KHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_copy_buffer_to_image_info: *const CopyBufferToImageInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyImageToBuffer2KHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_copy_image_to_buffer_info: *const CopyImageToBufferInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBlitImage2KHR =
    extern "system" fn(command_buffer: CommandBuffer, p_blit_image_info: *const BlitImageInfo2KHR);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdResolveImage2KHR = extern "system" fn(
    command_buffer: CommandBuffer,
    p_resolve_image_info: *const ResolveImageInfo2KHR,
);
pub struct KhrCopyCommands2Fn {
    pub cmd_copy_buffer2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_copy_buffer_info: *const CopyBufferInfo2KHR,
    ),
    pub cmd_copy_image2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_copy_image_info: *const CopyImageInfo2KHR,
    ),
    pub cmd_copy_buffer_to_image2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_copy_buffer_to_image_info: *const CopyBufferToImageInfo2KHR,
    ),
    pub cmd_copy_image_to_buffer2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_copy_image_to_buffer_info: *const CopyImageToBufferInfo2KHR,
    ),
    pub cmd_blit_image2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_blit_image_info: *const BlitImageInfo2KHR,
    ),
    pub cmd_resolve_image2_khr: extern "system" fn(
        command_buffer: CommandBuffer,
        p_resolve_image_info: *const ResolveImageInfo2KHR,
    ),
}
unsafe impl Send for KhrCopyCommands2Fn {}
unsafe impl Sync for KhrCopyCommands2Fn {}
impl ::std::clone::Clone for KhrCopyCommands2Fn {
    fn clone(&self) -> Self {
        KhrCopyCommands2Fn {
            cmd_copy_buffer2_khr: self.cmd_copy_buffer2_khr,
            cmd_copy_image2_khr: self.cmd_copy_image2_khr,
            cmd_copy_buffer_to_image2_khr: self.cmd_copy_buffer_to_image2_khr,
            cmd_copy_image_to_buffer2_khr: self.cmd_copy_image_to_buffer2_khr,
            cmd_blit_image2_khr: self.cmd_blit_image2_khr,
            cmd_resolve_image2_khr: self.cmd_resolve_image2_khr,
        }
    }
}
impl KhrCopyCommands2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrCopyCommands2Fn {
            cmd_copy_buffer2_khr: unsafe {
                extern "system" fn cmd_copy_buffer2_khr(
                    _command_buffer: CommandBuffer,
                    _p_copy_buffer_info: *const CopyBufferInfo2KHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_copy_buffer2_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCopyBuffer2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_buffer2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_image2_khr: unsafe {
                extern "system" fn cmd_copy_image2_khr(
                    _command_buffer: CommandBuffer,
                    _p_copy_image_info: *const CopyImageInfo2KHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_copy_image2_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCopyImage2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_image2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_buffer_to_image2_khr: unsafe {
                extern "system" fn cmd_copy_buffer_to_image2_khr(
                    _command_buffer: CommandBuffer,
                    _p_copy_buffer_to_image_info: *const CopyBufferToImageInfo2KHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_buffer_to_image2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdCopyBufferToImage2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_buffer_to_image2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_image_to_buffer2_khr: unsafe {
                extern "system" fn cmd_copy_image_to_buffer2_khr(
                    _command_buffer: CommandBuffer,
                    _p_copy_image_to_buffer_info: *const CopyImageToBufferInfo2KHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_image_to_buffer2_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdCopyImageToBuffer2KHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_image_to_buffer2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_blit_image2_khr: unsafe {
                extern "system" fn cmd_blit_image2_khr(
                    _command_buffer: CommandBuffer,
                    _p_blit_image_info: *const BlitImageInfo2KHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_blit_image2_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBlitImage2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_blit_image2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_resolve_image2_khr: unsafe {
                extern "system" fn cmd_resolve_image2_khr(
                    _command_buffer: CommandBuffer,
                    _p_resolve_image_info: *const ResolveImageInfo2KHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_resolve_image2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdResolveImage2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_resolve_image2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyBuffer2KHR.html>"]
    pub unsafe fn cmd_copy_buffer2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_copy_buffer_info: *const CopyBufferInfo2KHR,
    ) {
        (self.cmd_copy_buffer2_khr)(command_buffer, p_copy_buffer_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyImage2KHR.html>"]
    pub unsafe fn cmd_copy_image2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_copy_image_info: *const CopyImageInfo2KHR,
    ) {
        (self.cmd_copy_image2_khr)(command_buffer, p_copy_image_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyBufferToImage2KHR.html>"]
    pub unsafe fn cmd_copy_buffer_to_image2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_copy_buffer_to_image_info: *const CopyBufferToImageInfo2KHR,
    ) {
        (self.cmd_copy_buffer_to_image2_khr)(command_buffer, p_copy_buffer_to_image_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyImageToBuffer2KHR.html>"]
    pub unsafe fn cmd_copy_image_to_buffer2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_copy_image_to_buffer_info: *const CopyImageToBufferInfo2KHR,
    ) {
        (self.cmd_copy_image_to_buffer2_khr)(command_buffer, p_copy_image_to_buffer_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBlitImage2KHR.html>"]
    pub unsafe fn cmd_blit_image2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_blit_image_info: *const BlitImageInfo2KHR,
    ) {
        (self.cmd_blit_image2_khr)(command_buffer, p_blit_image_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResolveImage2KHR.html>"]
    pub unsafe fn cmd_resolve_image2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_resolve_image_info: *const ResolveImageInfo2KHR,
    ) {
        (self.cmd_resolve_image2_khr)(command_buffer, p_resolve_image_info)
    }
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const COPY_BUFFER_INFO_2_KHR: Self = Self(1_000_337_000);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const COPY_IMAGE_INFO_2_KHR: Self = Self(1_000_337_001);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const COPY_BUFFER_TO_IMAGE_INFO_2_KHR: Self = Self(1_000_337_002);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const COPY_IMAGE_TO_BUFFER_INFO_2_KHR: Self = Self(1_000_337_003);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const BLIT_IMAGE_INFO_2_KHR: Self = Self(1_000_337_004);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const RESOLVE_IMAGE_INFO_2_KHR: Self = Self(1_000_337_005);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const BUFFER_COPY_2_KHR: Self = Self(1_000_337_006);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const IMAGE_COPY_2_KHR: Self = Self(1_000_337_007);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const IMAGE_BLIT_2_KHR: Self = Self(1_000_337_008);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const BUFFER_IMAGE_COPY_2_KHR: Self = Self(1_000_337_009);
}
#[doc = "Generated from 'VK_KHR_copy_commands2'"]
impl StructureType {
    pub const IMAGE_RESOLVE_2_KHR: Self = Self(1_000_337_010);
}
impl ArmExtension339Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ARM_extension_339\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ArmExtension339Fn {}
unsafe impl Send for ArmExtension339Fn {}
unsafe impl Sync for ArmExtension339Fn {}
impl ::std::clone::Clone for ArmExtension339Fn {
    fn clone(&self) -> Self {
        ArmExtension339Fn {}
    }
}
impl ArmExtension339Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ArmExtension339Fn {}
    }
}
impl ExtExtension340Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_340\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension340Fn {}
unsafe impl Send for ExtExtension340Fn {}
unsafe impl Sync for ExtExtension340Fn {}
impl ::std::clone::Clone for ExtExtension340Fn {
    fn clone(&self) -> Self {
        ExtExtension340Fn {}
    }
}
impl ExtExtension340Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension340Fn {}
    }
}
impl Ext4444FormatsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_4444_formats\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct Ext4444FormatsFn {}
unsafe impl Send for Ext4444FormatsFn {}
unsafe impl Sync for Ext4444FormatsFn {}
impl ::std::clone::Clone for Ext4444FormatsFn {
    fn clone(&self) -> Self {
        Ext4444FormatsFn {}
    }
}
impl Ext4444FormatsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Ext4444FormatsFn {}
    }
}
#[doc = "Generated from 'VK_EXT_4444_formats'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT: Self = Self(1_000_340_000);
}
#[doc = "Generated from 'VK_EXT_4444_formats'"]
impl Format {
    pub const A4R4G4B4_UNORM_PACK16_EXT: Self = Self(1_000_340_000);
}
#[doc = "Generated from 'VK_EXT_4444_formats'"]
impl Format {
    pub const A4B4G4R4_UNORM_PACK16_EXT: Self = Self(1_000_340_001);
}
impl ExtExtension342Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_342\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension342Fn {}
unsafe impl Send for ExtExtension342Fn {}
unsafe impl Sync for ExtExtension342Fn {}
impl ::std::clone::Clone for ExtExtension342Fn {
    fn clone(&self) -> Self {
        ExtExtension342Fn {}
    }
}
impl ExtExtension342Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension342Fn {}
    }
}
impl ArmExtension343Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ARM_extension_343\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ArmExtension343Fn {}
unsafe impl Send for ArmExtension343Fn {}
unsafe impl Sync for ArmExtension343Fn {}
impl ::std::clone::Clone for ArmExtension343Fn {
    fn clone(&self) -> Self {
        ArmExtension343Fn {}
    }
}
impl ArmExtension343Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ArmExtension343Fn {}
    }
}
impl ArmExtension344Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ARM_extension_344\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ArmExtension344Fn {}
unsafe impl Send for ArmExtension344Fn {}
unsafe impl Sync for ArmExtension344Fn {}
impl ::std::clone::Clone for ArmExtension344Fn {
    fn clone(&self) -> Self {
        ArmExtension344Fn {}
    }
}
impl ArmExtension344Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ArmExtension344Fn {}
    }
}
impl ArmExtension345Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ARM_extension_345\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ArmExtension345Fn {}
unsafe impl Send for ArmExtension345Fn {}
unsafe impl Sync for ArmExtension345Fn {}
impl ::std::clone::Clone for ArmExtension345Fn {
    fn clone(&self) -> Self {
        ArmExtension345Fn {}
    }
}
impl ArmExtension345Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ArmExtension345Fn {}
    }
}
impl NvAcquireWinrtDisplayFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_acquire_winrt_display\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireWinrtDisplayNV =
    extern "system" fn(physical_device: PhysicalDevice, display: DisplayKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetWinrtDisplayNV = extern "system" fn(
    physical_device: PhysicalDevice,
    device_relative_id: u32,
    p_display: *mut DisplayKHR,
) -> Result;
pub struct NvAcquireWinrtDisplayFn {
    pub acquire_winrt_display_nv:
        extern "system" fn(physical_device: PhysicalDevice, display: DisplayKHR) -> Result,
    pub get_winrt_display_nv: extern "system" fn(
        physical_device: PhysicalDevice,
        device_relative_id: u32,
        p_display: *mut DisplayKHR,
    ) -> Result,
}
unsafe impl Send for NvAcquireWinrtDisplayFn {}
unsafe impl Sync for NvAcquireWinrtDisplayFn {}
impl ::std::clone::Clone for NvAcquireWinrtDisplayFn {
    fn clone(&self) -> Self {
        NvAcquireWinrtDisplayFn {
            acquire_winrt_display_nv: self.acquire_winrt_display_nv,
            get_winrt_display_nv: self.get_winrt_display_nv,
        }
    }
}
impl NvAcquireWinrtDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvAcquireWinrtDisplayFn {
            acquire_winrt_display_nv: unsafe {
                extern "system" fn acquire_winrt_display_nv(
                    _physical_device: PhysicalDevice,
                    _display: DisplayKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_winrt_display_nv)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireWinrtDisplayNV\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_winrt_display_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_winrt_display_nv: unsafe {
                extern "system" fn get_winrt_display_nv(
                    _physical_device: PhysicalDevice,
                    _device_relative_id: u32,
                    _p_display: *mut DisplayKHR,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_winrt_display_nv)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetWinrtDisplayNV\0");
                let val = _f(cname);
                if val.is_null() {
                    get_winrt_display_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireWinrtDisplayNV.html>"]
    pub unsafe fn acquire_winrt_display_nv(
        &self,
        physical_device: PhysicalDevice,
        display: DisplayKHR,
    ) -> Result {
        (self.acquire_winrt_display_nv)(physical_device, display)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetWinrtDisplayNV.html>"]
    pub unsafe fn get_winrt_display_nv(
        &self,
        physical_device: PhysicalDevice,
        device_relative_id: u32,
        p_display: *mut DisplayKHR,
    ) -> Result {
        (self.get_winrt_display_nv)(physical_device, device_relative_id, p_display)
    }
}
impl ExtDirectfbSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_directfb_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDirectFBSurfaceEXT = extern "system" fn(
    instance: Instance,
    p_create_info: *const DirectFBSurfaceCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT = extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    dfb: *mut IDirectFB,
) -> Bool32;
pub struct ExtDirectfbSurfaceFn {
    pub create_direct_fb_surface_ext: extern "system" fn(
        instance: Instance,
        p_create_info: *const DirectFBSurfaceCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result,
    pub get_physical_device_direct_fb_presentation_support_ext: extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        dfb: *mut IDirectFB,
    ) -> Bool32,
}
unsafe impl Send for ExtDirectfbSurfaceFn {}
unsafe impl Sync for ExtDirectfbSurfaceFn {}
impl ::std::clone::Clone for ExtDirectfbSurfaceFn {
    fn clone(&self) -> Self {
        ExtDirectfbSurfaceFn {
            create_direct_fb_surface_ext: self.create_direct_fb_surface_ext,
            get_physical_device_direct_fb_presentation_support_ext: self
                .get_physical_device_direct_fb_presentation_support_ext,
        }
    }
}
impl ExtDirectfbSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDirectfbSurfaceFn {
            create_direct_fb_surface_ext: unsafe {
                extern "system" fn create_direct_fb_surface_ext(
                    _instance: Instance,
                    _p_create_info: *const DirectFBSurfaceCreateInfoEXT,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_direct_fb_surface_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDirectFBSurfaceEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_direct_fb_surface_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_direct_fb_presentation_support_ext: unsafe {
                extern "system" fn get_physical_device_direct_fb_presentation_support_ext(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                    _dfb: *mut IDirectFB,
                ) -> Bool32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_direct_fb_presentation_support_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceDirectFBPresentationSupportEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_direct_fb_presentation_support_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDirectFBSurfaceEXT.html>"]
    pub unsafe fn create_direct_fb_surface_ext(
        &self,
        instance: Instance,
        p_create_info: *const DirectFBSurfaceCreateInfoEXT,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_direct_fb_surface_ext)(instance, p_create_info, p_allocator, p_surface)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceDirectFBPresentationSupportEXT.html>"]
    pub unsafe fn get_physical_device_direct_fb_presentation_support_ext(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        dfb: *mut IDirectFB,
    ) -> Bool32 {
        (self.get_physical_device_direct_fb_presentation_support_ext)(
            physical_device,
            queue_family_index,
            dfb,
        )
    }
}
#[doc = "Generated from 'VK_EXT_directfb_surface'"]
impl StructureType {
    pub const DIRECTFB_SURFACE_CREATE_INFO_EXT: Self = Self(1_000_346_000);
}
impl KhrExtension350Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_350\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension350Fn {}
unsafe impl Send for KhrExtension350Fn {}
unsafe impl Sync for KhrExtension350Fn {}
impl ::std::clone::Clone for KhrExtension350Fn {
    fn clone(&self) -> Self {
        KhrExtension350Fn {}
    }
}
impl KhrExtension350Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension350Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_extension_350'"]
impl PipelineCacheCreateFlags {
    pub const RESERVED_2_EXT: Self = Self(0b100);
}
impl NvExtension351Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_351\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension351Fn {}
unsafe impl Send for NvExtension351Fn {}
unsafe impl Sync for NvExtension351Fn {}
impl ::std::clone::Clone for NvExtension351Fn {
    fn clone(&self) -> Self {
        NvExtension351Fn {}
    }
}
impl NvExtension351Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension351Fn {}
    }
}
impl ValveMutableDescriptorTypeFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_VALVE_mutable_descriptor_type\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct ValveMutableDescriptorTypeFn {}
unsafe impl Send for ValveMutableDescriptorTypeFn {}
unsafe impl Sync for ValveMutableDescriptorTypeFn {}
impl ::std::clone::Clone for ValveMutableDescriptorTypeFn {
    fn clone(&self) -> Self {
        ValveMutableDescriptorTypeFn {}
    }
}
impl ValveMutableDescriptorTypeFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ValveMutableDescriptorTypeFn {}
    }
}
#[doc = "Generated from 'VK_VALVE_mutable_descriptor_type'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_VALVE: Self = Self(1_000_351_000);
}
#[doc = "Generated from 'VK_VALVE_mutable_descriptor_type'"]
impl StructureType {
    pub const MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE: Self = Self(1_000_351_002);
}
#[doc = "Generated from 'VK_VALVE_mutable_descriptor_type'"]
impl DescriptorType {
    pub const MUTABLE_VALVE: Self = Self(1_000_351_000);
}
#[doc = "Generated from 'VK_VALVE_mutable_descriptor_type'"]
impl DescriptorPoolCreateFlags {
    pub const HOST_ONLY_VALVE: Self = Self(0b100);
}
#[doc = "Generated from 'VK_VALVE_mutable_descriptor_type'"]
impl DescriptorSetLayoutCreateFlags {
    pub const HOST_ONLY_POOL_VALVE: Self = Self(0b100);
}
impl ExtExtension353Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_353\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension353Fn {}
unsafe impl Send for ExtExtension353Fn {}
unsafe impl Sync for ExtExtension353Fn {}
impl ::std::clone::Clone for ExtExtension353Fn {
    fn clone(&self) -> Self {
        ExtExtension353Fn {}
    }
}
impl ExtExtension353Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension353Fn {}
    }
}
impl ExtExtension354Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_354\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension354Fn {}
unsafe impl Send for ExtExtension354Fn {}
unsafe impl Sync for ExtExtension354Fn {}
impl ::std::clone::Clone for ExtExtension354Fn {
    fn clone(&self) -> Self {
        ExtExtension354Fn {}
    }
}
impl ExtExtension354Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension354Fn {}
    }
}
impl ExtExtension355Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_355\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension355Fn {}
unsafe impl Send for ExtExtension355Fn {}
unsafe impl Sync for ExtExtension355Fn {}
impl ::std::clone::Clone for ExtExtension355Fn {
    fn clone(&self) -> Self {
        ExtExtension355Fn {}
    }
}
impl ExtExtension355Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension355Fn {}
    }
}
impl ExtVertexAttributeAliasingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_vertex_attribute_aliasing\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtVertexAttributeAliasingFn {}
unsafe impl Send for ExtVertexAttributeAliasingFn {}
unsafe impl Sync for ExtVertexAttributeAliasingFn {}
impl ::std::clone::Clone for ExtVertexAttributeAliasingFn {
    fn clone(&self) -> Self {
        ExtVertexAttributeAliasingFn {}
    }
}
impl ExtVertexAttributeAliasingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVertexAttributeAliasingFn {}
    }
}
impl ExtExtension357Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_357\0")
            .expect("Wrong extension string")
    }
}
pub struct ExtExtension357Fn {}
unsafe impl Send for ExtExtension357Fn {}
unsafe impl Sync for ExtExtension357Fn {}
impl ::std::clone::Clone for ExtExtension357Fn {
    fn clone(&self) -> Self {
        ExtExtension357Fn {}
    }
}
impl ExtExtension357Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension357Fn {}
    }
}
impl KhrExtension358Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_358\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension358Fn {}
unsafe impl Send for KhrExtension358Fn {}
unsafe impl Sync for KhrExtension358Fn {}
impl ::std::clone::Clone for KhrExtension358Fn {
    fn clone(&self) -> Self {
        KhrExtension358Fn {}
    }
}
impl KhrExtension358Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension358Fn {}
    }
}
impl ExtExtension359Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_359\0")
            .expect("Wrong extension string")
    }
}
pub struct ExtExtension359Fn {}
unsafe impl Send for ExtExtension359Fn {}
unsafe impl Sync for ExtExtension359Fn {}
impl ::std::clone::Clone for ExtExtension359Fn {
    fn clone(&self) -> Self {
        ExtExtension359Fn {}
    }
}
impl ExtExtension359Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension359Fn {}
    }
}
impl ExtExtension360Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_360\0")
            .expect("Wrong extension string")
    }
}
pub struct ExtExtension360Fn {}
unsafe impl Send for ExtExtension360Fn {}
unsafe impl Sync for ExtExtension360Fn {}
impl ::std::clone::Clone for ExtExtension360Fn {
    fn clone(&self) -> Self {
        ExtExtension360Fn {}
    }
}
impl ExtExtension360Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension360Fn {}
    }
}
impl KhrExtension361Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_361\0")
            .expect("Wrong extension string")
    }
}
pub struct KhrExtension361Fn {}
unsafe impl Send for KhrExtension361Fn {}
unsafe impl Sync for KhrExtension361Fn {}
impl ::std::clone::Clone for KhrExtension361Fn {
    fn clone(&self) -> Self {
        KhrExtension361Fn {}
    }
}
impl KhrExtension361Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension361Fn {}
    }
}
impl ExtExtension362Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_362\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension362Fn {}
unsafe impl Send for ExtExtension362Fn {}
unsafe impl Sync for ExtExtension362Fn {}
impl ::std::clone::Clone for ExtExtension362Fn {
    fn clone(&self) -> Self {
        ExtExtension362Fn {}
    }
}
impl ExtExtension362Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension362Fn {}
    }
}
impl ExtExtension363Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_363\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension363Fn {}
unsafe impl Send for ExtExtension363Fn {}
unsafe impl Sync for ExtExtension363Fn {}
impl ::std::clone::Clone for ExtExtension363Fn {
    fn clone(&self) -> Self {
        ExtExtension363Fn {}
    }
}
impl ExtExtension363Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension363Fn {}
    }
}
impl FuchsiaExtension364Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_extension_364\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct FuchsiaExtension364Fn {}
unsafe impl Send for FuchsiaExtension364Fn {}
unsafe impl Sync for FuchsiaExtension364Fn {}
impl ::std::clone::Clone for FuchsiaExtension364Fn {
    fn clone(&self) -> Self {
        FuchsiaExtension364Fn {}
    }
}
impl FuchsiaExtension364Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExtension364Fn {}
    }
}
impl FuchsiaExtension365Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_extension_365\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct FuchsiaExtension365Fn {}
unsafe impl Send for FuchsiaExtension365Fn {}
unsafe impl Sync for FuchsiaExtension365Fn {}
impl ::std::clone::Clone for FuchsiaExtension365Fn {
    fn clone(&self) -> Self {
        FuchsiaExtension365Fn {}
    }
}
impl FuchsiaExtension365Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExtension365Fn {}
    }
}
impl FuchsiaExtension366Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_extension_366\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct FuchsiaExtension366Fn {}
unsafe impl Send for FuchsiaExtension366Fn {}
unsafe impl Sync for FuchsiaExtension366Fn {}
impl ::std::clone::Clone for FuchsiaExtension366Fn {
    fn clone(&self) -> Self {
        FuchsiaExtension366Fn {}
    }
}
impl FuchsiaExtension366Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExtension366Fn {}
    }
}
impl FuchsiaExtension367Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_extension_367\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct FuchsiaExtension367Fn {}
unsafe impl Send for FuchsiaExtension367Fn {}
unsafe impl Sync for FuchsiaExtension367Fn {}
impl ::std::clone::Clone for FuchsiaExtension367Fn {
    fn clone(&self) -> Self {
        FuchsiaExtension367Fn {}
    }
}
impl FuchsiaExtension367Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExtension367Fn {}
    }
}
impl FuchsiaExtension368Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_extension_368\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct FuchsiaExtension368Fn {}
unsafe impl Send for FuchsiaExtension368Fn {}
unsafe impl Sync for FuchsiaExtension368Fn {}
impl ::std::clone::Clone for FuchsiaExtension368Fn {
    fn clone(&self) -> Self {
        FuchsiaExtension368Fn {}
    }
}
impl FuchsiaExtension368Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExtension368Fn {}
    }
}
impl QcomExtension369Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_369\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct QcomExtension369Fn {}
unsafe impl Send for QcomExtension369Fn {}
unsafe impl Sync for QcomExtension369Fn {}
impl ::std::clone::Clone for QcomExtension369Fn {
    fn clone(&self) -> Self {
        QcomExtension369Fn {}
    }
}
impl QcomExtension369Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QcomExtension369Fn {}
    }
}
#[doc = "Generated from 'VK_QCOM_extension_369'"]
impl DescriptorBindingFlags {
    pub const RESERVED_4_QCOM: Self = Self(0b1_0000);
}
impl HuaweiExtension370Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_HUAWEI_extension_370\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct HuaweiExtension370Fn {}
unsafe impl Send for HuaweiExtension370Fn {}
unsafe impl Sync for HuaweiExtension370Fn {}
impl ::std::clone::Clone for HuaweiExtension370Fn {
    fn clone(&self) -> Self {
        HuaweiExtension370Fn {}
    }
}
impl HuaweiExtension370Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        HuaweiExtension370Fn {}
    }
}
impl HuaweiExtension371Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_HUAWEI_extension_371\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct HuaweiExtension371Fn {}
unsafe impl Send for HuaweiExtension371Fn {}
unsafe impl Sync for HuaweiExtension371Fn {}
impl ::std::clone::Clone for HuaweiExtension371Fn {
    fn clone(&self) -> Self {
        HuaweiExtension371Fn {}
    }
}
impl HuaweiExtension371Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        HuaweiExtension371Fn {}
    }
}
impl NvExtension372Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_372\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension372Fn {}
unsafe impl Send for NvExtension372Fn {}
unsafe impl Sync for NvExtension372Fn {}
impl ::std::clone::Clone for NvExtension372Fn {
    fn clone(&self) -> Self {
        NvExtension372Fn {}
    }
}
impl NvExtension372Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension372Fn {}
    }
}
#[doc = "Generated from 'VK_NV_extension_372'"]
impl BufferCreateFlags {
    pub const RESERVED_5_NV: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_NV_extension_372'"]
impl ImageCreateFlags {
    pub const RESERVED_15_NV: Self = Self(0b1000_0000_0000_0000);
}
impl NvExtension373Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_373\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension373Fn {}
unsafe impl Send for NvExtension373Fn {}
unsafe impl Sync for NvExtension373Fn {}
impl ::std::clone::Clone for NvExtension373Fn {
    fn clone(&self) -> Self {
        NvExtension373Fn {}
    }
}
impl NvExtension373Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension373Fn {}
    }
}
impl NvExtension374Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_374\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension374Fn {}
unsafe impl Send for NvExtension374Fn {}
unsafe impl Sync for NvExtension374Fn {}
impl ::std::clone::Clone for NvExtension374Fn {
    fn clone(&self) -> Self {
        NvExtension374Fn {}
    }
}
impl NvExtension374Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension374Fn {}
    }
}
#[doc = "Generated from 'VK_NV_extension_374'"]
impl ExternalFenceHandleTypeFlags {
    pub const RESERVED_4_NV: Self = Self(0b1_0000);
}
#[doc = "Generated from 'VK_NV_extension_374'"]
impl ExternalFenceHandleTypeFlags {
    pub const RESERVED_5_NV: Self = Self(0b10_0000);
}
impl NvExtension375Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_375\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension375Fn {}
unsafe impl Send for NvExtension375Fn {}
unsafe impl Sync for NvExtension375Fn {}
impl ::std::clone::Clone for NvExtension375Fn {
    fn clone(&self) -> Self {
        NvExtension375Fn {}
    }
}
impl NvExtension375Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension375Fn {}
    }
}
#[doc = "Generated from 'VK_NV_extension_375'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const RESERVED_5_NV: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_NV_extension_375'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const RESERVED_6_NV: Self = Self(0b100_0000);
}
impl ExtExtension376Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_376\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension376Fn {}
unsafe impl Send for ExtExtension376Fn {}
unsafe impl Sync for ExtExtension376Fn {}
impl ::std::clone::Clone for ExtExtension376Fn {
    fn clone(&self) -> Self {
        ExtExtension376Fn {}
    }
}
impl ExtExtension376Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension376Fn {}
    }
}
impl ExtExtension377Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_377\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension377Fn {}
unsafe impl Send for ExtExtension377Fn {}
unsafe impl Sync for ExtExtension377Fn {}
impl ::std::clone::Clone for ExtExtension377Fn {
    fn clone(&self) -> Self {
        ExtExtension377Fn {}
    }
}
impl ExtExtension377Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension377Fn {}
    }
}
impl NvExtension378Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_378\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct NvExtension378Fn {}
unsafe impl Send for NvExtension378Fn {}
unsafe impl Sync for NvExtension378Fn {}
impl ::std::clone::Clone for NvExtension378Fn {
    fn clone(&self) -> Self {
        NvExtension378Fn {}
    }
}
impl NvExtension378Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension378Fn {}
    }
}
impl QnxScreenSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QNX_screen_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
pub struct QnxScreenSurfaceFn {}
unsafe impl Send for QnxScreenSurfaceFn {}
unsafe impl Sync for QnxScreenSurfaceFn {}
impl ::std::clone::Clone for QnxScreenSurfaceFn {
    fn clone(&self) -> Self {
        QnxScreenSurfaceFn {}
    }
}
impl QnxScreenSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QnxScreenSurfaceFn {}
    }
}
#[doc = "Generated from 'VK_QNX_screen_surface'"]
impl StructureType {
    pub const SCREEN_SURFACE_CREATE_INFO_QNX: Self = Self(1_000_378_000);
}
impl KhrExtension380Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_380\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension380Fn {}
unsafe impl Send for KhrExtension380Fn {}
unsafe impl Sync for KhrExtension380Fn {}
impl ::std::clone::Clone for KhrExtension380Fn {
    fn clone(&self) -> Self {
        KhrExtension380Fn {}
    }
}
impl KhrExtension380Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension380Fn {}
    }
}
impl KhrExtension381Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_381\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension381Fn {}
unsafe impl Send for KhrExtension381Fn {}
unsafe impl Sync for KhrExtension381Fn {}
impl ::std::clone::Clone for KhrExtension381Fn {
    fn clone(&self) -> Self {
        KhrExtension381Fn {}
    }
}
impl KhrExtension381Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension381Fn {}
    }
}
impl ExtExtension382Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_382\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension382Fn {}
unsafe impl Send for ExtExtension382Fn {}
unsafe impl Sync for ExtExtension382Fn {}
impl ::std::clone::Clone for ExtExtension382Fn {
    fn clone(&self) -> Self {
        ExtExtension382Fn {}
    }
}
impl ExtExtension382Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension382Fn {}
    }
}
impl ExtExtension383Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_383\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension383Fn {}
unsafe impl Send for ExtExtension383Fn {}
unsafe impl Sync for ExtExtension383Fn {}
impl ::std::clone::Clone for ExtExtension383Fn {
    fn clone(&self) -> Self {
        ExtExtension383Fn {}
    }
}
impl ExtExtension383Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension383Fn {}
    }
}
impl ExtExtension384Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_384\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct ExtExtension384Fn {}
unsafe impl Send for ExtExtension384Fn {}
unsafe impl Sync for ExtExtension384Fn {}
impl ::std::clone::Clone for ExtExtension384Fn {
    fn clone(&self) -> Self {
        ExtExtension384Fn {}
    }
}
impl ExtExtension384Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension384Fn {}
    }
}
impl MesaExtension385Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_MESA_extension_385\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct MesaExtension385Fn {}
unsafe impl Send for MesaExtension385Fn {}
unsafe impl Sync for MesaExtension385Fn {}
impl ::std::clone::Clone for MesaExtension385Fn {
    fn clone(&self) -> Self {
        MesaExtension385Fn {}
    }
}
impl MesaExtension385Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        MesaExtension385Fn {}
    }
}
impl GoogleExtension386Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GOOGLE_extension_386\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct GoogleExtension386Fn {}
unsafe impl Send for GoogleExtension386Fn {}
unsafe impl Sync for GoogleExtension386Fn {}
impl ::std::clone::Clone for GoogleExtension386Fn {
    fn clone(&self) -> Self {
        GoogleExtension386Fn {}
    }
}
impl GoogleExtension386Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleExtension386Fn {}
    }
}
impl KhrExtension387Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_387\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
pub struct KhrExtension387Fn {}
unsafe impl Send for KhrExtension387Fn {}
unsafe impl Sync for KhrExtension387Fn {}
impl ::std::clone::Clone for KhrExtension387Fn {
    fn clone(&self) -> Self {
        KhrExtension387Fn {}
    }
}
impl KhrExtension387Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension387Fn {}
    }
}
