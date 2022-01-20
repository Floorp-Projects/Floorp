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
pub type PFN_vkDestroySurfaceKHR = unsafe extern "system" fn(
    instance: Instance,
    surface: SurfaceKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceSupportKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    surface: SurfaceKHR,
    p_supported: *mut Bool32,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_surface_capabilities: *mut SurfaceCapabilitiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceFormatsKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_surface_format_count: *mut u32,
    p_surface_formats: *mut SurfaceFormatKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfacePresentModesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_present_mode_count: *mut u32,
    p_present_modes: *mut PresentModeKHR,
) -> Result;
#[derive(Clone)]
pub struct KhrSurfaceFn {
    pub destroy_surface_khr: PFN_vkDestroySurfaceKHR,
    pub get_physical_device_surface_support_khr: PFN_vkGetPhysicalDeviceSurfaceSupportKHR,
    pub get_physical_device_surface_capabilities_khr: PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
    pub get_physical_device_surface_formats_khr: PFN_vkGetPhysicalDeviceSurfaceFormatsKHR,
    pub get_physical_device_surface_present_modes_khr:
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR,
}
unsafe impl Send for KhrSurfaceFn {}
unsafe impl Sync for KhrSurfaceFn {}
impl KhrSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSurfaceFn {
            destroy_surface_khr: unsafe {
                unsafe extern "system" fn destroy_surface_khr(
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
                unsafe extern "system" fn get_physical_device_surface_support_khr(
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
                unsafe extern "system" fn get_physical_device_surface_capabilities_khr(
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
                unsafe extern "system" fn get_physical_device_surface_formats_khr(
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
                unsafe extern "system" fn get_physical_device_surface_present_modes_khr(
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
pub type PFN_vkCreateSwapchainKHR = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const SwapchainCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_swapchain: *mut SwapchainKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroySwapchainKHR = unsafe extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainImagesKHR = unsafe extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_swapchain_image_count: *mut u32,
    p_swapchain_images: *mut Image,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireNextImageKHR = unsafe extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    timeout: u64,
    semaphore: Semaphore,
    fence: Fence,
    p_image_index: *mut u32,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueuePresentKHR =
    unsafe extern "system" fn(queue: Queue, p_present_info: *const PresentInfoKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupPresentCapabilitiesKHR = unsafe extern "system" fn(
    device: Device,
    p_device_group_present_capabilities: *mut DeviceGroupPresentCapabilitiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupSurfacePresentModesKHR = unsafe extern "system" fn(
    device: Device,
    surface: SurfaceKHR,
    p_modes: *mut DeviceGroupPresentModeFlagsKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDevicePresentRectanglesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_rect_count: *mut u32,
    p_rects: *mut Rect2D,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireNextImage2KHR = unsafe extern "system" fn(
    device: Device,
    p_acquire_info: *const AcquireNextImageInfoKHR,
    p_image_index: *mut u32,
) -> Result;
#[derive(Clone)]
pub struct KhrSwapchainFn {
    pub create_swapchain_khr: PFN_vkCreateSwapchainKHR,
    pub destroy_swapchain_khr: PFN_vkDestroySwapchainKHR,
    pub get_swapchain_images_khr: PFN_vkGetSwapchainImagesKHR,
    pub acquire_next_image_khr: PFN_vkAcquireNextImageKHR,
    pub queue_present_khr: PFN_vkQueuePresentKHR,
    pub get_device_group_present_capabilities_khr: PFN_vkGetDeviceGroupPresentCapabilitiesKHR,
    pub get_device_group_surface_present_modes_khr: PFN_vkGetDeviceGroupSurfacePresentModesKHR,
    pub get_physical_device_present_rectangles_khr: PFN_vkGetPhysicalDevicePresentRectanglesKHR,
    pub acquire_next_image2_khr: PFN_vkAcquireNextImage2KHR,
}
unsafe impl Send for KhrSwapchainFn {}
unsafe impl Sync for KhrSwapchainFn {}
impl KhrSwapchainFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSwapchainFn {
            create_swapchain_khr: unsafe {
                unsafe extern "system" fn create_swapchain_khr(
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
                unsafe extern "system" fn destroy_swapchain_khr(
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
                unsafe extern "system" fn get_swapchain_images_khr(
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
                unsafe extern "system" fn acquire_next_image_khr(
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
                unsafe extern "system" fn queue_present_khr(
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
                unsafe extern "system" fn get_device_group_present_capabilities_khr(
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
                unsafe extern "system" fn get_device_group_surface_present_modes_khr(
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
                unsafe extern "system" fn get_physical_device_present_rectangles_khr(
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
                unsafe extern "system" fn acquire_next_image2_khr(
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
pub type PFN_vkGetPhysicalDeviceDisplayPropertiesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayPropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayPlanePropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayPlaneSupportedDisplaysKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    plane_index: u32,
    p_display_count: *mut u32,
    p_displays: *mut DisplayKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayModePropertiesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    display: DisplayKHR,
    p_property_count: *mut u32,
    p_properties: *mut DisplayModePropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDisplayModeKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    display: DisplayKHR,
    p_create_info: *const DisplayModeCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_mode: *mut DisplayModeKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayPlaneCapabilitiesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    mode: DisplayModeKHR,
    plane_index: u32,
    p_capabilities: *mut DisplayPlaneCapabilitiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDisplayPlaneSurfaceKHR = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const DisplaySurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct KhrDisplayFn {
    pub get_physical_device_display_properties_khr: PFN_vkGetPhysicalDeviceDisplayPropertiesKHR,
    pub get_physical_device_display_plane_properties_khr:
        PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR,
    pub get_display_plane_supported_displays_khr: PFN_vkGetDisplayPlaneSupportedDisplaysKHR,
    pub get_display_mode_properties_khr: PFN_vkGetDisplayModePropertiesKHR,
    pub create_display_mode_khr: PFN_vkCreateDisplayModeKHR,
    pub get_display_plane_capabilities_khr: PFN_vkGetDisplayPlaneCapabilitiesKHR,
    pub create_display_plane_surface_khr: PFN_vkCreateDisplayPlaneSurfaceKHR,
}
unsafe impl Send for KhrDisplayFn {}
unsafe impl Sync for KhrDisplayFn {}
impl KhrDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDisplayFn {
            get_physical_device_display_properties_khr: unsafe {
                unsafe extern "system" fn get_physical_device_display_properties_khr(
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
                unsafe extern "system" fn get_physical_device_display_plane_properties_khr(
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
                unsafe extern "system" fn get_display_plane_supported_displays_khr(
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
                unsafe extern "system" fn get_display_mode_properties_khr(
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
                unsafe extern "system" fn create_display_mode_khr(
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
                unsafe extern "system" fn get_display_plane_capabilities_khr(
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
                unsafe extern "system" fn create_display_plane_surface_khr(
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
pub type PFN_vkCreateSharedSwapchainsKHR = unsafe extern "system" fn(
    device: Device,
    swapchain_count: u32,
    p_create_infos: *const SwapchainCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_swapchains: *mut SwapchainKHR,
) -> Result;
#[derive(Clone)]
pub struct KhrDisplaySwapchainFn {
    pub create_shared_swapchains_khr: PFN_vkCreateSharedSwapchainsKHR,
}
unsafe impl Send for KhrDisplaySwapchainFn {}
unsafe impl Sync for KhrDisplaySwapchainFn {}
impl KhrDisplaySwapchainFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDisplaySwapchainFn {
            create_shared_swapchains_khr: unsafe {
                unsafe extern "system" fn create_shared_swapchains_khr(
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
pub type PFN_vkCreateXlibSurfaceKHR = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const XlibSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    dpy: *mut Display,
    visual_id: VisualID,
) -> Bool32;
#[derive(Clone)]
pub struct KhrXlibSurfaceFn {
    pub create_xlib_surface_khr: PFN_vkCreateXlibSurfaceKHR,
    pub get_physical_device_xlib_presentation_support_khr:
        PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR,
}
unsafe impl Send for KhrXlibSurfaceFn {}
unsafe impl Sync for KhrXlibSurfaceFn {}
impl KhrXlibSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrXlibSurfaceFn {
            create_xlib_surface_khr: unsafe {
                unsafe extern "system" fn create_xlib_surface_khr(
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
                unsafe extern "system" fn get_physical_device_xlib_presentation_support_khr(
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
pub type PFN_vkCreateXcbSurfaceKHR = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const XcbSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    connection: *mut xcb_connection_t,
    visual_id: xcb_visualid_t,
) -> Bool32;
#[derive(Clone)]
pub struct KhrXcbSurfaceFn {
    pub create_xcb_surface_khr: PFN_vkCreateXcbSurfaceKHR,
    pub get_physical_device_xcb_presentation_support_khr:
        PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR,
}
unsafe impl Send for KhrXcbSurfaceFn {}
unsafe impl Sync for KhrXcbSurfaceFn {}
impl KhrXcbSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrXcbSurfaceFn {
            create_xcb_surface_khr: unsafe {
                unsafe extern "system" fn create_xcb_surface_khr(
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
                unsafe extern "system" fn get_physical_device_xcb_presentation_support_khr(
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
pub type PFN_vkCreateWaylandSurfaceKHR = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const WaylandSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    display: *mut wl_display,
)
    -> Bool32;
#[derive(Clone)]
pub struct KhrWaylandSurfaceFn {
    pub create_wayland_surface_khr: PFN_vkCreateWaylandSurfaceKHR,
    pub get_physical_device_wayland_presentation_support_khr:
        PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR,
}
unsafe impl Send for KhrWaylandSurfaceFn {}
unsafe impl Sync for KhrWaylandSurfaceFn {}
impl KhrWaylandSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrWaylandSurfaceFn {
            create_wayland_surface_khr: unsafe {
                unsafe extern "system" fn create_wayland_surface_khr(
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
                unsafe extern "system" fn get_physical_device_wayland_presentation_support_khr(
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
#[derive(Clone)]
pub struct KhrMirSurfaceFn {}
unsafe impl Send for KhrMirSurfaceFn {}
unsafe impl Sync for KhrMirSurfaceFn {}
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
pub type PFN_vkCreateAndroidSurfaceKHR = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const AndroidSurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct KhrAndroidSurfaceFn {
    pub create_android_surface_khr: PFN_vkCreateAndroidSurfaceKHR,
}
unsafe impl Send for KhrAndroidSurfaceFn {}
unsafe impl Sync for KhrAndroidSurfaceFn {}
impl KhrAndroidSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrAndroidSurfaceFn {
            create_android_surface_khr: unsafe {
                unsafe extern "system" fn create_android_surface_khr(
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
pub type PFN_vkCreateWin32SurfaceKHR = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const Win32SurfaceCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR =
    unsafe extern "system" fn(physical_device: PhysicalDevice, queue_family_index: u32) -> Bool32;
#[derive(Clone)]
pub struct KhrWin32SurfaceFn {
    pub create_win32_surface_khr: PFN_vkCreateWin32SurfaceKHR,
    pub get_physical_device_win32_presentation_support_khr:
        PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR,
}
unsafe impl Send for KhrWin32SurfaceFn {}
unsafe impl Sync for KhrWin32SurfaceFn {}
impl KhrWin32SurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrWin32SurfaceFn {
            create_win32_surface_khr: unsafe {
                unsafe extern "system" fn create_win32_surface_khr(
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
                unsafe extern "system" fn get_physical_device_win32_presentation_support_khr(
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
pub type PFN_vkGetSwapchainGrallocUsageANDROID = unsafe extern "system" fn(
    device: Device,
    format: Format,
    image_usage: ImageUsageFlags,
    gralloc_usage: *mut c_int,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireImageANDROID = unsafe extern "system" fn(
    device: Device,
    image: Image,
    native_fence_fd: c_int,
    semaphore: Semaphore,
    fence: Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueueSignalReleaseImageANDROID = unsafe extern "system" fn(
    queue: Queue,
    wait_semaphore_count: u32,
    p_wait_semaphores: *const Semaphore,
    image: Image,
    p_native_fence_fd: *mut c_int,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainGrallocUsage2ANDROID = unsafe extern "system" fn(
    device: Device,
    format: Format,
    image_usage: ImageUsageFlags,
    swapchain_image_usage: SwapchainImageUsageFlagsANDROID,
    gralloc_consumer_usage: *mut u64,
    gralloc_producer_usage: *mut u64,
) -> Result;
#[derive(Clone)]
pub struct AndroidNativeBufferFn {
    pub get_swapchain_gralloc_usage_android: PFN_vkGetSwapchainGrallocUsageANDROID,
    pub acquire_image_android: PFN_vkAcquireImageANDROID,
    pub queue_signal_release_image_android: PFN_vkQueueSignalReleaseImageANDROID,
    pub get_swapchain_gralloc_usage2_android: PFN_vkGetSwapchainGrallocUsage2ANDROID,
}
unsafe impl Send for AndroidNativeBufferFn {}
unsafe impl Sync for AndroidNativeBufferFn {}
impl AndroidNativeBufferFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AndroidNativeBufferFn {
            get_swapchain_gralloc_usage_android: unsafe {
                unsafe extern "system" fn get_swapchain_gralloc_usage_android(
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
                unsafe extern "system" fn acquire_image_android(
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
                unsafe extern "system" fn queue_signal_release_image_android(
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
                unsafe extern "system" fn get_swapchain_gralloc_usage2_android(
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
    pub const SPEC_VERSION: u32 = 10u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDebugReportCallbackEXT = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const DebugReportCallbackCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_callback: *mut DebugReportCallbackEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDebugReportCallbackEXT = unsafe extern "system" fn(
    instance: Instance,
    callback: DebugReportCallbackEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkDebugReportMessageEXT = unsafe extern "system" fn(
    instance: Instance,
    flags: DebugReportFlagsEXT,
    object_type: DebugReportObjectTypeEXT,
    object: u64,
    location: usize,
    message_code: i32,
    p_layer_prefix: *const c_char,
    p_message: *const c_char,
);
#[derive(Clone)]
pub struct ExtDebugReportFn {
    pub create_debug_report_callback_ext: PFN_vkCreateDebugReportCallbackEXT,
    pub destroy_debug_report_callback_ext: PFN_vkDestroyDebugReportCallbackEXT,
    pub debug_report_message_ext: PFN_vkDebugReportMessageEXT,
}
unsafe impl Send for ExtDebugReportFn {}
unsafe impl Sync for ExtDebugReportFn {}
impl ExtDebugReportFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDebugReportFn {
            create_debug_report_callback_ext: unsafe {
                unsafe extern "system" fn create_debug_report_callback_ext(
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
                unsafe extern "system" fn destroy_debug_report_callback_ext(
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
                unsafe extern "system" fn debug_report_message_ext(
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
    pub const DEBUG_REPORT_CREATE_INFO_EXT: Self = Self::DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
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
#[derive(Clone)]
pub struct NvGlslShaderFn {}
unsafe impl Send for NvGlslShaderFn {}
unsafe impl Sync for NvGlslShaderFn {}
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
#[derive(Clone)]
pub struct ExtDepthRangeUnrestrictedFn {}
unsafe impl Send for ExtDepthRangeUnrestrictedFn {}
unsafe impl Sync for ExtDepthRangeUnrestrictedFn {}
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
#[derive(Clone)]
pub struct KhrSamplerMirrorClampToEdgeFn {}
unsafe impl Send for KhrSamplerMirrorClampToEdgeFn {}
unsafe impl Sync for KhrSamplerMirrorClampToEdgeFn {}
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
    pub const MIRROR_CLAMP_TO_EDGE_KHR: Self = Self::MIRROR_CLAMP_TO_EDGE;
}
impl ImgFilterCubicFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_filter_cubic\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ImgFilterCubicFn {}
unsafe impl Send for ImgFilterCubicFn {}
unsafe impl Sync for ImgFilterCubicFn {}
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
#[derive(Clone)]
pub struct AmdExtension17Fn {}
unsafe impl Send for AmdExtension17Fn {}
unsafe impl Sync for AmdExtension17Fn {}
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
#[derive(Clone)]
pub struct AmdExtension18Fn {}
unsafe impl Send for AmdExtension18Fn {}
unsafe impl Sync for AmdExtension18Fn {}
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
#[derive(Clone)]
pub struct AmdRasterizationOrderFn {}
unsafe impl Send for AmdRasterizationOrderFn {}
unsafe impl Sync for AmdRasterizationOrderFn {}
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
#[derive(Clone)]
pub struct AmdExtension20Fn {}
unsafe impl Send for AmdExtension20Fn {}
unsafe impl Sync for AmdExtension20Fn {}
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
#[derive(Clone)]
pub struct AmdShaderTrinaryMinmaxFn {}
unsafe impl Send for AmdShaderTrinaryMinmaxFn {}
unsafe impl Sync for AmdShaderTrinaryMinmaxFn {}
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
#[derive(Clone)]
pub struct AmdShaderExplicitVertexParameterFn {}
unsafe impl Send for AmdShaderExplicitVertexParameterFn {}
unsafe impl Sync for AmdShaderExplicitVertexParameterFn {}
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
pub type PFN_vkDebugMarkerSetObjectTagEXT = unsafe extern "system" fn(
    device: Device,
    p_tag_info: *const DebugMarkerObjectTagInfoEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDebugMarkerSetObjectNameEXT = unsafe extern "system" fn(
    device: Device,
    p_name_info: *const DebugMarkerObjectNameInfoEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDebugMarkerBeginEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const DebugMarkerMarkerInfoEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDebugMarkerEndEXT = unsafe extern "system" fn(command_buffer: CommandBuffer);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDebugMarkerInsertEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const DebugMarkerMarkerInfoEXT,
);
#[derive(Clone)]
pub struct ExtDebugMarkerFn {
    pub debug_marker_set_object_tag_ext: PFN_vkDebugMarkerSetObjectTagEXT,
    pub debug_marker_set_object_name_ext: PFN_vkDebugMarkerSetObjectNameEXT,
    pub cmd_debug_marker_begin_ext: PFN_vkCmdDebugMarkerBeginEXT,
    pub cmd_debug_marker_end_ext: PFN_vkCmdDebugMarkerEndEXT,
    pub cmd_debug_marker_insert_ext: PFN_vkCmdDebugMarkerInsertEXT,
}
unsafe impl Send for ExtDebugMarkerFn {}
unsafe impl Sync for ExtDebugMarkerFn {}
impl ExtDebugMarkerFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDebugMarkerFn {
            debug_marker_set_object_tag_ext: unsafe {
                unsafe extern "system" fn debug_marker_set_object_tag_ext(
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
                unsafe extern "system" fn debug_marker_set_object_name_ext(
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
                unsafe extern "system" fn cmd_debug_marker_begin_ext(
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
                unsafe extern "system" fn cmd_debug_marker_end_ext(_command_buffer: CommandBuffer) {
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
                unsafe extern "system" fn cmd_debug_marker_insert_ext(
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
impl KhrVideoQueueFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_video_queue\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_video_profile: *const VideoProfileKHR,
    p_capabilities: *mut VideoCapabilitiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_video_format_info: *const PhysicalDeviceVideoFormatInfoKHR,
    p_video_format_property_count: *mut u32,
    p_video_format_properties: *mut VideoFormatPropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateVideoSessionKHR = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const VideoSessionCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_video_session: *mut VideoSessionKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyVideoSessionKHR = unsafe extern "system" fn(
    device: Device,
    video_session: VideoSessionKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetVideoSessionMemoryRequirementsKHR = unsafe extern "system" fn(
    device: Device,
    video_session: VideoSessionKHR,
    p_video_session_memory_requirements_count: *mut u32,
    p_video_session_memory_requirements: *mut VideoGetMemoryPropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkBindVideoSessionMemoryKHR = unsafe extern "system" fn(
    device: Device,
    video_session: VideoSessionKHR,
    video_session_bind_memory_count: u32,
    p_video_session_bind_memories: *const VideoBindMemoryKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateVideoSessionParametersKHR = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const VideoSessionParametersCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_video_session_parameters: *mut VideoSessionParametersKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkUpdateVideoSessionParametersKHR = unsafe extern "system" fn(
    device: Device,
    video_session_parameters: VideoSessionParametersKHR,
    p_update_info: *const VideoSessionParametersUpdateInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyVideoSessionParametersKHR = unsafe extern "system" fn(
    device: Device,
    video_session_parameters: VideoSessionParametersKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginVideoCodingKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_begin_info: *const VideoBeginCodingInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndVideoCodingKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_end_coding_info: *const VideoEndCodingInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdControlVideoCodingKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_coding_control_info: *const VideoCodingControlInfoKHR,
);
#[derive(Clone)]
pub struct KhrVideoQueueFn {
    pub get_physical_device_video_capabilities_khr: PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR,
    pub get_physical_device_video_format_properties_khr:
        PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR,
    pub create_video_session_khr: PFN_vkCreateVideoSessionKHR,
    pub destroy_video_session_khr: PFN_vkDestroyVideoSessionKHR,
    pub get_video_session_memory_requirements_khr: PFN_vkGetVideoSessionMemoryRequirementsKHR,
    pub bind_video_session_memory_khr: PFN_vkBindVideoSessionMemoryKHR,
    pub create_video_session_parameters_khr: PFN_vkCreateVideoSessionParametersKHR,
    pub update_video_session_parameters_khr: PFN_vkUpdateVideoSessionParametersKHR,
    pub destroy_video_session_parameters_khr: PFN_vkDestroyVideoSessionParametersKHR,
    pub cmd_begin_video_coding_khr: PFN_vkCmdBeginVideoCodingKHR,
    pub cmd_end_video_coding_khr: PFN_vkCmdEndVideoCodingKHR,
    pub cmd_control_video_coding_khr: PFN_vkCmdControlVideoCodingKHR,
}
unsafe impl Send for KhrVideoQueueFn {}
unsafe impl Sync for KhrVideoQueueFn {}
impl KhrVideoQueueFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrVideoQueueFn {
            get_physical_device_video_capabilities_khr: unsafe {
                unsafe extern "system" fn get_physical_device_video_capabilities_khr(
                    _physical_device: PhysicalDevice,
                    _p_video_profile: *const VideoProfileKHR,
                    _p_capabilities: *mut VideoCapabilitiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_video_capabilities_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceVideoCapabilitiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_video_capabilities_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_video_format_properties_khr: unsafe {
                unsafe extern "system" fn get_physical_device_video_format_properties_khr(
                    _physical_device: PhysicalDevice,
                    _p_video_format_info: *const PhysicalDeviceVideoFormatInfoKHR,
                    _p_video_format_property_count: *mut u32,
                    _p_video_format_properties: *mut VideoFormatPropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_video_format_properties_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceVideoFormatPropertiesKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_video_format_properties_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_video_session_khr: unsafe {
                unsafe extern "system" fn create_video_session_khr(
                    _device: Device,
                    _p_create_info: *const VideoSessionCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_video_session: *mut VideoSessionKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_video_session_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateVideoSessionKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    create_video_session_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_video_session_khr: unsafe {
                unsafe extern "system" fn destroy_video_session_khr(
                    _device: Device,
                    _video_session: VideoSessionKHR,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_video_session_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyVideoSessionKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_video_session_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_video_session_memory_requirements_khr: unsafe {
                unsafe extern "system" fn get_video_session_memory_requirements_khr(
                    _device: Device,
                    _video_session: VideoSessionKHR,
                    _p_video_session_memory_requirements_count: *mut u32,
                    _p_video_session_memory_requirements: *mut VideoGetMemoryPropertiesKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_video_session_memory_requirements_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetVideoSessionMemoryRequirementsKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_video_session_memory_requirements_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            bind_video_session_memory_khr: unsafe {
                unsafe extern "system" fn bind_video_session_memory_khr(
                    _device: Device,
                    _video_session: VideoSessionKHR,
                    _video_session_bind_memory_count: u32,
                    _p_video_session_bind_memories: *const VideoBindMemoryKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(bind_video_session_memory_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkBindVideoSessionMemoryKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    bind_video_session_memory_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_video_session_parameters_khr: unsafe {
                unsafe extern "system" fn create_video_session_parameters_khr(
                    _device: Device,
                    _p_create_info: *const VideoSessionParametersCreateInfoKHR,
                    _p_allocator: *const AllocationCallbacks,
                    _p_video_session_parameters: *mut VideoSessionParametersKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_video_session_parameters_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateVideoSessionParametersKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_video_session_parameters_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            update_video_session_parameters_khr: unsafe {
                unsafe extern "system" fn update_video_session_parameters_khr(
                    _device: Device,
                    _video_session_parameters: VideoSessionParametersKHR,
                    _p_update_info: *const VideoSessionParametersUpdateInfoKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(update_video_session_parameters_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkUpdateVideoSessionParametersKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    update_video_session_parameters_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_video_session_parameters_khr: unsafe {
                unsafe extern "system" fn destroy_video_session_parameters_khr(
                    _device: Device,
                    _video_session_parameters: VideoSessionParametersKHR,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_video_session_parameters_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyVideoSessionParametersKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_video_session_parameters_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_video_coding_khr: unsafe {
                unsafe extern "system" fn cmd_begin_video_coding_khr(
                    _command_buffer: CommandBuffer,
                    _p_begin_info: *const VideoBeginCodingInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_video_coding_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBeginVideoCodingKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_video_coding_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_video_coding_khr: unsafe {
                unsafe extern "system" fn cmd_end_video_coding_khr(
                    _command_buffer: CommandBuffer,
                    _p_end_coding_info: *const VideoEndCodingInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_end_video_coding_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdEndVideoCodingKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_video_coding_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_control_video_coding_khr: unsafe {
                unsafe extern "system" fn cmd_control_video_coding_khr(
                    _command_buffer: CommandBuffer,
                    _p_coding_control_info: *const VideoCodingControlInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_control_video_coding_khr)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdControlVideoCodingKHR\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_control_video_coding_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceVideoCapabilitiesKHR.html>"]
    pub unsafe fn get_physical_device_video_capabilities_khr(
        &self,
        physical_device: PhysicalDevice,
        p_video_profile: *const VideoProfileKHR,
        p_capabilities: *mut VideoCapabilitiesKHR,
    ) -> Result {
        (self.get_physical_device_video_capabilities_khr)(
            physical_device,
            p_video_profile,
            p_capabilities,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceVideoFormatPropertiesKHR.html>"]
    pub unsafe fn get_physical_device_video_format_properties_khr(
        &self,
        physical_device: PhysicalDevice,
        p_video_format_info: *const PhysicalDeviceVideoFormatInfoKHR,
        p_video_format_property_count: *mut u32,
        p_video_format_properties: *mut VideoFormatPropertiesKHR,
    ) -> Result {
        (self.get_physical_device_video_format_properties_khr)(
            physical_device,
            p_video_format_info,
            p_video_format_property_count,
            p_video_format_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateVideoSessionKHR.html>"]
    pub unsafe fn create_video_session_khr(
        &self,
        device: Device,
        p_create_info: *const VideoSessionCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_video_session: *mut VideoSessionKHR,
    ) -> Result {
        (self.create_video_session_khr)(device, p_create_info, p_allocator, p_video_session)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyVideoSessionKHR.html>"]
    pub unsafe fn destroy_video_session_khr(
        &self,
        device: Device,
        video_session: VideoSessionKHR,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_video_session_khr)(device, video_session, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetVideoSessionMemoryRequirementsKHR.html>"]
    pub unsafe fn get_video_session_memory_requirements_khr(
        &self,
        device: Device,
        video_session: VideoSessionKHR,
        p_video_session_memory_requirements_count: *mut u32,
        p_video_session_memory_requirements: *mut VideoGetMemoryPropertiesKHR,
    ) -> Result {
        (self.get_video_session_memory_requirements_khr)(
            device,
            video_session,
            p_video_session_memory_requirements_count,
            p_video_session_memory_requirements,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindVideoSessionMemoryKHR.html>"]
    pub unsafe fn bind_video_session_memory_khr(
        &self,
        device: Device,
        video_session: VideoSessionKHR,
        video_session_bind_memory_count: u32,
        p_video_session_bind_memories: *const VideoBindMemoryKHR,
    ) -> Result {
        (self.bind_video_session_memory_khr)(
            device,
            video_session,
            video_session_bind_memory_count,
            p_video_session_bind_memories,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateVideoSessionParametersKHR.html>"]
    pub unsafe fn create_video_session_parameters_khr(
        &self,
        device: Device,
        p_create_info: *const VideoSessionParametersCreateInfoKHR,
        p_allocator: *const AllocationCallbacks,
        p_video_session_parameters: *mut VideoSessionParametersKHR,
    ) -> Result {
        (self.create_video_session_parameters_khr)(
            device,
            p_create_info,
            p_allocator,
            p_video_session_parameters,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUpdateVideoSessionParametersKHR.html>"]
    pub unsafe fn update_video_session_parameters_khr(
        &self,
        device: Device,
        video_session_parameters: VideoSessionParametersKHR,
        p_update_info: *const VideoSessionParametersUpdateInfoKHR,
    ) -> Result {
        (self.update_video_session_parameters_khr)(device, video_session_parameters, p_update_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyVideoSessionParametersKHR.html>"]
    pub unsafe fn destroy_video_session_parameters_khr(
        &self,
        device: Device,
        video_session_parameters: VideoSessionParametersKHR,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_video_session_parameters_khr)(device, video_session_parameters, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginVideoCodingKHR.html>"]
    pub unsafe fn cmd_begin_video_coding_khr(
        &self,
        command_buffer: CommandBuffer,
        p_begin_info: *const VideoBeginCodingInfoKHR,
    ) {
        (self.cmd_begin_video_coding_khr)(command_buffer, p_begin_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndVideoCodingKHR.html>"]
    pub unsafe fn cmd_end_video_coding_khr(
        &self,
        command_buffer: CommandBuffer,
        p_end_coding_info: *const VideoEndCodingInfoKHR,
    ) {
        (self.cmd_end_video_coding_khr)(command_buffer, p_end_coding_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdControlVideoCodingKHR.html>"]
    pub unsafe fn cmd_control_video_coding_khr(
        &self,
        command_buffer: CommandBuffer,
        p_coding_control_info: *const VideoCodingControlInfoKHR,
    ) {
        (self.cmd_control_video_coding_khr)(command_buffer, p_coding_control_info)
    }
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_PROFILE_KHR: Self = Self(1_000_023_000);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_CAPABILITIES_KHR: Self = Self(1_000_023_001);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_PICTURE_RESOURCE_KHR: Self = Self(1_000_023_002);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_GET_MEMORY_PROPERTIES_KHR: Self = Self(1_000_023_003);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_BIND_MEMORY_KHR: Self = Self(1_000_023_004);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_SESSION_CREATE_INFO_KHR: Self = Self(1_000_023_005);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR: Self = Self(1_000_023_006);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_SESSION_PARAMETERS_UPDATE_INFO_KHR: Self = Self(1_000_023_007);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_BEGIN_CODING_INFO_KHR: Self = Self(1_000_023_008);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_END_CODING_INFO_KHR: Self = Self(1_000_023_009);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_CODING_CONTROL_INFO_KHR: Self = Self(1_000_023_010);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_REFERENCE_SLOT_KHR: Self = Self(1_000_023_011);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_QUEUE_FAMILY_PROPERTIES_2_KHR: Self = Self(1_000_023_012);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_PROFILES_KHR: Self = Self(1_000_023_013);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR: Self = Self(1_000_023_014);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl StructureType {
    pub const VIDEO_FORMAT_PROPERTIES_KHR: Self = Self(1_000_023_015);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl ObjectType {
    pub const VIDEO_SESSION_KHR: Self = Self(1_000_023_000);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl ObjectType {
    pub const VIDEO_SESSION_PARAMETERS_KHR: Self = Self(1_000_023_001);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl QueryType {
    pub const RESULT_STATUS_ONLY_KHR: Self = Self(1_000_023_000);
}
#[doc = "Generated from 'VK_KHR_video_queue'"]
impl QueryResultFlags {
    pub const WITH_STATUS_KHR: Self = Self(0b1_0000);
}
impl KhrVideoDecodeQueueFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_video_decode_queue\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDecodeVideoKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_frame_info: *const VideoDecodeInfoKHR,
);
#[derive(Clone)]
pub struct KhrVideoDecodeQueueFn {
    pub cmd_decode_video_khr: PFN_vkCmdDecodeVideoKHR,
}
unsafe impl Send for KhrVideoDecodeQueueFn {}
unsafe impl Sync for KhrVideoDecodeQueueFn {}
impl KhrVideoDecodeQueueFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrVideoDecodeQueueFn {
            cmd_decode_video_khr: unsafe {
                unsafe extern "system" fn cmd_decode_video_khr(
                    _command_buffer: CommandBuffer,
                    _p_frame_info: *const VideoDecodeInfoKHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_decode_video_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDecodeVideoKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_decode_video_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDecodeVideoKHR.html>"]
    pub unsafe fn cmd_decode_video_khr(
        &self,
        command_buffer: CommandBuffer,
        p_frame_info: *const VideoDecodeInfoKHR,
    ) {
        (self.cmd_decode_video_khr)(command_buffer, p_frame_info)
    }
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl StructureType {
    pub const VIDEO_DECODE_INFO_KHR: Self = Self(1_000_024_000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl QueueFlags {
    pub const VIDEO_DECODE_KHR: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl PipelineStageFlags2KHR {
    pub const VIDEO_DECODE: Self = Self(0b100_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl AccessFlags2KHR {
    pub const VIDEO_DECODE_READ: Self = Self(0b1000_0000_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl AccessFlags2KHR {
    pub const VIDEO_DECODE_WRITE: Self = Self(0b1_0000_0000_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl BufferUsageFlags {
    pub const VIDEO_DECODE_SRC_KHR: Self = Self(0b10_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl BufferUsageFlags {
    pub const VIDEO_DECODE_DST_KHR: Self = Self(0b100_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl ImageUsageFlags {
    pub const VIDEO_DECODE_DST_KHR: Self = Self(0b100_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl ImageUsageFlags {
    pub const VIDEO_DECODE_SRC_KHR: Self = Self(0b1000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl ImageUsageFlags {
    pub const VIDEO_DECODE_DPB_KHR: Self = Self(0b1_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl FormatFeatureFlags {
    pub const VIDEO_DECODE_OUTPUT_KHR: Self = Self(0b10_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl FormatFeatureFlags {
    pub const VIDEO_DECODE_DPB_KHR: Self = Self(0b100_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl ImageLayout {
    pub const VIDEO_DECODE_DST_KHR: Self = Self(1_000_024_000);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl ImageLayout {
    pub const VIDEO_DECODE_SRC_KHR: Self = Self(1_000_024_001);
}
#[doc = "Generated from 'VK_KHR_video_decode_queue'"]
impl ImageLayout {
    pub const VIDEO_DECODE_DPB_KHR: Self = Self(1_000_024_002);
}
impl AmdGcnShaderFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_gcn_shader\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct AmdGcnShaderFn {}
unsafe impl Send for AmdGcnShaderFn {}
unsafe impl Sync for AmdGcnShaderFn {}
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
#[derive(Clone)]
pub struct NvDedicatedAllocationFn {}
unsafe impl Send for NvDedicatedAllocationFn {}
unsafe impl Sync for NvDedicatedAllocationFn {}
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
#[derive(Clone)]
pub struct ExtExtension28Fn {}
unsafe impl Send for ExtExtension28Fn {}
unsafe impl Sync for ExtExtension28Fn {}
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
pub type PFN_vkCmdBindTransformFeedbackBuffersEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_binding: u32,
    binding_count: u32,
    p_buffers: *const Buffer,
    p_offsets: *const DeviceSize,
    p_sizes: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginTransformFeedbackEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_counter_buffer: u32,
    counter_buffer_count: u32,
    p_counter_buffers: *const Buffer,
    p_counter_buffer_offsets: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndTransformFeedbackEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_counter_buffer: u32,
    counter_buffer_count: u32,
    p_counter_buffers: *const Buffer,
    p_counter_buffer_offsets: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginQueryIndexedEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    query_pool: QueryPool,
    query: u32,
    flags: QueryControlFlags,
    index: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndQueryIndexedEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    query_pool: QueryPool,
    query: u32,
    index: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndirectByteCountEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    instance_count: u32,
    first_instance: u32,
    counter_buffer: Buffer,
    counter_buffer_offset: DeviceSize,
    counter_offset: u32,
    vertex_stride: u32,
);
#[derive(Clone)]
pub struct ExtTransformFeedbackFn {
    pub cmd_bind_transform_feedback_buffers_ext: PFN_vkCmdBindTransformFeedbackBuffersEXT,
    pub cmd_begin_transform_feedback_ext: PFN_vkCmdBeginTransformFeedbackEXT,
    pub cmd_end_transform_feedback_ext: PFN_vkCmdEndTransformFeedbackEXT,
    pub cmd_begin_query_indexed_ext: PFN_vkCmdBeginQueryIndexedEXT,
    pub cmd_end_query_indexed_ext: PFN_vkCmdEndQueryIndexedEXT,
    pub cmd_draw_indirect_byte_count_ext: PFN_vkCmdDrawIndirectByteCountEXT,
}
unsafe impl Send for ExtTransformFeedbackFn {}
unsafe impl Sync for ExtTransformFeedbackFn {}
impl ExtTransformFeedbackFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtTransformFeedbackFn {
            cmd_bind_transform_feedback_buffers_ext: unsafe {
                unsafe extern "system" fn cmd_bind_transform_feedback_buffers_ext(
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
                unsafe extern "system" fn cmd_begin_transform_feedback_ext(
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
                unsafe extern "system" fn cmd_end_transform_feedback_ext(
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
                unsafe extern "system" fn cmd_begin_query_indexed_ext(
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
                unsafe extern "system" fn cmd_end_query_indexed_ext(
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
                unsafe extern "system" fn cmd_draw_indirect_byte_count_ext(
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
impl NvxBinaryImportFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NVX_binary_import\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateCuModuleNVX = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const CuModuleCreateInfoNVX,
    p_allocator: *const AllocationCallbacks,
    p_module: *mut CuModuleNVX,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateCuFunctionNVX = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const CuFunctionCreateInfoNVX,
    p_allocator: *const AllocationCallbacks,
    p_function: *mut CuFunctionNVX,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyCuModuleNVX = unsafe extern "system" fn(
    device: Device,
    module: CuModuleNVX,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyCuFunctionNVX = unsafe extern "system" fn(
    device: Device,
    function: CuFunctionNVX,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCuLaunchKernelNVX =
    unsafe extern "system" fn(command_buffer: CommandBuffer, p_launch_info: *const CuLaunchInfoNVX);
#[derive(Clone)]
pub struct NvxBinaryImportFn {
    pub create_cu_module_nvx: PFN_vkCreateCuModuleNVX,
    pub create_cu_function_nvx: PFN_vkCreateCuFunctionNVX,
    pub destroy_cu_module_nvx: PFN_vkDestroyCuModuleNVX,
    pub destroy_cu_function_nvx: PFN_vkDestroyCuFunctionNVX,
    pub cmd_cu_launch_kernel_nvx: PFN_vkCmdCuLaunchKernelNVX,
}
unsafe impl Send for NvxBinaryImportFn {}
unsafe impl Sync for NvxBinaryImportFn {}
impl NvxBinaryImportFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvxBinaryImportFn {
            create_cu_module_nvx: unsafe {
                unsafe extern "system" fn create_cu_module_nvx(
                    _device: Device,
                    _p_create_info: *const CuModuleCreateInfoNVX,
                    _p_allocator: *const AllocationCallbacks,
                    _p_module: *mut CuModuleNVX,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_cu_module_nvx)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateCuModuleNVX\0");
                let val = _f(cname);
                if val.is_null() {
                    create_cu_module_nvx
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_cu_function_nvx: unsafe {
                unsafe extern "system" fn create_cu_function_nvx(
                    _device: Device,
                    _p_create_info: *const CuFunctionCreateInfoNVX,
                    _p_allocator: *const AllocationCallbacks,
                    _p_function: *mut CuFunctionNVX,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_cu_function_nvx)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateCuFunctionNVX\0");
                let val = _f(cname);
                if val.is_null() {
                    create_cu_function_nvx
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_cu_module_nvx: unsafe {
                unsafe extern "system" fn destroy_cu_module_nvx(
                    _device: Device,
                    _module: CuModuleNVX,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_cu_module_nvx)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyCuModuleNVX\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_cu_module_nvx
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_cu_function_nvx: unsafe {
                unsafe extern "system" fn destroy_cu_function_nvx(
                    _device: Device,
                    _function: CuFunctionNVX,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_cu_function_nvx)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyCuFunctionNVX\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_cu_function_nvx
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_cu_launch_kernel_nvx: unsafe {
                unsafe extern "system" fn cmd_cu_launch_kernel_nvx(
                    _command_buffer: CommandBuffer,
                    _p_launch_info: *const CuLaunchInfoNVX,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_cu_launch_kernel_nvx)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCuLaunchKernelNVX\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_cu_launch_kernel_nvx
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateCuModuleNVX.html>"]
    pub unsafe fn create_cu_module_nvx(
        &self,
        device: Device,
        p_create_info: *const CuModuleCreateInfoNVX,
        p_allocator: *const AllocationCallbacks,
        p_module: *mut CuModuleNVX,
    ) -> Result {
        (self.create_cu_module_nvx)(device, p_create_info, p_allocator, p_module)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateCuFunctionNVX.html>"]
    pub unsafe fn create_cu_function_nvx(
        &self,
        device: Device,
        p_create_info: *const CuFunctionCreateInfoNVX,
        p_allocator: *const AllocationCallbacks,
        p_function: *mut CuFunctionNVX,
    ) -> Result {
        (self.create_cu_function_nvx)(device, p_create_info, p_allocator, p_function)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyCuModuleNVX.html>"]
    pub unsafe fn destroy_cu_module_nvx(
        &self,
        device: Device,
        module: CuModuleNVX,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_cu_module_nvx)(device, module, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyCuFunctionNVX.html>"]
    pub unsafe fn destroy_cu_function_nvx(
        &self,
        device: Device,
        function: CuFunctionNVX,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_cu_function_nvx)(device, function, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCuLaunchKernelNVX.html>"]
    pub unsafe fn cmd_cu_launch_kernel_nvx(
        &self,
        command_buffer: CommandBuffer,
        p_launch_info: *const CuLaunchInfoNVX,
    ) {
        (self.cmd_cu_launch_kernel_nvx)(command_buffer, p_launch_info)
    }
}
#[doc = "Generated from 'VK_NVX_binary_import'"]
impl StructureType {
    pub const CU_MODULE_CREATE_INFO_NVX: Self = Self(1_000_029_000);
}
#[doc = "Generated from 'VK_NVX_binary_import'"]
impl StructureType {
    pub const CU_FUNCTION_CREATE_INFO_NVX: Self = Self(1_000_029_001);
}
#[doc = "Generated from 'VK_NVX_binary_import'"]
impl StructureType {
    pub const CU_LAUNCH_INFO_NVX: Self = Self(1_000_029_002);
}
#[doc = "Generated from 'VK_NVX_binary_import'"]
impl ObjectType {
    pub const CU_MODULE_NVX: Self = Self(1_000_029_000);
}
#[doc = "Generated from 'VK_NVX_binary_import'"]
impl ObjectType {
    pub const CU_FUNCTION_NVX: Self = Self(1_000_029_001);
}
#[doc = "Generated from 'VK_NVX_binary_import'"]
impl DebugReportObjectTypeEXT {
    pub const CU_MODULE_NVX: Self = Self(1_000_029_000);
}
#[doc = "Generated from 'VK_NVX_binary_import'"]
impl DebugReportObjectTypeEXT {
    pub const CU_FUNCTION_NVX: Self = Self(1_000_029_001);
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
    unsafe extern "system" fn(device: Device, p_info: *const ImageViewHandleInfoNVX) -> u32;
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageViewAddressNVX = unsafe extern "system" fn(
    device: Device,
    image_view: ImageView,
    p_properties: *mut ImageViewAddressPropertiesNVX,
) -> Result;
#[derive(Clone)]
pub struct NvxImageViewHandleFn {
    pub get_image_view_handle_nvx: PFN_vkGetImageViewHandleNVX,
    pub get_image_view_address_nvx: PFN_vkGetImageViewAddressNVX,
}
unsafe impl Send for NvxImageViewHandleFn {}
unsafe impl Sync for NvxImageViewHandleFn {}
impl NvxImageViewHandleFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvxImageViewHandleFn {
            get_image_view_handle_nvx: unsafe {
                unsafe extern "system" fn get_image_view_handle_nvx(
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
                unsafe extern "system" fn get_image_view_address_nvx(
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
#[derive(Clone)]
pub struct AmdExtension32Fn {}
unsafe impl Send for AmdExtension32Fn {}
unsafe impl Sync for AmdExtension32Fn {}
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
#[derive(Clone)]
pub struct AmdExtension33Fn {}
unsafe impl Send for AmdExtension33Fn {}
unsafe impl Sync for AmdExtension33Fn {}
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
pub type PFN_vkCmdDrawIndirectCount = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    count_buffer: Buffer,
    count_buffer_offset: DeviceSize,
    max_draw_count: u32,
    stride: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndexedIndirectCount = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    count_buffer: Buffer,
    count_buffer_offset: DeviceSize,
    max_draw_count: u32,
    stride: u32,
);
#[derive(Clone)]
pub struct AmdDrawIndirectCountFn {
    pub cmd_draw_indirect_count_amd: PFN_vkCmdDrawIndirectCount,
    pub cmd_draw_indexed_indirect_count_amd: PFN_vkCmdDrawIndexedIndirectCount,
}
unsafe impl Send for AmdDrawIndirectCountFn {}
unsafe impl Sync for AmdDrawIndirectCountFn {}
impl AmdDrawIndirectCountFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdDrawIndirectCountFn {
            cmd_draw_indirect_count_amd: unsafe {
                unsafe extern "system" fn cmd_draw_indirect_count_amd(
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
                unsafe extern "system" fn cmd_draw_indexed_indirect_count_amd(
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
#[derive(Clone)]
pub struct AmdExtension35Fn {}
unsafe impl Send for AmdExtension35Fn {}
unsafe impl Sync for AmdExtension35Fn {}
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
#[derive(Clone)]
pub struct AmdNegativeViewportHeightFn {}
unsafe impl Send for AmdNegativeViewportHeightFn {}
unsafe impl Sync for AmdNegativeViewportHeightFn {}
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
#[derive(Clone)]
pub struct AmdGpuShaderHalfFloatFn {}
unsafe impl Send for AmdGpuShaderHalfFloatFn {}
unsafe impl Sync for AmdGpuShaderHalfFloatFn {}
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
#[derive(Clone)]
pub struct AmdShaderBallotFn {}
unsafe impl Send for AmdShaderBallotFn {}
unsafe impl Sync for AmdShaderBallotFn {}
impl AmdShaderBallotFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderBallotFn {}
    }
}
impl ExtVideoEncodeH264Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_video_encode_h264\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[derive(Clone)]
pub struct ExtVideoEncodeH264Fn {}
unsafe impl Send for ExtVideoEncodeH264Fn {}
unsafe impl Sync for ExtVideoEncodeH264Fn {}
impl ExtVideoEncodeH264Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVideoEncodeH264Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_CAPABILITIES_EXT: Self = Self(1_000_038_000);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_SESSION_CREATE_INFO_EXT: Self = Self(1_000_038_001);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_EXT: Self = Self(1_000_038_002);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_EXT: Self = Self(1_000_038_003);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_VCL_FRAME_INFO_EXT: Self = Self(1_000_038_004);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_DPB_SLOT_INFO_EXT: Self = Self(1_000_038_005);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_NALU_SLICE_EXT: Self = Self(1_000_038_006);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_EMIT_PICTURE_PARAMETERS_EXT: Self = Self(1_000_038_007);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl StructureType {
    pub const VIDEO_ENCODE_H264_PROFILE_EXT: Self = Self(1_000_038_008);
}
#[doc = "Generated from 'VK_EXT_video_encode_h264'"]
impl VideoCodecOperationFlagsKHR {
    pub const ENCODE_H264_EXT: Self = Self(0b1_0000_0000_0000_0000);
}
impl ExtVideoEncodeH265Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_video_encode_h265\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtVideoEncodeH265Fn {}
unsafe impl Send for ExtVideoEncodeH265Fn {}
unsafe impl Sync for ExtVideoEncodeH265Fn {}
impl ExtVideoEncodeH265Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVideoEncodeH265Fn {}
    }
}
impl ExtVideoDecodeH264Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_video_decode_h264\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[derive(Clone)]
pub struct ExtVideoDecodeH264Fn {}
unsafe impl Send for ExtVideoDecodeH264Fn {}
unsafe impl Sync for ExtVideoDecodeH264Fn {}
impl ExtVideoDecodeH264Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVideoDecodeH264Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_CAPABILITIES_EXT: Self = Self(1_000_040_000);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_SESSION_CREATE_INFO_EXT: Self = Self(1_000_040_001);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_PICTURE_INFO_EXT: Self = Self(1_000_040_002);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_MVC_EXT: Self = Self(1_000_040_003);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_PROFILE_EXT: Self = Self(1_000_040_004);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_EXT: Self = Self(1_000_040_005);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_EXT: Self = Self(1_000_040_006);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl StructureType {
    pub const VIDEO_DECODE_H264_DPB_SLOT_INFO_EXT: Self = Self(1_000_040_007);
}
#[doc = "Generated from 'VK_EXT_video_decode_h264'"]
impl VideoCodecOperationFlagsKHR {
    pub const DECODE_H264_EXT: Self = Self(0b1);
}
impl AmdTextureGatherBiasLodFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_texture_gather_bias_lod\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct AmdTextureGatherBiasLodFn {}
unsafe impl Send for AmdTextureGatherBiasLodFn {}
unsafe impl Sync for AmdTextureGatherBiasLodFn {}
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
pub type PFN_vkGetShaderInfoAMD = unsafe extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    shader_stage: ShaderStageFlags,
    info_type: ShaderInfoTypeAMD,
    p_info_size: *mut usize,
    p_info: *mut c_void,
) -> Result;
#[derive(Clone)]
pub struct AmdShaderInfoFn {
    pub get_shader_info_amd: PFN_vkGetShaderInfoAMD,
}
unsafe impl Send for AmdShaderInfoFn {}
unsafe impl Sync for AmdShaderInfoFn {}
impl AmdShaderInfoFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdShaderInfoFn {
            get_shader_info_amd: unsafe {
                unsafe extern "system" fn get_shader_info_amd(
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
#[derive(Clone)]
pub struct AmdExtension44Fn {}
unsafe impl Send for AmdExtension44Fn {}
unsafe impl Sync for AmdExtension44Fn {}
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
#[derive(Clone)]
pub struct AmdExtension45Fn {}
unsafe impl Send for AmdExtension45Fn {}
unsafe impl Sync for AmdExtension45Fn {}
impl AmdExtension45Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension45Fn {}
    }
}
#[doc = "Generated from 'VK_AMD_extension_45'"]
impl PipelineCreateFlags {
    pub const RESERVED_21_AMD: Self = Self(0b10_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_45'"]
impl PipelineCreateFlags {
    pub const RESERVED_22_AMD: Self = Self(0b100_0000_0000_0000_0000_0000);
}
impl AmdExtension46Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_46\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct AmdExtension46Fn {}
unsafe impl Send for AmdExtension46Fn {}
unsafe impl Sync for AmdExtension46Fn {}
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
#[derive(Clone)]
pub struct AmdShaderImageLoadStoreLodFn {}
unsafe impl Send for AmdShaderImageLoadStoreLodFn {}
unsafe impl Sync for AmdShaderImageLoadStoreLodFn {}
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
#[derive(Clone)]
pub struct NvxExtension48Fn {}
unsafe impl Send for NvxExtension48Fn {}
unsafe impl Sync for NvxExtension48Fn {}
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
#[derive(Clone)]
pub struct GoogleExtension49Fn {}
unsafe impl Send for GoogleExtension49Fn {}
unsafe impl Sync for GoogleExtension49Fn {}
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
pub type PFN_vkCreateStreamDescriptorSurfaceGGP = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const StreamDescriptorSurfaceCreateInfoGGP,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct GgpStreamDescriptorSurfaceFn {
    pub create_stream_descriptor_surface_ggp: PFN_vkCreateStreamDescriptorSurfaceGGP,
}
unsafe impl Send for GgpStreamDescriptorSurfaceFn {}
unsafe impl Sync for GgpStreamDescriptorSurfaceFn {}
impl GgpStreamDescriptorSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpStreamDescriptorSurfaceFn {
            create_stream_descriptor_surface_ggp: unsafe {
                unsafe extern "system" fn create_stream_descriptor_surface_ggp(
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
#[derive(Clone)]
pub struct NvCornerSampledImageFn {}
unsafe impl Send for NvCornerSampledImageFn {}
unsafe impl Sync for NvCornerSampledImageFn {}
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
#[derive(Clone)]
pub struct NvExtension52Fn {}
unsafe impl Send for NvExtension52Fn {}
unsafe impl Sync for NvExtension52Fn {}
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
#[derive(Clone)]
pub struct NvExtension53Fn {}
unsafe impl Send for NvExtension53Fn {}
unsafe impl Sync for NvExtension53Fn {}
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
#[derive(Clone)]
pub struct KhrMultiviewFn {}
unsafe impl Send for KhrMultiviewFn {}
unsafe impl Sync for KhrMultiviewFn {}
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
    pub const RENDER_PASS_MULTIVIEW_CREATE_INFO_KHR: Self = Self::RENDER_PASS_MULTIVIEW_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_multiview'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR: Self =
        Self::PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
}
#[doc = "Generated from 'VK_KHR_multiview'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR: Self =
        Self::PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_multiview'"]
impl DependencyFlags {
    pub const VIEW_LOCAL_KHR: Self = Self::VIEW_LOCAL;
}
impl ImgFormatPvrtcFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_format_pvrtc\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ImgFormatPvrtcFn {}
unsafe impl Send for ImgFormatPvrtcFn {}
unsafe impl Sync for ImgFormatPvrtcFn {}
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
pub type PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV =
    unsafe extern "system" fn(
        physical_device: PhysicalDevice,
        format: Format,
        ty: ImageType,
        tiling: ImageTiling,
        usage: ImageUsageFlags,
        flags: ImageCreateFlags,
        external_handle_type: ExternalMemoryHandleTypeFlagsNV,
        p_external_image_format_properties: *mut ExternalImageFormatPropertiesNV,
    ) -> Result;
#[derive(Clone)]
pub struct NvExternalMemoryCapabilitiesFn {
    pub get_physical_device_external_image_format_properties_nv:
        PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV,
}
unsafe impl Send for NvExternalMemoryCapabilitiesFn {}
unsafe impl Sync for NvExternalMemoryCapabilitiesFn {}
impl NvExternalMemoryCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExternalMemoryCapabilitiesFn {
            get_physical_device_external_image_format_properties_nv: unsafe {
                unsafe extern "system" fn get_physical_device_external_image_format_properties_nv(
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
#[derive(Clone)]
pub struct NvExternalMemoryFn {}
unsafe impl Send for NvExternalMemoryFn {}
unsafe impl Sync for NvExternalMemoryFn {}
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
pub type PFN_vkGetMemoryWin32HandleNV = unsafe extern "system" fn(
    device: Device,
    memory: DeviceMemory,
    handle_type: ExternalMemoryHandleTypeFlagsNV,
    p_handle: *mut HANDLE,
) -> Result;
#[derive(Clone)]
pub struct NvExternalMemoryWin32Fn {
    pub get_memory_win32_handle_nv: PFN_vkGetMemoryWin32HandleNV,
}
unsafe impl Send for NvExternalMemoryWin32Fn {}
unsafe impl Sync for NvExternalMemoryWin32Fn {}
impl NvExternalMemoryWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExternalMemoryWin32Fn {
            get_memory_win32_handle_nv: unsafe {
                unsafe extern "system" fn get_memory_win32_handle_nv(
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
#[derive(Clone)]
pub struct NvWin32KeyedMutexFn {}
unsafe impl Send for NvWin32KeyedMutexFn {}
unsafe impl Sync for NvWin32KeyedMutexFn {}
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
pub type PFN_vkGetPhysicalDeviceFeatures2 = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_features: *mut PhysicalDeviceFeatures2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceProperties2 = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_properties: *mut PhysicalDeviceProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceFormatProperties2 = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    format: Format,
    p_format_properties: *mut FormatProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceImageFormatProperties2 = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_image_format_info: *const PhysicalDeviceImageFormatInfo2,
    p_image_format_properties: *mut ImageFormatProperties2,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceQueueFamilyProperties2 = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_queue_family_property_count: *mut u32,
    p_queue_family_properties: *mut QueueFamilyProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceMemoryProperties2 = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_memory_properties: *mut PhysicalDeviceMemoryProperties2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_format_info: *const PhysicalDeviceSparseImageFormatInfo2,
    p_property_count: *mut u32,
    p_properties: *mut SparseImageFormatProperties2,
);
#[derive(Clone)]
pub struct KhrGetPhysicalDeviceProperties2Fn {
    pub get_physical_device_features2_khr: PFN_vkGetPhysicalDeviceFeatures2,
    pub get_physical_device_properties2_khr: PFN_vkGetPhysicalDeviceProperties2,
    pub get_physical_device_format_properties2_khr: PFN_vkGetPhysicalDeviceFormatProperties2,
    pub get_physical_device_image_format_properties2_khr:
        PFN_vkGetPhysicalDeviceImageFormatProperties2,
    pub get_physical_device_queue_family_properties2_khr:
        PFN_vkGetPhysicalDeviceQueueFamilyProperties2,
    pub get_physical_device_memory_properties2_khr: PFN_vkGetPhysicalDeviceMemoryProperties2,
    pub get_physical_device_sparse_image_format_properties2_khr:
        PFN_vkGetPhysicalDeviceSparseImageFormatProperties2,
}
unsafe impl Send for KhrGetPhysicalDeviceProperties2Fn {}
unsafe impl Sync for KhrGetPhysicalDeviceProperties2Fn {}
impl KhrGetPhysicalDeviceProperties2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetPhysicalDeviceProperties2Fn {
            get_physical_device_features2_khr: unsafe {
                unsafe extern "system" fn get_physical_device_features2_khr(
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
                unsafe extern "system" fn get_physical_device_properties2_khr(
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
                unsafe extern "system" fn get_physical_device_format_properties2_khr(
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
                unsafe extern "system" fn get_physical_device_image_format_properties2_khr(
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
                unsafe extern "system" fn get_physical_device_queue_family_properties2_khr(
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
                unsafe extern "system" fn get_physical_device_memory_properties2_khr(
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
                unsafe extern "system" fn get_physical_device_sparse_image_format_properties2_khr(
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
    pub const PHYSICAL_DEVICE_FEATURES_2_KHR: Self = Self::PHYSICAL_DEVICE_FEATURES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PROPERTIES_2_KHR: Self = Self::PHYSICAL_DEVICE_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const FORMAT_PROPERTIES_2_KHR: Self = Self::FORMAT_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const IMAGE_FORMAT_PROPERTIES_2_KHR: Self = Self::IMAGE_FORMAT_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR: Self =
        Self::PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const QUEUE_FAMILY_PROPERTIES_2_KHR: Self = Self::QUEUE_FAMILY_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR: Self =
        Self::PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const SPARSE_IMAGE_FORMAT_PROPERTIES_2_KHR: Self = Self::SPARSE_IMAGE_FORMAT_PROPERTIES_2;
}
#[doc = "Generated from 'VK_KHR_get_physical_device_properties2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2_KHR: Self =
        Self::PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2;
}
impl KhrDeviceGroupFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_device_group\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupPeerMemoryFeatures = unsafe extern "system" fn(
    device: Device,
    heap_index: u32,
    local_device_index: u32,
    remote_device_index: u32,
    p_peer_memory_features: *mut PeerMemoryFeatureFlags,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDeviceMask =
    unsafe extern "system" fn(command_buffer: CommandBuffer, device_mask: u32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDispatchBase = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    base_group_x: u32,
    base_group_y: u32,
    base_group_z: u32,
    group_count_x: u32,
    group_count_y: u32,
    group_count_z: u32,
);
#[derive(Clone)]
pub struct KhrDeviceGroupFn {
    pub get_device_group_peer_memory_features_khr: PFN_vkGetDeviceGroupPeerMemoryFeatures,
    pub cmd_set_device_mask_khr: PFN_vkCmdSetDeviceMask,
    pub cmd_dispatch_base_khr: PFN_vkCmdDispatchBase,
    pub get_device_group_present_capabilities_khr:
        crate::vk::PFN_vkGetDeviceGroupPresentCapabilitiesKHR,
    pub get_device_group_surface_present_modes_khr:
        crate::vk::PFN_vkGetDeviceGroupSurfacePresentModesKHR,
    pub get_physical_device_present_rectangles_khr:
        crate::vk::PFN_vkGetPhysicalDevicePresentRectanglesKHR,
    pub acquire_next_image2_khr: crate::vk::PFN_vkAcquireNextImage2KHR,
}
unsafe impl Send for KhrDeviceGroupFn {}
unsafe impl Sync for KhrDeviceGroupFn {}
impl KhrDeviceGroupFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDeviceGroupFn {
            get_device_group_peer_memory_features_khr: unsafe {
                unsafe extern "system" fn get_device_group_peer_memory_features_khr(
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
                unsafe extern "system" fn cmd_set_device_mask_khr(
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
                unsafe extern "system" fn cmd_dispatch_base_khr(
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
                unsafe extern "system" fn get_device_group_present_capabilities_khr(
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
                unsafe extern "system" fn get_device_group_surface_present_modes_khr(
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
                unsafe extern "system" fn get_physical_device_present_rectangles_khr(
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
                unsafe extern "system" fn acquire_next_image2_khr(
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
    pub const MEMORY_ALLOCATE_FLAGS_INFO_KHR: Self = Self::MEMORY_ALLOCATE_FLAGS_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_RENDER_PASS_BEGIN_INFO_KHR: Self =
        Self::DEVICE_GROUP_RENDER_PASS_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHR: Self =
        Self::DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_SUBMIT_INFO_KHR: Self = Self::DEVICE_GROUP_SUBMIT_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const DEVICE_GROUP_BIND_SPARSE_INFO_KHR: Self = Self::DEVICE_GROUP_BIND_SPARSE_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const COPY_SRC_KHR: Self = Self::COPY_SRC;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const COPY_DST_KHR: Self = Self::COPY_DST;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const GENERIC_SRC_KHR: Self = Self::GENERIC_SRC;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PeerMemoryFeatureFlags {
    pub const GENERIC_DST_KHR: Self = Self::GENERIC_DST;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl MemoryAllocateFlags {
    pub const DEVICE_MASK_KHR: Self = Self::DEVICE_MASK;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl PipelineCreateFlags {
    pub const VIEW_INDEX_FROM_DEVICE_INDEX_KHR: Self = Self::VIEW_INDEX_FROM_DEVICE_INDEX;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl DependencyFlags {
    pub const DEVICE_GROUP_KHR: Self = Self::DEVICE_GROUP;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO_KHR: Self =
        Self::BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl StructureType {
    pub const BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHR: Self =
        Self::BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group'"]
impl ImageCreateFlags {
    pub const SPLIT_INSTANCE_BIND_REGIONS_KHR: Self = Self::SPLIT_INSTANCE_BIND_REGIONS;
}
impl ExtValidationFlagsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_validation_flags\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[derive(Clone)]
pub struct ExtValidationFlagsFn {}
unsafe impl Send for ExtValidationFlagsFn {}
unsafe impl Sync for ExtValidationFlagsFn {}
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
pub type PFN_vkCreateViSurfaceNN = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const ViSurfaceCreateInfoNN,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct NnViSurfaceFn {
    pub create_vi_surface_nn: PFN_vkCreateViSurfaceNN,
}
unsafe impl Send for NnViSurfaceFn {}
unsafe impl Sync for NnViSurfaceFn {}
impl NnViSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NnViSurfaceFn {
            create_vi_surface_nn: unsafe {
                unsafe extern "system" fn create_vi_surface_nn(
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
#[derive(Clone)]
pub struct KhrShaderDrawParametersFn {}
unsafe impl Send for KhrShaderDrawParametersFn {}
unsafe impl Sync for KhrShaderDrawParametersFn {}
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
#[derive(Clone)]
pub struct ExtShaderSubgroupBallotFn {}
unsafe impl Send for ExtShaderSubgroupBallotFn {}
unsafe impl Sync for ExtShaderSubgroupBallotFn {}
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
#[derive(Clone)]
pub struct ExtShaderSubgroupVoteFn {}
unsafe impl Send for ExtShaderSubgroupVoteFn {}
unsafe impl Sync for ExtShaderSubgroupVoteFn {}
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
#[derive(Clone)]
pub struct ExtTextureCompressionAstcHdrFn {}
unsafe impl Send for ExtTextureCompressionAstcHdrFn {}
unsafe impl Sync for ExtTextureCompressionAstcHdrFn {}
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
#[derive(Clone)]
pub struct ExtAstcDecodeModeFn {}
unsafe impl Send for ExtAstcDecodeModeFn {}
unsafe impl Sync for ExtAstcDecodeModeFn {}
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
#[derive(Clone)]
pub struct ImgExtension69Fn {}
unsafe impl Send for ImgExtension69Fn {}
unsafe impl Sync for ImgExtension69Fn {}
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
pub type PFN_vkTrimCommandPool = unsafe extern "system" fn(
    device: Device,
    command_pool: CommandPool,
    flags: CommandPoolTrimFlags,
);
#[derive(Clone)]
pub struct KhrMaintenance1Fn {
    pub trim_command_pool_khr: PFN_vkTrimCommandPool,
}
unsafe impl Send for KhrMaintenance1Fn {}
unsafe impl Sync for KhrMaintenance1Fn {}
impl KhrMaintenance1Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrMaintenance1Fn {
            trim_command_pool_khr: unsafe {
                unsafe extern "system" fn trim_command_pool_khr(
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
    pub const ERROR_OUT_OF_POOL_MEMORY_KHR: Self = Self::ERROR_OUT_OF_POOL_MEMORY;
}
#[doc = "Generated from 'VK_KHR_maintenance1'"]
impl FormatFeatureFlags {
    pub const TRANSFER_SRC_KHR: Self = Self::TRANSFER_SRC;
}
#[doc = "Generated from 'VK_KHR_maintenance1'"]
impl FormatFeatureFlags {
    pub const TRANSFER_DST_KHR: Self = Self::TRANSFER_DST;
}
#[doc = "Generated from 'VK_KHR_maintenance1'"]
impl ImageCreateFlags {
    pub const TYPE_2D_ARRAY_COMPATIBLE_KHR: Self = Self::TYPE_2D_ARRAY_COMPATIBLE;
}
impl KhrDeviceGroupCreationFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_device_group_creation\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkEnumeratePhysicalDeviceGroups = unsafe extern "system" fn(
    instance: Instance,
    p_physical_device_group_count: *mut u32,
    p_physical_device_group_properties: *mut PhysicalDeviceGroupProperties,
) -> Result;
#[derive(Clone)]
pub struct KhrDeviceGroupCreationFn {
    pub enumerate_physical_device_groups_khr: PFN_vkEnumeratePhysicalDeviceGroups,
}
unsafe impl Send for KhrDeviceGroupCreationFn {}
unsafe impl Sync for KhrDeviceGroupCreationFn {}
impl KhrDeviceGroupCreationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDeviceGroupCreationFn {
            enumerate_physical_device_groups_khr: unsafe {
                unsafe extern "system" fn enumerate_physical_device_groups_khr(
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
    pub const PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR: Self = Self::PHYSICAL_DEVICE_GROUP_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_device_group_creation'"]
impl StructureType {
    pub const DEVICE_GROUP_DEVICE_CREATE_INFO_KHR: Self = Self::DEVICE_GROUP_DEVICE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_device_group_creation'"]
impl MemoryHeapFlags {
    pub const MULTI_INSTANCE_KHR: Self = Self::MULTI_INSTANCE;
}
impl KhrExternalMemoryCapabilitiesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_memory_capabilities\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceExternalBufferProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_external_buffer_info: *const PhysicalDeviceExternalBufferInfo,
    p_external_buffer_properties: *mut ExternalBufferProperties,
);
#[derive(Clone)]
pub struct KhrExternalMemoryCapabilitiesFn {
    pub get_physical_device_external_buffer_properties_khr:
        PFN_vkGetPhysicalDeviceExternalBufferProperties,
}
unsafe impl Send for KhrExternalMemoryCapabilitiesFn {}
unsafe impl Sync for KhrExternalMemoryCapabilitiesFn {}
impl KhrExternalMemoryCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalMemoryCapabilitiesFn {
            get_physical_device_external_buffer_properties_khr: unsafe {
                unsafe extern "system" fn get_physical_device_external_buffer_properties_khr(
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
        Self::PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR: Self = Self::EXTERNAL_IMAGE_FORMAT_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO_KHR: Self =
        Self::PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const EXTERNAL_BUFFER_PROPERTIES_KHR: Self = Self::EXTERNAL_BUFFER_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_ID_PROPERTIES_KHR: Self = Self::PHYSICAL_DEVICE_ID_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const OPAQUE_FD_KHR: Self = Self::OPAQUE_FD;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const OPAQUE_WIN32_KHR: Self = Self::OPAQUE_WIN32;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const OPAQUE_WIN32_KMT_KHR: Self = Self::OPAQUE_WIN32_KMT;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D11_TEXTURE_KHR: Self = Self::D3D11_TEXTURE;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D11_TEXTURE_KMT_KHR: Self = Self::D3D11_TEXTURE_KMT;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D12_HEAP_KHR: Self = Self::D3D12_HEAP;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryHandleTypeFlags {
    pub const D3D12_RESOURCE_KHR: Self = Self::D3D12_RESOURCE;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryFeatureFlags {
    pub const DEDICATED_ONLY_KHR: Self = Self::DEDICATED_ONLY;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryFeatureFlags {
    pub const EXPORTABLE_KHR: Self = Self::EXPORTABLE;
}
#[doc = "Generated from 'VK_KHR_external_memory_capabilities'"]
impl ExternalMemoryFeatureFlags {
    pub const IMPORTABLE_KHR: Self = Self::IMPORTABLE;
}
impl KhrExternalMemoryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_memory\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrExternalMemoryFn {}
unsafe impl Send for KhrExternalMemoryFn {}
unsafe impl Sync for KhrExternalMemoryFn {}
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
        Self::EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory'"]
impl StructureType {
    pub const EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR: Self = Self::EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory'"]
impl StructureType {
    pub const EXPORT_MEMORY_ALLOCATE_INFO_KHR: Self = Self::EXPORT_MEMORY_ALLOCATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_memory'"]
impl Result {
    pub const ERROR_INVALID_EXTERNAL_HANDLE_KHR: Self = Self::ERROR_INVALID_EXTERNAL_HANDLE;
}
impl KhrExternalMemoryWin32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_memory_win32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryWin32HandleKHR = unsafe extern "system" fn(
    device: Device,
    p_get_win32_handle_info: *const MemoryGetWin32HandleInfoKHR,
    p_handle: *mut HANDLE,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryWin32HandlePropertiesKHR = unsafe extern "system" fn(
    device: Device,
    handle_type: ExternalMemoryHandleTypeFlags,
    handle: HANDLE,
    p_memory_win32_handle_properties: *mut MemoryWin32HandlePropertiesKHR,
) -> Result;
#[derive(Clone)]
pub struct KhrExternalMemoryWin32Fn {
    pub get_memory_win32_handle_khr: PFN_vkGetMemoryWin32HandleKHR,
    pub get_memory_win32_handle_properties_khr: PFN_vkGetMemoryWin32HandlePropertiesKHR,
}
unsafe impl Send for KhrExternalMemoryWin32Fn {}
unsafe impl Sync for KhrExternalMemoryWin32Fn {}
impl KhrExternalMemoryWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalMemoryWin32Fn {
            get_memory_win32_handle_khr: unsafe {
                unsafe extern "system" fn get_memory_win32_handle_khr(
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
                unsafe extern "system" fn get_memory_win32_handle_properties_khr(
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
pub type PFN_vkGetMemoryFdKHR = unsafe extern "system" fn(
    device: Device,
    p_get_fd_info: *const MemoryGetFdInfoKHR,
    p_fd: *mut c_int,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryFdPropertiesKHR = unsafe extern "system" fn(
    device: Device,
    handle_type: ExternalMemoryHandleTypeFlags,
    fd: c_int,
    p_memory_fd_properties: *mut MemoryFdPropertiesKHR,
) -> Result;
#[derive(Clone)]
pub struct KhrExternalMemoryFdFn {
    pub get_memory_fd_khr: PFN_vkGetMemoryFdKHR,
    pub get_memory_fd_properties_khr: PFN_vkGetMemoryFdPropertiesKHR,
}
unsafe impl Send for KhrExternalMemoryFdFn {}
unsafe impl Sync for KhrExternalMemoryFdFn {}
impl KhrExternalMemoryFdFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalMemoryFdFn {
            get_memory_fd_khr: unsafe {
                unsafe extern "system" fn get_memory_fd_khr(
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
                unsafe extern "system" fn get_memory_fd_properties_khr(
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
#[derive(Clone)]
pub struct KhrWin32KeyedMutexFn {}
unsafe impl Send for KhrWin32KeyedMutexFn {}
unsafe impl Sync for KhrWin32KeyedMutexFn {}
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
pub type PFN_vkGetPhysicalDeviceExternalSemaphoreProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_external_semaphore_info: *const PhysicalDeviceExternalSemaphoreInfo,
    p_external_semaphore_properties: *mut ExternalSemaphoreProperties,
);
#[derive(Clone)]
pub struct KhrExternalSemaphoreCapabilitiesFn {
    pub get_physical_device_external_semaphore_properties_khr:
        PFN_vkGetPhysicalDeviceExternalSemaphoreProperties,
}
unsafe impl Send for KhrExternalSemaphoreCapabilitiesFn {}
unsafe impl Sync for KhrExternalSemaphoreCapabilitiesFn {}
impl KhrExternalSemaphoreCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalSemaphoreCapabilitiesFn {
            get_physical_device_external_semaphore_properties_khr: unsafe {
                unsafe extern "system" fn get_physical_device_external_semaphore_properties_khr(
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
        Self::PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl StructureType {
    pub const EXTERNAL_SEMAPHORE_PROPERTIES_KHR: Self = Self::EXTERNAL_SEMAPHORE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const OPAQUE_FD_KHR: Self = Self::OPAQUE_FD;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const OPAQUE_WIN32_KHR: Self = Self::OPAQUE_WIN32;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const OPAQUE_WIN32_KMT_KHR: Self = Self::OPAQUE_WIN32_KMT;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const D3D12_FENCE_KHR: Self = Self::D3D12_FENCE;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const SYNC_FD_KHR: Self = Self::SYNC_FD;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreFeatureFlags {
    pub const EXPORTABLE_KHR: Self = Self::EXPORTABLE;
}
#[doc = "Generated from 'VK_KHR_external_semaphore_capabilities'"]
impl ExternalSemaphoreFeatureFlags {
    pub const IMPORTABLE_KHR: Self = Self::IMPORTABLE;
}
impl KhrExternalSemaphoreFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_semaphore\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrExternalSemaphoreFn {}
unsafe impl Send for KhrExternalSemaphoreFn {}
unsafe impl Sync for KhrExternalSemaphoreFn {}
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
    pub const EXPORT_SEMAPHORE_CREATE_INFO_KHR: Self = Self::EXPORT_SEMAPHORE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_semaphore'"]
impl SemaphoreImportFlags {
    pub const TEMPORARY_KHR: Self = Self::TEMPORARY;
}
impl KhrExternalSemaphoreWin32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_semaphore_win32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkImportSemaphoreWin32HandleKHR = unsafe extern "system" fn(
    device: Device,
    p_import_semaphore_win32_handle_info: *const ImportSemaphoreWin32HandleInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSemaphoreWin32HandleKHR = unsafe extern "system" fn(
    device: Device,
    p_get_win32_handle_info: *const SemaphoreGetWin32HandleInfoKHR,
    p_handle: *mut HANDLE,
) -> Result;
#[derive(Clone)]
pub struct KhrExternalSemaphoreWin32Fn {
    pub import_semaphore_win32_handle_khr: PFN_vkImportSemaphoreWin32HandleKHR,
    pub get_semaphore_win32_handle_khr: PFN_vkGetSemaphoreWin32HandleKHR,
}
unsafe impl Send for KhrExternalSemaphoreWin32Fn {}
unsafe impl Sync for KhrExternalSemaphoreWin32Fn {}
impl KhrExternalSemaphoreWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalSemaphoreWin32Fn {
            import_semaphore_win32_handle_khr: unsafe {
                unsafe extern "system" fn import_semaphore_win32_handle_khr(
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
                unsafe extern "system" fn get_semaphore_win32_handle_khr(
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
pub type PFN_vkImportSemaphoreFdKHR = unsafe extern "system" fn(
    device: Device,
    p_import_semaphore_fd_info: *const ImportSemaphoreFdInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSemaphoreFdKHR = unsafe extern "system" fn(
    device: Device,
    p_get_fd_info: *const SemaphoreGetFdInfoKHR,
    p_fd: *mut c_int,
) -> Result;
#[derive(Clone)]
pub struct KhrExternalSemaphoreFdFn {
    pub import_semaphore_fd_khr: PFN_vkImportSemaphoreFdKHR,
    pub get_semaphore_fd_khr: PFN_vkGetSemaphoreFdKHR,
}
unsafe impl Send for KhrExternalSemaphoreFdFn {}
unsafe impl Sync for KhrExternalSemaphoreFdFn {}
impl KhrExternalSemaphoreFdFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalSemaphoreFdFn {
            import_semaphore_fd_khr: unsafe {
                unsafe extern "system" fn import_semaphore_fd_khr(
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
                unsafe extern "system" fn get_semaphore_fd_khr(
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
pub type PFN_vkCmdPushDescriptorSetKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_bind_point: PipelineBindPoint,
    layout: PipelineLayout,
    set: u32,
    descriptor_write_count: u32,
    p_descriptor_writes: *const WriteDescriptorSet,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPushDescriptorSetWithTemplateKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    descriptor_update_template: DescriptorUpdateTemplate,
    layout: PipelineLayout,
    set: u32,
    p_data: *const c_void,
);
#[derive(Clone)]
pub struct KhrPushDescriptorFn {
    pub cmd_push_descriptor_set_khr: PFN_vkCmdPushDescriptorSetKHR,
    pub cmd_push_descriptor_set_with_template_khr: PFN_vkCmdPushDescriptorSetWithTemplateKHR,
}
unsafe impl Send for KhrPushDescriptorFn {}
unsafe impl Sync for KhrPushDescriptorFn {}
impl KhrPushDescriptorFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPushDescriptorFn {
            cmd_push_descriptor_set_khr: unsafe {
                unsafe extern "system" fn cmd_push_descriptor_set_khr(
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
                unsafe extern "system" fn cmd_push_descriptor_set_with_template_khr(
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
pub type PFN_vkCmdBeginConditionalRenderingEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_conditional_rendering_begin: *const ConditionalRenderingBeginInfoEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndConditionalRenderingEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer);
#[derive(Clone)]
pub struct ExtConditionalRenderingFn {
    pub cmd_begin_conditional_rendering_ext: PFN_vkCmdBeginConditionalRenderingEXT,
    pub cmd_end_conditional_rendering_ext: PFN_vkCmdEndConditionalRenderingEXT,
}
unsafe impl Send for ExtConditionalRenderingFn {}
unsafe impl Sync for ExtConditionalRenderingFn {}
impl ExtConditionalRenderingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtConditionalRenderingFn {
            cmd_begin_conditional_rendering_ext: unsafe {
                unsafe extern "system" fn cmd_begin_conditional_rendering_ext(
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
                unsafe extern "system" fn cmd_end_conditional_rendering_ext(
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
#[derive(Clone)]
pub struct KhrShaderFloat16Int8Fn {}
unsafe impl Send for KhrShaderFloat16Int8Fn {}
unsafe impl Sync for KhrShaderFloat16Int8Fn {}
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
        Self::PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
}
#[doc = "Generated from 'VK_KHR_shader_float16_int8'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR: Self =
        Self::PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
}
impl Khr16bitStorageFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_16bit_storage\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct Khr16bitStorageFn {}
unsafe impl Send for Khr16bitStorageFn {}
unsafe impl Sync for Khr16bitStorageFn {}
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
        Self::PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
}
impl KhrIncrementalPresentFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_incremental_present\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[derive(Clone)]
pub struct KhrIncrementalPresentFn {}
unsafe impl Send for KhrIncrementalPresentFn {}
unsafe impl Sync for KhrIncrementalPresentFn {}
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
pub type PFN_vkCreateDescriptorUpdateTemplate = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const DescriptorUpdateTemplateCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_descriptor_update_template: *mut DescriptorUpdateTemplate,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDescriptorUpdateTemplate = unsafe extern "system" fn(
    device: Device,
    descriptor_update_template: DescriptorUpdateTemplate,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkUpdateDescriptorSetWithTemplate = unsafe extern "system" fn(
    device: Device,
    descriptor_set: DescriptorSet,
    descriptor_update_template: DescriptorUpdateTemplate,
    p_data: *const c_void,
);
#[derive(Clone)]
pub struct KhrDescriptorUpdateTemplateFn {
    pub create_descriptor_update_template_khr: PFN_vkCreateDescriptorUpdateTemplate,
    pub destroy_descriptor_update_template_khr: PFN_vkDestroyDescriptorUpdateTemplate,
    pub update_descriptor_set_with_template_khr: PFN_vkUpdateDescriptorSetWithTemplate,
    pub cmd_push_descriptor_set_with_template_khr:
        crate::vk::PFN_vkCmdPushDescriptorSetWithTemplateKHR,
}
unsafe impl Send for KhrDescriptorUpdateTemplateFn {}
unsafe impl Sync for KhrDescriptorUpdateTemplateFn {}
impl KhrDescriptorUpdateTemplateFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDescriptorUpdateTemplateFn {
            create_descriptor_update_template_khr: unsafe {
                unsafe extern "system" fn create_descriptor_update_template_khr(
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
                unsafe extern "system" fn destroy_descriptor_update_template_khr(
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
                unsafe extern "system" fn update_descriptor_set_with_template_khr(
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
                unsafe extern "system" fn cmd_push_descriptor_set_with_template_khr(
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
        Self::DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_descriptor_update_template'"]
impl ObjectType {
    pub const DESCRIPTOR_UPDATE_TEMPLATE_KHR: Self = Self::DESCRIPTOR_UPDATE_TEMPLATE;
}
#[doc = "Generated from 'VK_KHR_descriptor_update_template'"]
impl DescriptorUpdateTemplateType {
    pub const DESCRIPTOR_SET_KHR: Self = Self::DESCRIPTOR_SET;
}
#[doc = "Generated from 'VK_KHR_descriptor_update_template'"]
impl DebugReportObjectTypeEXT {
    pub const DESCRIPTOR_UPDATE_TEMPLATE_KHR: Self = Self::DESCRIPTOR_UPDATE_TEMPLATE;
}
impl NvxDeviceGeneratedCommandsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NVX_device_generated_commands\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 3u32;
}
#[derive(Clone)]
pub struct NvxDeviceGeneratedCommandsFn {}
unsafe impl Send for NvxDeviceGeneratedCommandsFn {}
unsafe impl Sync for NvxDeviceGeneratedCommandsFn {}
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
pub type PFN_vkCmdSetViewportWScalingNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_viewport: u32,
    viewport_count: u32,
    p_viewport_w_scalings: *const ViewportWScalingNV,
);
#[derive(Clone)]
pub struct NvClipSpaceWScalingFn {
    pub cmd_set_viewport_w_scaling_nv: PFN_vkCmdSetViewportWScalingNV,
}
unsafe impl Send for NvClipSpaceWScalingFn {}
unsafe impl Sync for NvClipSpaceWScalingFn {}
impl NvClipSpaceWScalingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvClipSpaceWScalingFn {
            cmd_set_viewport_w_scaling_nv: unsafe {
                unsafe extern "system" fn cmd_set_viewport_w_scaling_nv(
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
    unsafe extern "system" fn(physical_device: PhysicalDevice, display: DisplayKHR) -> Result;
#[derive(Clone)]
pub struct ExtDirectModeDisplayFn {
    pub release_display_ext: PFN_vkReleaseDisplayEXT,
}
unsafe impl Send for ExtDirectModeDisplayFn {}
unsafe impl Sync for ExtDirectModeDisplayFn {}
impl ExtDirectModeDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDirectModeDisplayFn {
            release_display_ext: unsafe {
                unsafe extern "system" fn release_display_ext(
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
pub type PFN_vkAcquireXlibDisplayEXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    dpy: *mut Display,
    display: DisplayKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetRandROutputDisplayEXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    dpy: *mut Display,
    rr_output: RROutput,
    p_display: *mut DisplayKHR,
) -> Result;
#[derive(Clone)]
pub struct ExtAcquireXlibDisplayFn {
    pub acquire_xlib_display_ext: PFN_vkAcquireXlibDisplayEXT,
    pub get_rand_r_output_display_ext: PFN_vkGetRandROutputDisplayEXT,
}
unsafe impl Send for ExtAcquireXlibDisplayFn {}
unsafe impl Sync for ExtAcquireXlibDisplayFn {}
impl ExtAcquireXlibDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtAcquireXlibDisplayFn {
            acquire_xlib_display_ext: unsafe {
                unsafe extern "system" fn acquire_xlib_display_ext(
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
                unsafe extern "system" fn get_rand_r_output_display_ext(
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
pub type PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    surface: SurfaceKHR,
    p_surface_capabilities: *mut SurfaceCapabilities2EXT,
) -> Result;
#[derive(Clone)]
pub struct ExtDisplaySurfaceCounterFn {
    pub get_physical_device_surface_capabilities2_ext:
        PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT,
}
unsafe impl Send for ExtDisplaySurfaceCounterFn {}
unsafe impl Sync for ExtDisplaySurfaceCounterFn {}
impl ExtDisplaySurfaceCounterFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDisplaySurfaceCounterFn {
            get_physical_device_surface_capabilities2_ext: unsafe {
                unsafe extern "system" fn get_physical_device_surface_capabilities2_ext(
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
    pub const SURFACE_CAPABILITIES2_EXT: Self = Self::SURFACE_CAPABILITIES_2_EXT;
}
impl ExtDisplayControlFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_display_control\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkDisplayPowerControlEXT = unsafe extern "system" fn(
    device: Device,
    display: DisplayKHR,
    p_display_power_info: *const DisplayPowerInfoEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkRegisterDeviceEventEXT = unsafe extern "system" fn(
    device: Device,
    p_device_event_info: *const DeviceEventInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_fence: *mut Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkRegisterDisplayEventEXT = unsafe extern "system" fn(
    device: Device,
    display: DisplayKHR,
    p_display_event_info: *const DisplayEventInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_fence: *mut Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSwapchainCounterEXT = unsafe extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    counter: SurfaceCounterFlagsEXT,
    p_counter_value: *mut u64,
) -> Result;
#[derive(Clone)]
pub struct ExtDisplayControlFn {
    pub display_power_control_ext: PFN_vkDisplayPowerControlEXT,
    pub register_device_event_ext: PFN_vkRegisterDeviceEventEXT,
    pub register_display_event_ext: PFN_vkRegisterDisplayEventEXT,
    pub get_swapchain_counter_ext: PFN_vkGetSwapchainCounterEXT,
}
unsafe impl Send for ExtDisplayControlFn {}
unsafe impl Sync for ExtDisplayControlFn {}
impl ExtDisplayControlFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDisplayControlFn {
            display_power_control_ext: unsafe {
                unsafe extern "system" fn display_power_control_ext(
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
                unsafe extern "system" fn register_device_event_ext(
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
                unsafe extern "system" fn register_display_event_ext(
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
                unsafe extern "system" fn get_swapchain_counter_ext(
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
pub type PFN_vkGetRefreshCycleDurationGOOGLE = unsafe extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_display_timing_properties: *mut RefreshCycleDurationGOOGLE,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPastPresentationTimingGOOGLE = unsafe extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    p_presentation_timing_count: *mut u32,
    p_presentation_timings: *mut PastPresentationTimingGOOGLE,
) -> Result;
#[derive(Clone)]
pub struct GoogleDisplayTimingFn {
    pub get_refresh_cycle_duration_google: PFN_vkGetRefreshCycleDurationGOOGLE,
    pub get_past_presentation_timing_google: PFN_vkGetPastPresentationTimingGOOGLE,
}
unsafe impl Send for GoogleDisplayTimingFn {}
unsafe impl Sync for GoogleDisplayTimingFn {}
impl GoogleDisplayTimingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GoogleDisplayTimingFn {
            get_refresh_cycle_duration_google: unsafe {
                unsafe extern "system" fn get_refresh_cycle_duration_google(
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
                unsafe extern "system" fn get_past_presentation_timing_google(
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
#[derive(Clone)]
pub struct NvSampleMaskOverrideCoverageFn {}
unsafe impl Send for NvSampleMaskOverrideCoverageFn {}
unsafe impl Sync for NvSampleMaskOverrideCoverageFn {}
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
#[derive(Clone)]
pub struct NvGeometryShaderPassthroughFn {}
unsafe impl Send for NvGeometryShaderPassthroughFn {}
unsafe impl Sync for NvGeometryShaderPassthroughFn {}
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
#[derive(Clone)]
pub struct NvViewportArray2Fn {}
unsafe impl Send for NvViewportArray2Fn {}
unsafe impl Sync for NvViewportArray2Fn {}
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
#[derive(Clone)]
pub struct NvxMultiviewPerViewAttributesFn {}
unsafe impl Send for NvxMultiviewPerViewAttributesFn {}
unsafe impl Sync for NvxMultiviewPerViewAttributesFn {}
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
#[derive(Clone)]
pub struct NvViewportSwizzleFn {}
unsafe impl Send for NvViewportSwizzleFn {}
unsafe impl Sync for NvViewportSwizzleFn {}
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
pub type PFN_vkCmdSetDiscardRectangleEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_discard_rectangle: u32,
    discard_rectangle_count: u32,
    p_discard_rectangles: *const Rect2D,
);
#[derive(Clone)]
pub struct ExtDiscardRectanglesFn {
    pub cmd_set_discard_rectangle_ext: PFN_vkCmdSetDiscardRectangleEXT,
}
unsafe impl Send for ExtDiscardRectanglesFn {}
unsafe impl Sync for ExtDiscardRectanglesFn {}
impl ExtDiscardRectanglesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDiscardRectanglesFn {
            cmd_set_discard_rectangle_ext: unsafe {
                unsafe extern "system" fn cmd_set_discard_rectangle_ext(
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
#[derive(Clone)]
pub struct NvExtension101Fn {}
unsafe impl Send for NvExtension101Fn {}
unsafe impl Sync for NvExtension101Fn {}
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
#[derive(Clone)]
pub struct ExtConservativeRasterizationFn {}
unsafe impl Send for ExtConservativeRasterizationFn {}
unsafe impl Sync for ExtConservativeRasterizationFn {}
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
#[derive(Clone)]
pub struct ExtDepthClipEnableFn {}
unsafe impl Send for ExtDepthClipEnableFn {}
unsafe impl Sync for ExtDepthClipEnableFn {}
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
#[derive(Clone)]
pub struct NvExtension104Fn {}
unsafe impl Send for NvExtension104Fn {}
unsafe impl Sync for NvExtension104Fn {}
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
#[derive(Clone)]
pub struct ExtSwapchainColorspaceFn {}
unsafe impl Send for ExtSwapchainColorspaceFn {}
unsafe impl Sync for ExtSwapchainColorspaceFn {}
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
    pub const DCI_P3_LINEAR_EXT: Self = Self::DISPLAY_P3_LINEAR_EXT;
}
impl ExtHdrMetadataFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_hdr_metadata\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkSetHdrMetadataEXT = unsafe extern "system" fn(
    device: Device,
    swapchain_count: u32,
    p_swapchains: *const SwapchainKHR,
    p_metadata: *const HdrMetadataEXT,
);
#[derive(Clone)]
pub struct ExtHdrMetadataFn {
    pub set_hdr_metadata_ext: PFN_vkSetHdrMetadataEXT,
}
unsafe impl Send for ExtHdrMetadataFn {}
unsafe impl Sync for ExtHdrMetadataFn {}
impl ExtHdrMetadataFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtHdrMetadataFn {
            set_hdr_metadata_ext: unsafe {
                unsafe extern "system" fn set_hdr_metadata_ext(
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
#[derive(Clone)]
pub struct ImgExtension107Fn {}
unsafe impl Send for ImgExtension107Fn {}
unsafe impl Sync for ImgExtension107Fn {}
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
#[derive(Clone)]
pub struct ImgExtension108Fn {}
unsafe impl Send for ImgExtension108Fn {}
unsafe impl Sync for ImgExtension108Fn {}
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
#[derive(Clone)]
pub struct KhrImagelessFramebufferFn {}
unsafe impl Send for KhrImagelessFramebufferFn {}
unsafe impl Sync for KhrImagelessFramebufferFn {}
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
        Self::PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl StructureType {
    pub const FRAMEBUFFER_ATTACHMENTS_CREATE_INFO_KHR: Self =
        Self::FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl StructureType {
    pub const FRAMEBUFFER_ATTACHMENT_IMAGE_INFO_KHR: Self = Self::FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl StructureType {
    pub const RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR: Self = Self::RENDER_PASS_ATTACHMENT_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_imageless_framebuffer'"]
impl FramebufferCreateFlags {
    pub const IMAGELESS_KHR: Self = Self::IMAGELESS;
}
impl KhrCreateRenderpass2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_create_renderpass2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateRenderPass2 = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const RenderPassCreateInfo2,
    p_allocator: *const AllocationCallbacks,
    p_render_pass: *mut RenderPass,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginRenderPass2 = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_render_pass_begin: *const RenderPassBeginInfo,
    p_subpass_begin_info: *const SubpassBeginInfo,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdNextSubpass2 = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_subpass_begin_info: *const SubpassBeginInfo,
    p_subpass_end_info: *const SubpassEndInfo,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndRenderPass2 = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_subpass_end_info: *const SubpassEndInfo,
);
#[derive(Clone)]
pub struct KhrCreateRenderpass2Fn {
    pub create_render_pass2_khr: PFN_vkCreateRenderPass2,
    pub cmd_begin_render_pass2_khr: PFN_vkCmdBeginRenderPass2,
    pub cmd_next_subpass2_khr: PFN_vkCmdNextSubpass2,
    pub cmd_end_render_pass2_khr: PFN_vkCmdEndRenderPass2,
}
unsafe impl Send for KhrCreateRenderpass2Fn {}
unsafe impl Sync for KhrCreateRenderpass2Fn {}
impl KhrCreateRenderpass2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrCreateRenderpass2Fn {
            create_render_pass2_khr: unsafe {
                unsafe extern "system" fn create_render_pass2_khr(
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
                unsafe extern "system" fn cmd_begin_render_pass2_khr(
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
                unsafe extern "system" fn cmd_next_subpass2_khr(
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
                unsafe extern "system" fn cmd_end_render_pass2_khr(
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
    pub const ATTACHMENT_DESCRIPTION_2_KHR: Self = Self::ATTACHMENT_DESCRIPTION_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const ATTACHMENT_REFERENCE_2_KHR: Self = Self::ATTACHMENT_REFERENCE_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_DESCRIPTION_2_KHR: Self = Self::SUBPASS_DESCRIPTION_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_DEPENDENCY_2_KHR: Self = Self::SUBPASS_DEPENDENCY_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const RENDER_PASS_CREATE_INFO_2_KHR: Self = Self::RENDER_PASS_CREATE_INFO_2;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_BEGIN_INFO_KHR: Self = Self::SUBPASS_BEGIN_INFO;
}
#[doc = "Generated from 'VK_KHR_create_renderpass2'"]
impl StructureType {
    pub const SUBPASS_END_INFO_KHR: Self = Self::SUBPASS_END_INFO;
}
impl ImgExtension111Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_IMG_extension_111\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ImgExtension111Fn {}
unsafe impl Send for ImgExtension111Fn {}
unsafe impl Sync for ImgExtension111Fn {}
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
    unsafe extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result;
#[derive(Clone)]
pub struct KhrSharedPresentableImageFn {
    pub get_swapchain_status_khr: PFN_vkGetSwapchainStatusKHR,
}
unsafe impl Send for KhrSharedPresentableImageFn {}
unsafe impl Sync for KhrSharedPresentableImageFn {}
impl KhrSharedPresentableImageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSharedPresentableImageFn {
            get_swapchain_status_khr: unsafe {
                unsafe extern "system" fn get_swapchain_status_khr(
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
pub type PFN_vkGetPhysicalDeviceExternalFenceProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_external_fence_info: *const PhysicalDeviceExternalFenceInfo,
    p_external_fence_properties: *mut ExternalFenceProperties,
);
#[derive(Clone)]
pub struct KhrExternalFenceCapabilitiesFn {
    pub get_physical_device_external_fence_properties_khr:
        PFN_vkGetPhysicalDeviceExternalFenceProperties,
}
unsafe impl Send for KhrExternalFenceCapabilitiesFn {}
unsafe impl Sync for KhrExternalFenceCapabilitiesFn {}
impl KhrExternalFenceCapabilitiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalFenceCapabilitiesFn {
            get_physical_device_external_fence_properties_khr: unsafe {
                unsafe extern "system" fn get_physical_device_external_fence_properties_khr(
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
        Self::PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl StructureType {
    pub const EXTERNAL_FENCE_PROPERTIES_KHR: Self = Self::EXTERNAL_FENCE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const OPAQUE_FD_KHR: Self = Self::OPAQUE_FD;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const OPAQUE_WIN32_KHR: Self = Self::OPAQUE_WIN32;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const OPAQUE_WIN32_KMT_KHR: Self = Self::OPAQUE_WIN32_KMT;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceHandleTypeFlags {
    pub const SYNC_FD_KHR: Self = Self::SYNC_FD;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceFeatureFlags {
    pub const EXPORTABLE_KHR: Self = Self::EXPORTABLE;
}
#[doc = "Generated from 'VK_KHR_external_fence_capabilities'"]
impl ExternalFenceFeatureFlags {
    pub const IMPORTABLE_KHR: Self = Self::IMPORTABLE;
}
impl KhrExternalFenceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_fence\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrExternalFenceFn {}
unsafe impl Send for KhrExternalFenceFn {}
unsafe impl Sync for KhrExternalFenceFn {}
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
    pub const EXPORT_FENCE_CREATE_INFO_KHR: Self = Self::EXPORT_FENCE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_external_fence'"]
impl FenceImportFlags {
    pub const TEMPORARY_KHR: Self = Self::TEMPORARY;
}
impl KhrExternalFenceWin32Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_external_fence_win32\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkImportFenceWin32HandleKHR = unsafe extern "system" fn(
    device: Device,
    p_import_fence_win32_handle_info: *const ImportFenceWin32HandleInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetFenceWin32HandleKHR = unsafe extern "system" fn(
    device: Device,
    p_get_win32_handle_info: *const FenceGetWin32HandleInfoKHR,
    p_handle: *mut HANDLE,
) -> Result;
#[derive(Clone)]
pub struct KhrExternalFenceWin32Fn {
    pub import_fence_win32_handle_khr: PFN_vkImportFenceWin32HandleKHR,
    pub get_fence_win32_handle_khr: PFN_vkGetFenceWin32HandleKHR,
}
unsafe impl Send for KhrExternalFenceWin32Fn {}
unsafe impl Sync for KhrExternalFenceWin32Fn {}
impl KhrExternalFenceWin32Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalFenceWin32Fn {
            import_fence_win32_handle_khr: unsafe {
                unsafe extern "system" fn import_fence_win32_handle_khr(
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
                unsafe extern "system" fn get_fence_win32_handle_khr(
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
pub type PFN_vkImportFenceFdKHR = unsafe extern "system" fn(
    device: Device,
    p_import_fence_fd_info: *const ImportFenceFdInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetFenceFdKHR = unsafe extern "system" fn(
    device: Device,
    p_get_fd_info: *const FenceGetFdInfoKHR,
    p_fd: *mut c_int,
) -> Result;
#[derive(Clone)]
pub struct KhrExternalFenceFdFn {
    pub import_fence_fd_khr: PFN_vkImportFenceFdKHR,
    pub get_fence_fd_khr: PFN_vkGetFenceFdKHR,
}
unsafe impl Send for KhrExternalFenceFdFn {}
unsafe impl Sync for KhrExternalFenceFdFn {}
impl KhrExternalFenceFdFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExternalFenceFdFn {
            import_fence_fd_khr: unsafe {
                unsafe extern "system" fn import_fence_fd_khr(
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
                unsafe extern "system" fn get_fence_fd_khr(
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
    unsafe extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        p_counter_count: *mut u32,
        p_counters: *mut PerformanceCounterKHR,
        p_counter_descriptions: *mut PerformanceCounterDescriptionKHR,
    ) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR =
    unsafe extern "system" fn(
        physical_device: PhysicalDevice,
        p_performance_query_create_info: *const QueryPoolPerformanceCreateInfoKHR,
        p_num_passes: *mut u32,
    );
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireProfilingLockKHR =
    unsafe extern "system" fn(device: Device, p_info: *const AcquireProfilingLockInfoKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkReleaseProfilingLockKHR = unsafe extern "system" fn(device: Device);
#[derive(Clone)]
pub struct KhrPerformanceQueryFn {
    pub enumerate_physical_device_queue_family_performance_query_counters_khr:
        PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR,
    pub get_physical_device_queue_family_performance_query_passes_khr:
        PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR,
    pub acquire_profiling_lock_khr: PFN_vkAcquireProfilingLockKHR,
    pub release_profiling_lock_khr: PFN_vkReleaseProfilingLockKHR,
}
unsafe impl Send for KhrPerformanceQueryFn {}
unsafe impl Sync for KhrPerformanceQueryFn {}
impl KhrPerformanceQueryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPerformanceQueryFn {
            enumerate_physical_device_queue_family_performance_query_counters_khr: unsafe {
                unsafe extern "system" fn enumerate_physical_device_queue_family_performance_query_counters_khr(
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
                unsafe extern "system" fn get_physical_device_queue_family_performance_query_passes_khr(
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
                unsafe extern "system" fn acquire_profiling_lock_khr(
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
                unsafe extern "system" fn release_profiling_lock_khr(_device: Device) {
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
#[derive(Clone)]
pub struct KhrMaintenance2Fn {}
unsafe impl Send for KhrMaintenance2Fn {}
unsafe impl Sync for KhrMaintenance2Fn {}
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
    pub const BLOCK_TEXEL_VIEW_COMPATIBLE_KHR: Self = Self::BLOCK_TEXEL_VIEW_COMPATIBLE;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl ImageCreateFlags {
    pub const EXTENDED_USAGE_KHR: Self = Self::EXTENDED_USAGE;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES_KHR: Self =
        Self::PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO_KHR: Self =
        Self::RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const IMAGE_VIEW_USAGE_CREATE_INFO_KHR: Self = Self::IMAGE_VIEW_USAGE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl StructureType {
    pub const PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO_KHR: Self =
        Self::PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl ImageLayout {
    pub const DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR: Self =
        Self::DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl ImageLayout {
    pub const DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR: Self =
        Self::DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl PointClippingBehavior {
    pub const ALL_CLIP_PLANES_KHR: Self = Self::ALL_CLIP_PLANES;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl PointClippingBehavior {
    pub const USER_CLIP_PLANES_ONLY_KHR: Self = Self::USER_CLIP_PLANES_ONLY;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl TessellationDomainOrigin {
    pub const UPPER_LEFT_KHR: Self = Self::UPPER_LEFT;
}
#[doc = "Generated from 'VK_KHR_maintenance2'"]
impl TessellationDomainOrigin {
    pub const LOWER_LEFT_KHR: Self = Self::LOWER_LEFT;
}
impl KhrExtension119Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_119\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension119Fn {}
unsafe impl Send for KhrExtension119Fn {}
unsafe impl Sync for KhrExtension119Fn {}
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
pub type PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_surface_capabilities: *mut SurfaceCapabilities2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfaceFormats2KHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_surface_format_count: *mut u32,
    p_surface_formats: *mut SurfaceFormat2KHR,
) -> Result;
#[derive(Clone)]
pub struct KhrGetSurfaceCapabilities2Fn {
    pub get_physical_device_surface_capabilities2_khr:
        PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR,
    pub get_physical_device_surface_formats2_khr: PFN_vkGetPhysicalDeviceSurfaceFormats2KHR,
}
unsafe impl Send for KhrGetSurfaceCapabilities2Fn {}
unsafe impl Sync for KhrGetSurfaceCapabilities2Fn {}
impl KhrGetSurfaceCapabilities2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetSurfaceCapabilities2Fn {
            get_physical_device_surface_capabilities2_khr: unsafe {
                unsafe extern "system" fn get_physical_device_surface_capabilities2_khr(
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
                unsafe extern "system" fn get_physical_device_surface_formats2_khr(
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
#[derive(Clone)]
pub struct KhrVariablePointersFn {}
unsafe impl Send for KhrVariablePointersFn {}
unsafe impl Sync for KhrVariablePointersFn {}
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
        Self::PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES;
}
#[doc = "Generated from 'VK_KHR_variable_pointers'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES_KHR: Self =
        Self::PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES_KHR;
}
impl KhrGetDisplayProperties2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_get_display_properties2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDisplayProperties2KHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayProperties2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut DisplayPlaneProperties2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayModeProperties2KHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    display: DisplayKHR,
    p_property_count: *mut u32,
    p_properties: *mut DisplayModeProperties2KHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDisplayPlaneCapabilities2KHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_display_plane_info: *const DisplayPlaneInfo2KHR,
    p_capabilities: *mut DisplayPlaneCapabilities2KHR,
) -> Result;
#[derive(Clone)]
pub struct KhrGetDisplayProperties2Fn {
    pub get_physical_device_display_properties2_khr: PFN_vkGetPhysicalDeviceDisplayProperties2KHR,
    pub get_physical_device_display_plane_properties2_khr:
        PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR,
    pub get_display_mode_properties2_khr: PFN_vkGetDisplayModeProperties2KHR,
    pub get_display_plane_capabilities2_khr: PFN_vkGetDisplayPlaneCapabilities2KHR,
}
unsafe impl Send for KhrGetDisplayProperties2Fn {}
unsafe impl Sync for KhrGetDisplayProperties2Fn {}
impl KhrGetDisplayProperties2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetDisplayProperties2Fn {
            get_physical_device_display_properties2_khr: unsafe {
                unsafe extern "system" fn get_physical_device_display_properties2_khr(
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
                unsafe extern "system" fn get_physical_device_display_plane_properties2_khr(
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
                unsafe extern "system" fn get_display_mode_properties2_khr(
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
                unsafe extern "system" fn get_display_plane_capabilities2_khr(
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
pub type PFN_vkCreateIOSSurfaceMVK = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const IOSSurfaceCreateInfoMVK,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct MvkIosSurfaceFn {
    pub create_ios_surface_mvk: PFN_vkCreateIOSSurfaceMVK,
}
unsafe impl Send for MvkIosSurfaceFn {}
unsafe impl Sync for MvkIosSurfaceFn {}
impl MvkIosSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        MvkIosSurfaceFn {
            create_ios_surface_mvk: unsafe {
                unsafe extern "system" fn create_ios_surface_mvk(
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
pub type PFN_vkCreateMacOSSurfaceMVK = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const MacOSSurfaceCreateInfoMVK,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct MvkMacosSurfaceFn {
    pub create_mac_os_surface_mvk: PFN_vkCreateMacOSSurfaceMVK,
}
unsafe impl Send for MvkMacosSurfaceFn {}
unsafe impl Sync for MvkMacosSurfaceFn {}
impl MvkMacosSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        MvkMacosSurfaceFn {
            create_mac_os_surface_mvk: unsafe {
                unsafe extern "system" fn create_mac_os_surface_mvk(
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
#[derive(Clone)]
pub struct MvkMoltenvkFn {}
unsafe impl Send for MvkMoltenvkFn {}
unsafe impl Sync for MvkMoltenvkFn {}
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
#[derive(Clone)]
pub struct ExtExternalMemoryDmaBufFn {}
unsafe impl Send for ExtExternalMemoryDmaBufFn {}
unsafe impl Sync for ExtExternalMemoryDmaBufFn {}
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
#[derive(Clone)]
pub struct ExtQueueFamilyForeignFn {}
unsafe impl Send for ExtQueueFamilyForeignFn {}
unsafe impl Sync for ExtQueueFamilyForeignFn {}
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
#[derive(Clone)]
pub struct KhrDedicatedAllocationFn {}
unsafe impl Send for KhrDedicatedAllocationFn {}
unsafe impl Sync for KhrDedicatedAllocationFn {}
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
    pub const MEMORY_DEDICATED_REQUIREMENTS_KHR: Self = Self::MEMORY_DEDICATED_REQUIREMENTS;
}
#[doc = "Generated from 'VK_KHR_dedicated_allocation'"]
impl StructureType {
    pub const MEMORY_DEDICATED_ALLOCATE_INFO_KHR: Self = Self::MEMORY_DEDICATED_ALLOCATE_INFO;
}
impl ExtDebugUtilsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_debug_utils\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkSetDebugUtilsObjectNameEXT = unsafe extern "system" fn(
    device: Device,
    p_name_info: *const DebugUtilsObjectNameInfoEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkSetDebugUtilsObjectTagEXT = unsafe extern "system" fn(
    device: Device,
    p_tag_info: *const DebugUtilsObjectTagInfoEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueueBeginDebugUtilsLabelEXT =
    unsafe extern "system" fn(queue: Queue, p_label_info: *const DebugUtilsLabelEXT);
#[allow(non_camel_case_types)]
pub type PFN_vkQueueEndDebugUtilsLabelEXT = unsafe extern "system" fn(queue: Queue);
#[allow(non_camel_case_types)]
pub type PFN_vkQueueInsertDebugUtilsLabelEXT =
    unsafe extern "system" fn(queue: Queue, p_label_info: *const DebugUtilsLabelEXT);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginDebugUtilsLabelEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_label_info: *const DebugUtilsLabelEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndDebugUtilsLabelEXT = unsafe extern "system" fn(command_buffer: CommandBuffer);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdInsertDebugUtilsLabelEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_label_info: *const DebugUtilsLabelEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDebugUtilsMessengerEXT = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const DebugUtilsMessengerCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_messenger: *mut DebugUtilsMessengerEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDebugUtilsMessengerEXT = unsafe extern "system" fn(
    instance: Instance,
    messenger: DebugUtilsMessengerEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkSubmitDebugUtilsMessageEXT = unsafe extern "system" fn(
    instance: Instance,
    message_severity: DebugUtilsMessageSeverityFlagsEXT,
    message_types: DebugUtilsMessageTypeFlagsEXT,
    p_callback_data: *const DebugUtilsMessengerCallbackDataEXT,
);
#[derive(Clone)]
pub struct ExtDebugUtilsFn {
    pub set_debug_utils_object_name_ext: PFN_vkSetDebugUtilsObjectNameEXT,
    pub set_debug_utils_object_tag_ext: PFN_vkSetDebugUtilsObjectTagEXT,
    pub queue_begin_debug_utils_label_ext: PFN_vkQueueBeginDebugUtilsLabelEXT,
    pub queue_end_debug_utils_label_ext: PFN_vkQueueEndDebugUtilsLabelEXT,
    pub queue_insert_debug_utils_label_ext: PFN_vkQueueInsertDebugUtilsLabelEXT,
    pub cmd_begin_debug_utils_label_ext: PFN_vkCmdBeginDebugUtilsLabelEXT,
    pub cmd_end_debug_utils_label_ext: PFN_vkCmdEndDebugUtilsLabelEXT,
    pub cmd_insert_debug_utils_label_ext: PFN_vkCmdInsertDebugUtilsLabelEXT,
    pub create_debug_utils_messenger_ext: PFN_vkCreateDebugUtilsMessengerEXT,
    pub destroy_debug_utils_messenger_ext: PFN_vkDestroyDebugUtilsMessengerEXT,
    pub submit_debug_utils_message_ext: PFN_vkSubmitDebugUtilsMessageEXT,
}
unsafe impl Send for ExtDebugUtilsFn {}
unsafe impl Sync for ExtDebugUtilsFn {}
impl ExtDebugUtilsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDebugUtilsFn {
            set_debug_utils_object_name_ext: unsafe {
                unsafe extern "system" fn set_debug_utils_object_name_ext(
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
                unsafe extern "system" fn set_debug_utils_object_tag_ext(
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
                unsafe extern "system" fn queue_begin_debug_utils_label_ext(
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
                unsafe extern "system" fn queue_end_debug_utils_label_ext(_queue: Queue) {
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
                unsafe extern "system" fn queue_insert_debug_utils_label_ext(
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
                unsafe extern "system" fn cmd_begin_debug_utils_label_ext(
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
                unsafe extern "system" fn cmd_end_debug_utils_label_ext(
                    _command_buffer: CommandBuffer,
                ) {
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
                unsafe extern "system" fn cmd_insert_debug_utils_label_ext(
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
                unsafe extern "system" fn create_debug_utils_messenger_ext(
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
                unsafe extern "system" fn destroy_debug_utils_messenger_ext(
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
                unsafe extern "system" fn submit_debug_utils_message_ext(
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
pub type PFN_vkGetAndroidHardwareBufferPropertiesANDROID = unsafe extern "system" fn(
    device: Device,
    buffer: *const AHardwareBuffer,
    p_properties: *mut AndroidHardwareBufferPropertiesANDROID,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryAndroidHardwareBufferANDROID = unsafe extern "system" fn(
    device: Device,
    p_info: *const MemoryGetAndroidHardwareBufferInfoANDROID,
    p_buffer: *mut *mut AHardwareBuffer,
) -> Result;
#[derive(Clone)]
pub struct AndroidExternalMemoryAndroidHardwareBufferFn {
    pub get_android_hardware_buffer_properties_android:
        PFN_vkGetAndroidHardwareBufferPropertiesANDROID,
    pub get_memory_android_hardware_buffer_android: PFN_vkGetMemoryAndroidHardwareBufferANDROID,
}
unsafe impl Send for AndroidExternalMemoryAndroidHardwareBufferFn {}
unsafe impl Sync for AndroidExternalMemoryAndroidHardwareBufferFn {}
impl AndroidExternalMemoryAndroidHardwareBufferFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AndroidExternalMemoryAndroidHardwareBufferFn {
            get_android_hardware_buffer_properties_android: unsafe {
                unsafe extern "system" fn get_android_hardware_buffer_properties_android(
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
                unsafe extern "system" fn get_memory_android_hardware_buffer_android(
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
#[derive(Clone)]
pub struct ExtSamplerFilterMinmaxFn {}
unsafe impl Send for ExtSamplerFilterMinmaxFn {}
unsafe impl Sync for ExtSamplerFilterMinmaxFn {}
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
        Self::PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl StructureType {
    pub const SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT: Self =
        Self::SAMPLER_REDUCTION_MODE_CREATE_INFO;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_FILTER_MINMAX_EXT: Self = Self::SAMPLED_IMAGE_FILTER_MINMAX;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl SamplerReductionMode {
    pub const WEIGHTED_AVERAGE_EXT: Self = Self::WEIGHTED_AVERAGE;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl SamplerReductionMode {
    pub const MIN_EXT: Self = Self::MIN;
}
#[doc = "Generated from 'VK_EXT_sampler_filter_minmax'"]
impl SamplerReductionMode {
    pub const MAX_EXT: Self = Self::MAX;
}
impl KhrStorageBufferStorageClassFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_storage_buffer_storage_class\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrStorageBufferStorageClassFn {}
unsafe impl Send for KhrStorageBufferStorageClassFn {}
unsafe impl Sync for KhrStorageBufferStorageClassFn {}
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
#[derive(Clone)]
pub struct AmdGpuShaderInt16Fn {}
unsafe impl Send for AmdGpuShaderInt16Fn {}
unsafe impl Sync for AmdGpuShaderInt16Fn {}
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
#[derive(Clone)]
pub struct AmdExtension134Fn {}
unsafe impl Send for AmdExtension134Fn {}
unsafe impl Sync for AmdExtension134Fn {}
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
#[derive(Clone)]
pub struct AmdExtension135Fn {}
unsafe impl Send for AmdExtension135Fn {}
unsafe impl Sync for AmdExtension135Fn {}
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
#[derive(Clone)]
pub struct AmdExtension136Fn {}
unsafe impl Send for AmdExtension136Fn {}
unsafe impl Sync for AmdExtension136Fn {}
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
#[derive(Clone)]
pub struct AmdMixedAttachmentSamplesFn {}
unsafe impl Send for AmdMixedAttachmentSamplesFn {}
unsafe impl Sync for AmdMixedAttachmentSamplesFn {}
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
#[derive(Clone)]
pub struct AmdShaderFragmentMaskFn {}
unsafe impl Send for AmdShaderFragmentMaskFn {}
unsafe impl Sync for AmdShaderFragmentMaskFn {}
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
#[derive(Clone)]
pub struct ExtInlineUniformBlockFn {}
unsafe impl Send for ExtInlineUniformBlockFn {}
unsafe impl Sync for ExtInlineUniformBlockFn {}
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
#[derive(Clone)]
pub struct AmdExtension140Fn {}
unsafe impl Send for AmdExtension140Fn {}
unsafe impl Sync for AmdExtension140Fn {}
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
#[derive(Clone)]
pub struct ExtShaderStencilExportFn {}
unsafe impl Send for ExtShaderStencilExportFn {}
unsafe impl Sync for ExtShaderStencilExportFn {}
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
#[derive(Clone)]
pub struct AmdExtension142Fn {}
unsafe impl Send for AmdExtension142Fn {}
unsafe impl Sync for AmdExtension142Fn {}
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
#[derive(Clone)]
pub struct AmdExtension143Fn {}
unsafe impl Send for AmdExtension143Fn {}
unsafe impl Sync for AmdExtension143Fn {}
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
pub type PFN_vkCmdSetSampleLocationsEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_sample_locations_info: *const SampleLocationsInfoEXT,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    samples: SampleCountFlags,
    p_multisample_properties: *mut MultisamplePropertiesEXT,
);
#[derive(Clone)]
pub struct ExtSampleLocationsFn {
    pub cmd_set_sample_locations_ext: PFN_vkCmdSetSampleLocationsEXT,
    pub get_physical_device_multisample_properties_ext:
        PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT,
}
unsafe impl Send for ExtSampleLocationsFn {}
unsafe impl Sync for ExtSampleLocationsFn {}
impl ExtSampleLocationsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtSampleLocationsFn {
            cmd_set_sample_locations_ext: unsafe {
                unsafe extern "system" fn cmd_set_sample_locations_ext(
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
                unsafe extern "system" fn get_physical_device_multisample_properties_ext(
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
#[derive(Clone)]
pub struct KhrRelaxedBlockLayoutFn {}
unsafe impl Send for KhrRelaxedBlockLayoutFn {}
unsafe impl Sync for KhrRelaxedBlockLayoutFn {}
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
pub type PFN_vkGetImageMemoryRequirements2 = unsafe extern "system" fn(
    device: Device,
    p_info: *const ImageMemoryRequirementsInfo2,
    p_memory_requirements: *mut MemoryRequirements2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetBufferMemoryRequirements2 = unsafe extern "system" fn(
    device: Device,
    p_info: *const BufferMemoryRequirementsInfo2,
    p_memory_requirements: *mut MemoryRequirements2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageSparseMemoryRequirements2 = unsafe extern "system" fn(
    device: Device,
    p_info: *const ImageSparseMemoryRequirementsInfo2,
    p_sparse_memory_requirement_count: *mut u32,
    p_sparse_memory_requirements: *mut SparseImageMemoryRequirements2,
);
#[derive(Clone)]
pub struct KhrGetMemoryRequirements2Fn {
    pub get_image_memory_requirements2_khr: PFN_vkGetImageMemoryRequirements2,
    pub get_buffer_memory_requirements2_khr: PFN_vkGetBufferMemoryRequirements2,
    pub get_image_sparse_memory_requirements2_khr: PFN_vkGetImageSparseMemoryRequirements2,
}
unsafe impl Send for KhrGetMemoryRequirements2Fn {}
unsafe impl Sync for KhrGetMemoryRequirements2Fn {}
impl KhrGetMemoryRequirements2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrGetMemoryRequirements2Fn {
            get_image_memory_requirements2_khr: unsafe {
                unsafe extern "system" fn get_image_memory_requirements2_khr(
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
                unsafe extern "system" fn get_buffer_memory_requirements2_khr(
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
                unsafe extern "system" fn get_image_sparse_memory_requirements2_khr(
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
    pub const BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR: Self = Self::BUFFER_MEMORY_REQUIREMENTS_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR: Self = Self::IMAGE_MEMORY_REQUIREMENTS_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2_KHR: Self =
        Self::IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const MEMORY_REQUIREMENTS_2_KHR: Self = Self::MEMORY_REQUIREMENTS_2;
}
#[doc = "Generated from 'VK_KHR_get_memory_requirements2'"]
impl StructureType {
    pub const SPARSE_IMAGE_MEMORY_REQUIREMENTS_2_KHR: Self =
        Self::SPARSE_IMAGE_MEMORY_REQUIREMENTS_2;
}
impl KhrImageFormatListFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_image_format_list\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrImageFormatListFn {}
unsafe impl Send for KhrImageFormatListFn {}
unsafe impl Sync for KhrImageFormatListFn {}
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
    pub const IMAGE_FORMAT_LIST_CREATE_INFO_KHR: Self = Self::IMAGE_FORMAT_LIST_CREATE_INFO;
}
impl ExtBlendOperationAdvancedFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_blend_operation_advanced\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[derive(Clone)]
pub struct ExtBlendOperationAdvancedFn {}
unsafe impl Send for ExtBlendOperationAdvancedFn {}
unsafe impl Sync for ExtBlendOperationAdvancedFn {}
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
#[derive(Clone)]
pub struct NvFragmentCoverageToColorFn {}
unsafe impl Send for NvFragmentCoverageToColorFn {}
unsafe impl Sync for NvFragmentCoverageToColorFn {}
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
    pub const SPEC_VERSION: u32 = 12u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateAccelerationStructureKHR = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const AccelerationStructureCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_acceleration_structure: *mut AccelerationStructureKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyAccelerationStructureKHR = unsafe extern "system" fn(
    device: Device,
    acceleration_structure: AccelerationStructureKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBuildAccelerationStructuresKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    info_count: u32,
    p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
    pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBuildAccelerationStructuresIndirectKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    info_count: u32,
    p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
    p_indirect_device_addresses: *const DeviceAddress,
    p_indirect_strides: *const u32,
    pp_max_primitive_counts: *const *const u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkBuildAccelerationStructuresKHR = unsafe extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    info_count: u32,
    p_infos: *const AccelerationStructureBuildGeometryInfoKHR,
    pp_build_range_infos: *const *const AccelerationStructureBuildRangeInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCopyAccelerationStructureKHR = unsafe extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    p_info: *const CopyAccelerationStructureInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCopyAccelerationStructureToMemoryKHR = unsafe extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCopyMemoryToAccelerationStructureKHR = unsafe extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkWriteAccelerationStructuresPropertiesKHR = unsafe extern "system" fn(
    device: Device,
    acceleration_structure_count: u32,
    p_acceleration_structures: *const AccelerationStructureKHR,
    query_type: QueryType,
    data_size: usize,
    p_data: *mut c_void,
    stride: usize,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyAccelerationStructureKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_info: *const CopyAccelerationStructureInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyAccelerationStructureToMemoryKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_info: *const CopyAccelerationStructureToMemoryInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyMemoryToAccelerationStructureKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_info: *const CopyMemoryToAccelerationStructureInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureDeviceAddressKHR =
    unsafe extern "system" fn(
        device: Device,
        p_info: *const AccelerationStructureDeviceAddressInfoKHR,
    ) -> DeviceAddress;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteAccelerationStructuresPropertiesKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    acceleration_structure_count: u32,
    p_acceleration_structures: *const AccelerationStructureKHR,
    query_type: QueryType,
    query_pool: QueryPool,
    first_query: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceAccelerationStructureCompatibilityKHR = unsafe extern "system" fn(
    device: Device,
    p_version_info: *const AccelerationStructureVersionInfoKHR,
    p_compatibility: *mut AccelerationStructureCompatibilityKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureBuildSizesKHR = unsafe extern "system" fn(
    device: Device,
    build_type: AccelerationStructureBuildTypeKHR,
    p_build_info: *const AccelerationStructureBuildGeometryInfoKHR,
    p_max_primitive_counts: *const u32,
    p_size_info: *mut AccelerationStructureBuildSizesInfoKHR,
);
#[derive(Clone)]
pub struct KhrAccelerationStructureFn {
    pub create_acceleration_structure_khr: PFN_vkCreateAccelerationStructureKHR,
    pub destroy_acceleration_structure_khr: PFN_vkDestroyAccelerationStructureKHR,
    pub cmd_build_acceleration_structures_khr: PFN_vkCmdBuildAccelerationStructuresKHR,
    pub cmd_build_acceleration_structures_indirect_khr:
        PFN_vkCmdBuildAccelerationStructuresIndirectKHR,
    pub build_acceleration_structures_khr: PFN_vkBuildAccelerationStructuresKHR,
    pub copy_acceleration_structure_khr: PFN_vkCopyAccelerationStructureKHR,
    pub copy_acceleration_structure_to_memory_khr: PFN_vkCopyAccelerationStructureToMemoryKHR,
    pub copy_memory_to_acceleration_structure_khr: PFN_vkCopyMemoryToAccelerationStructureKHR,
    pub write_acceleration_structures_properties_khr:
        PFN_vkWriteAccelerationStructuresPropertiesKHR,
    pub cmd_copy_acceleration_structure_khr: PFN_vkCmdCopyAccelerationStructureKHR,
    pub cmd_copy_acceleration_structure_to_memory_khr:
        PFN_vkCmdCopyAccelerationStructureToMemoryKHR,
    pub cmd_copy_memory_to_acceleration_structure_khr:
        PFN_vkCmdCopyMemoryToAccelerationStructureKHR,
    pub get_acceleration_structure_device_address_khr:
        PFN_vkGetAccelerationStructureDeviceAddressKHR,
    pub cmd_write_acceleration_structures_properties_khr:
        PFN_vkCmdWriteAccelerationStructuresPropertiesKHR,
    pub get_device_acceleration_structure_compatibility_khr:
        PFN_vkGetDeviceAccelerationStructureCompatibilityKHR,
    pub get_acceleration_structure_build_sizes_khr: PFN_vkGetAccelerationStructureBuildSizesKHR,
}
unsafe impl Send for KhrAccelerationStructureFn {}
unsafe impl Sync for KhrAccelerationStructureFn {}
impl KhrAccelerationStructureFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrAccelerationStructureFn {
            create_acceleration_structure_khr: unsafe {
                unsafe extern "system" fn create_acceleration_structure_khr(
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
                unsafe extern "system" fn destroy_acceleration_structure_khr(
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
                unsafe extern "system" fn cmd_build_acceleration_structures_khr(
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
                unsafe extern "system" fn cmd_build_acceleration_structures_indirect_khr(
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
                unsafe extern "system" fn build_acceleration_structures_khr(
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
                unsafe extern "system" fn copy_acceleration_structure_khr(
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
                unsafe extern "system" fn copy_acceleration_structure_to_memory_khr(
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
                unsafe extern "system" fn copy_memory_to_acceleration_structure_khr(
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
                unsafe extern "system" fn write_acceleration_structures_properties_khr(
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
                unsafe extern "system" fn cmd_copy_acceleration_structure_khr(
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
                unsafe extern "system" fn cmd_copy_acceleration_structure_to_memory_khr(
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
                unsafe extern "system" fn cmd_copy_memory_to_acceleration_structure_khr(
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
                unsafe extern "system" fn get_acceleration_structure_device_address_khr(
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
                unsafe extern "system" fn cmd_write_acceleration_structures_properties_khr(
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
                unsafe extern "system" fn get_device_acceleration_structure_compatibility_khr(
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
                unsafe extern "system" fn get_acceleration_structure_build_sizes_khr(
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
pub type PFN_vkCmdTraceRaysKHR = unsafe extern "system" fn(
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
pub type PFN_vkCreateRayTracingPipelinesKHR = unsafe extern "system" fn(
    device: Device,
    deferred_operation: DeferredOperationKHR,
    pipeline_cache: PipelineCache,
    create_info_count: u32,
    p_create_infos: *const RayTracingPipelineCreateInfoKHR,
    p_allocator: *const AllocationCallbacks,
    p_pipelines: *mut Pipeline,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetRayTracingShaderGroupHandlesKHR = unsafe extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    first_group: u32,
    group_count: u32,
    data_size: usize,
    p_data: *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR =
    unsafe extern "system" fn(
        device: Device,
        pipeline: Pipeline,
        first_group: u32,
        group_count: u32,
        data_size: usize,
        p_data: *mut c_void,
    ) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdTraceRaysIndirectKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_raygen_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_miss_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_hit_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    p_callable_shader_binding_table: *const StridedDeviceAddressRegionKHR,
    indirect_device_address: DeviceAddress,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetRayTracingShaderGroupStackSizeKHR = unsafe extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    group: u32,
    group_shader: ShaderGroupShaderKHR,
) -> DeviceSize;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetRayTracingPipelineStackSizeKHR =
    unsafe extern "system" fn(command_buffer: CommandBuffer, pipeline_stack_size: u32);
#[derive(Clone)]
pub struct KhrRayTracingPipelineFn {
    pub cmd_trace_rays_khr: PFN_vkCmdTraceRaysKHR,
    pub create_ray_tracing_pipelines_khr: PFN_vkCreateRayTracingPipelinesKHR,
    pub get_ray_tracing_shader_group_handles_khr: PFN_vkGetRayTracingShaderGroupHandlesKHR,
    pub get_ray_tracing_capture_replay_shader_group_handles_khr:
        PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR,
    pub cmd_trace_rays_indirect_khr: PFN_vkCmdTraceRaysIndirectKHR,
    pub get_ray_tracing_shader_group_stack_size_khr: PFN_vkGetRayTracingShaderGroupStackSizeKHR,
    pub cmd_set_ray_tracing_pipeline_stack_size_khr: PFN_vkCmdSetRayTracingPipelineStackSizeKHR,
}
unsafe impl Send for KhrRayTracingPipelineFn {}
unsafe impl Sync for KhrRayTracingPipelineFn {}
impl KhrRayTracingPipelineFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrRayTracingPipelineFn {
            cmd_trace_rays_khr: unsafe {
                unsafe extern "system" fn cmd_trace_rays_khr(
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
                unsafe extern "system" fn create_ray_tracing_pipelines_khr(
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
                unsafe extern "system" fn get_ray_tracing_shader_group_handles_khr(
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
                unsafe extern "system" fn get_ray_tracing_capture_replay_shader_group_handles_khr(
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
                unsafe extern "system" fn cmd_trace_rays_indirect_khr(
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
                unsafe extern "system" fn get_ray_tracing_shader_group_stack_size_khr(
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
                unsafe extern "system" fn cmd_set_ray_tracing_pipeline_stack_size_khr(
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
#[derive(Clone)]
pub struct KhrRayQueryFn {}
unsafe impl Send for KhrRayQueryFn {}
unsafe impl Sync for KhrRayQueryFn {}
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
#[derive(Clone)]
pub struct NvExtension152Fn {}
unsafe impl Send for NvExtension152Fn {}
unsafe impl Sync for NvExtension152Fn {}
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
#[derive(Clone)]
pub struct NvFramebufferMixedSamplesFn {}
unsafe impl Send for NvFramebufferMixedSamplesFn {}
unsafe impl Sync for NvFramebufferMixedSamplesFn {}
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
#[derive(Clone)]
pub struct NvFillRectangleFn {}
unsafe impl Send for NvFillRectangleFn {}
unsafe impl Sync for NvFillRectangleFn {}
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
#[derive(Clone)]
pub struct NvShaderSmBuiltinsFn {}
unsafe impl Send for NvShaderSmBuiltinsFn {}
unsafe impl Sync for NvShaderSmBuiltinsFn {}
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
#[derive(Clone)]
pub struct ExtPostDepthCoverageFn {}
unsafe impl Send for ExtPostDepthCoverageFn {}
unsafe impl Sync for ExtPostDepthCoverageFn {}
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
pub type PFN_vkCreateSamplerYcbcrConversion = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const SamplerYcbcrConversionCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_ycbcr_conversion: *mut SamplerYcbcrConversion,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroySamplerYcbcrConversion = unsafe extern "system" fn(
    device: Device,
    ycbcr_conversion: SamplerYcbcrConversion,
    p_allocator: *const AllocationCallbacks,
);
#[derive(Clone)]
pub struct KhrSamplerYcbcrConversionFn {
    pub create_sampler_ycbcr_conversion_khr: PFN_vkCreateSamplerYcbcrConversion,
    pub destroy_sampler_ycbcr_conversion_khr: PFN_vkDestroySamplerYcbcrConversion,
}
unsafe impl Send for KhrSamplerYcbcrConversionFn {}
unsafe impl Sync for KhrSamplerYcbcrConversionFn {}
impl KhrSamplerYcbcrConversionFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSamplerYcbcrConversionFn {
            create_sampler_ycbcr_conversion_khr: unsafe {
                unsafe extern "system" fn create_sampler_ycbcr_conversion_khr(
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
                unsafe extern "system" fn destroy_sampler_ycbcr_conversion_khr(
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
        Self::SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const SAMPLER_YCBCR_CONVERSION_INFO_KHR: Self = Self::SAMPLER_YCBCR_CONVERSION_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const BIND_IMAGE_PLANE_MEMORY_INFO_KHR: Self = Self::BIND_IMAGE_PLANE_MEMORY_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO_KHR: Self =
        Self::IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES_KHR: Self =
        Self::PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl StructureType {
    pub const SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES_KHR: Self =
        Self::SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl DebugReportObjectTypeEXT {
    pub const SAMPLER_YCBCR_CONVERSION_KHR: Self = Self::SAMPLER_YCBCR_CONVERSION;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ObjectType {
    pub const SAMPLER_YCBCR_CONVERSION_KHR: Self = Self::SAMPLER_YCBCR_CONVERSION;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8B8G8R8_422_UNORM_KHR: Self = Self::G8B8G8R8_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B8G8R8G8_422_UNORM_KHR: Self = Self::B8G8R8G8_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8_R8_3PLANE_420_UNORM_KHR: Self = Self::G8_B8_R8_3PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8R8_2PLANE_420_UNORM_KHR: Self = Self::G8_B8R8_2PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8_R8_3PLANE_422_UNORM_KHR: Self = Self::G8_B8_R8_3PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8R8_2PLANE_422_UNORM_KHR: Self = Self::G8_B8R8_2PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G8_B8_R8_3PLANE_444_UNORM_KHR: Self = Self::G8_B8_R8_3PLANE_444_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R10X6_UNORM_PACK16_KHR: Self = Self::R10X6_UNORM_PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R10X6G10X6_UNORM_2PACK16_KHR: Self = Self::R10X6G10X6_UNORM_2PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR: Self =
        Self::R10X6G10X6B10X6A10X6_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR: Self =
        Self::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR: Self =
        Self::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR: Self =
        Self::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR: Self =
        Self::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR: Self =
        Self::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR: Self =
        Self::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR: Self =
        Self::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R12X4_UNORM_PACK16_KHR: Self = Self::R12X4_UNORM_PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R12X4G12X4_UNORM_2PACK16_KHR: Self = Self::R12X4G12X4_UNORM_2PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR: Self =
        Self::R12X4G12X4B12X4A12X4_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR: Self =
        Self::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR: Self =
        Self::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR: Self =
        Self::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR: Self =
        Self::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR: Self =
        Self::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR: Self =
        Self::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR: Self =
        Self::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16B16G16R16_422_UNORM_KHR: Self = Self::G16B16G16R16_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const B16G16R16G16_422_UNORM_KHR: Self = Self::B16G16R16G16_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16_R16_3PLANE_420_UNORM_KHR: Self = Self::G16_B16_R16_3PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16R16_2PLANE_420_UNORM_KHR: Self = Self::G16_B16R16_2PLANE_420_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16_R16_3PLANE_422_UNORM_KHR: Self = Self::G16_B16_R16_3PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16R16_2PLANE_422_UNORM_KHR: Self = Self::G16_B16R16_2PLANE_422_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl Format {
    pub const G16_B16_R16_3PLANE_444_UNORM_KHR: Self = Self::G16_B16_R16_3PLANE_444_UNORM;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageAspectFlags {
    pub const PLANE_0_KHR: Self = Self::PLANE_0;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageAspectFlags {
    pub const PLANE_1_KHR: Self = Self::PLANE_1;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageAspectFlags {
    pub const PLANE_2_KHR: Self = Self::PLANE_2;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ImageCreateFlags {
    pub const DISJOINT_KHR: Self = Self::DISJOINT;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const MIDPOINT_CHROMA_SAMPLES_KHR: Self = Self::MIDPOINT_CHROMA_SAMPLES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_KHR: Self =
        Self::SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_KHR: Self =
        Self::SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_KHR: Self =
        Self::SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_KHR: Self =
        Self::SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const DISJOINT_KHR: Self = Self::DISJOINT;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl FormatFeatureFlags {
    pub const COSITED_CHROMA_SAMPLES_KHR: Self = Self::COSITED_CHROMA_SAMPLES;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const RGB_IDENTITY_KHR: Self = Self::RGB_IDENTITY;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_IDENTITY_KHR: Self = Self::YCBCR_IDENTITY;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_709_KHR: Self = Self::YCBCR_709;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_601_KHR: Self = Self::YCBCR_601;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrModelConversion {
    pub const YCBCR_2020_KHR: Self = Self::YCBCR_2020;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrRange {
    pub const ITU_FULL_KHR: Self = Self::ITU_FULL;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl SamplerYcbcrRange {
    pub const ITU_NARROW_KHR: Self = Self::ITU_NARROW;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ChromaLocation {
    pub const COSITED_EVEN_KHR: Self = Self::COSITED_EVEN;
}
#[doc = "Generated from 'VK_KHR_sampler_ycbcr_conversion'"]
impl ChromaLocation {
    pub const MIDPOINT_KHR: Self = Self::MIDPOINT;
}
impl KhrBindMemory2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_bind_memory2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkBindBufferMemory2 = unsafe extern "system" fn(
    device: Device,
    bind_info_count: u32,
    p_bind_infos: *const BindBufferMemoryInfo,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkBindImageMemory2 = unsafe extern "system" fn(
    device: Device,
    bind_info_count: u32,
    p_bind_infos: *const BindImageMemoryInfo,
) -> Result;
#[derive(Clone)]
pub struct KhrBindMemory2Fn {
    pub bind_buffer_memory2_khr: PFN_vkBindBufferMemory2,
    pub bind_image_memory2_khr: PFN_vkBindImageMemory2,
}
unsafe impl Send for KhrBindMemory2Fn {}
unsafe impl Sync for KhrBindMemory2Fn {}
impl KhrBindMemory2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrBindMemory2Fn {
            bind_buffer_memory2_khr: unsafe {
                unsafe extern "system" fn bind_buffer_memory2_khr(
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
                unsafe extern "system" fn bind_image_memory2_khr(
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
    pub const BIND_BUFFER_MEMORY_INFO_KHR: Self = Self::BIND_BUFFER_MEMORY_INFO;
}
#[doc = "Generated from 'VK_KHR_bind_memory2'"]
impl StructureType {
    pub const BIND_IMAGE_MEMORY_INFO_KHR: Self = Self::BIND_IMAGE_MEMORY_INFO;
}
#[doc = "Generated from 'VK_KHR_bind_memory2'"]
impl ImageCreateFlags {
    pub const ALIAS_KHR: Self = Self::ALIAS;
}
impl ExtImageDrmFormatModifierFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_image_drm_format_modifier\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageDrmFormatModifierPropertiesEXT = unsafe extern "system" fn(
    device: Device,
    image: Image,
    p_properties: *mut ImageDrmFormatModifierPropertiesEXT,
) -> Result;
#[derive(Clone)]
pub struct ExtImageDrmFormatModifierFn {
    pub get_image_drm_format_modifier_properties_ext: PFN_vkGetImageDrmFormatModifierPropertiesEXT,
}
unsafe impl Send for ExtImageDrmFormatModifierFn {}
unsafe impl Sync for ExtImageDrmFormatModifierFn {}
impl ExtImageDrmFormatModifierFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtImageDrmFormatModifierFn {
            get_image_drm_format_modifier_properties_ext: unsafe {
                unsafe extern "system" fn get_image_drm_format_modifier_properties_ext(
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
#[derive(Clone)]
pub struct ExtExtension160Fn {}
unsafe impl Send for ExtExtension160Fn {}
unsafe impl Sync for ExtExtension160Fn {}
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
pub type PFN_vkCreateValidationCacheEXT = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const ValidationCacheCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_validation_cache: *mut ValidationCacheEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyValidationCacheEXT = unsafe extern "system" fn(
    device: Device,
    validation_cache: ValidationCacheEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkMergeValidationCachesEXT = unsafe extern "system" fn(
    device: Device,
    dst_cache: ValidationCacheEXT,
    src_cache_count: u32,
    p_src_caches: *const ValidationCacheEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetValidationCacheDataEXT = unsafe extern "system" fn(
    device: Device,
    validation_cache: ValidationCacheEXT,
    p_data_size: *mut usize,
    p_data: *mut c_void,
) -> Result;
#[derive(Clone)]
pub struct ExtValidationCacheFn {
    pub create_validation_cache_ext: PFN_vkCreateValidationCacheEXT,
    pub destroy_validation_cache_ext: PFN_vkDestroyValidationCacheEXT,
    pub merge_validation_caches_ext: PFN_vkMergeValidationCachesEXT,
    pub get_validation_cache_data_ext: PFN_vkGetValidationCacheDataEXT,
}
unsafe impl Send for ExtValidationCacheFn {}
unsafe impl Sync for ExtValidationCacheFn {}
impl ExtValidationCacheFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtValidationCacheFn {
            create_validation_cache_ext: unsafe {
                unsafe extern "system" fn create_validation_cache_ext(
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
                unsafe extern "system" fn destroy_validation_cache_ext(
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
                unsafe extern "system" fn merge_validation_caches_ext(
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
                unsafe extern "system" fn get_validation_cache_data_ext(
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
#[derive(Clone)]
pub struct ExtDescriptorIndexingFn {}
unsafe impl Send for ExtDescriptorIndexingFn {}
unsafe impl Sync for ExtDescriptorIndexingFn {}
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
        Self::DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: Self =
        Self::PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT: Self =
        Self::PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT: Self =
        Self::DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl StructureType {
    pub const DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT: Self =
        Self::DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const UPDATE_AFTER_BIND_EXT: Self = Self::UPDATE_AFTER_BIND;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const UPDATE_UNUSED_WHILE_PENDING_EXT: Self = Self::UPDATE_UNUSED_WHILE_PENDING;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const PARTIALLY_BOUND_EXT: Self = Self::PARTIALLY_BOUND;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorBindingFlags {
    pub const VARIABLE_DESCRIPTOR_COUNT_EXT: Self = Self::VARIABLE_DESCRIPTOR_COUNT;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorPoolCreateFlags {
    pub const UPDATE_AFTER_BIND_EXT: Self = Self::UPDATE_AFTER_BIND;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl DescriptorSetLayoutCreateFlags {
    pub const UPDATE_AFTER_BIND_POOL_EXT: Self = Self::UPDATE_AFTER_BIND_POOL;
}
#[doc = "Generated from 'VK_EXT_descriptor_indexing'"]
impl Result {
    pub const ERROR_FRAGMENTATION_EXT: Self = Self::ERROR_FRAGMENTATION;
}
impl ExtShaderViewportIndexLayerFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_viewport_index_layer\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtShaderViewportIndexLayerFn {}
unsafe impl Send for ExtShaderViewportIndexLayerFn {}
unsafe impl Sync for ExtShaderViewportIndexLayerFn {}
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
#[derive(Clone)]
pub struct KhrPortabilitySubsetFn {}
unsafe impl Send for KhrPortabilitySubsetFn {}
unsafe impl Sync for KhrPortabilitySubsetFn {}
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
pub type PFN_vkCmdBindShadingRateImageNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    image_view: ImageView,
    image_layout: ImageLayout,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetViewportShadingRatePaletteNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_viewport: u32,
    viewport_count: u32,
    p_shading_rate_palettes: *const ShadingRatePaletteNV,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetCoarseSampleOrderNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    sample_order_type: CoarseSampleOrderTypeNV,
    custom_sample_order_count: u32,
    p_custom_sample_orders: *const CoarseSampleOrderCustomNV,
);
#[derive(Clone)]
pub struct NvShadingRateImageFn {
    pub cmd_bind_shading_rate_image_nv: PFN_vkCmdBindShadingRateImageNV,
    pub cmd_set_viewport_shading_rate_palette_nv: PFN_vkCmdSetViewportShadingRatePaletteNV,
    pub cmd_set_coarse_sample_order_nv: PFN_vkCmdSetCoarseSampleOrderNV,
}
unsafe impl Send for NvShadingRateImageFn {}
unsafe impl Sync for NvShadingRateImageFn {}
impl NvShadingRateImageFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvShadingRateImageFn {
            cmd_bind_shading_rate_image_nv: unsafe {
                unsafe extern "system" fn cmd_bind_shading_rate_image_nv(
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
                unsafe extern "system" fn cmd_set_viewport_shading_rate_palette_nv(
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
                unsafe extern "system" fn cmd_set_coarse_sample_order_nv(
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
    pub const SHADING_RATE_OPTIMAL_NV: Self = Self::FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl DynamicState {
    pub const VIEWPORT_SHADING_RATE_PALETTE_NV: Self = Self(1_000_164_004);
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl AccessFlags {
    pub const SHADING_RATE_IMAGE_READ_NV: Self = Self::FRAGMENT_SHADING_RATE_ATTACHMENT_READ_KHR;
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl ImageUsageFlags {
    pub const SHADING_RATE_IMAGE_NV: Self = Self::FRAGMENT_SHADING_RATE_ATTACHMENT_KHR;
}
#[doc = "Generated from 'VK_NV_shading_rate_image'"]
impl PipelineStageFlags {
    pub const SHADING_RATE_IMAGE_NV: Self = Self::FRAGMENT_SHADING_RATE_ATTACHMENT_KHR;
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
pub type PFN_vkCreateAccelerationStructureNV = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const AccelerationStructureCreateInfoNV,
    p_allocator: *const AllocationCallbacks,
    p_acceleration_structure: *mut AccelerationStructureNV,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyAccelerationStructureNV = unsafe extern "system" fn(
    device: Device,
    acceleration_structure: AccelerationStructureNV,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureMemoryRequirementsNV = unsafe extern "system" fn(
    device: Device,
    p_info: *const AccelerationStructureMemoryRequirementsInfoNV,
    p_memory_requirements: *mut MemoryRequirements2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkBindAccelerationStructureMemoryNV = unsafe extern "system" fn(
    device: Device,
    bind_info_count: u32,
    p_bind_infos: *const BindAccelerationStructureMemoryInfoNV,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBuildAccelerationStructureNV = unsafe extern "system" fn(
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
pub type PFN_vkCmdCopyAccelerationStructureNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    dst: AccelerationStructureNV,
    src: AccelerationStructureNV,
    mode: CopyAccelerationStructureModeKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdTraceRaysNV = unsafe extern "system" fn(
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
pub type PFN_vkCreateRayTracingPipelinesNV = unsafe extern "system" fn(
    device: Device,
    pipeline_cache: PipelineCache,
    create_info_count: u32,
    p_create_infos: *const RayTracingPipelineCreateInfoNV,
    p_allocator: *const AllocationCallbacks,
    p_pipelines: *mut Pipeline,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetAccelerationStructureHandleNV = unsafe extern "system" fn(
    device: Device,
    acceleration_structure: AccelerationStructureNV,
    data_size: usize,
    p_data: *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteAccelerationStructuresPropertiesNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    acceleration_structure_count: u32,
    p_acceleration_structures: *const AccelerationStructureNV,
    query_type: QueryType,
    query_pool: QueryPool,
    first_query: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCompileDeferredNV =
    unsafe extern "system" fn(device: Device, pipeline: Pipeline, shader: u32) -> Result;
#[derive(Clone)]
pub struct NvRayTracingFn {
    pub create_acceleration_structure_nv: PFN_vkCreateAccelerationStructureNV,
    pub destroy_acceleration_structure_nv: PFN_vkDestroyAccelerationStructureNV,
    pub get_acceleration_structure_memory_requirements_nv:
        PFN_vkGetAccelerationStructureMemoryRequirementsNV,
    pub bind_acceleration_structure_memory_nv: PFN_vkBindAccelerationStructureMemoryNV,
    pub cmd_build_acceleration_structure_nv: PFN_vkCmdBuildAccelerationStructureNV,
    pub cmd_copy_acceleration_structure_nv: PFN_vkCmdCopyAccelerationStructureNV,
    pub cmd_trace_rays_nv: PFN_vkCmdTraceRaysNV,
    pub create_ray_tracing_pipelines_nv: PFN_vkCreateRayTracingPipelinesNV,
    pub get_ray_tracing_shader_group_handles_nv:
        crate::vk::PFN_vkGetRayTracingShaderGroupHandlesKHR,
    pub get_acceleration_structure_handle_nv: PFN_vkGetAccelerationStructureHandleNV,
    pub cmd_write_acceleration_structures_properties_nv:
        PFN_vkCmdWriteAccelerationStructuresPropertiesNV,
    pub compile_deferred_nv: PFN_vkCompileDeferredNV,
}
unsafe impl Send for NvRayTracingFn {}
unsafe impl Sync for NvRayTracingFn {}
impl NvRayTracingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvRayTracingFn {
            create_acceleration_structure_nv: unsafe {
                unsafe extern "system" fn create_acceleration_structure_nv(
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
                unsafe extern "system" fn destroy_acceleration_structure_nv(
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
                unsafe extern "system" fn get_acceleration_structure_memory_requirements_nv(
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
                unsafe extern "system" fn bind_acceleration_structure_memory_nv(
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
                unsafe extern "system" fn cmd_build_acceleration_structure_nv(
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
                unsafe extern "system" fn cmd_copy_acceleration_structure_nv(
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
                unsafe extern "system" fn cmd_trace_rays_nv(
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
                unsafe extern "system" fn create_ray_tracing_pipelines_nv(
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
                unsafe extern "system" fn get_ray_tracing_shader_group_handles_nv(
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
                unsafe extern "system" fn get_acceleration_structure_handle_nv(
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
                unsafe extern "system" fn cmd_write_acceleration_structures_properties_nv(
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
                unsafe extern "system" fn compile_deferred_nv(
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
    pub const RAYGEN_NV: Self = Self::RAYGEN_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const ANY_HIT_NV: Self = Self::ANY_HIT_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const CLOSEST_HIT_NV: Self = Self::CLOSEST_HIT_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const MISS_NV: Self = Self::MISS_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const INTERSECTION_NV: Self = Self::INTERSECTION_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl ShaderStageFlags {
    pub const CALLABLE_NV: Self = Self::CALLABLE_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl PipelineStageFlags {
    pub const RAY_TRACING_SHADER_NV: Self = Self::RAY_TRACING_SHADER_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl PipelineStageFlags {
    pub const ACCELERATION_STRUCTURE_BUILD_NV: Self = Self::ACCELERATION_STRUCTURE_BUILD_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BufferUsageFlags {
    pub const RAY_TRACING_NV: Self = Self::SHADER_BINDING_TABLE_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl PipelineBindPoint {
    pub const RAY_TRACING_NV: Self = Self::RAY_TRACING_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl DescriptorType {
    pub const ACCELERATION_STRUCTURE_NV: Self = Self(1_000_165_000);
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccessFlags {
    pub const ACCELERATION_STRUCTURE_READ_NV: Self = Self::ACCELERATION_STRUCTURE_READ_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccessFlags {
    pub const ACCELERATION_STRUCTURE_WRITE_NV: Self = Self::ACCELERATION_STRUCTURE_WRITE_KHR;
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
    pub const NONE_NV: Self = Self::NONE_KHR;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl RayTracingShaderGroupTypeKHR {
    pub const GENERAL_NV: Self = Self::GENERAL;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl RayTracingShaderGroupTypeKHR {
    pub const TRIANGLES_HIT_GROUP_NV: Self = Self::TRIANGLES_HIT_GROUP;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl RayTracingShaderGroupTypeKHR {
    pub const PROCEDURAL_HIT_GROUP_NV: Self = Self::PROCEDURAL_HIT_GROUP;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryTypeKHR {
    pub const TRIANGLES_NV: Self = Self::TRIANGLES;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryTypeKHR {
    pub const AABBS_NV: Self = Self::AABBS;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccelerationStructureTypeKHR {
    pub const TOP_LEVEL_NV: Self = Self::TOP_LEVEL;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl AccelerationStructureTypeKHR {
    pub const BOTTOM_LEVEL_NV: Self = Self::BOTTOM_LEVEL;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryFlagsKHR {
    pub const OPAQUE_NV: Self = Self::OPAQUE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryFlagsKHR {
    pub const NO_DUPLICATE_ANY_HIT_INVOCATION_NV: Self = Self::NO_DUPLICATE_ANY_HIT_INVOCATION;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const TRIANGLE_CULL_DISABLE_NV: Self = Self::TRIANGLE_FACING_CULL_DISABLE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const TRIANGLE_FRONT_COUNTERCLOCKWISE_NV: Self = Self::TRIANGLE_FRONT_COUNTERCLOCKWISE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const FORCE_OPAQUE_NV: Self = Self::FORCE_OPAQUE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl GeometryInstanceFlagsKHR {
    pub const FORCE_NO_OPAQUE_NV: Self = Self::FORCE_NO_OPAQUE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const ALLOW_UPDATE_NV: Self = Self::ALLOW_UPDATE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const ALLOW_COMPACTION_NV: Self = Self::ALLOW_COMPACTION;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const PREFER_FAST_TRACE_NV: Self = Self::PREFER_FAST_TRACE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const PREFER_FAST_BUILD_NV: Self = Self::PREFER_FAST_BUILD;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const LOW_MEMORY_NV: Self = Self::LOW_MEMORY;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl CopyAccelerationStructureModeKHR {
    pub const CLONE_NV: Self = Self::CLONE;
}
#[doc = "Generated from 'VK_NV_ray_tracing'"]
impl CopyAccelerationStructureModeKHR {
    pub const COMPACT_NV: Self = Self::COMPACT;
}
impl NvRepresentativeFragmentTestFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_representative_fragment_test\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[derive(Clone)]
pub struct NvRepresentativeFragmentTestFn {}
unsafe impl Send for NvRepresentativeFragmentTestFn {}
unsafe impl Sync for NvRepresentativeFragmentTestFn {}
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
#[derive(Clone)]
pub struct NvExtension168Fn {}
unsafe impl Send for NvExtension168Fn {}
unsafe impl Sync for NvExtension168Fn {}
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
pub type PFN_vkGetDescriptorSetLayoutSupport = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const DescriptorSetLayoutCreateInfo,
    p_support: *mut DescriptorSetLayoutSupport,
);
#[derive(Clone)]
pub struct KhrMaintenance3Fn {
    pub get_descriptor_set_layout_support_khr: PFN_vkGetDescriptorSetLayoutSupport,
}
unsafe impl Send for KhrMaintenance3Fn {}
unsafe impl Sync for KhrMaintenance3Fn {}
impl KhrMaintenance3Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrMaintenance3Fn {
            get_descriptor_set_layout_support_khr: unsafe {
                unsafe extern "system" fn get_descriptor_set_layout_support_khr(
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
        Self::PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_maintenance3'"]
impl StructureType {
    pub const DESCRIPTOR_SET_LAYOUT_SUPPORT_KHR: Self = Self::DESCRIPTOR_SET_LAYOUT_SUPPORT;
}
impl KhrDrawIndirectCountFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_draw_indirect_count\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrDrawIndirectCountFn {
    pub cmd_draw_indirect_count_khr: crate::vk::PFN_vkCmdDrawIndirectCount,
    pub cmd_draw_indexed_indirect_count_khr: crate::vk::PFN_vkCmdDrawIndexedIndirectCount,
}
unsafe impl Send for KhrDrawIndirectCountFn {}
unsafe impl Sync for KhrDrawIndirectCountFn {}
impl KhrDrawIndirectCountFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDrawIndirectCountFn {
            cmd_draw_indirect_count_khr: unsafe {
                unsafe extern "system" fn cmd_draw_indirect_count_khr(
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
                unsafe extern "system" fn cmd_draw_indexed_indirect_count_khr(
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
#[derive(Clone)]
pub struct ExtFilterCubicFn {}
unsafe impl Send for ExtFilterCubicFn {}
unsafe impl Sync for ExtFilterCubicFn {}
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
    pub const CUBIC_EXT: Self = Self::CUBIC_IMG;
}
#[doc = "Generated from 'VK_EXT_filter_cubic'"]
impl FormatFeatureFlags {
    pub const SAMPLED_IMAGE_FILTER_CUBIC_EXT: Self = Self::SAMPLED_IMAGE_FILTER_CUBIC_IMG;
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
#[derive(Clone)]
pub struct QcomRenderPassShaderResolveFn {}
unsafe impl Send for QcomRenderPassShaderResolveFn {}
unsafe impl Sync for QcomRenderPassShaderResolveFn {}
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
#[derive(Clone)]
pub struct QcomExtension173Fn {}
unsafe impl Send for QcomExtension173Fn {}
unsafe impl Sync for QcomExtension173Fn {}
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
#[derive(Clone)]
pub struct QcomExtension174Fn {}
unsafe impl Send for QcomExtension174Fn {}
unsafe impl Sync for QcomExtension174Fn {}
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
#[derive(Clone)]
pub struct ExtGlobalPriorityFn {}
unsafe impl Send for ExtGlobalPriorityFn {}
unsafe impl Sync for ExtGlobalPriorityFn {}
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
#[derive(Clone)]
pub struct KhrShaderSubgroupExtendedTypesFn {}
unsafe impl Send for KhrShaderSubgroupExtendedTypesFn {}
unsafe impl Sync for KhrShaderSubgroupExtendedTypesFn {}
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
        Self::PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES;
}
impl ExtExtension177Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_177\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension177Fn {}
unsafe impl Send for ExtExtension177Fn {}
unsafe impl Sync for ExtExtension177Fn {}
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
#[derive(Clone)]
pub struct Khr8bitStorageFn {}
unsafe impl Send for Khr8bitStorageFn {}
unsafe impl Sync for Khr8bitStorageFn {}
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
        Self::PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
}
impl ExtExternalMemoryHostFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_external_memory_host\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryHostPointerPropertiesEXT = unsafe extern "system" fn(
    device: Device,
    handle_type: ExternalMemoryHandleTypeFlags,
    p_host_pointer: *const c_void,
    p_memory_host_pointer_properties: *mut MemoryHostPointerPropertiesEXT,
) -> Result;
#[derive(Clone)]
pub struct ExtExternalMemoryHostFn {
    pub get_memory_host_pointer_properties_ext: PFN_vkGetMemoryHostPointerPropertiesEXT,
}
unsafe impl Send for ExtExternalMemoryHostFn {}
unsafe impl Sync for ExtExternalMemoryHostFn {}
impl ExtExternalMemoryHostFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExternalMemoryHostFn {
            get_memory_host_pointer_properties_ext: unsafe {
                unsafe extern "system" fn get_memory_host_pointer_properties_ext(
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
pub type PFN_vkCmdWriteBufferMarkerAMD = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_stage: PipelineStageFlags,
    dst_buffer: Buffer,
    dst_offset: DeviceSize,
    marker: u32,
);
#[derive(Clone)]
pub struct AmdBufferMarkerFn {
    pub cmd_write_buffer_marker_amd: PFN_vkCmdWriteBufferMarkerAMD,
}
unsafe impl Send for AmdBufferMarkerFn {}
unsafe impl Sync for AmdBufferMarkerFn {}
impl AmdBufferMarkerFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdBufferMarkerFn {
            cmd_write_buffer_marker_amd: unsafe {
                unsafe extern "system" fn cmd_write_buffer_marker_amd(
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
#[derive(Clone)]
pub struct KhrShaderAtomicInt64Fn {}
unsafe impl Send for KhrShaderAtomicInt64Fn {}
unsafe impl Sync for KhrShaderAtomicInt64Fn {}
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
        Self::PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES;
}
impl KhrShaderClockFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_clock\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrShaderClockFn {}
unsafe impl Send for KhrShaderClockFn {}
unsafe impl Sync for KhrShaderClockFn {}
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
#[derive(Clone)]
pub struct AmdExtension183Fn {}
unsafe impl Send for AmdExtension183Fn {}
unsafe impl Sync for AmdExtension183Fn {}
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
#[derive(Clone)]
pub struct AmdPipelineCompilerControlFn {}
unsafe impl Send for AmdPipelineCompilerControlFn {}
unsafe impl Sync for AmdPipelineCompilerControlFn {}
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
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_time_domain_count: *mut u32,
    p_time_domains: *mut TimeDomainEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetCalibratedTimestampsEXT = unsafe extern "system" fn(
    device: Device,
    timestamp_count: u32,
    p_timestamp_infos: *const CalibratedTimestampInfoEXT,
    p_timestamps: *mut u64,
    p_max_deviation: *mut u64,
) -> Result;
#[derive(Clone)]
pub struct ExtCalibratedTimestampsFn {
    pub get_physical_device_calibrateable_time_domains_ext:
        PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
    pub get_calibrated_timestamps_ext: PFN_vkGetCalibratedTimestampsEXT,
}
unsafe impl Send for ExtCalibratedTimestampsFn {}
unsafe impl Sync for ExtCalibratedTimestampsFn {}
impl ExtCalibratedTimestampsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtCalibratedTimestampsFn {
            get_physical_device_calibrateable_time_domains_ext: unsafe {
                unsafe extern "system" fn get_physical_device_calibrateable_time_domains_ext(
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
                unsafe extern "system" fn get_calibrated_timestamps_ext(
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
#[derive(Clone)]
pub struct AmdShaderCorePropertiesFn {}
unsafe impl Send for AmdShaderCorePropertiesFn {}
unsafe impl Sync for AmdShaderCorePropertiesFn {}
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
#[derive(Clone)]
pub struct AmdExtension187Fn {}
unsafe impl Send for AmdExtension187Fn {}
unsafe impl Sync for AmdExtension187Fn {}
impl AmdExtension187Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension187Fn {}
    }
}
impl ExtVideoDecodeH265Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_video_decode_h265\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtVideoDecodeH265Fn {}
unsafe impl Send for ExtVideoDecodeH265Fn {}
unsafe impl Sync for ExtVideoDecodeH265Fn {}
impl ExtVideoDecodeH265Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVideoDecodeH265Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl StructureType {
    pub const VIDEO_DECODE_H265_CAPABILITIES_EXT: Self = Self(1_000_187_000);
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl StructureType {
    pub const VIDEO_DECODE_H265_SESSION_CREATE_INFO_EXT: Self = Self(1_000_187_001);
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl StructureType {
    pub const VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_EXT: Self = Self(1_000_187_002);
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl StructureType {
    pub const VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_EXT: Self = Self(1_000_187_003);
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl StructureType {
    pub const VIDEO_DECODE_H265_PROFILE_EXT: Self = Self(1_000_187_004);
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl StructureType {
    pub const VIDEO_DECODE_H265_PICTURE_INFO_EXT: Self = Self(1_000_187_005);
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl StructureType {
    pub const VIDEO_DECODE_H265_DPB_SLOT_INFO_EXT: Self = Self(1_000_187_006);
}
#[doc = "Generated from 'VK_EXT_video_decode_h265'"]
impl VideoCodecOperationFlagsKHR {
    pub const DECODE_H265_EXT: Self = Self(0b10);
}
impl AmdExtension189Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_189\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct AmdExtension189Fn {}
unsafe impl Send for AmdExtension189Fn {}
unsafe impl Sync for AmdExtension189Fn {}
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
#[derive(Clone)]
pub struct AmdMemoryOverallocationBehaviorFn {}
unsafe impl Send for AmdMemoryOverallocationBehaviorFn {}
unsafe impl Sync for AmdMemoryOverallocationBehaviorFn {}
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
#[derive(Clone)]
pub struct ExtVertexAttributeDivisorFn {}
unsafe impl Send for ExtVertexAttributeDivisorFn {}
unsafe impl Sync for ExtVertexAttributeDivisorFn {}
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
#[derive(Clone)]
pub struct GgpFrameTokenFn {}
unsafe impl Send for GgpFrameTokenFn {}
unsafe impl Sync for GgpFrameTokenFn {}
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
#[derive(Clone)]
pub struct ExtPipelineCreationFeedbackFn {}
unsafe impl Send for ExtPipelineCreationFeedbackFn {}
unsafe impl Sync for ExtPipelineCreationFeedbackFn {}
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
#[derive(Clone)]
pub struct GoogleExtension194Fn {}
unsafe impl Send for GoogleExtension194Fn {}
unsafe impl Sync for GoogleExtension194Fn {}
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
#[derive(Clone)]
pub struct GoogleExtension195Fn {}
unsafe impl Send for GoogleExtension195Fn {}
unsafe impl Sync for GoogleExtension195Fn {}
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
#[derive(Clone)]
pub struct GoogleExtension196Fn {}
unsafe impl Send for GoogleExtension196Fn {}
unsafe impl Sync for GoogleExtension196Fn {}
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
#[derive(Clone)]
pub struct KhrDriverPropertiesFn {}
unsafe impl Send for KhrDriverPropertiesFn {}
unsafe impl Sync for KhrDriverPropertiesFn {}
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
    pub const PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR: Self = Self::PHYSICAL_DEVICE_DRIVER_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const AMD_PROPRIETARY_KHR: Self = Self::AMD_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const AMD_OPEN_SOURCE_KHR: Self = Self::AMD_OPEN_SOURCE;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const MESA_RADV_KHR: Self = Self::MESA_RADV;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const NVIDIA_PROPRIETARY_KHR: Self = Self::NVIDIA_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const INTEL_PROPRIETARY_WINDOWS_KHR: Self = Self::INTEL_PROPRIETARY_WINDOWS;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const INTEL_OPEN_SOURCE_MESA_KHR: Self = Self::INTEL_OPEN_SOURCE_MESA;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const IMAGINATION_PROPRIETARY_KHR: Self = Self::IMAGINATION_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const QUALCOMM_PROPRIETARY_KHR: Self = Self::QUALCOMM_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const ARM_PROPRIETARY_KHR: Self = Self::ARM_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const GOOGLE_SWIFTSHADER_KHR: Self = Self::GOOGLE_SWIFTSHADER;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const GGP_PROPRIETARY_KHR: Self = Self::GGP_PROPRIETARY;
}
#[doc = "Generated from 'VK_KHR_driver_properties'"]
impl DriverId {
    pub const BROADCOM_PROPRIETARY_KHR: Self = Self::BROADCOM_PROPRIETARY;
}
impl KhrShaderFloatControlsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_float_controls\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
#[derive(Clone)]
pub struct KhrShaderFloatControlsFn {}
unsafe impl Send for KhrShaderFloatControlsFn {}
unsafe impl Sync for KhrShaderFloatControlsFn {}
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
        Self::PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_shader_float_controls'"]
impl ShaderFloatControlsIndependence {
    pub const TYPE_32_ONLY_KHR: Self = Self::TYPE_32_ONLY;
}
#[doc = "Generated from 'VK_KHR_shader_float_controls'"]
impl ShaderFloatControlsIndependence {
    pub const ALL_KHR: Self = Self::ALL;
}
#[doc = "Generated from 'VK_KHR_shader_float_controls'"]
impl ShaderFloatControlsIndependence {
    pub const NONE_KHR: Self = Self::NONE;
}
impl NvShaderSubgroupPartitionedFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_shader_subgroup_partitioned\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct NvShaderSubgroupPartitionedFn {}
unsafe impl Send for NvShaderSubgroupPartitionedFn {}
unsafe impl Sync for NvShaderSubgroupPartitionedFn {}
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
#[derive(Clone)]
pub struct KhrDepthStencilResolveFn {}
unsafe impl Send for KhrDepthStencilResolveFn {}
unsafe impl Sync for KhrDepthStencilResolveFn {}
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
        Self::PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl StructureType {
    pub const SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR: Self =
        Self::SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const NONE_KHR: Self = Self::NONE;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const SAMPLE_ZERO_KHR: Self = Self::SAMPLE_ZERO;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const AVERAGE_KHR: Self = Self::AVERAGE;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const MIN_KHR: Self = Self::MIN;
}
#[doc = "Generated from 'VK_KHR_depth_stencil_resolve'"]
impl ResolveModeFlags {
    pub const MAX_KHR: Self = Self::MAX;
}
impl KhrSwapchainMutableFormatFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_swapchain_mutable_format\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrSwapchainMutableFormatFn {}
unsafe impl Send for KhrSwapchainMutableFormatFn {}
unsafe impl Sync for KhrSwapchainMutableFormatFn {}
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
#[derive(Clone)]
pub struct NvComputeShaderDerivativesFn {}
unsafe impl Send for NvComputeShaderDerivativesFn {}
unsafe impl Sync for NvComputeShaderDerivativesFn {}
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
    unsafe extern "system" fn(command_buffer: CommandBuffer, task_count: u32, first_task: u32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawMeshTasksIndirectNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    draw_count: u32,
    stride: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawMeshTasksIndirectCountNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    count_buffer: Buffer,
    count_buffer_offset: DeviceSize,
    max_draw_count: u32,
    stride: u32,
);
#[derive(Clone)]
pub struct NvMeshShaderFn {
    pub cmd_draw_mesh_tasks_nv: PFN_vkCmdDrawMeshTasksNV,
    pub cmd_draw_mesh_tasks_indirect_nv: PFN_vkCmdDrawMeshTasksIndirectNV,
    pub cmd_draw_mesh_tasks_indirect_count_nv: PFN_vkCmdDrawMeshTasksIndirectCountNV,
}
unsafe impl Send for NvMeshShaderFn {}
unsafe impl Sync for NvMeshShaderFn {}
impl NvMeshShaderFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvMeshShaderFn {
            cmd_draw_mesh_tasks_nv: unsafe {
                unsafe extern "system" fn cmd_draw_mesh_tasks_nv(
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
                unsafe extern "system" fn cmd_draw_mesh_tasks_indirect_nv(
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
                unsafe extern "system" fn cmd_draw_mesh_tasks_indirect_count_nv(
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
#[derive(Clone)]
pub struct NvFragmentShaderBarycentricFn {}
unsafe impl Send for NvFragmentShaderBarycentricFn {}
unsafe impl Sync for NvFragmentShaderBarycentricFn {}
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
#[derive(Clone)]
pub struct NvShaderImageFootprintFn {}
unsafe impl Send for NvShaderImageFootprintFn {}
unsafe impl Sync for NvShaderImageFootprintFn {}
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
pub type PFN_vkCmdSetExclusiveScissorNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_exclusive_scissor: u32,
    exclusive_scissor_count: u32,
    p_exclusive_scissors: *const Rect2D,
);
#[derive(Clone)]
pub struct NvScissorExclusiveFn {
    pub cmd_set_exclusive_scissor_nv: PFN_vkCmdSetExclusiveScissorNV,
}
unsafe impl Send for NvScissorExclusiveFn {}
unsafe impl Sync for NvScissorExclusiveFn {}
impl NvScissorExclusiveFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvScissorExclusiveFn {
            cmd_set_exclusive_scissor_nv: unsafe {
                unsafe extern "system" fn cmd_set_exclusive_scissor_nv(
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
    unsafe extern "system" fn(command_buffer: CommandBuffer, p_checkpoint_marker: *const c_void);
#[allow(non_camel_case_types)]
pub type PFN_vkGetQueueCheckpointDataNV = unsafe extern "system" fn(
    queue: Queue,
    p_checkpoint_data_count: *mut u32,
    p_checkpoint_data: *mut CheckpointDataNV,
);
#[derive(Clone)]
pub struct NvDeviceDiagnosticCheckpointsFn {
    pub cmd_set_checkpoint_nv: PFN_vkCmdSetCheckpointNV,
    pub get_queue_checkpoint_data_nv: PFN_vkGetQueueCheckpointDataNV,
}
unsafe impl Send for NvDeviceDiagnosticCheckpointsFn {}
unsafe impl Sync for NvDeviceDiagnosticCheckpointsFn {}
impl NvDeviceDiagnosticCheckpointsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvDeviceDiagnosticCheckpointsFn {
            cmd_set_checkpoint_nv: unsafe {
                unsafe extern "system" fn cmd_set_checkpoint_nv(
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
                unsafe extern "system" fn get_queue_checkpoint_data_nv(
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
    unsafe extern "system" fn(device: Device, semaphore: Semaphore, p_value: *mut u64) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkWaitSemaphores = unsafe extern "system" fn(
    device: Device,
    p_wait_info: *const SemaphoreWaitInfo,
    timeout: u64,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkSignalSemaphore =
    unsafe extern "system" fn(device: Device, p_signal_info: *const SemaphoreSignalInfo) -> Result;
#[derive(Clone)]
pub struct KhrTimelineSemaphoreFn {
    pub get_semaphore_counter_value_khr: PFN_vkGetSemaphoreCounterValue,
    pub wait_semaphores_khr: PFN_vkWaitSemaphores,
    pub signal_semaphore_khr: PFN_vkSignalSemaphore,
}
unsafe impl Send for KhrTimelineSemaphoreFn {}
unsafe impl Sync for KhrTimelineSemaphoreFn {}
impl KhrTimelineSemaphoreFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrTimelineSemaphoreFn {
            get_semaphore_counter_value_khr: unsafe {
                unsafe extern "system" fn get_semaphore_counter_value_khr(
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
                unsafe extern "system" fn wait_semaphores_khr(
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
                unsafe extern "system" fn signal_semaphore_khr(
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
        Self::PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES_KHR: Self =
        Self::PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const SEMAPHORE_TYPE_CREATE_INFO_KHR: Self = Self::SEMAPHORE_TYPE_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR: Self = Self::TIMELINE_SEMAPHORE_SUBMIT_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const SEMAPHORE_WAIT_INFO_KHR: Self = Self::SEMAPHORE_WAIT_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl StructureType {
    pub const SEMAPHORE_SIGNAL_INFO_KHR: Self = Self::SEMAPHORE_SIGNAL_INFO;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl SemaphoreType {
    pub const BINARY_KHR: Self = Self::BINARY;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl SemaphoreType {
    pub const TIMELINE_KHR: Self = Self::TIMELINE;
}
#[doc = "Generated from 'VK_KHR_timeline_semaphore'"]
impl SemaphoreWaitFlags {
    pub const ANY_KHR: Self = Self::ANY;
}
impl KhrExtension209Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_209\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension209Fn {}
unsafe impl Send for KhrExtension209Fn {}
unsafe impl Sync for KhrExtension209Fn {}
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
#[derive(Clone)]
pub struct IntelShaderIntegerFunctions2Fn {}
unsafe impl Send for IntelShaderIntegerFunctions2Fn {}
unsafe impl Sync for IntelShaderIntegerFunctions2Fn {}
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
pub type PFN_vkInitializePerformanceApiINTEL = unsafe extern "system" fn(
    device: Device,
    p_initialize_info: *const InitializePerformanceApiInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkUninitializePerformanceApiINTEL = unsafe extern "system" fn(device: Device);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPerformanceMarkerINTEL = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const PerformanceMarkerInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPerformanceStreamMarkerINTEL = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_marker_info: *const PerformanceStreamMarkerInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPerformanceOverrideINTEL = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_override_info: *const PerformanceOverrideInfoINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquirePerformanceConfigurationINTEL = unsafe extern "system" fn(
    device: Device,
    p_acquire_info: *const PerformanceConfigurationAcquireInfoINTEL,
    p_configuration: *mut PerformanceConfigurationINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkReleasePerformanceConfigurationINTEL = unsafe extern "system" fn(
    device: Device,
    configuration: PerformanceConfigurationINTEL,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueueSetPerformanceConfigurationINTEL =
    unsafe extern "system" fn(queue: Queue, configuration: PerformanceConfigurationINTEL) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPerformanceParameterINTEL = unsafe extern "system" fn(
    device: Device,
    parameter: PerformanceParameterTypeINTEL,
    p_value: *mut PerformanceValueINTEL,
) -> Result;
#[derive(Clone)]
pub struct IntelPerformanceQueryFn {
    pub initialize_performance_api_intel: PFN_vkInitializePerformanceApiINTEL,
    pub uninitialize_performance_api_intel: PFN_vkUninitializePerformanceApiINTEL,
    pub cmd_set_performance_marker_intel: PFN_vkCmdSetPerformanceMarkerINTEL,
    pub cmd_set_performance_stream_marker_intel: PFN_vkCmdSetPerformanceStreamMarkerINTEL,
    pub cmd_set_performance_override_intel: PFN_vkCmdSetPerformanceOverrideINTEL,
    pub acquire_performance_configuration_intel: PFN_vkAcquirePerformanceConfigurationINTEL,
    pub release_performance_configuration_intel: PFN_vkReleasePerformanceConfigurationINTEL,
    pub queue_set_performance_configuration_intel: PFN_vkQueueSetPerformanceConfigurationINTEL,
    pub get_performance_parameter_intel: PFN_vkGetPerformanceParameterINTEL,
}
unsafe impl Send for IntelPerformanceQueryFn {}
unsafe impl Sync for IntelPerformanceQueryFn {}
impl IntelPerformanceQueryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelPerformanceQueryFn {
            initialize_performance_api_intel: unsafe {
                unsafe extern "system" fn initialize_performance_api_intel(
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
                unsafe extern "system" fn uninitialize_performance_api_intel(_device: Device) {
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
                unsafe extern "system" fn cmd_set_performance_marker_intel(
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
                unsafe extern "system" fn cmd_set_performance_stream_marker_intel(
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
                unsafe extern "system" fn cmd_set_performance_override_intel(
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
                unsafe extern "system" fn acquire_performance_configuration_intel(
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
                unsafe extern "system" fn release_performance_configuration_intel(
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
                unsafe extern "system" fn queue_set_performance_configuration_intel(
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
                unsafe extern "system" fn get_performance_parameter_intel(
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
        Self::QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL;
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
#[derive(Clone)]
pub struct KhrVulkanMemoryModelFn {}
unsafe impl Send for KhrVulkanMemoryModelFn {}
unsafe impl Sync for KhrVulkanMemoryModelFn {}
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
        Self::PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
}
impl ExtPciBusInfoFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_pci_bus_info\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[derive(Clone)]
pub struct ExtPciBusInfoFn {}
unsafe impl Send for ExtPciBusInfoFn {}
unsafe impl Sync for ExtPciBusInfoFn {}
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
pub type PFN_vkSetLocalDimmingAMD = unsafe extern "system" fn(
    device: Device,
    swap_chain: SwapchainKHR,
    local_dimming_enable: Bool32,
);
#[derive(Clone)]
pub struct AmdDisplayNativeHdrFn {
    pub set_local_dimming_amd: PFN_vkSetLocalDimmingAMD,
}
unsafe impl Send for AmdDisplayNativeHdrFn {}
unsafe impl Sync for AmdDisplayNativeHdrFn {}
impl AmdDisplayNativeHdrFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdDisplayNativeHdrFn {
            set_local_dimming_amd: unsafe {
                unsafe extern "system" fn set_local_dimming_amd(
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
pub type PFN_vkCreateImagePipeSurfaceFUCHSIA = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const ImagePipeSurfaceCreateInfoFUCHSIA,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct FuchsiaImagepipeSurfaceFn {
    pub create_image_pipe_surface_fuchsia: PFN_vkCreateImagePipeSurfaceFUCHSIA,
}
unsafe impl Send for FuchsiaImagepipeSurfaceFn {}
unsafe impl Sync for FuchsiaImagepipeSurfaceFn {}
impl FuchsiaImagepipeSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaImagepipeSurfaceFn {
            create_image_pipe_surface_fuchsia: unsafe {
                unsafe extern "system" fn create_image_pipe_surface_fuchsia(
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
#[derive(Clone)]
pub struct KhrShaderTerminateInvocationFn {}
unsafe impl Send for KhrShaderTerminateInvocationFn {}
unsafe impl Sync for KhrShaderTerminateInvocationFn {}
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
#[derive(Clone)]
pub struct GoogleExtension217Fn {}
unsafe impl Send for GoogleExtension217Fn {}
unsafe impl Sync for GoogleExtension217Fn {}
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
pub type PFN_vkCreateMetalSurfaceEXT = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const MetalSurfaceCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct ExtMetalSurfaceFn {
    pub create_metal_surface_ext: PFN_vkCreateMetalSurfaceEXT,
}
unsafe impl Send for ExtMetalSurfaceFn {}
unsafe impl Sync for ExtMetalSurfaceFn {}
impl ExtMetalSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtMetalSurfaceFn {
            create_metal_surface_ext: unsafe {
                unsafe extern "system" fn create_metal_surface_ext(
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
#[derive(Clone)]
pub struct ExtFragmentDensityMapFn {}
unsafe impl Send for ExtFragmentDensityMapFn {}
unsafe impl Sync for ExtFragmentDensityMapFn {}
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
#[derive(Clone)]
pub struct ExtExtension220Fn {}
unsafe impl Send for ExtExtension220Fn {}
unsafe impl Sync for ExtExtension220Fn {}
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
#[derive(Clone)]
pub struct KhrExtension221Fn {}
unsafe impl Send for KhrExtension221Fn {}
unsafe impl Sync for KhrExtension221Fn {}
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
#[derive(Clone)]
pub struct ExtScalarBlockLayoutFn {}
unsafe impl Send for ExtScalarBlockLayoutFn {}
unsafe impl Sync for ExtScalarBlockLayoutFn {}
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
        Self::PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
}
impl ExtExtension223Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_223\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension223Fn {}
unsafe impl Send for ExtExtension223Fn {}
unsafe impl Sync for ExtExtension223Fn {}
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
#[derive(Clone)]
pub struct GoogleHlslFunctionality1Fn {}
unsafe impl Send for GoogleHlslFunctionality1Fn {}
unsafe impl Sync for GoogleHlslFunctionality1Fn {}
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
#[derive(Clone)]
pub struct GoogleDecorateStringFn {}
unsafe impl Send for GoogleDecorateStringFn {}
unsafe impl Sync for GoogleDecorateStringFn {}
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
#[derive(Clone)]
pub struct ExtSubgroupSizeControlFn {}
unsafe impl Send for ExtSubgroupSizeControlFn {}
unsafe impl Sync for ExtSubgroupSizeControlFn {}
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
pub type PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_fragment_shading_rate_count: *mut u32,
    p_fragment_shading_rates: *mut PhysicalDeviceFragmentShadingRateKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetFragmentShadingRateKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_fragment_size: *const Extent2D,
    combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
);
#[derive(Clone)]
pub struct KhrFragmentShadingRateFn {
    pub get_physical_device_fragment_shading_rates_khr:
        PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR,
    pub cmd_set_fragment_shading_rate_khr: PFN_vkCmdSetFragmentShadingRateKHR,
}
unsafe impl Send for KhrFragmentShadingRateFn {}
unsafe impl Sync for KhrFragmentShadingRateFn {}
impl KhrFragmentShadingRateFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrFragmentShadingRateFn {
            get_physical_device_fragment_shading_rates_khr: unsafe {
                unsafe extern "system" fn get_physical_device_fragment_shading_rates_khr(
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
                unsafe extern "system" fn cmd_set_fragment_shading_rate_khr(
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
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR: Self = Self(1_000_164_003);
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
        Self(0b1000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl ImageUsageFlags {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_KHR: Self = Self(0b1_0000_0000);
}
#[doc = "Generated from 'VK_KHR_fragment_shading_rate'"]
impl PipelineStageFlags {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_KHR: Self = Self(0b100_0000_0000_0000_0000_0000);
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
#[derive(Clone)]
pub struct AmdShaderCoreProperties2Fn {}
unsafe impl Send for AmdShaderCoreProperties2Fn {}
unsafe impl Sync for AmdShaderCoreProperties2Fn {}
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
#[derive(Clone)]
pub struct AmdExtension229Fn {}
unsafe impl Send for AmdExtension229Fn {}
unsafe impl Sync for AmdExtension229Fn {}
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
#[derive(Clone)]
pub struct AmdDeviceCoherentMemoryFn {}
unsafe impl Send for AmdDeviceCoherentMemoryFn {}
unsafe impl Sync for AmdDeviceCoherentMemoryFn {}
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
#[derive(Clone)]
pub struct AmdExtension231Fn {}
unsafe impl Send for AmdExtension231Fn {}
unsafe impl Sync for AmdExtension231Fn {}
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
#[derive(Clone)]
pub struct AmdExtension232Fn {}
unsafe impl Send for AmdExtension232Fn {}
unsafe impl Sync for AmdExtension232Fn {}
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
#[derive(Clone)]
pub struct AmdExtension233Fn {}
unsafe impl Send for AmdExtension233Fn {}
unsafe impl Sync for AmdExtension233Fn {}
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
#[derive(Clone)]
pub struct AmdExtension234Fn {}
unsafe impl Send for AmdExtension234Fn {}
unsafe impl Sync for AmdExtension234Fn {}
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
#[derive(Clone)]
pub struct ExtShaderImageAtomicInt64Fn {}
unsafe impl Send for ExtShaderImageAtomicInt64Fn {}
unsafe impl Sync for ExtShaderImageAtomicInt64Fn {}
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
#[derive(Clone)]
pub struct AmdExtension236Fn {}
unsafe impl Send for AmdExtension236Fn {}
unsafe impl Sync for AmdExtension236Fn {}
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
#[derive(Clone)]
pub struct KhrSpirv14Fn {}
unsafe impl Send for KhrSpirv14Fn {}
unsafe impl Sync for KhrSpirv14Fn {}
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
#[derive(Clone)]
pub struct ExtMemoryBudgetFn {}
unsafe impl Send for ExtMemoryBudgetFn {}
unsafe impl Sync for ExtMemoryBudgetFn {}
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
#[derive(Clone)]
pub struct ExtMemoryPriorityFn {}
unsafe impl Send for ExtMemoryPriorityFn {}
unsafe impl Sync for ExtMemoryPriorityFn {}
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
#[derive(Clone)]
pub struct KhrSurfaceProtectedCapabilitiesFn {}
unsafe impl Send for KhrSurfaceProtectedCapabilitiesFn {}
unsafe impl Sync for KhrSurfaceProtectedCapabilitiesFn {}
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
#[derive(Clone)]
pub struct NvDedicatedAllocationImageAliasingFn {}
unsafe impl Send for NvDedicatedAllocationImageAliasingFn {}
unsafe impl Sync for NvDedicatedAllocationImageAliasingFn {}
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
#[derive(Clone)]
pub struct KhrSeparateDepthStencilLayoutsFn {}
unsafe impl Send for KhrSeparateDepthStencilLayoutsFn {}
unsafe impl Sync for KhrSeparateDepthStencilLayoutsFn {}
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
        Self::PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl StructureType {
    pub const ATTACHMENT_REFERENCE_STENCIL_LAYOUT_KHR: Self =
        Self::ATTACHMENT_REFERENCE_STENCIL_LAYOUT;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl StructureType {
    pub const ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT_KHR: Self =
        Self::ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const DEPTH_ATTACHMENT_OPTIMAL_KHR: Self = Self::DEPTH_ATTACHMENT_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const DEPTH_READ_ONLY_OPTIMAL_KHR: Self = Self::DEPTH_READ_ONLY_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const STENCIL_ATTACHMENT_OPTIMAL_KHR: Self = Self::STENCIL_ATTACHMENT_OPTIMAL;
}
#[doc = "Generated from 'VK_KHR_separate_depth_stencil_layouts'"]
impl ImageLayout {
    pub const STENCIL_READ_ONLY_OPTIMAL_KHR: Self = Self::STENCIL_READ_ONLY_OPTIMAL;
}
impl IntelExtension243Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_INTEL_extension_243\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct IntelExtension243Fn {}
unsafe impl Send for IntelExtension243Fn {}
unsafe impl Sync for IntelExtension243Fn {}
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
#[derive(Clone)]
pub struct MesaExtension244Fn {}
unsafe impl Send for MesaExtension244Fn {}
unsafe impl Sync for MesaExtension244Fn {}
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
pub type PFN_vkGetBufferDeviceAddress = unsafe extern "system" fn(
    device: Device,
    p_info: *const BufferDeviceAddressInfo,
) -> DeviceAddress;
#[derive(Clone)]
pub struct ExtBufferDeviceAddressFn {
    pub get_buffer_device_address_ext: PFN_vkGetBufferDeviceAddress,
}
unsafe impl Send for ExtBufferDeviceAddressFn {}
unsafe impl Sync for ExtBufferDeviceAddressFn {}
impl ExtBufferDeviceAddressFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtBufferDeviceAddressFn {
            get_buffer_device_address_ext: unsafe {
                unsafe extern "system" fn get_buffer_device_address_ext(
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
        Self::PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_DEVICE_ADDRESS_INFO_EXT: Self = Self::BUFFER_DEVICE_ADDRESS_INFO;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT: Self = Self(1_000_244_002);
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl BufferUsageFlags {
    pub const SHADER_DEVICE_ADDRESS_EXT: Self = Self::SHADER_DEVICE_ADDRESS;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl BufferCreateFlags {
    pub const DEVICE_ADDRESS_CAPTURE_REPLAY_EXT: Self = Self::DEVICE_ADDRESS_CAPTURE_REPLAY;
}
#[doc = "Generated from 'VK_EXT_buffer_device_address'"]
impl Result {
    pub const ERROR_INVALID_DEVICE_ADDRESS_EXT: Self = Self::ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
}
impl ExtToolingInfoFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_tooling_info\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceToolPropertiesEXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_tool_count: *mut u32,
    p_tool_properties: *mut PhysicalDeviceToolPropertiesEXT,
) -> Result;
#[derive(Clone)]
pub struct ExtToolingInfoFn {
    pub get_physical_device_tool_properties_ext: PFN_vkGetPhysicalDeviceToolPropertiesEXT,
}
unsafe impl Send for ExtToolingInfoFn {}
unsafe impl Sync for ExtToolingInfoFn {}
impl ExtToolingInfoFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtToolingInfoFn {
            get_physical_device_tool_properties_ext: unsafe {
                unsafe extern "system" fn get_physical_device_tool_properties_ext(
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
#[derive(Clone)]
pub struct ExtSeparateStencilUsageFn {}
unsafe impl Send for ExtSeparateStencilUsageFn {}
unsafe impl Sync for ExtSeparateStencilUsageFn {}
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
    pub const IMAGE_STENCIL_USAGE_CREATE_INFO_EXT: Self = Self::IMAGE_STENCIL_USAGE_CREATE_INFO;
}
impl ExtValidationFeaturesFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_validation_features\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 5u32;
}
#[derive(Clone)]
pub struct ExtValidationFeaturesFn {}
unsafe impl Send for ExtValidationFeaturesFn {}
unsafe impl Sync for ExtValidationFeaturesFn {}
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
impl KhrPresentWaitFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_present_wait\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkWaitForPresentKHR = unsafe extern "system" fn(
    device: Device,
    swapchain: SwapchainKHR,
    present_id: u64,
    timeout: u64,
) -> Result;
#[derive(Clone)]
pub struct KhrPresentWaitFn {
    pub wait_for_present_khr: PFN_vkWaitForPresentKHR,
}
unsafe impl Send for KhrPresentWaitFn {}
unsafe impl Sync for KhrPresentWaitFn {}
impl KhrPresentWaitFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPresentWaitFn {
            wait_for_present_khr: unsafe {
                unsafe extern "system" fn wait_for_present_khr(
                    _device: Device,
                    _swapchain: SwapchainKHR,
                    _present_id: u64,
                    _timeout: u64,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(wait_for_present_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkWaitForPresentKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    wait_for_present_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitForPresentKHR.html>"]
    pub unsafe fn wait_for_present_khr(
        &self,
        device: Device,
        swapchain: SwapchainKHR,
        present_id: u64,
        timeout: u64,
    ) -> Result {
        (self.wait_for_present_khr)(device, swapchain, present_id, timeout)
    }
}
#[doc = "Generated from 'VK_KHR_present_wait'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR: Self = Self(1_000_248_000);
}
impl NvCooperativeMatrixFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_cooperative_matrix\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut CooperativeMatrixPropertiesNV,
)
    -> Result;
#[derive(Clone)]
pub struct NvCooperativeMatrixFn {
    pub get_physical_device_cooperative_matrix_properties_nv:
        PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV,
}
unsafe impl Send for NvCooperativeMatrixFn {}
unsafe impl Sync for NvCooperativeMatrixFn {}
impl NvCooperativeMatrixFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvCooperativeMatrixFn {
            get_physical_device_cooperative_matrix_properties_nv: unsafe {
                unsafe extern "system" fn get_physical_device_cooperative_matrix_properties_nv(
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
    unsafe extern "system" fn(
        physical_device: PhysicalDevice,
        p_combination_count: *mut u32,
        p_combinations: *mut FramebufferMixedSamplesCombinationNV,
    ) -> Result;
#[derive(Clone)]
pub struct NvCoverageReductionModeFn {
    pub get_physical_device_supported_framebuffer_mixed_samples_combinations_nv:
        PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV,
}
unsafe impl Send for NvCoverageReductionModeFn {}
unsafe impl Sync for NvCoverageReductionModeFn {}
impl NvCoverageReductionModeFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvCoverageReductionModeFn {
            get_physical_device_supported_framebuffer_mixed_samples_combinations_nv: unsafe {
                unsafe extern "system" fn get_physical_device_supported_framebuffer_mixed_samples_combinations_nv(
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
#[derive(Clone)]
pub struct ExtFragmentShaderInterlockFn {}
unsafe impl Send for ExtFragmentShaderInterlockFn {}
unsafe impl Sync for ExtFragmentShaderInterlockFn {}
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
#[derive(Clone)]
pub struct ExtYcbcrImageArraysFn {}
unsafe impl Send for ExtYcbcrImageArraysFn {}
unsafe impl Sync for ExtYcbcrImageArraysFn {}
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
#[derive(Clone)]
pub struct KhrUniformBufferStandardLayoutFn {}
unsafe impl Send for KhrUniformBufferStandardLayoutFn {}
unsafe impl Sync for KhrUniformBufferStandardLayoutFn {}
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
        Self::PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES;
}
impl ExtProvokingVertexFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_provoking_vertex\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtProvokingVertexFn {}
unsafe impl Send for ExtProvokingVertexFn {}
unsafe impl Sync for ExtProvokingVertexFn {}
impl ExtProvokingVertexFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtProvokingVertexFn {}
    }
}
#[doc = "Generated from 'VK_EXT_provoking_vertex'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT: Self = Self(1_000_254_000);
}
#[doc = "Generated from 'VK_EXT_provoking_vertex'"]
impl StructureType {
    pub const PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT: Self =
        Self(1_000_254_001);
}
#[doc = "Generated from 'VK_EXT_provoking_vertex'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT: Self = Self(1_000_254_002);
}
impl ExtFullScreenExclusiveFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_full_screen_exclusive\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 4u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_present_mode_count: *mut u32,
    p_present_modes: *mut PresentModeKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireFullScreenExclusiveModeEXT =
    unsafe extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkReleaseFullScreenExclusiveModeEXT =
    unsafe extern "system" fn(device: Device, swapchain: SwapchainKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceGroupSurfacePresentModes2EXT = unsafe extern "system" fn(
    device: Device,
    p_surface_info: *const PhysicalDeviceSurfaceInfo2KHR,
    p_modes: *mut DeviceGroupPresentModeFlagsKHR,
) -> Result;
#[derive(Clone)]
pub struct ExtFullScreenExclusiveFn {
    pub get_physical_device_surface_present_modes2_ext:
        PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT,
    pub acquire_full_screen_exclusive_mode_ext: PFN_vkAcquireFullScreenExclusiveModeEXT,
    pub release_full_screen_exclusive_mode_ext: PFN_vkReleaseFullScreenExclusiveModeEXT,
    pub get_device_group_surface_present_modes2_ext: PFN_vkGetDeviceGroupSurfacePresentModes2EXT,
}
unsafe impl Send for ExtFullScreenExclusiveFn {}
unsafe impl Sync for ExtFullScreenExclusiveFn {}
impl ExtFullScreenExclusiveFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtFullScreenExclusiveFn {
            get_physical_device_surface_present_modes2_ext: unsafe {
                unsafe extern "system" fn get_physical_device_surface_present_modes2_ext(
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
                unsafe extern "system" fn acquire_full_screen_exclusive_mode_ext(
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
                unsafe extern "system" fn release_full_screen_exclusive_mode_ext(
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
                unsafe extern "system" fn get_device_group_surface_present_modes2_ext(
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
pub type PFN_vkCreateHeadlessSurfaceEXT = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const HeadlessSurfaceCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[derive(Clone)]
pub struct ExtHeadlessSurfaceFn {
    pub create_headless_surface_ext: PFN_vkCreateHeadlessSurfaceEXT,
}
unsafe impl Send for ExtHeadlessSurfaceFn {}
unsafe impl Sync for ExtHeadlessSurfaceFn {}
impl ExtHeadlessSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtHeadlessSurfaceFn {
            create_headless_surface_ext: unsafe {
                unsafe extern "system" fn create_headless_surface_ext(
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
    unsafe extern "system" fn(device: Device, p_info: *const BufferDeviceAddressInfo) -> u64;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceMemoryOpaqueCaptureAddress = unsafe extern "system" fn(
    device: Device,
    p_info: *const DeviceMemoryOpaqueCaptureAddressInfo,
) -> u64;
#[derive(Clone)]
pub struct KhrBufferDeviceAddressFn {
    pub get_buffer_device_address_khr: crate::vk::PFN_vkGetBufferDeviceAddress,
    pub get_buffer_opaque_capture_address_khr: PFN_vkGetBufferOpaqueCaptureAddress,
    pub get_device_memory_opaque_capture_address_khr: PFN_vkGetDeviceMemoryOpaqueCaptureAddress,
}
unsafe impl Send for KhrBufferDeviceAddressFn {}
unsafe impl Sync for KhrBufferDeviceAddressFn {}
impl KhrBufferDeviceAddressFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrBufferDeviceAddressFn {
            get_buffer_device_address_khr: unsafe {
                unsafe extern "system" fn get_buffer_device_address_khr(
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
                unsafe extern "system" fn get_buffer_opaque_capture_address_khr(
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
                unsafe extern "system" fn get_device_memory_opaque_capture_address_khr(
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
        Self::PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_DEVICE_ADDRESS_INFO_KHR: Self = Self::BUFFER_DEVICE_ADDRESS_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO_KHR: Self =
        Self::BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO_KHR: Self =
        Self::MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl StructureType {
    pub const DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO_KHR: Self =
        Self::DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl BufferUsageFlags {
    pub const SHADER_DEVICE_ADDRESS_KHR: Self = Self::SHADER_DEVICE_ADDRESS;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl BufferCreateFlags {
    pub const DEVICE_ADDRESS_CAPTURE_REPLAY_KHR: Self = Self::DEVICE_ADDRESS_CAPTURE_REPLAY;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl MemoryAllocateFlags {
    pub const DEVICE_ADDRESS_KHR: Self = Self::DEVICE_ADDRESS;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl MemoryAllocateFlags {
    pub const DEVICE_ADDRESS_CAPTURE_REPLAY_KHR: Self = Self::DEVICE_ADDRESS_CAPTURE_REPLAY;
}
#[doc = "Generated from 'VK_KHR_buffer_device_address'"]
impl Result {
    pub const ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR: Self =
        Self::ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS;
}
impl ExtExtension259Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_259\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension259Fn {}
unsafe impl Send for ExtExtension259Fn {}
unsafe impl Sync for ExtExtension259Fn {}
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
pub type PFN_vkCmdSetLineStippleEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    line_stipple_factor: u32,
    line_stipple_pattern: u16,
);
#[derive(Clone)]
pub struct ExtLineRasterizationFn {
    pub cmd_set_line_stipple_ext: PFN_vkCmdSetLineStippleEXT,
}
unsafe impl Send for ExtLineRasterizationFn {}
unsafe impl Sync for ExtLineRasterizationFn {}
impl ExtLineRasterizationFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtLineRasterizationFn {
            cmd_set_line_stipple_ext: unsafe {
                unsafe extern "system" fn cmd_set_line_stipple_ext(
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
#[derive(Clone)]
pub struct ExtShaderAtomicFloatFn {}
unsafe impl Send for ExtShaderAtomicFloatFn {}
unsafe impl Sync for ExtShaderAtomicFloatFn {}
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
pub type PFN_vkResetQueryPool = unsafe extern "system" fn(
    device: Device,
    query_pool: QueryPool,
    first_query: u32,
    query_count: u32,
);
#[derive(Clone)]
pub struct ExtHostQueryResetFn {
    pub reset_query_pool_ext: PFN_vkResetQueryPool,
}
unsafe impl Send for ExtHostQueryResetFn {}
unsafe impl Sync for ExtHostQueryResetFn {}
impl ExtHostQueryResetFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtHostQueryResetFn {
            reset_query_pool_ext: unsafe {
                unsafe extern "system" fn reset_query_pool_ext(
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
        Self::PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
}
impl GgpExtension263Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_extension_263\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct GgpExtension263Fn {}
unsafe impl Send for GgpExtension263Fn {}
unsafe impl Sync for GgpExtension263Fn {}
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
#[derive(Clone)]
pub struct BrcmExtension264Fn {}
unsafe impl Send for BrcmExtension264Fn {}
unsafe impl Sync for BrcmExtension264Fn {}
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
#[derive(Clone)]
pub struct BrcmExtension265Fn {}
unsafe impl Send for BrcmExtension265Fn {}
unsafe impl Sync for BrcmExtension265Fn {}
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
#[derive(Clone)]
pub struct ExtIndexTypeUint8Fn {}
unsafe impl Send for ExtIndexTypeUint8Fn {}
unsafe impl Sync for ExtIndexTypeUint8Fn {}
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
#[derive(Clone)]
pub struct ExtExtension267Fn {}
unsafe impl Send for ExtExtension267Fn {}
unsafe impl Sync for ExtExtension267Fn {}
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
    unsafe extern "system" fn(command_buffer: CommandBuffer, cull_mode: CullModeFlags);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetFrontFaceEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, front_face: FrontFace);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPrimitiveTopologyEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, primitive_topology: PrimitiveTopology);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetViewportWithCountEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    viewport_count: u32,
    p_viewports: *const Viewport,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetScissorWithCountEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    scissor_count: u32,
    p_scissors: *const Rect2D,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindVertexBuffers2EXT = unsafe extern "system" fn(
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
    unsafe extern "system" fn(command_buffer: CommandBuffer, depth_test_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthWriteEnableEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, depth_write_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthCompareOpEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, depth_compare_op: CompareOp);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthBoundsTestEnableEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, depth_bounds_test_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetStencilTestEnableEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, stencil_test_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetStencilOpEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    face_mask: StencilFaceFlags,
    fail_op: StencilOp,
    pass_op: StencilOp,
    depth_fail_op: StencilOp,
    compare_op: CompareOp,
);
#[derive(Clone)]
pub struct ExtExtendedDynamicStateFn {
    pub cmd_set_cull_mode_ext: PFN_vkCmdSetCullModeEXT,
    pub cmd_set_front_face_ext: PFN_vkCmdSetFrontFaceEXT,
    pub cmd_set_primitive_topology_ext: PFN_vkCmdSetPrimitiveTopologyEXT,
    pub cmd_set_viewport_with_count_ext: PFN_vkCmdSetViewportWithCountEXT,
    pub cmd_set_scissor_with_count_ext: PFN_vkCmdSetScissorWithCountEXT,
    pub cmd_bind_vertex_buffers2_ext: PFN_vkCmdBindVertexBuffers2EXT,
    pub cmd_set_depth_test_enable_ext: PFN_vkCmdSetDepthTestEnableEXT,
    pub cmd_set_depth_write_enable_ext: PFN_vkCmdSetDepthWriteEnableEXT,
    pub cmd_set_depth_compare_op_ext: PFN_vkCmdSetDepthCompareOpEXT,
    pub cmd_set_depth_bounds_test_enable_ext: PFN_vkCmdSetDepthBoundsTestEnableEXT,
    pub cmd_set_stencil_test_enable_ext: PFN_vkCmdSetStencilTestEnableEXT,
    pub cmd_set_stencil_op_ext: PFN_vkCmdSetStencilOpEXT,
}
unsafe impl Send for ExtExtendedDynamicStateFn {}
unsafe impl Sync for ExtExtendedDynamicStateFn {}
impl ExtExtendedDynamicStateFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtendedDynamicStateFn {
            cmd_set_cull_mode_ext: unsafe {
                unsafe extern "system" fn cmd_set_cull_mode_ext(
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
                unsafe extern "system" fn cmd_set_front_face_ext(
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
                unsafe extern "system" fn cmd_set_primitive_topology_ext(
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
                unsafe extern "system" fn cmd_set_viewport_with_count_ext(
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
                unsafe extern "system" fn cmd_set_scissor_with_count_ext(
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
                unsafe extern "system" fn cmd_bind_vertex_buffers2_ext(
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
                unsafe extern "system" fn cmd_set_depth_test_enable_ext(
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
                unsafe extern "system" fn cmd_set_depth_write_enable_ext(
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
                unsafe extern "system" fn cmd_set_depth_compare_op_ext(
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
                unsafe extern "system" fn cmd_set_depth_bounds_test_enable_ext(
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
                unsafe extern "system" fn cmd_set_stencil_test_enable_ext(
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
                unsafe extern "system" fn cmd_set_stencil_op_ext(
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
pub type PFN_vkCreateDeferredOperationKHR = unsafe extern "system" fn(
    device: Device,
    p_allocator: *const AllocationCallbacks,
    p_deferred_operation: *mut DeferredOperationKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDeferredOperationKHR = unsafe extern "system" fn(
    device: Device,
    operation: DeferredOperationKHR,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeferredOperationMaxConcurrencyKHR =
    unsafe extern "system" fn(device: Device, operation: DeferredOperationKHR) -> u32;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeferredOperationResultKHR =
    unsafe extern "system" fn(device: Device, operation: DeferredOperationKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDeferredOperationJoinKHR =
    unsafe extern "system" fn(device: Device, operation: DeferredOperationKHR) -> Result;
#[derive(Clone)]
pub struct KhrDeferredHostOperationsFn {
    pub create_deferred_operation_khr: PFN_vkCreateDeferredOperationKHR,
    pub destroy_deferred_operation_khr: PFN_vkDestroyDeferredOperationKHR,
    pub get_deferred_operation_max_concurrency_khr: PFN_vkGetDeferredOperationMaxConcurrencyKHR,
    pub get_deferred_operation_result_khr: PFN_vkGetDeferredOperationResultKHR,
    pub deferred_operation_join_khr: PFN_vkDeferredOperationJoinKHR,
}
unsafe impl Send for KhrDeferredHostOperationsFn {}
unsafe impl Sync for KhrDeferredHostOperationsFn {}
impl KhrDeferredHostOperationsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrDeferredHostOperationsFn {
            create_deferred_operation_khr: unsafe {
                unsafe extern "system" fn create_deferred_operation_khr(
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
                unsafe extern "system" fn destroy_deferred_operation_khr(
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
                unsafe extern "system" fn get_deferred_operation_max_concurrency_khr(
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
                unsafe extern "system" fn get_deferred_operation_result_khr(
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
                unsafe extern "system" fn deferred_operation_join_khr(
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
pub type PFN_vkGetPipelineExecutablePropertiesKHR = unsafe extern "system" fn(
    device: Device,
    p_pipeline_info: *const PipelineInfoKHR,
    p_executable_count: *mut u32,
    p_properties: *mut PipelineExecutablePropertiesKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPipelineExecutableStatisticsKHR = unsafe extern "system" fn(
    device: Device,
    p_executable_info: *const PipelineExecutableInfoKHR,
    p_statistic_count: *mut u32,
    p_statistics: *mut PipelineExecutableStatisticKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPipelineExecutableInternalRepresentationsKHR =
    unsafe extern "system" fn(
        device: Device,
        p_executable_info: *const PipelineExecutableInfoKHR,
        p_internal_representation_count: *mut u32,
        p_internal_representations: *mut PipelineExecutableInternalRepresentationKHR,
    ) -> Result;
#[derive(Clone)]
pub struct KhrPipelineExecutablePropertiesFn {
    pub get_pipeline_executable_properties_khr: PFN_vkGetPipelineExecutablePropertiesKHR,
    pub get_pipeline_executable_statistics_khr: PFN_vkGetPipelineExecutableStatisticsKHR,
    pub get_pipeline_executable_internal_representations_khr:
        PFN_vkGetPipelineExecutableInternalRepresentationsKHR,
}
unsafe impl Send for KhrPipelineExecutablePropertiesFn {}
unsafe impl Sync for KhrPipelineExecutablePropertiesFn {}
impl KhrPipelineExecutablePropertiesFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPipelineExecutablePropertiesFn {
            get_pipeline_executable_properties_khr: unsafe {
                unsafe extern "system" fn get_pipeline_executable_properties_khr(
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
                unsafe extern "system" fn get_pipeline_executable_statistics_khr(
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
                unsafe extern "system" fn get_pipeline_executable_internal_representations_khr(
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
#[derive(Clone)]
pub struct IntelExtension271Fn {}
unsafe impl Send for IntelExtension271Fn {}
unsafe impl Sync for IntelExtension271Fn {}
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
#[derive(Clone)]
pub struct IntelExtension272Fn {}
unsafe impl Send for IntelExtension272Fn {}
unsafe impl Sync for IntelExtension272Fn {}
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
#[derive(Clone)]
pub struct IntelExtension273Fn {}
unsafe impl Send for IntelExtension273Fn {}
unsafe impl Sync for IntelExtension273Fn {}
impl IntelExtension273Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        IntelExtension273Fn {}
    }
}
impl ExtShaderAtomicFloat2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_shader_atomic_float2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtShaderAtomicFloat2Fn {}
unsafe impl Send for ExtShaderAtomicFloat2Fn {}
unsafe impl Sync for ExtShaderAtomicFloat2Fn {}
impl ExtShaderAtomicFloat2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtShaderAtomicFloat2Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_shader_atomic_float2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT: Self = Self(1_000_273_000);
}
impl KhrExtension275Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_275\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension275Fn {}
unsafe impl Send for KhrExtension275Fn {}
unsafe impl Sync for KhrExtension275Fn {}
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
#[derive(Clone)]
pub struct KhrExtension276Fn {}
unsafe impl Send for KhrExtension276Fn {}
unsafe impl Sync for KhrExtension276Fn {}
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
#[derive(Clone)]
pub struct ExtShaderDemoteToHelperInvocationFn {}
unsafe impl Send for ExtShaderDemoteToHelperInvocationFn {}
unsafe impl Sync for ExtShaderDemoteToHelperInvocationFn {}
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
pub type PFN_vkGetGeneratedCommandsMemoryRequirementsNV = unsafe extern "system" fn(
    device: Device,
    p_info: *const GeneratedCommandsMemoryRequirementsInfoNV,
    p_memory_requirements: *mut MemoryRequirements2,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPreprocessGeneratedCommandsNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_generated_commands_info: *const GeneratedCommandsInfoNV,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdExecuteGeneratedCommandsNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    is_preprocessed: Bool32,
    p_generated_commands_info: *const GeneratedCommandsInfoNV,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindPipelineShaderGroupNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_bind_point: PipelineBindPoint,
    pipeline: Pipeline,
    group_index: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateIndirectCommandsLayoutNV = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const IndirectCommandsLayoutCreateInfoNV,
    p_allocator: *const AllocationCallbacks,
    p_indirect_commands_layout: *mut IndirectCommandsLayoutNV,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyIndirectCommandsLayoutNV = unsafe extern "system" fn(
    device: Device,
    indirect_commands_layout: IndirectCommandsLayoutNV,
    p_allocator: *const AllocationCallbacks,
);
#[derive(Clone)]
pub struct NvDeviceGeneratedCommandsFn {
    pub get_generated_commands_memory_requirements_nv:
        PFN_vkGetGeneratedCommandsMemoryRequirementsNV,
    pub cmd_preprocess_generated_commands_nv: PFN_vkCmdPreprocessGeneratedCommandsNV,
    pub cmd_execute_generated_commands_nv: PFN_vkCmdExecuteGeneratedCommandsNV,
    pub cmd_bind_pipeline_shader_group_nv: PFN_vkCmdBindPipelineShaderGroupNV,
    pub create_indirect_commands_layout_nv: PFN_vkCreateIndirectCommandsLayoutNV,
    pub destroy_indirect_commands_layout_nv: PFN_vkDestroyIndirectCommandsLayoutNV,
}
unsafe impl Send for NvDeviceGeneratedCommandsFn {}
unsafe impl Sync for NvDeviceGeneratedCommandsFn {}
impl NvDeviceGeneratedCommandsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvDeviceGeneratedCommandsFn {
            get_generated_commands_memory_requirements_nv: unsafe {
                unsafe extern "system" fn get_generated_commands_memory_requirements_nv(
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
                unsafe extern "system" fn cmd_preprocess_generated_commands_nv(
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
                unsafe extern "system" fn cmd_execute_generated_commands_nv(
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
                unsafe extern "system" fn cmd_bind_pipeline_shader_group_nv(
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
                unsafe extern "system" fn create_indirect_commands_layout_nv(
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
                unsafe extern "system" fn destroy_indirect_commands_layout_nv(
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
impl NvInheritedViewportScissorFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_inherited_viewport_scissor\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct NvInheritedViewportScissorFn {}
unsafe impl Send for NvInheritedViewportScissorFn {}
unsafe impl Sync for NvInheritedViewportScissorFn {}
impl NvInheritedViewportScissorFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvInheritedViewportScissorFn {}
    }
}
#[doc = "Generated from 'VK_NV_inherited_viewport_scissor'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_INHERITED_VIEWPORT_SCISSOR_FEATURES_NV: Self = Self(1_000_278_000);
}
#[doc = "Generated from 'VK_NV_inherited_viewport_scissor'"]
impl StructureType {
    pub const COMMAND_BUFFER_INHERITANCE_VIEWPORT_SCISSOR_INFO_NV: Self = Self(1_000_278_001);
}
impl KhrExtension280Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_280\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension280Fn {}
unsafe impl Send for KhrExtension280Fn {}
unsafe impl Sync for KhrExtension280Fn {}
impl KhrExtension280Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension280Fn {}
    }
}
impl KhrShaderIntegerDotProductFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_integer_dot_product\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrShaderIntegerDotProductFn {}
unsafe impl Send for KhrShaderIntegerDotProductFn {}
unsafe impl Sync for KhrShaderIntegerDotProductFn {}
impl KhrShaderIntegerDotProductFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderIntegerDotProductFn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_integer_dot_product'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES_KHR: Self = Self(1_000_280_000);
}
#[doc = "Generated from 'VK_KHR_shader_integer_dot_product'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES_KHR: Self = Self(1_000_280_001);
}
impl ExtTexelBufferAlignmentFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_texel_buffer_alignment\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtTexelBufferAlignmentFn {}
unsafe impl Send for ExtTexelBufferAlignmentFn {}
unsafe impl Sync for ExtTexelBufferAlignmentFn {}
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
    pub const SPEC_VERSION: u32 = 2u32;
}
#[derive(Clone)]
pub struct QcomRenderPassTransformFn {}
unsafe impl Send for QcomRenderPassTransformFn {}
unsafe impl Sync for QcomRenderPassTransformFn {}
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
#[derive(Clone)]
pub struct ExtExtension284Fn {}
unsafe impl Send for ExtExtension284Fn {}
unsafe impl Sync for ExtExtension284Fn {}
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
#[derive(Clone)]
pub struct ExtDeviceMemoryReportFn {}
unsafe impl Send for ExtDeviceMemoryReportFn {}
unsafe impl Sync for ExtDeviceMemoryReportFn {}
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
impl ExtAcquireDrmDisplayFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_acquire_drm_display\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkAcquireDrmDisplayEXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    drm_fd: i32,
    display: DisplayKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDrmDisplayEXT = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    drm_fd: i32,
    connector_id: u32,
    display: *mut DisplayKHR,
) -> Result;
#[derive(Clone)]
pub struct ExtAcquireDrmDisplayFn {
    pub acquire_drm_display_ext: PFN_vkAcquireDrmDisplayEXT,
    pub get_drm_display_ext: PFN_vkGetDrmDisplayEXT,
}
unsafe impl Send for ExtAcquireDrmDisplayFn {}
unsafe impl Sync for ExtAcquireDrmDisplayFn {}
impl ExtAcquireDrmDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtAcquireDrmDisplayFn {
            acquire_drm_display_ext: unsafe {
                unsafe extern "system" fn acquire_drm_display_ext(
                    _physical_device: PhysicalDevice,
                    _drm_fd: i32,
                    _display: DisplayKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(acquire_drm_display_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAcquireDrmDisplayEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    acquire_drm_display_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_drm_display_ext: unsafe {
                unsafe extern "system" fn get_drm_display_ext(
                    _physical_device: PhysicalDevice,
                    _drm_fd: i32,
                    _connector_id: u32,
                    _display: *mut DisplayKHR,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_drm_display_ext)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetDrmDisplayEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    get_drm_display_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAcquireDrmDisplayEXT.html>"]
    pub unsafe fn acquire_drm_display_ext(
        &self,
        physical_device: PhysicalDevice,
        drm_fd: i32,
        display: DisplayKHR,
    ) -> Result {
        (self.acquire_drm_display_ext)(physical_device, drm_fd, display)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDrmDisplayEXT.html>"]
    pub unsafe fn get_drm_display_ext(
        &self,
        physical_device: PhysicalDevice,
        drm_fd: i32,
        connector_id: u32,
        display: *mut DisplayKHR,
    ) -> Result {
        (self.get_drm_display_ext)(physical_device, drm_fd, connector_id, display)
    }
}
impl ExtRobustness2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_robustness2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtRobustness2Fn {}
unsafe impl Send for ExtRobustness2Fn {}
unsafe impl Sync for ExtRobustness2Fn {}
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
#[derive(Clone)]
pub struct ExtCustomBorderColorFn {}
unsafe impl Send for ExtCustomBorderColorFn {}
unsafe impl Sync for ExtCustomBorderColorFn {}
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
#[derive(Clone)]
pub struct ExtExtension289Fn {}
unsafe impl Send for ExtExtension289Fn {}
unsafe impl Sync for ExtExtension289Fn {}
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
#[derive(Clone)]
pub struct GoogleUserTypeFn {}
unsafe impl Send for GoogleUserTypeFn {}
unsafe impl Sync for GoogleUserTypeFn {}
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
#[derive(Clone)]
pub struct KhrPipelineLibraryFn {}
unsafe impl Send for KhrPipelineLibraryFn {}
unsafe impl Sync for KhrPipelineLibraryFn {}
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
#[derive(Clone)]
pub struct NvExtension292Fn {}
unsafe impl Send for NvExtension292Fn {}
unsafe impl Sync for NvExtension292Fn {}
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
#[derive(Clone)]
pub struct NvExtension293Fn {}
unsafe impl Send for NvExtension293Fn {}
unsafe impl Sync for NvExtension293Fn {}
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
#[derive(Clone)]
pub struct KhrShaderNonSemanticInfoFn {}
unsafe impl Send for KhrShaderNonSemanticInfoFn {}
unsafe impl Sync for KhrShaderNonSemanticInfoFn {}
impl KhrShaderNonSemanticInfoFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderNonSemanticInfoFn {}
    }
}
impl KhrPresentIdFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_present_id\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrPresentIdFn {}
unsafe impl Send for KhrPresentIdFn {}
unsafe impl Sync for KhrPresentIdFn {}
impl KhrPresentIdFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrPresentIdFn {}
    }
}
#[doc = "Generated from 'VK_KHR_present_id'"]
impl StructureType {
    pub const PRESENT_ID_KHR: Self = Self(1_000_294_000);
}
#[doc = "Generated from 'VK_KHR_present_id'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR: Self = Self(1_000_294_001);
}
impl ExtPrivateDataFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_private_data\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreatePrivateDataSlotEXT = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const PrivateDataSlotCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_private_data_slot: *mut PrivateDataSlotEXT,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyPrivateDataSlotEXT = unsafe extern "system" fn(
    device: Device,
    private_data_slot: PrivateDataSlotEXT,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkSetPrivateDataEXT = unsafe extern "system" fn(
    device: Device,
    object_type: ObjectType,
    object_handle: u64,
    private_data_slot: PrivateDataSlotEXT,
    data: u64,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPrivateDataEXT = unsafe extern "system" fn(
    device: Device,
    object_type: ObjectType,
    object_handle: u64,
    private_data_slot: PrivateDataSlotEXT,
    p_data: *mut u64,
);
#[derive(Clone)]
pub struct ExtPrivateDataFn {
    pub create_private_data_slot_ext: PFN_vkCreatePrivateDataSlotEXT,
    pub destroy_private_data_slot_ext: PFN_vkDestroyPrivateDataSlotEXT,
    pub set_private_data_ext: PFN_vkSetPrivateDataEXT,
    pub get_private_data_ext: PFN_vkGetPrivateDataEXT,
}
unsafe impl Send for ExtPrivateDataFn {}
unsafe impl Sync for ExtPrivateDataFn {}
impl ExtPrivateDataFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPrivateDataFn {
            create_private_data_slot_ext: unsafe {
                unsafe extern "system" fn create_private_data_slot_ext(
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
                unsafe extern "system" fn destroy_private_data_slot_ext(
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
                unsafe extern "system" fn set_private_data_ext(
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
                unsafe extern "system" fn get_private_data_ext(
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
#[derive(Clone)]
pub struct KhrExtension297Fn {}
unsafe impl Send for KhrExtension297Fn {}
unsafe impl Sync for KhrExtension297Fn {}
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
#[derive(Clone)]
pub struct ExtPipelineCreationCacheControlFn {}
unsafe impl Send for ExtPipelineCreationCacheControlFn {}
unsafe impl Sync for ExtPipelineCreationCacheControlFn {}
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
    pub const ERROR_PIPELINE_COMPILE_REQUIRED_EXT: Self = Self::PIPELINE_COMPILE_REQUIRED_EXT;
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
#[derive(Clone)]
pub struct KhrExtension299Fn {}
unsafe impl Send for KhrExtension299Fn {}
unsafe impl Sync for KhrExtension299Fn {}
impl KhrExtension299Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension299Fn {}
    }
}
#[doc = "Generated from 'VK_KHR_extension_299'"]
impl MemoryHeapFlags {
    pub const RESERVED_2_KHR: Self = Self(0b100);
}
#[doc = "Generated from 'VK_KHR_extension_299'"]
impl PipelineCacheCreateFlags {
    pub const RESERVED_1_KHR: Self = Self::RESERVED_1_EXT;
}
#[doc = "Generated from 'VK_KHR_extension_299'"]
impl PipelineCacheCreateFlags {
    pub const RESERVED_2_KHR: Self = Self(0b100);
}
impl KhrVideoEncodeQueueFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_video_encode_queue\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEncodeVideoKHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_encode_info: *const VideoEncodeInfoKHR,
);
#[derive(Clone)]
pub struct KhrVideoEncodeQueueFn {
    pub cmd_encode_video_khr: PFN_vkCmdEncodeVideoKHR,
}
unsafe impl Send for KhrVideoEncodeQueueFn {}
unsafe impl Sync for KhrVideoEncodeQueueFn {}
impl KhrVideoEncodeQueueFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrVideoEncodeQueueFn {
            cmd_encode_video_khr: unsafe {
                unsafe extern "system" fn cmd_encode_video_khr(
                    _command_buffer: CommandBuffer,
                    _p_encode_info: *const VideoEncodeInfoKHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_encode_video_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdEncodeVideoKHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_encode_video_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEncodeVideoKHR.html>"]
    pub unsafe fn cmd_encode_video_khr(
        &self,
        command_buffer: CommandBuffer,
        p_encode_info: *const VideoEncodeInfoKHR,
    ) {
        (self.cmd_encode_video_khr)(command_buffer, p_encode_info)
    }
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl PipelineStageFlags2KHR {
    pub const VIDEO_ENCODE: Self = Self(0b1000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl AccessFlags2KHR {
    pub const VIDEO_ENCODE_READ: Self = Self(0b10_0000_0000_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl AccessFlags2KHR {
    pub const VIDEO_ENCODE_WRITE: Self = Self(0b100_0000_0000_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl StructureType {
    pub const VIDEO_ENCODE_INFO_KHR: Self = Self(1_000_299_000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl StructureType {
    pub const VIDEO_ENCODE_RATE_CONTROL_INFO_KHR: Self = Self(1_000_299_001);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl QueueFlags {
    pub const VIDEO_ENCODE_KHR: Self = Self(0b100_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl BufferUsageFlags {
    pub const VIDEO_ENCODE_DST_KHR: Self = Self(0b1000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl BufferUsageFlags {
    pub const VIDEO_ENCODE_SRC_KHR: Self = Self(0b1_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl ImageUsageFlags {
    pub const VIDEO_ENCODE_DST_KHR: Self = Self(0b10_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl ImageUsageFlags {
    pub const VIDEO_ENCODE_SRC_KHR: Self = Self(0b100_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl ImageUsageFlags {
    pub const VIDEO_ENCODE_DPB_KHR: Self = Self(0b1000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl FormatFeatureFlags {
    pub const VIDEO_ENCODE_INPUT_KHR: Self = Self(0b1000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl FormatFeatureFlags {
    pub const VIDEO_ENCODE_DPB_KHR: Self = Self(0b1_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl ImageLayout {
    pub const VIDEO_ENCODE_DST_KHR: Self = Self(1_000_299_000);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl ImageLayout {
    pub const VIDEO_ENCODE_SRC_KHR: Self = Self(1_000_299_001);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl ImageLayout {
    pub const VIDEO_ENCODE_DPB_KHR: Self = Self(1_000_299_002);
}
#[doc = "Generated from 'VK_KHR_video_encode_queue'"]
impl QueryType {
    pub const VIDEO_ENCODESTREAM_BUFFER_RANGE_KHR: Self = Self(1_000_299_000);
}
impl NvDeviceDiagnosticsConfigFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_device_diagnostics_config\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct NvDeviceDiagnosticsConfigFn {}
unsafe impl Send for NvDeviceDiagnosticsConfigFn {}
unsafe impl Sync for NvDeviceDiagnosticsConfigFn {}
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
#[derive(Clone)]
pub struct QcomRenderPassStoreOpsFn {}
unsafe impl Send for QcomRenderPassStoreOpsFn {}
unsafe impl Sync for QcomRenderPassStoreOpsFn {}
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
    pub const NONE_QCOM: Self = Self::NONE_EXT;
}
impl QcomExtension303Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_303\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct QcomExtension303Fn {}
unsafe impl Send for QcomExtension303Fn {}
unsafe impl Sync for QcomExtension303Fn {}
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
#[derive(Clone)]
pub struct QcomExtension304Fn {}
unsafe impl Send for QcomExtension304Fn {}
unsafe impl Sync for QcomExtension304Fn {}
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
#[derive(Clone)]
pub struct QcomExtension305Fn {}
unsafe impl Send for QcomExtension305Fn {}
unsafe impl Sync for QcomExtension305Fn {}
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
#[derive(Clone)]
pub struct QcomExtension306Fn {}
unsafe impl Send for QcomExtension306Fn {}
unsafe impl Sync for QcomExtension306Fn {}
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
#[derive(Clone)]
pub struct QcomExtension307Fn {}
unsafe impl Send for QcomExtension307Fn {}
unsafe impl Sync for QcomExtension307Fn {}
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
#[derive(Clone)]
pub struct NvExtension308Fn {}
unsafe impl Send for NvExtension308Fn {}
unsafe impl Sync for NvExtension308Fn {}
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
#[derive(Clone)]
pub struct KhrExtension309Fn {}
unsafe impl Send for KhrExtension309Fn {}
unsafe impl Sync for KhrExtension309Fn {}
impl KhrExtension309Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension309Fn {}
    }
}
impl QcomExtension310Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QCOM_extension_310\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct QcomExtension310Fn {}
unsafe impl Send for QcomExtension310Fn {}
unsafe impl Sync for QcomExtension310Fn {}
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
#[derive(Clone)]
pub struct NvExtension311Fn {}
unsafe impl Send for NvExtension311Fn {}
unsafe impl Sync for NvExtension311Fn {}
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
#[derive(Clone)]
pub struct ExtExtension312Fn {}
unsafe impl Send for ExtExtension312Fn {}
unsafe impl Sync for ExtExtension312Fn {}
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
#[derive(Clone)]
pub struct ExtExtension313Fn {}
unsafe impl Send for ExtExtension313Fn {}
unsafe impl Sync for ExtExtension313Fn {}
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
#[derive(Clone)]
pub struct AmdExtension314Fn {}
unsafe impl Send for AmdExtension314Fn {}
unsafe impl Sync for AmdExtension314Fn {}
impl AmdExtension314Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension314Fn {}
    }
}
impl KhrSynchronization2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_synchronization2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetEvent2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    event: Event,
    p_dependency_info: *const DependencyInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdResetEvent2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    event: Event,
    stage_mask: PipelineStageFlags2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWaitEvents2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    event_count: u32,
    p_events: *const Event,
    p_dependency_infos: *const DependencyInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPipelineBarrier2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_dependency_info: *const DependencyInfoKHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteTimestamp2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    stage: PipelineStageFlags2KHR,
    query_pool: QueryPool,
    query: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkQueueSubmit2KHR = unsafe extern "system" fn(
    queue: Queue,
    submit_count: u32,
    p_submits: *const SubmitInfo2KHR,
    fence: Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteBufferMarker2AMD = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    stage: PipelineStageFlags2KHR,
    dst_buffer: Buffer,
    dst_offset: DeviceSize,
    marker: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetQueueCheckpointData2NV = unsafe extern "system" fn(
    queue: Queue,
    p_checkpoint_data_count: *mut u32,
    p_checkpoint_data: *mut CheckpointData2NV,
);
#[derive(Clone)]
pub struct KhrSynchronization2Fn {
    pub cmd_set_event2_khr: PFN_vkCmdSetEvent2KHR,
    pub cmd_reset_event2_khr: PFN_vkCmdResetEvent2KHR,
    pub cmd_wait_events2_khr: PFN_vkCmdWaitEvents2KHR,
    pub cmd_pipeline_barrier2_khr: PFN_vkCmdPipelineBarrier2KHR,
    pub cmd_write_timestamp2_khr: PFN_vkCmdWriteTimestamp2KHR,
    pub queue_submit2_khr: PFN_vkQueueSubmit2KHR,
    pub cmd_write_buffer_marker2_amd: PFN_vkCmdWriteBufferMarker2AMD,
    pub get_queue_checkpoint_data2_nv: PFN_vkGetQueueCheckpointData2NV,
}
unsafe impl Send for KhrSynchronization2Fn {}
unsafe impl Sync for KhrSynchronization2Fn {}
impl KhrSynchronization2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrSynchronization2Fn {
            cmd_set_event2_khr: unsafe {
                unsafe extern "system" fn cmd_set_event2_khr(
                    _command_buffer: CommandBuffer,
                    _event: Event,
                    _p_dependency_info: *const DependencyInfoKHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_event2_khr)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetEvent2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_event2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_reset_event2_khr: unsafe {
                unsafe extern "system" fn cmd_reset_event2_khr(
                    _command_buffer: CommandBuffer,
                    _event: Event,
                    _stage_mask: PipelineStageFlags2KHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_reset_event2_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdResetEvent2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_reset_event2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_wait_events2_khr: unsafe {
                unsafe extern "system" fn cmd_wait_events2_khr(
                    _command_buffer: CommandBuffer,
                    _event_count: u32,
                    _p_events: *const Event,
                    _p_dependency_infos: *const DependencyInfoKHR,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_wait_events2_khr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdWaitEvents2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_wait_events2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_pipeline_barrier2_khr: unsafe {
                unsafe extern "system" fn cmd_pipeline_barrier2_khr(
                    _command_buffer: CommandBuffer,
                    _p_dependency_info: *const DependencyInfoKHR,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_pipeline_barrier2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdPipelineBarrier2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_pipeline_barrier2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_write_timestamp2_khr: unsafe {
                unsafe extern "system" fn cmd_write_timestamp2_khr(
                    _command_buffer: CommandBuffer,
                    _stage: PipelineStageFlags2KHR,
                    _query_pool: QueryPool,
                    _query: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_write_timestamp2_khr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdWriteTimestamp2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_write_timestamp2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_submit2_khr: unsafe {
                unsafe extern "system" fn queue_submit2_khr(
                    _queue: Queue,
                    _submit_count: u32,
                    _p_submits: *const SubmitInfo2KHR,
                    _fence: Fence,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(queue_submit2_khr)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkQueueSubmit2KHR\0");
                let val = _f(cname);
                if val.is_null() {
                    queue_submit2_khr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_write_buffer_marker2_amd: unsafe {
                unsafe extern "system" fn cmd_write_buffer_marker2_amd(
                    _command_buffer: CommandBuffer,
                    _stage: PipelineStageFlags2KHR,
                    _dst_buffer: Buffer,
                    _dst_offset: DeviceSize,
                    _marker: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_write_buffer_marker2_amd)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdWriteBufferMarker2AMD\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_write_buffer_marker2_amd
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_queue_checkpoint_data2_nv: unsafe {
                unsafe extern "system" fn get_queue_checkpoint_data2_nv(
                    _queue: Queue,
                    _p_checkpoint_data_count: *mut u32,
                    _p_checkpoint_data: *mut CheckpointData2NV,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_queue_checkpoint_data2_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetQueueCheckpointData2NV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_queue_checkpoint_data2_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetEvent2KHR.html>"]
    pub unsafe fn cmd_set_event2_khr(
        &self,
        command_buffer: CommandBuffer,
        event: Event,
        p_dependency_info: *const DependencyInfoKHR,
    ) {
        (self.cmd_set_event2_khr)(command_buffer, event, p_dependency_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResetEvent2KHR.html>"]
    pub unsafe fn cmd_reset_event2_khr(
        &self,
        command_buffer: CommandBuffer,
        event: Event,
        stage_mask: PipelineStageFlags2KHR,
    ) {
        (self.cmd_reset_event2_khr)(command_buffer, event, stage_mask)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWaitEvents2KHR.html>"]
    pub unsafe fn cmd_wait_events2_khr(
        &self,
        command_buffer: CommandBuffer,
        event_count: u32,
        p_events: *const Event,
        p_dependency_infos: *const DependencyInfoKHR,
    ) {
        (self.cmd_wait_events2_khr)(command_buffer, event_count, p_events, p_dependency_infos)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPipelineBarrier2KHR.html>"]
    pub unsafe fn cmd_pipeline_barrier2_khr(
        &self,
        command_buffer: CommandBuffer,
        p_dependency_info: *const DependencyInfoKHR,
    ) {
        (self.cmd_pipeline_barrier2_khr)(command_buffer, p_dependency_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteTimestamp2KHR.html>"]
    pub unsafe fn cmd_write_timestamp2_khr(
        &self,
        command_buffer: CommandBuffer,
        stage: PipelineStageFlags2KHR,
        query_pool: QueryPool,
        query: u32,
    ) {
        (self.cmd_write_timestamp2_khr)(command_buffer, stage, query_pool, query)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueSubmit2KHR.html>"]
    pub unsafe fn queue_submit2_khr(
        &self,
        queue: Queue,
        submit_count: u32,
        p_submits: *const SubmitInfo2KHR,
        fence: Fence,
    ) -> Result {
        (self.queue_submit2_khr)(queue, submit_count, p_submits, fence)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteBufferMarker2AMD.html>"]
    pub unsafe fn cmd_write_buffer_marker2_amd(
        &self,
        command_buffer: CommandBuffer,
        stage: PipelineStageFlags2KHR,
        dst_buffer: Buffer,
        dst_offset: DeviceSize,
        marker: u32,
    ) {
        (self.cmd_write_buffer_marker2_amd)(command_buffer, stage, dst_buffer, dst_offset, marker)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetQueueCheckpointData2NV.html>"]
    pub unsafe fn get_queue_checkpoint_data2_nv(
        &self,
        queue: Queue,
        p_checkpoint_data_count: *mut u32,
        p_checkpoint_data: *mut CheckpointData2NV,
    ) {
        (self.get_queue_checkpoint_data2_nv)(queue, p_checkpoint_data_count, p_checkpoint_data)
    }
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const MEMORY_BARRIER_2_KHR: Self = Self(1_000_314_000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const BUFFER_MEMORY_BARRIER_2_KHR: Self = Self(1_000_314_001);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const IMAGE_MEMORY_BARRIER_2_KHR: Self = Self(1_000_314_002);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const DEPENDENCY_INFO_KHR: Self = Self(1_000_314_003);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const SUBMIT_INFO_2_KHR: Self = Self(1_000_314_004);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const SEMAPHORE_SUBMIT_INFO_KHR: Self = Self(1_000_314_005);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const COMMAND_BUFFER_SUBMIT_INFO_KHR: Self = Self(1_000_314_006);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR: Self = Self(1_000_314_007);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl EventCreateFlags {
    pub const DEVICE_ONLY_KHR: Self = Self(0b1);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl ImageLayout {
    pub const READ_ONLY_OPTIMAL_KHR: Self = Self(1_000_314_000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl ImageLayout {
    pub const ATTACHMENT_OPTIMAL_KHR: Self = Self(1_000_314_001);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags {
    pub const NONE_KHR: Self = Self(0);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags {
    pub const NONE_KHR: Self = Self(0);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const TRANSFORM_FEEDBACK_EXT: Self = Self(0b1_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const TRANSFORM_FEEDBACK_WRITE_EXT: Self = Self(0b10_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const TRANSFORM_FEEDBACK_COUNTER_READ_EXT: Self = Self(0b100_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT: Self =
        Self(0b1000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const CONDITIONAL_RENDERING_EXT: Self = Self(0b100_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const CONDITIONAL_RENDERING_READ_EXT: Self = Self(0b1_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const COMMAND_PREPROCESS_NV: Self = Self(0b10_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const COMMAND_PREPROCESS_READ_NV: Self = Self(0b10_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const COMMAND_PREPROCESS_WRITE_NV: Self = Self(0b100_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT: Self = Self(0b100_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const FRAGMENT_SHADING_RATE_ATTACHMENT_READ: Self = Self(0b1000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const SHADING_RATE_IMAGE_NV: Self = Self::FRAGMENT_SHADING_RATE_ATTACHMENT;
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const SHADING_RATE_IMAGE_READ_NV: Self = Self::FRAGMENT_SHADING_RATE_ATTACHMENT_READ;
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const ACCELERATION_STRUCTURE_BUILD: Self = Self(0b10_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const ACCELERATION_STRUCTURE_READ: Self = Self(0b10_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const ACCELERATION_STRUCTURE_WRITE: Self = Self(0b100_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const RAY_TRACING_SHADER: Self = Self(0b10_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const RAY_TRACING_SHADER_NV: Self = Self::RAY_TRACING_SHADER;
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const ACCELERATION_STRUCTURE_BUILD_NV: Self = Self::ACCELERATION_STRUCTURE_BUILD;
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const ACCELERATION_STRUCTURE_READ_NV: Self = Self::ACCELERATION_STRUCTURE_READ;
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const ACCELERATION_STRUCTURE_WRITE_NV: Self = Self::ACCELERATION_STRUCTURE_WRITE;
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const FRAGMENT_DENSITY_PROCESS_EXT: Self = Self(0b1000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const FRAGMENT_DENSITY_MAP_READ_EXT: Self = Self(0b1_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl AccessFlags2KHR {
    pub const COLOR_ATTACHMENT_READ_NONCOHERENT_EXT: Self = Self(0b1000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const TASK_SHADER_NV: Self = Self(0b1000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl PipelineStageFlags2KHR {
    pub const MESH_SHADER_NV: Self = Self(0b1_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const QUEUE_FAMILY_CHECKPOINT_PROPERTIES_2_NV: Self = Self(1_000_314_008);
}
#[doc = "Generated from 'VK_KHR_synchronization2'"]
impl StructureType {
    pub const CHECKPOINT_DATA_2_NV: Self = Self(1_000_314_009);
}
impl AmdExtension316Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_316\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct AmdExtension316Fn {}
unsafe impl Send for AmdExtension316Fn {}
unsafe impl Sync for AmdExtension316Fn {}
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
#[derive(Clone)]
pub struct AmdExtension317Fn {}
unsafe impl Send for AmdExtension317Fn {}
unsafe impl Sync for AmdExtension317Fn {}
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
#[derive(Clone)]
pub struct AmdExtension318Fn {}
unsafe impl Send for AmdExtension318Fn {}
unsafe impl Sync for AmdExtension318Fn {}
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
#[derive(Clone)]
pub struct AmdExtension319Fn {}
unsafe impl Send for AmdExtension319Fn {}
unsafe impl Sync for AmdExtension319Fn {}
impl AmdExtension319Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension319Fn {}
    }
}
#[doc = "Generated from 'VK_AMD_extension_319'"]
impl DescriptorSetLayoutCreateFlags {
    pub const RESERVED_3_AMD: Self = Self(0b1000);
}
#[doc = "Generated from 'VK_AMD_extension_319'"]
impl PipelineLayoutCreateFlags {
    pub const RESERVED_0_AMD: Self = Self(0b1);
}
impl AmdExtension320Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_320\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct AmdExtension320Fn {}
unsafe impl Send for AmdExtension320Fn {}
unsafe impl Sync for AmdExtension320Fn {}
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
#[derive(Clone)]
pub struct AmdExtension321Fn {}
unsafe impl Send for AmdExtension321Fn {}
unsafe impl Sync for AmdExtension321Fn {}
impl AmdExtension321Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension321Fn {}
    }
}
#[doc = "Generated from 'VK_AMD_extension_321'"]
impl PipelineCreateFlags {
    pub const RESERVED_23_AMD: Self = Self(0b1000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_AMD_extension_321'"]
impl PipelineCreateFlags {
    pub const RESERVED_10_AMD: Self = Self(0b100_0000_0000);
}
impl AmdExtension322Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_AMD_extension_322\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct AmdExtension322Fn {}
unsafe impl Send for AmdExtension322Fn {}
unsafe impl Sync for AmdExtension322Fn {}
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
#[derive(Clone)]
pub struct AmdExtension323Fn {}
unsafe impl Send for AmdExtension323Fn {}
unsafe impl Sync for AmdExtension323Fn {}
impl AmdExtension323Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        AmdExtension323Fn {}
    }
}
impl KhrShaderSubgroupUniformControlFlowFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_shader_subgroup_uniform_control_flow\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct KhrShaderSubgroupUniformControlFlowFn {}
unsafe impl Send for KhrShaderSubgroupUniformControlFlowFn {}
unsafe impl Sync for KhrShaderSubgroupUniformControlFlowFn {}
impl KhrShaderSubgroupUniformControlFlowFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrShaderSubgroupUniformControlFlowFn {}
    }
}
#[doc = "Generated from 'VK_KHR_shader_subgroup_uniform_control_flow'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR: Self =
        Self(1_000_323_000);
}
impl KhrExtension325Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_325\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension325Fn {}
unsafe impl Send for KhrExtension325Fn {}
unsafe impl Sync for KhrExtension325Fn {}
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
#[derive(Clone)]
pub struct KhrZeroInitializeWorkgroupMemoryFn {}
unsafe impl Send for KhrZeroInitializeWorkgroupMemoryFn {}
unsafe impl Sync for KhrZeroInitializeWorkgroupMemoryFn {}
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
pub type PFN_vkCmdSetFragmentShadingRateEnumNV = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    shading_rate: FragmentShadingRateNV,
    combiner_ops: *const [FragmentShadingRateCombinerOpKHR; 2],
);
#[derive(Clone)]
pub struct NvFragmentShadingRateEnumsFn {
    pub cmd_set_fragment_shading_rate_enum_nv: PFN_vkCmdSetFragmentShadingRateEnumNV,
}
unsafe impl Send for NvFragmentShadingRateEnumsFn {}
unsafe impl Sync for NvFragmentShadingRateEnumsFn {}
impl NvFragmentShadingRateEnumsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvFragmentShadingRateEnumsFn {
            cmd_set_fragment_shading_rate_enum_nv: unsafe {
                unsafe extern "system" fn cmd_set_fragment_shading_rate_enum_nv(
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
impl NvRayTracingMotionBlurFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_ray_tracing_motion_blur\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct NvRayTracingMotionBlurFn {}
unsafe impl Send for NvRayTracingMotionBlurFn {}
unsafe impl Sync for NvRayTracingMotionBlurFn {}
impl NvRayTracingMotionBlurFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvRayTracingMotionBlurFn {}
    }
}
#[doc = "Generated from 'VK_NV_ray_tracing_motion_blur'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_GEOMETRY_MOTION_TRIANGLES_DATA_NV: Self = Self(1_000_327_000);
}
#[doc = "Generated from 'VK_NV_ray_tracing_motion_blur'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV: Self = Self(1_000_327_001);
}
#[doc = "Generated from 'VK_NV_ray_tracing_motion_blur'"]
impl StructureType {
    pub const ACCELERATION_STRUCTURE_MOTION_INFO_NV: Self = Self(1_000_327_002);
}
#[doc = "Generated from 'VK_NV_ray_tracing_motion_blur'"]
impl BuildAccelerationStructureFlagsKHR {
    pub const MOTION_NV: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_NV_ray_tracing_motion_blur'"]
impl AccelerationStructureCreateFlagsKHR {
    pub const MOTION_NV: Self = Self(0b100);
}
#[doc = "Generated from 'VK_NV_ray_tracing_motion_blur'"]
impl PipelineCreateFlags {
    pub const RAY_TRACING_ALLOW_MOTION_NV: Self = Self(0b1_0000_0000_0000_0000_0000);
}
impl NvExtension329Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_329\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension329Fn {}
unsafe impl Send for NvExtension329Fn {}
unsafe impl Sync for NvExtension329Fn {}
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
#[derive(Clone)]
pub struct NvExtension330Fn {}
unsafe impl Send for NvExtension330Fn {}
unsafe impl Sync for NvExtension330Fn {}
impl NvExtension330Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension330Fn {}
    }
}
impl ExtYcbcr2plane444FormatsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_ycbcr_2plane_444_formats\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtYcbcr2plane444FormatsFn {}
unsafe impl Send for ExtYcbcr2plane444FormatsFn {}
unsafe impl Sync for ExtYcbcr2plane444FormatsFn {}
impl ExtYcbcr2plane444FormatsFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtYcbcr2plane444FormatsFn {}
    }
}
#[doc = "Generated from 'VK_EXT_ycbcr_2plane_444_formats'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT: Self = Self(1_000_330_000);
}
#[doc = "Generated from 'VK_EXT_ycbcr_2plane_444_formats'"]
impl Format {
    pub const G8_B8R8_2PLANE_444_UNORM_EXT: Self = Self(1_000_330_000);
}
#[doc = "Generated from 'VK_EXT_ycbcr_2plane_444_formats'"]
impl Format {
    pub const G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT: Self = Self(1_000_330_001);
}
#[doc = "Generated from 'VK_EXT_ycbcr_2plane_444_formats'"]
impl Format {
    pub const G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT: Self = Self(1_000_330_002);
}
#[doc = "Generated from 'VK_EXT_ycbcr_2plane_444_formats'"]
impl Format {
    pub const G16_B16R16_2PLANE_444_UNORM_EXT: Self = Self(1_000_330_003);
}
impl NvExtension332Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_332\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension332Fn {}
unsafe impl Send for NvExtension332Fn {}
unsafe impl Sync for NvExtension332Fn {}
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
#[derive(Clone)]
pub struct ExtFragmentDensityMap2Fn {}
unsafe impl Send for ExtFragmentDensityMap2Fn {}
unsafe impl Sync for ExtFragmentDensityMap2Fn {}
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
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct QcomRotatedCopyCommandsFn {}
unsafe impl Send for QcomRotatedCopyCommandsFn {}
unsafe impl Sync for QcomRotatedCopyCommandsFn {}
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
#[derive(Clone)]
pub struct KhrExtension335Fn {}
unsafe impl Send for KhrExtension335Fn {}
unsafe impl Sync for KhrExtension335Fn {}
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
#[derive(Clone)]
pub struct ExtImageRobustnessFn {}
unsafe impl Send for ExtImageRobustnessFn {}
unsafe impl Sync for ExtImageRobustnessFn {}
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
#[derive(Clone)]
pub struct KhrWorkgroupMemoryExplicitLayoutFn {}
unsafe impl Send for KhrWorkgroupMemoryExplicitLayoutFn {}
unsafe impl Sync for KhrWorkgroupMemoryExplicitLayoutFn {}
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
pub type PFN_vkCmdCopyBuffer2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_copy_buffer_info: *const CopyBufferInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyImage2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_copy_image_info: *const CopyImageInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyBufferToImage2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_copy_buffer_to_image_info: *const CopyBufferToImageInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyImageToBuffer2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_copy_image_to_buffer_info: *const CopyImageToBufferInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBlitImage2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_blit_image_info: *const BlitImageInfo2KHR,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdResolveImage2KHR = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_resolve_image_info: *const ResolveImageInfo2KHR,
);
#[derive(Clone)]
pub struct KhrCopyCommands2Fn {
    pub cmd_copy_buffer2_khr: PFN_vkCmdCopyBuffer2KHR,
    pub cmd_copy_image2_khr: PFN_vkCmdCopyImage2KHR,
    pub cmd_copy_buffer_to_image2_khr: PFN_vkCmdCopyBufferToImage2KHR,
    pub cmd_copy_image_to_buffer2_khr: PFN_vkCmdCopyImageToBuffer2KHR,
    pub cmd_blit_image2_khr: PFN_vkCmdBlitImage2KHR,
    pub cmd_resolve_image2_khr: PFN_vkCmdResolveImage2KHR,
}
unsafe impl Send for KhrCopyCommands2Fn {}
unsafe impl Sync for KhrCopyCommands2Fn {}
impl KhrCopyCommands2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrCopyCommands2Fn {
            cmd_copy_buffer2_khr: unsafe {
                unsafe extern "system" fn cmd_copy_buffer2_khr(
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
                unsafe extern "system" fn cmd_copy_image2_khr(
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
                unsafe extern "system" fn cmd_copy_buffer_to_image2_khr(
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
                unsafe extern "system" fn cmd_copy_image_to_buffer2_khr(
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
                unsafe extern "system" fn cmd_blit_image2_khr(
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
                unsafe extern "system" fn cmd_resolve_image2_khr(
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
#[derive(Clone)]
pub struct ArmExtension339Fn {}
unsafe impl Send for ArmExtension339Fn {}
unsafe impl Sync for ArmExtension339Fn {}
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
#[derive(Clone)]
pub struct ExtExtension340Fn {}
unsafe impl Send for ExtExtension340Fn {}
unsafe impl Sync for ExtExtension340Fn {}
impl ExtExtension340Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension340Fn {}
    }
}
#[doc = "Generated from 'VK_EXT_extension_340'"]
impl ImageUsageFlags {
    pub const RESERVED_19_EXT: Self = Self(0b1000_0000_0000_0000_0000);
}
impl Ext4444FormatsFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_4444_formats\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct Ext4444FormatsFn {}
unsafe impl Send for Ext4444FormatsFn {}
unsafe impl Sync for Ext4444FormatsFn {}
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
#[derive(Clone)]
pub struct ExtExtension342Fn {}
unsafe impl Send for ExtExtension342Fn {}
unsafe impl Sync for ExtExtension342Fn {}
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
#[derive(Clone)]
pub struct ArmExtension343Fn {}
unsafe impl Send for ArmExtension343Fn {}
unsafe impl Sync for ArmExtension343Fn {}
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
#[derive(Clone)]
pub struct ArmExtension344Fn {}
unsafe impl Send for ArmExtension344Fn {}
unsafe impl Sync for ArmExtension344Fn {}
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
#[derive(Clone)]
pub struct ArmExtension345Fn {}
unsafe impl Send for ArmExtension345Fn {}
unsafe impl Sync for ArmExtension345Fn {}
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
    unsafe extern "system" fn(physical_device: PhysicalDevice, display: DisplayKHR) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetWinrtDisplayNV = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    device_relative_id: u32,
    p_display: *mut DisplayKHR,
) -> Result;
#[derive(Clone)]
pub struct NvAcquireWinrtDisplayFn {
    pub acquire_winrt_display_nv: PFN_vkAcquireWinrtDisplayNV,
    pub get_winrt_display_nv: PFN_vkGetWinrtDisplayNV,
}
unsafe impl Send for NvAcquireWinrtDisplayFn {}
unsafe impl Sync for NvAcquireWinrtDisplayFn {}
impl NvAcquireWinrtDisplayFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvAcquireWinrtDisplayFn {
            acquire_winrt_display_nv: unsafe {
                unsafe extern "system" fn acquire_winrt_display_nv(
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
                unsafe extern "system" fn get_winrt_display_nv(
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
pub type PFN_vkCreateDirectFBSurfaceEXT = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const DirectFBSurfaceCreateInfoEXT,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT =
    unsafe extern "system" fn(
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        dfb: *mut IDirectFB,
    ) -> Bool32;
#[derive(Clone)]
pub struct ExtDirectfbSurfaceFn {
    pub create_direct_fb_surface_ext: PFN_vkCreateDirectFBSurfaceEXT,
    pub get_physical_device_direct_fb_presentation_support_ext:
        PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT,
}
unsafe impl Send for ExtDirectfbSurfaceFn {}
unsafe impl Sync for ExtDirectfbSurfaceFn {}
impl ExtDirectfbSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDirectfbSurfaceFn {
            create_direct_fb_surface_ext: unsafe {
                unsafe extern "system" fn create_direct_fb_surface_ext(
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
                unsafe extern "system" fn get_physical_device_direct_fb_presentation_support_ext(
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
#[derive(Clone)]
pub struct KhrExtension350Fn {}
unsafe impl Send for KhrExtension350Fn {}
unsafe impl Sync for KhrExtension350Fn {}
impl KhrExtension350Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension350Fn {}
    }
}
impl NvExtension351Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_351\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension351Fn {}
unsafe impl Send for NvExtension351Fn {}
unsafe impl Sync for NvExtension351Fn {}
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
#[derive(Clone)]
pub struct ValveMutableDescriptorTypeFn {}
unsafe impl Send for ValveMutableDescriptorTypeFn {}
unsafe impl Sync for ValveMutableDescriptorTypeFn {}
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
impl ExtVertexInputDynamicStateFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_vertex_input_dynamic_state\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetVertexInputEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    vertex_binding_description_count: u32,
    p_vertex_binding_descriptions: *const VertexInputBindingDescription2EXT,
    vertex_attribute_description_count: u32,
    p_vertex_attribute_descriptions: *const VertexInputAttributeDescription2EXT,
);
#[derive(Clone)]
pub struct ExtVertexInputDynamicStateFn {
    pub cmd_set_vertex_input_ext: PFN_vkCmdSetVertexInputEXT,
}
unsafe impl Send for ExtVertexInputDynamicStateFn {}
unsafe impl Sync for ExtVertexInputDynamicStateFn {}
impl ExtVertexInputDynamicStateFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVertexInputDynamicStateFn {
            cmd_set_vertex_input_ext: unsafe {
                unsafe extern "system" fn cmd_set_vertex_input_ext(
                    _command_buffer: CommandBuffer,
                    _vertex_binding_description_count: u32,
                    _p_vertex_binding_descriptions: *const VertexInputBindingDescription2EXT,
                    _vertex_attribute_description_count: u32,
                    _p_vertex_attribute_descriptions: *const VertexInputAttributeDescription2EXT,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_vertex_input_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetVertexInputEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_vertex_input_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetVertexInputEXT.html>"]
    pub unsafe fn cmd_set_vertex_input_ext(
        &self,
        command_buffer: CommandBuffer,
        vertex_binding_description_count: u32,
        p_vertex_binding_descriptions: *const VertexInputBindingDescription2EXT,
        vertex_attribute_description_count: u32,
        p_vertex_attribute_descriptions: *const VertexInputAttributeDescription2EXT,
    ) {
        (self.cmd_set_vertex_input_ext)(
            command_buffer,
            vertex_binding_description_count,
            p_vertex_binding_descriptions,
            vertex_attribute_description_count,
            p_vertex_attribute_descriptions,
        )
    }
}
#[doc = "Generated from 'VK_EXT_vertex_input_dynamic_state'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT: Self = Self(1_000_352_000);
}
#[doc = "Generated from 'VK_EXT_vertex_input_dynamic_state'"]
impl StructureType {
    pub const VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT: Self = Self(1_000_352_001);
}
#[doc = "Generated from 'VK_EXT_vertex_input_dynamic_state'"]
impl StructureType {
    pub const VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT: Self = Self(1_000_352_002);
}
#[doc = "Generated from 'VK_EXT_vertex_input_dynamic_state'"]
impl DynamicState {
    pub const VERTEX_INPUT_EXT: Self = Self(1_000_352_000);
}
impl ExtPhysicalDeviceDrmFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_physical_device_drm\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtPhysicalDeviceDrmFn {}
unsafe impl Send for ExtPhysicalDeviceDrmFn {}
unsafe impl Sync for ExtPhysicalDeviceDrmFn {}
impl ExtPhysicalDeviceDrmFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPhysicalDeviceDrmFn {}
    }
}
#[doc = "Generated from 'VK_EXT_physical_device_drm'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_DRM_PROPERTIES_EXT: Self = Self(1_000_353_000);
}
impl ExtExtension355Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_355\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension355Fn {}
unsafe impl Send for ExtExtension355Fn {}
unsafe impl Sync for ExtExtension355Fn {}
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
#[derive(Clone)]
pub struct ExtVertexAttributeAliasingFn {}
unsafe impl Send for ExtVertexAttributeAliasingFn {}
unsafe impl Sync for ExtVertexAttributeAliasingFn {}
impl ExtVertexAttributeAliasingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtVertexAttributeAliasingFn {}
    }
}
impl ExtPrimitiveTopologyListRestartFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_primitive_topology_list_restart\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtPrimitiveTopologyListRestartFn {}
unsafe impl Send for ExtPrimitiveTopologyListRestartFn {}
unsafe impl Sync for ExtPrimitiveTopologyListRestartFn {}
impl ExtPrimitiveTopologyListRestartFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPrimitiveTopologyListRestartFn {}
    }
}
#[doc = "Generated from 'VK_EXT_primitive_topology_list_restart'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT: Self =
        Self(1_000_356_000);
}
impl KhrExtension358Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_358\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension358Fn {}
unsafe impl Send for KhrExtension358Fn {}
unsafe impl Sync for KhrExtension358Fn {}
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
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension359Fn {}
unsafe impl Send for ExtExtension359Fn {}
unsafe impl Sync for ExtExtension359Fn {}
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
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension360Fn {}
unsafe impl Send for ExtExtension360Fn {}
unsafe impl Sync for ExtExtension360Fn {}
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
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension361Fn {}
unsafe impl Send for KhrExtension361Fn {}
unsafe impl Sync for KhrExtension361Fn {}
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
#[derive(Clone)]
pub struct ExtExtension362Fn {}
unsafe impl Send for ExtExtension362Fn {}
unsafe impl Sync for ExtExtension362Fn {}
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
#[derive(Clone)]
pub struct ExtExtension363Fn {}
unsafe impl Send for ExtExtension363Fn {}
unsafe impl Sync for ExtExtension363Fn {}
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
#[derive(Clone)]
pub struct FuchsiaExtension364Fn {}
unsafe impl Send for FuchsiaExtension364Fn {}
unsafe impl Sync for FuchsiaExtension364Fn {}
impl FuchsiaExtension364Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExtension364Fn {}
    }
}
impl FuchsiaExternalMemoryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_external_memory\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryZirconHandleFUCHSIA = unsafe extern "system" fn(
    device: Device,
    p_get_zircon_handle_info: *const MemoryGetZirconHandleInfoFUCHSIA,
    p_zircon_handle: *mut zx_handle_t,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA = unsafe extern "system" fn(
    device: Device,
    handle_type: ExternalMemoryHandleTypeFlags,
    zircon_handle: zx_handle_t,
    p_memory_zircon_handle_properties: *mut MemoryZirconHandlePropertiesFUCHSIA,
) -> Result;
#[derive(Clone)]
pub struct FuchsiaExternalMemoryFn {
    pub get_memory_zircon_handle_fuchsia: PFN_vkGetMemoryZirconHandleFUCHSIA,
    pub get_memory_zircon_handle_properties_fuchsia: PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA,
}
unsafe impl Send for FuchsiaExternalMemoryFn {}
unsafe impl Sync for FuchsiaExternalMemoryFn {}
impl FuchsiaExternalMemoryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExternalMemoryFn {
            get_memory_zircon_handle_fuchsia: unsafe {
                unsafe extern "system" fn get_memory_zircon_handle_fuchsia(
                    _device: Device,
                    _p_get_zircon_handle_info: *const MemoryGetZirconHandleInfoFUCHSIA,
                    _p_zircon_handle: *mut zx_handle_t,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_zircon_handle_fuchsia)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetMemoryZirconHandleFUCHSIA\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_memory_zircon_handle_fuchsia
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_memory_zircon_handle_properties_fuchsia: unsafe {
                unsafe extern "system" fn get_memory_zircon_handle_properties_fuchsia(
                    _device: Device,
                    _handle_type: ExternalMemoryHandleTypeFlags,
                    _zircon_handle: zx_handle_t,
                    _p_memory_zircon_handle_properties: *mut MemoryZirconHandlePropertiesFUCHSIA,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_zircon_handle_properties_fuchsia)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetMemoryZirconHandlePropertiesFUCHSIA\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_memory_zircon_handle_properties_fuchsia
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryZirconHandleFUCHSIA.html>"]
    pub unsafe fn get_memory_zircon_handle_fuchsia(
        &self,
        device: Device,
        p_get_zircon_handle_info: *const MemoryGetZirconHandleInfoFUCHSIA,
        p_zircon_handle: *mut zx_handle_t,
    ) -> Result {
        (self.get_memory_zircon_handle_fuchsia)(device, p_get_zircon_handle_info, p_zircon_handle)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryZirconHandlePropertiesFUCHSIA.html>"]
    pub unsafe fn get_memory_zircon_handle_properties_fuchsia(
        &self,
        device: Device,
        handle_type: ExternalMemoryHandleTypeFlags,
        zircon_handle: zx_handle_t,
        p_memory_zircon_handle_properties: *mut MemoryZirconHandlePropertiesFUCHSIA,
    ) -> Result {
        (self.get_memory_zircon_handle_properties_fuchsia)(
            device,
            handle_type,
            zircon_handle,
            p_memory_zircon_handle_properties,
        )
    }
}
#[doc = "Generated from 'VK_FUCHSIA_external_memory'"]
impl StructureType {
    pub const IMPORT_MEMORY_ZIRCON_HANDLE_INFO_FUCHSIA: Self = Self(1_000_364_000);
}
#[doc = "Generated from 'VK_FUCHSIA_external_memory'"]
impl StructureType {
    pub const MEMORY_ZIRCON_HANDLE_PROPERTIES_FUCHSIA: Self = Self(1_000_364_001);
}
#[doc = "Generated from 'VK_FUCHSIA_external_memory'"]
impl StructureType {
    pub const MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA: Self = Self(1_000_364_002);
}
#[doc = "Generated from 'VK_FUCHSIA_external_memory'"]
impl ExternalMemoryHandleTypeFlags {
    pub const ZIRCON_VMO_FUCHSIA: Self = Self(0b1000_0000_0000);
}
impl FuchsiaExternalSemaphoreFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_external_semaphore\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkImportSemaphoreZirconHandleFUCHSIA = unsafe extern "system" fn(
    device: Device,
    p_import_semaphore_zircon_handle_info: *const ImportSemaphoreZirconHandleInfoFUCHSIA,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetSemaphoreZirconHandleFUCHSIA = unsafe extern "system" fn(
    device: Device,
    p_get_zircon_handle_info: *const SemaphoreGetZirconHandleInfoFUCHSIA,
    p_zircon_handle: *mut zx_handle_t,
) -> Result;
#[derive(Clone)]
pub struct FuchsiaExternalSemaphoreFn {
    pub import_semaphore_zircon_handle_fuchsia: PFN_vkImportSemaphoreZirconHandleFUCHSIA,
    pub get_semaphore_zircon_handle_fuchsia: PFN_vkGetSemaphoreZirconHandleFUCHSIA,
}
unsafe impl Send for FuchsiaExternalSemaphoreFn {}
unsafe impl Sync for FuchsiaExternalSemaphoreFn {}
impl FuchsiaExternalSemaphoreFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FuchsiaExternalSemaphoreFn {
            import_semaphore_zircon_handle_fuchsia: unsafe {
                unsafe extern "system" fn import_semaphore_zircon_handle_fuchsia(
                    _device: Device,
                    _p_import_semaphore_zircon_handle_info : * const ImportSemaphoreZirconHandleInfoFUCHSIA,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(import_semaphore_zircon_handle_fuchsia)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkImportSemaphoreZirconHandleFUCHSIA\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    import_semaphore_zircon_handle_fuchsia
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_semaphore_zircon_handle_fuchsia: unsafe {
                unsafe extern "system" fn get_semaphore_zircon_handle_fuchsia(
                    _device: Device,
                    _p_get_zircon_handle_info: *const SemaphoreGetZirconHandleInfoFUCHSIA,
                    _p_zircon_handle: *mut zx_handle_t,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_semaphore_zircon_handle_fuchsia)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetSemaphoreZirconHandleFUCHSIA\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_semaphore_zircon_handle_fuchsia
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkImportSemaphoreZirconHandleFUCHSIA.html>"]
    pub unsafe fn import_semaphore_zircon_handle_fuchsia(
        &self,
        device: Device,
        p_import_semaphore_zircon_handle_info: *const ImportSemaphoreZirconHandleInfoFUCHSIA,
    ) -> Result {
        (self.import_semaphore_zircon_handle_fuchsia)(device, p_import_semaphore_zircon_handle_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreZirconHandleFUCHSIA.html>"]
    pub unsafe fn get_semaphore_zircon_handle_fuchsia(
        &self,
        device: Device,
        p_get_zircon_handle_info: *const SemaphoreGetZirconHandleInfoFUCHSIA,
        p_zircon_handle: *mut zx_handle_t,
    ) -> Result {
        (self.get_semaphore_zircon_handle_fuchsia)(
            device,
            p_get_zircon_handle_info,
            p_zircon_handle,
        )
    }
}
#[doc = "Generated from 'VK_FUCHSIA_external_semaphore'"]
impl StructureType {
    pub const IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA: Self = Self(1_000_365_000);
}
#[doc = "Generated from 'VK_FUCHSIA_external_semaphore'"]
impl StructureType {
    pub const SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA: Self = Self(1_000_365_001);
}
#[doc = "Generated from 'VK_FUCHSIA_external_semaphore'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const ZIRCON_EVENT_FUCHSIA: Self = Self(0b1000_0000);
}
impl FuchsiaExtension367Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FUCHSIA_extension_367\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct FuchsiaExtension367Fn {}
unsafe impl Send for FuchsiaExtension367Fn {}
unsafe impl Sync for FuchsiaExtension367Fn {}
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
#[derive(Clone)]
pub struct FuchsiaExtension368Fn {}
unsafe impl Send for FuchsiaExtension368Fn {}
unsafe impl Sync for FuchsiaExtension368Fn {}
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
#[derive(Clone)]
pub struct QcomExtension369Fn {}
unsafe impl Send for QcomExtension369Fn {}
unsafe impl Sync for QcomExtension369Fn {}
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
impl HuaweiSubpassShadingFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_HUAWEI_subpass_shading\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 2u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI = unsafe extern "system" fn(
    device: Device,
    renderpass: RenderPass,
    p_max_workgroup_size: *mut Extent2D,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSubpassShadingHUAWEI = unsafe extern "system" fn(command_buffer: CommandBuffer);
#[derive(Clone)]
pub struct HuaweiSubpassShadingFn {
    pub get_device_subpass_shading_max_workgroup_size_huawei:
        PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI,
    pub cmd_subpass_shading_huawei: PFN_vkCmdSubpassShadingHUAWEI,
}
unsafe impl Send for HuaweiSubpassShadingFn {}
unsafe impl Sync for HuaweiSubpassShadingFn {}
impl HuaweiSubpassShadingFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        HuaweiSubpassShadingFn {
            get_device_subpass_shading_max_workgroup_size_huawei: unsafe {
                unsafe extern "system" fn get_device_subpass_shading_max_workgroup_size_huawei(
                    _device: Device,
                    _renderpass: RenderPass,
                    _p_max_workgroup_size: *mut Extent2D,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_subpass_shading_max_workgroup_size_huawei)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_subpass_shading_max_workgroup_size_huawei
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_subpass_shading_huawei: unsafe {
                unsafe extern "system" fn cmd_subpass_shading_huawei(
                    _command_buffer: CommandBuffer,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_subpass_shading_huawei)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSubpassShadingHUAWEI\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_subpass_shading_huawei
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI.html>"]
    pub unsafe fn get_device_subpass_shading_max_workgroup_size_huawei(
        &self,
        device: Device,
        renderpass: RenderPass,
        p_max_workgroup_size: *mut Extent2D,
    ) -> Result {
        (self.get_device_subpass_shading_max_workgroup_size_huawei)(
            device,
            renderpass,
            p_max_workgroup_size,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSubpassShadingHUAWEI.html>"]
    pub unsafe fn cmd_subpass_shading_huawei(&self, command_buffer: CommandBuffer) {
        (self.cmd_subpass_shading_huawei)(command_buffer)
    }
}
#[doc = "Generated from 'VK_HUAWEI_subpass_shading'"]
impl StructureType {
    pub const SUBPASS_SHADING_PIPELINE_CREATE_INFO_HUAWEI: Self = Self(1_000_369_000);
}
#[doc = "Generated from 'VK_HUAWEI_subpass_shading'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SUBPASS_SHADING_FEATURES_HUAWEI: Self = Self(1_000_369_001);
}
#[doc = "Generated from 'VK_HUAWEI_subpass_shading'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_SUBPASS_SHADING_PROPERTIES_HUAWEI: Self = Self(1_000_369_002);
}
#[doc = "Generated from 'VK_HUAWEI_subpass_shading'"]
impl PipelineBindPoint {
    pub const SUBPASS_SHADING_HUAWEI: Self = Self(1_000_369_003);
}
#[doc = "Generated from 'VK_HUAWEI_subpass_shading'"]
impl PipelineStageFlags2KHR {
    pub const SUBPASS_SHADING_HUAWEI: Self =
        Self(0b1000_0000_0000_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_HUAWEI_subpass_shading'"]
impl ShaderStageFlags {
    pub const SUBPASS_SHADING_HUAWEI: Self = Self(0b100_0000_0000_0000);
}
impl HuaweiInvocationMaskFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_HUAWEI_invocation_mask\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindInvocationMaskHUAWEI = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    image_view: ImageView,
    image_layout: ImageLayout,
);
#[derive(Clone)]
pub struct HuaweiInvocationMaskFn {
    pub cmd_bind_invocation_mask_huawei: PFN_vkCmdBindInvocationMaskHUAWEI,
}
unsafe impl Send for HuaweiInvocationMaskFn {}
unsafe impl Sync for HuaweiInvocationMaskFn {}
impl HuaweiInvocationMaskFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        HuaweiInvocationMaskFn {
            cmd_bind_invocation_mask_huawei: unsafe {
                unsafe extern "system" fn cmd_bind_invocation_mask_huawei(
                    _command_buffer: CommandBuffer,
                    _image_view: ImageView,
                    _image_layout: ImageLayout,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_invocation_mask_huawei)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdBindInvocationMaskHUAWEI\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_invocation_mask_huawei
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindInvocationMaskHUAWEI.html>"]
    pub unsafe fn cmd_bind_invocation_mask_huawei(
        &self,
        command_buffer: CommandBuffer,
        image_view: ImageView,
        image_layout: ImageLayout,
    ) {
        (self.cmd_bind_invocation_mask_huawei)(command_buffer, image_view, image_layout)
    }
}
#[doc = "Generated from 'VK_HUAWEI_invocation_mask'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_INVOCATION_MASK_FEATURES_HUAWEI: Self = Self(1_000_370_000);
}
#[doc = "Generated from 'VK_HUAWEI_invocation_mask'"]
impl AccessFlags2KHR {
    pub const INVOCATION_MASK_READ_HUAWEI: Self =
        Self(0b1000_0000_0000_0000_0000_0000_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_HUAWEI_invocation_mask'"]
impl ImageUsageFlags {
    pub const INVOCATION_MASK_HUAWEI: Self = Self(0b100_0000_0000_0000_0000);
}
#[doc = "Generated from 'VK_HUAWEI_invocation_mask'"]
impl PipelineStageFlags2KHR {
    pub const INVOCATION_MASK_HUAWEI: Self =
        Self(0b1_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000);
}
impl NvExternalMemoryRdmaFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_external_memory_rdma\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetMemoryRemoteAddressNV = unsafe extern "system" fn(
    device: Device,
    p_memory_get_remote_address_info: *const MemoryGetRemoteAddressInfoNV,
    p_address: *mut RemoteAddressNV,
) -> Result;
#[derive(Clone)]
pub struct NvExternalMemoryRdmaFn {
    pub get_memory_remote_address_nv: PFN_vkGetMemoryRemoteAddressNV,
}
unsafe impl Send for NvExternalMemoryRdmaFn {}
unsafe impl Sync for NvExternalMemoryRdmaFn {}
impl NvExternalMemoryRdmaFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExternalMemoryRdmaFn {
            get_memory_remote_address_nv: unsafe {
                unsafe extern "system" fn get_memory_remote_address_nv(
                    _device: Device,
                    _p_memory_get_remote_address_info: *const MemoryGetRemoteAddressInfoNV,
                    _p_address: *mut RemoteAddressNV,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_memory_remote_address_nv)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetMemoryRemoteAddressNV\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_memory_remote_address_nv
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetMemoryRemoteAddressNV.html>"]
    pub unsafe fn get_memory_remote_address_nv(
        &self,
        device: Device,
        p_memory_get_remote_address_info: *const MemoryGetRemoteAddressInfoNV,
        p_address: *mut RemoteAddressNV,
    ) -> Result {
        (self.get_memory_remote_address_nv)(device, p_memory_get_remote_address_info, p_address)
    }
}
#[doc = "Generated from 'VK_NV_external_memory_rdma'"]
impl StructureType {
    pub const MEMORY_GET_REMOTE_ADDRESS_INFO_NV: Self = Self(1_000_371_000);
}
#[doc = "Generated from 'VK_NV_external_memory_rdma'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTERNAL_MEMORY_RDMA_FEATURES_NV: Self = Self(1_000_371_001);
}
#[doc = "Generated from 'VK_NV_external_memory_rdma'"]
impl MemoryPropertyFlags {
    pub const RDMA_CAPABLE_NV: Self = Self(0b1_0000_0000);
}
#[doc = "Generated from 'VK_NV_external_memory_rdma'"]
impl ExternalMemoryHandleTypeFlags {
    pub const RDMA_ADDRESS_NV: Self = Self(0b1_0000_0000_0000);
}
impl NvExtension373Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_373\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension373Fn {}
unsafe impl Send for NvExtension373Fn {}
unsafe impl Sync for NvExtension373Fn {}
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
#[derive(Clone)]
pub struct NvExtension374Fn {}
unsafe impl Send for NvExtension374Fn {}
unsafe impl Sync for NvExtension374Fn {}
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
#[doc = "Generated from 'VK_NV_extension_374'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const RESERVED_5_NV: Self = Self(0b10_0000);
}
#[doc = "Generated from 'VK_NV_extension_374'"]
impl ExternalSemaphoreHandleTypeFlags {
    pub const RESERVED_6_NV: Self = Self(0b100_0000);
}
impl NvExtension375Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_375\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension375Fn {}
unsafe impl Send for NvExtension375Fn {}
unsafe impl Sync for NvExtension375Fn {}
impl NvExtension375Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension375Fn {}
    }
}
#[doc = "Generated from 'VK_NV_extension_375'"]
impl ExternalMemoryHandleTypeFlags {
    pub const RESERVED_13_NV: Self = Self(0b10_0000_0000_0000);
}
impl ExtExtension376Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_376\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension376Fn {}
unsafe impl Send for ExtExtension376Fn {}
unsafe impl Sync for ExtExtension376Fn {}
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
#[derive(Clone)]
pub struct ExtExtension377Fn {}
unsafe impl Send for ExtExtension377Fn {}
unsafe impl Sync for ExtExtension377Fn {}
impl ExtExtension377Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension377Fn {}
    }
}
impl ExtExtendedDynamicState2Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extended_dynamic_state2\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPatchControlPointsEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, patch_control_points: u32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetRasterizerDiscardEnableEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, rasterizer_discard_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthBiasEnableEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, depth_bias_enable: Bool32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetLogicOpEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, logic_op: LogicOp);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetPrimitiveRestartEnableEXT =
    unsafe extern "system" fn(command_buffer: CommandBuffer, primitive_restart_enable: Bool32);
#[derive(Clone)]
pub struct ExtExtendedDynamicState2Fn {
    pub cmd_set_patch_control_points_ext: PFN_vkCmdSetPatchControlPointsEXT,
    pub cmd_set_rasterizer_discard_enable_ext: PFN_vkCmdSetRasterizerDiscardEnableEXT,
    pub cmd_set_depth_bias_enable_ext: PFN_vkCmdSetDepthBiasEnableEXT,
    pub cmd_set_logic_op_ext: PFN_vkCmdSetLogicOpEXT,
    pub cmd_set_primitive_restart_enable_ext: PFN_vkCmdSetPrimitiveRestartEnableEXT,
}
unsafe impl Send for ExtExtendedDynamicState2Fn {}
unsafe impl Sync for ExtExtendedDynamicState2Fn {}
impl ExtExtendedDynamicState2Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtendedDynamicState2Fn {
            cmd_set_patch_control_points_ext: unsafe {
                unsafe extern "system" fn cmd_set_patch_control_points_ext(
                    _command_buffer: CommandBuffer,
                    _patch_control_points: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_patch_control_points_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetPatchControlPointsEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_patch_control_points_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_rasterizer_discard_enable_ext: unsafe {
                unsafe extern "system" fn cmd_set_rasterizer_discard_enable_ext(
                    _command_buffer: CommandBuffer,
                    _rasterizer_discard_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_rasterizer_discard_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetRasterizerDiscardEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_rasterizer_discard_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_depth_bias_enable_ext: unsafe {
                unsafe extern "system" fn cmd_set_depth_bias_enable_ext(
                    _command_buffer: CommandBuffer,
                    _depth_bias_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_depth_bias_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetDepthBiasEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_depth_bias_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_logic_op_ext: unsafe {
                unsafe extern "system" fn cmd_set_logic_op_ext(
                    _command_buffer: CommandBuffer,
                    _logic_op: LogicOp,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_logic_op_ext)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetLogicOpEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_logic_op_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_primitive_restart_enable_ext: unsafe {
                unsafe extern "system" fn cmd_set_primitive_restart_enable_ext(
                    _command_buffer: CommandBuffer,
                    _primitive_restart_enable: Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_primitive_restart_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetPrimitiveRestartEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_primitive_restart_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetPatchControlPointsEXT.html>"]
    pub unsafe fn cmd_set_patch_control_points_ext(
        &self,
        command_buffer: CommandBuffer,
        patch_control_points: u32,
    ) {
        (self.cmd_set_patch_control_points_ext)(command_buffer, patch_control_points)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetRasterizerDiscardEnableEXT.html>"]
    pub unsafe fn cmd_set_rasterizer_discard_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        rasterizer_discard_enable: Bool32,
    ) {
        (self.cmd_set_rasterizer_discard_enable_ext)(command_buffer, rasterizer_discard_enable)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBiasEnableEXT.html>"]
    pub unsafe fn cmd_set_depth_bias_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        depth_bias_enable: Bool32,
    ) {
        (self.cmd_set_depth_bias_enable_ext)(command_buffer, depth_bias_enable)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetLogicOpEXT.html>"]
    pub unsafe fn cmd_set_logic_op_ext(&self, command_buffer: CommandBuffer, logic_op: LogicOp) {
        (self.cmd_set_logic_op_ext)(command_buffer, logic_op)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetPrimitiveRestartEnableEXT.html>"]
    pub unsafe fn cmd_set_primitive_restart_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        primitive_restart_enable: Bool32,
    ) {
        (self.cmd_set_primitive_restart_enable_ext)(command_buffer, primitive_restart_enable)
    }
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state2'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT: Self = Self(1_000_377_000);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state2'"]
impl DynamicState {
    pub const PATCH_CONTROL_POINTS_EXT: Self = Self(1_000_377_000);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state2'"]
impl DynamicState {
    pub const RASTERIZER_DISCARD_ENABLE_EXT: Self = Self(1_000_377_001);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state2'"]
impl DynamicState {
    pub const DEPTH_BIAS_ENABLE_EXT: Self = Self(1_000_377_002);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state2'"]
impl DynamicState {
    pub const LOGIC_OP_EXT: Self = Self(1_000_377_003);
}
#[doc = "Generated from 'VK_EXT_extended_dynamic_state2'"]
impl DynamicState {
    pub const PRIMITIVE_RESTART_ENABLE_EXT: Self = Self(1_000_377_004);
}
impl QnxScreenSurfaceFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_QNX_screen_surface\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateScreenSurfaceQNX = unsafe extern "system" fn(
    instance: Instance,
    p_create_info: *const ScreenSurfaceCreateInfoQNX,
    p_allocator: *const AllocationCallbacks,
    p_surface: *mut SurfaceKHR,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    queue_family_index: u32,
    window: *mut _screen_window,
) -> Bool32;
#[derive(Clone)]
pub struct QnxScreenSurfaceFn {
    pub create_screen_surface_qnx: PFN_vkCreateScreenSurfaceQNX,
    pub get_physical_device_screen_presentation_support_qnx:
        PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX,
}
unsafe impl Send for QnxScreenSurfaceFn {}
unsafe impl Sync for QnxScreenSurfaceFn {}
impl QnxScreenSurfaceFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        QnxScreenSurfaceFn {
            create_screen_surface_qnx: unsafe {
                unsafe extern "system" fn create_screen_surface_qnx(
                    _instance: Instance,
                    _p_create_info: *const ScreenSurfaceCreateInfoQNX,
                    _p_allocator: *const AllocationCallbacks,
                    _p_surface: *mut SurfaceKHR,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_screen_surface_qnx)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateScreenSurfaceQNX\0");
                let val = _f(cname);
                if val.is_null() {
                    create_screen_surface_qnx
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_screen_presentation_support_qnx: unsafe {
                unsafe extern "system" fn get_physical_device_screen_presentation_support_qnx(
                    _physical_device: PhysicalDevice,
                    _queue_family_index: u32,
                    _window: *mut _screen_window,
                ) -> Bool32 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_screen_presentation_support_qnx)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceScreenPresentationSupportQNX\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_screen_presentation_support_qnx
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateScreenSurfaceQNX.html>"]
    pub unsafe fn create_screen_surface_qnx(
        &self,
        instance: Instance,
        p_create_info: *const ScreenSurfaceCreateInfoQNX,
        p_allocator: *const AllocationCallbacks,
        p_surface: *mut SurfaceKHR,
    ) -> Result {
        (self.create_screen_surface_qnx)(instance, p_create_info, p_allocator, p_surface)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceScreenPresentationSupportQNX.html>"]
    pub unsafe fn get_physical_device_screen_presentation_support_qnx(
        &self,
        physical_device: PhysicalDevice,
        queue_family_index: u32,
        window: *mut _screen_window,
    ) -> Bool32 {
        (self.get_physical_device_screen_presentation_support_qnx)(
            physical_device,
            queue_family_index,
            window,
        )
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
#[derive(Clone)]
pub struct KhrExtension380Fn {}
unsafe impl Send for KhrExtension380Fn {}
unsafe impl Sync for KhrExtension380Fn {}
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
#[derive(Clone)]
pub struct KhrExtension381Fn {}
unsafe impl Send for KhrExtension381Fn {}
unsafe impl Sync for KhrExtension381Fn {}
impl KhrExtension381Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension381Fn {}
    }
}
impl ExtColorWriteEnableFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_color_write_enable\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetColorWriteEnableEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    attachment_count: u32,
    p_color_write_enables: *const Bool32,
);
#[derive(Clone)]
pub struct ExtColorWriteEnableFn {
    pub cmd_set_color_write_enable_ext: PFN_vkCmdSetColorWriteEnableEXT,
}
unsafe impl Send for ExtColorWriteEnableFn {}
unsafe impl Sync for ExtColorWriteEnableFn {}
impl ExtColorWriteEnableFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtColorWriteEnableFn {
            cmd_set_color_write_enable_ext: unsafe {
                unsafe extern "system" fn cmd_set_color_write_enable_ext(
                    _command_buffer: CommandBuffer,
                    _attachment_count: u32,
                    _p_color_write_enables: *const Bool32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_color_write_enable_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetColorWriteEnableEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_color_write_enable_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetColorWriteEnableEXT.html>"]
    pub unsafe fn cmd_set_color_write_enable_ext(
        &self,
        command_buffer: CommandBuffer,
        attachment_count: u32,
        p_color_write_enables: *const Bool32,
    ) {
        (self.cmd_set_color_write_enable_ext)(
            command_buffer,
            attachment_count,
            p_color_write_enables,
        )
    }
}
#[doc = "Generated from 'VK_EXT_color_write_enable'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT: Self = Self(1_000_381_000);
}
#[doc = "Generated from 'VK_EXT_color_write_enable'"]
impl StructureType {
    pub const PIPELINE_COLOR_WRITE_CREATE_INFO_EXT: Self = Self(1_000_381_001);
}
#[doc = "Generated from 'VK_EXT_color_write_enable'"]
impl DynamicState {
    pub const COLOR_WRITE_ENABLE_EXT: Self = Self(1_000_381_000);
}
impl ExtExtension383Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_383\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension383Fn {}
unsafe impl Send for ExtExtension383Fn {}
unsafe impl Sync for ExtExtension383Fn {}
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
#[derive(Clone)]
pub struct ExtExtension384Fn {}
unsafe impl Send for ExtExtension384Fn {}
unsafe impl Sync for ExtExtension384Fn {}
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
#[derive(Clone)]
pub struct MesaExtension385Fn {}
unsafe impl Send for MesaExtension385Fn {}
unsafe impl Sync for MesaExtension385Fn {}
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
#[derive(Clone)]
pub struct GoogleExtension386Fn {}
unsafe impl Send for GoogleExtension386Fn {}
unsafe impl Sync for GoogleExtension386Fn {}
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
#[derive(Clone)]
pub struct KhrExtension387Fn {}
unsafe impl Send for KhrExtension387Fn {}
unsafe impl Sync for KhrExtension387Fn {}
impl KhrExtension387Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension387Fn {}
    }
}
impl ExtExtension388Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_388\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension388Fn {}
unsafe impl Send for ExtExtension388Fn {}
unsafe impl Sync for ExtExtension388Fn {}
impl ExtExtension388Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension388Fn {}
    }
}
impl ExtGlobalPriorityQueryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_global_priority_query\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtGlobalPriorityQueryFn {}
unsafe impl Send for ExtGlobalPriorityQueryFn {}
unsafe impl Sync for ExtGlobalPriorityQueryFn {}
impl ExtGlobalPriorityQueryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtGlobalPriorityQueryFn {}
    }
}
#[doc = "Generated from 'VK_EXT_global_priority_query'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT: Self = Self(1_000_388_000);
}
#[doc = "Generated from 'VK_EXT_global_priority_query'"]
impl StructureType {
    pub const QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT: Self = Self(1_000_388_001);
}
impl ExtExtension390Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_390\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension390Fn {}
unsafe impl Send for ExtExtension390Fn {}
unsafe impl Sync for ExtExtension390Fn {}
impl ExtExtension390Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension390Fn {}
    }
}
impl ExtExtension391Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_391\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension391Fn {}
unsafe impl Send for ExtExtension391Fn {}
unsafe impl Sync for ExtExtension391Fn {}
impl ExtExtension391Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension391Fn {}
    }
}
impl ExtExtension392Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_392\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension392Fn {}
unsafe impl Send for ExtExtension392Fn {}
unsafe impl Sync for ExtExtension392Fn {}
impl ExtExtension392Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension392Fn {}
    }
}
impl ExtMultiDrawFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_multi_draw\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawMultiEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    draw_count: u32,
    p_vertex_info: *const MultiDrawInfoEXT,
    instance_count: u32,
    first_instance: u32,
    stride: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawMultiIndexedEXT = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    draw_count: u32,
    p_index_info: *const MultiDrawIndexedInfoEXT,
    instance_count: u32,
    first_instance: u32,
    stride: u32,
    p_vertex_offset: *const i32,
);
#[derive(Clone)]
pub struct ExtMultiDrawFn {
    pub cmd_draw_multi_ext: PFN_vkCmdDrawMultiEXT,
    pub cmd_draw_multi_indexed_ext: PFN_vkCmdDrawMultiIndexedEXT,
}
unsafe impl Send for ExtMultiDrawFn {}
unsafe impl Sync for ExtMultiDrawFn {}
impl ExtMultiDrawFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtMultiDrawFn {
            cmd_draw_multi_ext: unsafe {
                unsafe extern "system" fn cmd_draw_multi_ext(
                    _command_buffer: CommandBuffer,
                    _draw_count: u32,
                    _p_vertex_info: *const MultiDrawInfoEXT,
                    _instance_count: u32,
                    _first_instance: u32,
                    _stride: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_draw_multi_ext)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawMultiEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_multi_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_multi_indexed_ext: unsafe {
                unsafe extern "system" fn cmd_draw_multi_indexed_ext(
                    _command_buffer: CommandBuffer,
                    _draw_count: u32,
                    _p_index_info: *const MultiDrawIndexedInfoEXT,
                    _instance_count: u32,
                    _first_instance: u32,
                    _stride: u32,
                    _p_vertex_offset: *const i32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_multi_indexed_ext)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawMultiIndexedEXT\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_multi_indexed_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMultiEXT.html>"]
    pub unsafe fn cmd_draw_multi_ext(
        &self,
        command_buffer: CommandBuffer,
        draw_count: u32,
        p_vertex_info: *const MultiDrawInfoEXT,
        instance_count: u32,
        first_instance: u32,
        stride: u32,
    ) {
        (self.cmd_draw_multi_ext)(
            command_buffer,
            draw_count,
            p_vertex_info,
            instance_count,
            first_instance,
            stride,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawMultiIndexedEXT.html>"]
    pub unsafe fn cmd_draw_multi_indexed_ext(
        &self,
        command_buffer: CommandBuffer,
        draw_count: u32,
        p_index_info: *const MultiDrawIndexedInfoEXT,
        instance_count: u32,
        first_instance: u32,
        stride: u32,
        p_vertex_offset: *const i32,
    ) {
        (self.cmd_draw_multi_indexed_ext)(
            command_buffer,
            draw_count,
            p_index_info,
            instance_count,
            first_instance,
            stride,
            p_vertex_offset,
        )
    }
}
#[doc = "Generated from 'VK_EXT_multi_draw'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT: Self = Self(1_000_392_000);
}
#[doc = "Generated from 'VK_EXT_multi_draw'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT: Self = Self(1_000_392_001);
}
impl ExtExtension394Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_394\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension394Fn {}
unsafe impl Send for ExtExtension394Fn {}
unsafe impl Sync for ExtExtension394Fn {}
impl ExtExtension394Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension394Fn {}
    }
}
impl KhrExtension395Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_395\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension395Fn {}
unsafe impl Send for KhrExtension395Fn {}
unsafe impl Sync for KhrExtension395Fn {}
impl KhrExtension395Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension395Fn {}
    }
}
impl KhrExtension396Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_396\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension396Fn {}
unsafe impl Send for KhrExtension396Fn {}
unsafe impl Sync for KhrExtension396Fn {}
impl KhrExtension396Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension396Fn {}
    }
}
impl NvExtension397Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_397\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension397Fn {}
unsafe impl Send for NvExtension397Fn {}
unsafe impl Sync for NvExtension397Fn {}
impl NvExtension397Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension397Fn {}
    }
}
impl NvExtension398Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_398\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension398Fn {}
unsafe impl Send for NvExtension398Fn {}
unsafe impl Sync for NvExtension398Fn {}
impl NvExtension398Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension398Fn {}
    }
}
impl JuiceExtension399Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_JUICE_extension_399\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct JuiceExtension399Fn {}
unsafe impl Send for JuiceExtension399Fn {}
unsafe impl Sync for JuiceExtension399Fn {}
impl JuiceExtension399Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        JuiceExtension399Fn {}
    }
}
impl JuiceExtension400Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_JUICE_extension_400\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct JuiceExtension400Fn {}
unsafe impl Send for JuiceExtension400Fn {}
unsafe impl Sync for JuiceExtension400Fn {}
impl JuiceExtension400Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        JuiceExtension400Fn {}
    }
}
impl ExtLoadStoreOpNoneFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_load_store_op_none\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[derive(Clone)]
pub struct ExtLoadStoreOpNoneFn {}
unsafe impl Send for ExtLoadStoreOpNoneFn {}
unsafe impl Sync for ExtLoadStoreOpNoneFn {}
impl ExtLoadStoreOpNoneFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtLoadStoreOpNoneFn {}
    }
}
#[doc = "Generated from 'VK_EXT_load_store_op_none'"]
impl AttachmentLoadOp {
    pub const NONE_EXT: Self = Self(1_000_400_000);
}
#[doc = "Generated from 'VK_EXT_load_store_op_none'"]
impl AttachmentStoreOp {
    pub const NONE_EXT: Self = Self(1_000_301_000);
}
impl FbExtension402Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FB_extension_402\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct FbExtension402Fn {}
unsafe impl Send for FbExtension402Fn {}
unsafe impl Sync for FbExtension402Fn {}
impl FbExtension402Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FbExtension402Fn {}
    }
}
impl FbExtension403Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FB_extension_403\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct FbExtension403Fn {}
unsafe impl Send for FbExtension403Fn {}
unsafe impl Sync for FbExtension403Fn {}
impl FbExtension403Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FbExtension403Fn {}
    }
}
impl FbExtension404Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_FB_extension_404\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct FbExtension404Fn {}
unsafe impl Send for FbExtension404Fn {}
unsafe impl Sync for FbExtension404Fn {}
impl FbExtension404Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        FbExtension404Fn {}
    }
}
impl HuaweiExtension405Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_HUAWEI_extension_405\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct HuaweiExtension405Fn {}
unsafe impl Send for HuaweiExtension405Fn {}
unsafe impl Sync for HuaweiExtension405Fn {}
impl HuaweiExtension405Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        HuaweiExtension405Fn {}
    }
}
impl HuaweiExtension406Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_HUAWEI_extension_406\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct HuaweiExtension406Fn {}
unsafe impl Send for HuaweiExtension406Fn {}
unsafe impl Sync for HuaweiExtension406Fn {}
impl HuaweiExtension406Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        HuaweiExtension406Fn {}
    }
}
impl GgpExtension407Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_extension_407\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct GgpExtension407Fn {}
unsafe impl Send for GgpExtension407Fn {}
unsafe impl Sync for GgpExtension407Fn {}
impl GgpExtension407Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpExtension407Fn {}
    }
}
impl GgpExtension408Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_extension_408\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct GgpExtension408Fn {}
unsafe impl Send for GgpExtension408Fn {}
unsafe impl Sync for GgpExtension408Fn {}
impl GgpExtension408Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpExtension408Fn {}
    }
}
impl GgpExtension409Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_extension_409\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct GgpExtension409Fn {}
unsafe impl Send for GgpExtension409Fn {}
unsafe impl Sync for GgpExtension409Fn {}
impl GgpExtension409Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpExtension409Fn {}
    }
}
impl GgpExtension410Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_extension_410\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct GgpExtension410Fn {}
unsafe impl Send for GgpExtension410Fn {}
unsafe impl Sync for GgpExtension410Fn {}
impl GgpExtension410Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpExtension410Fn {}
    }
}
impl GgpExtension411Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_GGP_extension_411\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct GgpExtension411Fn {}
unsafe impl Send for GgpExtension411Fn {}
unsafe impl Sync for GgpExtension411Fn {}
impl GgpExtension411Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        GgpExtension411Fn {}
    }
}
impl NvExtension412Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_412\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension412Fn {}
unsafe impl Send for NvExtension412Fn {}
unsafe impl Sync for NvExtension412Fn {}
impl NvExtension412Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension412Fn {}
    }
}
impl ExtPageableDeviceLocalMemoryFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_pageable_device_local_memory\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 1u32;
}
#[allow(non_camel_case_types)]
pub type PFN_vkSetDeviceMemoryPriorityEXT =
    unsafe extern "system" fn(device: Device, memory: DeviceMemory, priority: f32);
#[derive(Clone)]
pub struct ExtPageableDeviceLocalMemoryFn {
    pub set_device_memory_priority_ext: PFN_vkSetDeviceMemoryPriorityEXT,
}
unsafe impl Send for ExtPageableDeviceLocalMemoryFn {}
unsafe impl Sync for ExtPageableDeviceLocalMemoryFn {}
impl ExtPageableDeviceLocalMemoryFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtPageableDeviceLocalMemoryFn {
            set_device_memory_priority_ext: unsafe {
                unsafe extern "system" fn set_device_memory_priority_ext(
                    _device: Device,
                    _memory: DeviceMemory,
                    _priority: f32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(set_device_memory_priority_ext)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkSetDeviceMemoryPriorityEXT\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    set_device_memory_priority_ext
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetDeviceMemoryPriorityEXT.html>"]
    pub unsafe fn set_device_memory_priority_ext(
        &self,
        device: Device,
        memory: DeviceMemory,
        priority: f32,
    ) {
        (self.set_device_memory_priority_ext)(device, memory, priority)
    }
}
#[doc = "Generated from 'VK_EXT_pageable_device_local_memory'"]
impl StructureType {
    pub const PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT: Self = Self(1_000_412_000);
}
impl NvExtension414Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_NV_extension_414\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct NvExtension414Fn {}
unsafe impl Send for NvExtension414Fn {}
unsafe impl Sync for NvExtension414Fn {}
impl NvExtension414Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        NvExtension414Fn {}
    }
}
impl HuaweiExtension415Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_HUAWEI_extension_415\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct HuaweiExtension415Fn {}
unsafe impl Send for HuaweiExtension415Fn {}
unsafe impl Sync for HuaweiExtension415Fn {}
impl HuaweiExtension415Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        HuaweiExtension415Fn {}
    }
}
impl ArmExtension416Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ARM_extension_416\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ArmExtension416Fn {}
unsafe impl Send for ArmExtension416Fn {}
unsafe impl Sync for ArmExtension416Fn {}
impl ArmExtension416Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ArmExtension416Fn {}
    }
}
impl KhrExtension417Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_417\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension417Fn {}
unsafe impl Send for KhrExtension417Fn {}
unsafe impl Sync for KhrExtension417Fn {}
impl KhrExtension417Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension417Fn {}
    }
}
impl ArmExtension418Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_ARM_extension_418\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ArmExtension418Fn {}
unsafe impl Send for ArmExtension418Fn {}
unsafe impl Sync for ArmExtension418Fn {}
impl ArmExtension418Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ArmExtension418Fn {}
    }
}
impl ExtExtension419Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_419\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension419Fn {}
unsafe impl Send for ExtExtension419Fn {}
unsafe impl Sync for ExtExtension419Fn {}
impl ExtExtension419Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension419Fn {}
    }
}
impl ExtExtension420Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_420\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension420Fn {}
unsafe impl Send for ExtExtension420Fn {}
unsafe impl Sync for ExtExtension420Fn {}
impl ExtExtension420Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension420Fn {}
    }
}
impl KhrExtension421Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_KHR_extension_421\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct KhrExtension421Fn {}
unsafe impl Send for KhrExtension421Fn {}
unsafe impl Sync for KhrExtension421Fn {}
impl KhrExtension421Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        KhrExtension421Fn {}
    }
}
impl ExtExtension422Fn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_extension_422\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtExtension422Fn {}
unsafe impl Send for ExtExtension422Fn {}
unsafe impl Sync for ExtExtension422Fn {}
impl ExtExtension422Fn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtExtension422Fn {}
    }
}
impl ExtDisableCubeMapWrapFn {
    pub fn name() -> &'static ::std::ffi::CStr {
        ::std::ffi::CStr::from_bytes_with_nul(b"VK_EXT_disable_cube_map_wrap\0")
            .expect("Wrong extension string")
    }
    pub const SPEC_VERSION: u32 = 0u32;
}
#[derive(Clone)]
pub struct ExtDisableCubeMapWrapFn {}
unsafe impl Send for ExtDisableCubeMapWrapFn {}
unsafe impl Sync for ExtDisableCubeMapWrapFn {}
impl ExtDisableCubeMapWrapFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        ExtDisableCubeMapWrapFn {}
    }
}
#[doc = "Generated from 'VK_EXT_disable_cube_map_wrap'"]
impl SamplerCreateFlags {
    pub const RESERVED_2_EXT: Self = Self(0b100);
}
