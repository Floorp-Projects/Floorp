use crate::vk::bitflags::*;
use crate::vk::definitions::*;
use crate::vk::enums::*;
use std::os::raw::*;
#[allow(non_camel_case_types)]
pub type PFN_vkGetInstanceProcAddr =
    unsafe extern "system" fn(instance: Instance, p_name: *const c_char) -> PFN_vkVoidFunction;
#[derive(Clone)]
pub struct StaticFn {
    pub get_instance_proc_addr: PFN_vkGetInstanceProcAddr,
}
unsafe impl Send for StaticFn {}
unsafe impl Sync for StaticFn {}
impl StaticFn {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            get_instance_proc_addr: unsafe {
                unsafe extern "system" fn get_instance_proc_addr(
                    _instance: Instance,
                    _p_name: *const c_char,
                ) -> PFN_vkVoidFunction {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_instance_proc_addr)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetInstanceProcAddr\0");
                let val = _f(cname);
                if val.is_null() {
                    get_instance_proc_addr
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetInstanceProcAddr.html>"]
    pub unsafe fn get_instance_proc_addr(
        &self,
        instance: Instance,
        p_name: *const c_char,
    ) -> PFN_vkVoidFunction {
        (self.get_instance_proc_addr)(instance, p_name)
    }
}
#[allow(non_camel_case_types)]
pub type PFN_vkCreateInstance = unsafe extern "system" fn(
    p_create_info: *const InstanceCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_instance: *mut Instance,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkEnumerateInstanceExtensionProperties = unsafe extern "system" fn(
    p_layer_name: *const c_char,
    p_property_count: *mut u32,
    p_properties: *mut ExtensionProperties,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkEnumerateInstanceLayerProperties = unsafe extern "system" fn(
    p_property_count: *mut u32,
    p_properties: *mut LayerProperties,
) -> Result;
#[derive(Clone)]
pub struct EntryFnV1_0 {
    pub create_instance: PFN_vkCreateInstance,
    pub enumerate_instance_extension_properties: PFN_vkEnumerateInstanceExtensionProperties,
    pub enumerate_instance_layer_properties: PFN_vkEnumerateInstanceLayerProperties,
}
unsafe impl Send for EntryFnV1_0 {}
unsafe impl Sync for EntryFnV1_0 {}
impl EntryFnV1_0 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            create_instance: unsafe {
                unsafe extern "system" fn create_instance(
                    _p_create_info: *const InstanceCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_instance: *mut Instance,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_instance)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateInstance\0");
                let val = _f(cname);
                if val.is_null() {
                    create_instance
                } else {
                    ::std::mem::transmute(val)
                }
            },
            enumerate_instance_extension_properties: unsafe {
                unsafe extern "system" fn enumerate_instance_extension_properties(
                    _p_layer_name: *const c_char,
                    _p_property_count: *mut u32,
                    _p_properties: *mut ExtensionProperties,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_instance_extension_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumerateInstanceExtensionProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_instance_extension_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            enumerate_instance_layer_properties: unsafe {
                unsafe extern "system" fn enumerate_instance_layer_properties(
                    _p_property_count: *mut u32,
                    _p_properties: *mut LayerProperties,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_instance_layer_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumerateInstanceLayerProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_instance_layer_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateInstance.html>"]
    pub unsafe fn create_instance(
        &self,
        p_create_info: *const InstanceCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_instance: *mut Instance,
    ) -> Result {
        (self.create_instance)(p_create_info, p_allocator, p_instance)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceExtensionProperties.html>"]
    pub unsafe fn enumerate_instance_extension_properties(
        &self,
        p_layer_name: *const c_char,
        p_property_count: *mut u32,
        p_properties: *mut ExtensionProperties,
    ) -> Result {
        (self.enumerate_instance_extension_properties)(p_layer_name, p_property_count, p_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceLayerProperties.html>"]
    pub unsafe fn enumerate_instance_layer_properties(
        &self,
        p_property_count: *mut u32,
        p_properties: *mut LayerProperties,
    ) -> Result {
        (self.enumerate_instance_layer_properties)(p_property_count, p_properties)
    }
}
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyInstance =
    unsafe extern "system" fn(instance: Instance, p_allocator: *const AllocationCallbacks);
#[allow(non_camel_case_types)]
pub type PFN_vkEnumeratePhysicalDevices = unsafe extern "system" fn(
    instance: Instance,
    p_physical_device_count: *mut u32,
    p_physical_devices: *mut PhysicalDevice,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceFeatures = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_features: *mut PhysicalDeviceFeatures,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceFormatProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    format: Format,
    p_format_properties: *mut FormatProperties,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceImageFormatProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    format: Format,
    ty: ImageType,
    tiling: ImageTiling,
    usage: ImageUsageFlags,
    flags: ImageCreateFlags,
    p_image_format_properties: *mut ImageFormatProperties,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_properties: *mut PhysicalDeviceProperties,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceQueueFamilyProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_queue_family_property_count: *mut u32,
    p_queue_family_properties: *mut QueueFamilyProperties,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceMemoryProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_memory_properties: *mut PhysicalDeviceMemoryProperties,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceProcAddr =
    unsafe extern "system" fn(device: Device, p_name: *const c_char) -> PFN_vkVoidFunction;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDevice = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_create_info: *const DeviceCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_device: *mut Device,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkEnumerateDeviceExtensionProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_layer_name: *const c_char,
    p_property_count: *mut u32,
    p_properties: *mut ExtensionProperties,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkEnumerateDeviceLayerProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    p_property_count: *mut u32,
    p_properties: *mut LayerProperties,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetPhysicalDeviceSparseImageFormatProperties = unsafe extern "system" fn(
    physical_device: PhysicalDevice,
    format: Format,
    ty: ImageType,
    samples: SampleCountFlags,
    usage: ImageUsageFlags,
    tiling: ImageTiling,
    p_property_count: *mut u32,
    p_properties: *mut SparseImageFormatProperties,
);
#[derive(Clone)]
pub struct InstanceFnV1_0 {
    pub destroy_instance: PFN_vkDestroyInstance,
    pub enumerate_physical_devices: PFN_vkEnumeratePhysicalDevices,
    pub get_physical_device_features: PFN_vkGetPhysicalDeviceFeatures,
    pub get_physical_device_format_properties: PFN_vkGetPhysicalDeviceFormatProperties,
    pub get_physical_device_image_format_properties: PFN_vkGetPhysicalDeviceImageFormatProperties,
    pub get_physical_device_properties: PFN_vkGetPhysicalDeviceProperties,
    pub get_physical_device_queue_family_properties: PFN_vkGetPhysicalDeviceQueueFamilyProperties,
    pub get_physical_device_memory_properties: PFN_vkGetPhysicalDeviceMemoryProperties,
    pub get_device_proc_addr: PFN_vkGetDeviceProcAddr,
    pub create_device: PFN_vkCreateDevice,
    pub enumerate_device_extension_properties: PFN_vkEnumerateDeviceExtensionProperties,
    pub enumerate_device_layer_properties: PFN_vkEnumerateDeviceLayerProperties,
    pub get_physical_device_sparse_image_format_properties:
        PFN_vkGetPhysicalDeviceSparseImageFormatProperties,
}
unsafe impl Send for InstanceFnV1_0 {}
unsafe impl Sync for InstanceFnV1_0 {}
impl InstanceFnV1_0 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            destroy_instance: unsafe {
                unsafe extern "system" fn destroy_instance(
                    _instance: Instance,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_instance)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyInstance\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_instance
                } else {
                    ::std::mem::transmute(val)
                }
            },
            enumerate_physical_devices: unsafe {
                unsafe extern "system" fn enumerate_physical_devices(
                    _instance: Instance,
                    _p_physical_device_count: *mut u32,
                    _p_physical_devices: *mut PhysicalDevice,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_physical_devices)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumeratePhysicalDevices\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_physical_devices
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_features: unsafe {
                unsafe extern "system" fn get_physical_device_features(
                    _physical_device: PhysicalDevice,
                    _p_features: *mut PhysicalDeviceFeatures,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_features)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceFeatures\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_features
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_format_properties: unsafe {
                unsafe extern "system" fn get_physical_device_format_properties(
                    _physical_device: PhysicalDevice,
                    _format: Format,
                    _p_format_properties: *mut FormatProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_format_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceFormatProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_format_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_image_format_properties: unsafe {
                unsafe extern "system" fn get_physical_device_image_format_properties(
                    _physical_device: PhysicalDevice,
                    _format: Format,
                    _ty: ImageType,
                    _tiling: ImageTiling,
                    _usage: ImageUsageFlags,
                    _flags: ImageCreateFlags,
                    _p_image_format_properties: *mut ImageFormatProperties,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_image_format_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceImageFormatProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_image_format_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_properties: unsafe {
                unsafe extern "system" fn get_physical_device_properties(
                    _physical_device: PhysicalDevice,
                    _p_properties: *mut PhysicalDeviceProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_queue_family_properties: unsafe {
                unsafe extern "system" fn get_physical_device_queue_family_properties(
                    _physical_device: PhysicalDevice,
                    _p_queue_family_property_count: *mut u32,
                    _p_queue_family_properties: *mut QueueFamilyProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_queue_family_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceQueueFamilyProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_queue_family_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_memory_properties: unsafe {
                unsafe extern "system" fn get_physical_device_memory_properties(
                    _physical_device: PhysicalDevice,
                    _p_memory_properties: *mut PhysicalDeviceMemoryProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_memory_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceMemoryProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_memory_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_proc_addr: unsafe {
                unsafe extern "system" fn get_device_proc_addr(
                    _device: Device,
                    _p_name: *const c_char,
                ) -> PFN_vkVoidFunction {
                    panic!(concat!("Unable to load ", stringify!(get_device_proc_addr)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetDeviceProcAddr\0");
                let val = _f(cname);
                if val.is_null() {
                    get_device_proc_addr
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_device: unsafe {
                unsafe extern "system" fn create_device(
                    _physical_device: PhysicalDevice,
                    _p_create_info: *const DeviceCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_device: *mut Device,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_device)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateDevice\0");
                let val = _f(cname);
                if val.is_null() {
                    create_device
                } else {
                    ::std::mem::transmute(val)
                }
            },
            enumerate_device_extension_properties: unsafe {
                unsafe extern "system" fn enumerate_device_extension_properties(
                    _physical_device: PhysicalDevice,
                    _p_layer_name: *const c_char,
                    _p_property_count: *mut u32,
                    _p_properties: *mut ExtensionProperties,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_device_extension_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumerateDeviceExtensionProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_device_extension_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            enumerate_device_layer_properties: unsafe {
                unsafe extern "system" fn enumerate_device_layer_properties(
                    _physical_device: PhysicalDevice,
                    _p_property_count: *mut u32,
                    _p_properties: *mut LayerProperties,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_device_layer_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumerateDeviceLayerProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_device_layer_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_sparse_image_format_properties: unsafe {
                unsafe extern "system" fn get_physical_device_sparse_image_format_properties(
                    _physical_device: PhysicalDevice,
                    _format: Format,
                    _ty: ImageType,
                    _samples: SampleCountFlags,
                    _usage: ImageUsageFlags,
                    _tiling: ImageTiling,
                    _p_property_count: *mut u32,
                    _p_properties: *mut SparseImageFormatProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_sparse_image_format_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSparseImageFormatProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_sparse_image_format_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyInstance.html>"]
    pub unsafe fn destroy_instance(
        &self,
        instance: Instance,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_instance)(instance, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumeratePhysicalDevices.html>"]
    pub unsafe fn enumerate_physical_devices(
        &self,
        instance: Instance,
        p_physical_device_count: *mut u32,
        p_physical_devices: *mut PhysicalDevice,
    ) -> Result {
        (self.enumerate_physical_devices)(instance, p_physical_device_count, p_physical_devices)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFeatures.html>"]
    pub unsafe fn get_physical_device_features(
        &self,
        physical_device: PhysicalDevice,
        p_features: *mut PhysicalDeviceFeatures,
    ) {
        (self.get_physical_device_features)(physical_device, p_features)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFormatProperties.html>"]
    pub unsafe fn get_physical_device_format_properties(
        &self,
        physical_device: PhysicalDevice,
        format: Format,
        p_format_properties: *mut FormatProperties,
    ) {
        (self.get_physical_device_format_properties)(physical_device, format, p_format_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties.html>"]
    pub unsafe fn get_physical_device_image_format_properties(
        &self,
        physical_device: PhysicalDevice,
        format: Format,
        ty: ImageType,
        tiling: ImageTiling,
        usage: ImageUsageFlags,
        flags: ImageCreateFlags,
        p_image_format_properties: *mut ImageFormatProperties,
    ) -> Result {
        (self.get_physical_device_image_format_properties)(
            physical_device,
            format,
            ty,
            tiling,
            usage,
            flags,
            p_image_format_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceProperties.html>"]
    pub unsafe fn get_physical_device_properties(
        &self,
        physical_device: PhysicalDevice,
        p_properties: *mut PhysicalDeviceProperties,
    ) {
        (self.get_physical_device_properties)(physical_device, p_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties.html>"]
    pub unsafe fn get_physical_device_queue_family_properties(
        &self,
        physical_device: PhysicalDevice,
        p_queue_family_property_count: *mut u32,
        p_queue_family_properties: *mut QueueFamilyProperties,
    ) {
        (self.get_physical_device_queue_family_properties)(
            physical_device,
            p_queue_family_property_count,
            p_queue_family_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceMemoryProperties.html>"]
    pub unsafe fn get_physical_device_memory_properties(
        &self,
        physical_device: PhysicalDevice,
        p_memory_properties: *mut PhysicalDeviceMemoryProperties,
    ) {
        (self.get_physical_device_memory_properties)(physical_device, p_memory_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceProcAddr.html>"]
    pub unsafe fn get_device_proc_addr(
        &self,
        device: Device,
        p_name: *const c_char,
    ) -> PFN_vkVoidFunction {
        (self.get_device_proc_addr)(device, p_name)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDevice.html>"]
    pub unsafe fn create_device(
        &self,
        physical_device: PhysicalDevice,
        p_create_info: *const DeviceCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_device: *mut Device,
    ) -> Result {
        (self.create_device)(physical_device, p_create_info, p_allocator, p_device)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateDeviceExtensionProperties.html>"]
    pub unsafe fn enumerate_device_extension_properties(
        &self,
        physical_device: PhysicalDevice,
        p_layer_name: *const c_char,
        p_property_count: *mut u32,
        p_properties: *mut ExtensionProperties,
    ) -> Result {
        (self.enumerate_device_extension_properties)(
            physical_device,
            p_layer_name,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateDeviceLayerProperties.html>"]
    pub unsafe fn enumerate_device_layer_properties(
        &self,
        physical_device: PhysicalDevice,
        p_property_count: *mut u32,
        p_properties: *mut LayerProperties,
    ) -> Result {
        (self.enumerate_device_layer_properties)(physical_device, p_property_count, p_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSparseImageFormatProperties.html>"]
    pub unsafe fn get_physical_device_sparse_image_format_properties(
        &self,
        physical_device: PhysicalDevice,
        format: Format,
        ty: ImageType,
        samples: SampleCountFlags,
        usage: ImageUsageFlags,
        tiling: ImageTiling,
        p_property_count: *mut u32,
        p_properties: *mut SparseImageFormatProperties,
    ) {
        (self.get_physical_device_sparse_image_format_properties)(
            physical_device,
            format,
            ty,
            samples,
            usage,
            tiling,
            p_property_count,
            p_properties,
        )
    }
}
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDevice =
    unsafe extern "system" fn(device: Device, p_allocator: *const AllocationCallbacks);
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceQueue = unsafe extern "system" fn(
    device: Device,
    queue_family_index: u32,
    queue_index: u32,
    p_queue: *mut Queue,
);
#[allow(non_camel_case_types)]
pub type PFN_vkQueueSubmit = unsafe extern "system" fn(
    queue: Queue,
    submit_count: u32,
    p_submits: *const SubmitInfo,
    fence: Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkQueueWaitIdle = unsafe extern "system" fn(queue: Queue) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDeviceWaitIdle = unsafe extern "system" fn(device: Device) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAllocateMemory = unsafe extern "system" fn(
    device: Device,
    p_allocate_info: *const MemoryAllocateInfo,
    p_allocator: *const AllocationCallbacks,
    p_memory: *mut DeviceMemory,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkFreeMemory = unsafe extern "system" fn(
    device: Device,
    memory: DeviceMemory,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkMapMemory = unsafe extern "system" fn(
    device: Device,
    memory: DeviceMemory,
    offset: DeviceSize,
    size: DeviceSize,
    flags: MemoryMapFlags,
    pp_data: *mut *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkUnmapMemory = unsafe extern "system" fn(device: Device, memory: DeviceMemory);
#[allow(non_camel_case_types)]
pub type PFN_vkFlushMappedMemoryRanges = unsafe extern "system" fn(
    device: Device,
    memory_range_count: u32,
    p_memory_ranges: *const MappedMemoryRange,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkInvalidateMappedMemoryRanges = unsafe extern "system" fn(
    device: Device,
    memory_range_count: u32,
    p_memory_ranges: *const MappedMemoryRange,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceMemoryCommitment = unsafe extern "system" fn(
    device: Device,
    memory: DeviceMemory,
    p_committed_memory_in_bytes: *mut DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkBindBufferMemory = unsafe extern "system" fn(
    device: Device,
    buffer: Buffer,
    memory: DeviceMemory,
    memory_offset: DeviceSize,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkBindImageMemory = unsafe extern "system" fn(
    device: Device,
    image: Image,
    memory: DeviceMemory,
    memory_offset: DeviceSize,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetBufferMemoryRequirements = unsafe extern "system" fn(
    device: Device,
    buffer: Buffer,
    p_memory_requirements: *mut MemoryRequirements,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageMemoryRequirements = unsafe extern "system" fn(
    device: Device,
    image: Image,
    p_memory_requirements: *mut MemoryRequirements,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageSparseMemoryRequirements = unsafe extern "system" fn(
    device: Device,
    image: Image,
    p_sparse_memory_requirement_count: *mut u32,
    p_sparse_memory_requirements: *mut SparseImageMemoryRequirements,
);
#[allow(non_camel_case_types)]
pub type PFN_vkQueueBindSparse = unsafe extern "system" fn(
    queue: Queue,
    bind_info_count: u32,
    p_bind_info: *const BindSparseInfo,
    fence: Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateFence = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const FenceCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_fence: *mut Fence,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyFence = unsafe extern "system" fn(
    device: Device,
    fence: Fence,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkResetFences =
    unsafe extern "system" fn(device: Device, fence_count: u32, p_fences: *const Fence) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkGetFenceStatus = unsafe extern "system" fn(device: Device, fence: Fence) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkWaitForFences = unsafe extern "system" fn(
    device: Device,
    fence_count: u32,
    p_fences: *const Fence,
    wait_all: Bool32,
    timeout: u64,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateSemaphore = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const SemaphoreCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_semaphore: *mut Semaphore,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroySemaphore = unsafe extern "system" fn(
    device: Device,
    semaphore: Semaphore,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateEvent = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const EventCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_event: *mut Event,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyEvent = unsafe extern "system" fn(
    device: Device,
    event: Event,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetEventStatus = unsafe extern "system" fn(device: Device, event: Event) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkSetEvent = unsafe extern "system" fn(device: Device, event: Event) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkResetEvent = unsafe extern "system" fn(device: Device, event: Event) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateQueryPool = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const QueryPoolCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_query_pool: *mut QueryPool,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyQueryPool = unsafe extern "system" fn(
    device: Device,
    query_pool: QueryPool,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetQueryPoolResults = unsafe extern "system" fn(
    device: Device,
    query_pool: QueryPool,
    first_query: u32,
    query_count: u32,
    data_size: usize,
    p_data: *mut c_void,
    stride: DeviceSize,
    flags: QueryResultFlags,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateBuffer = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const BufferCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_buffer: *mut Buffer,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyBuffer = unsafe extern "system" fn(
    device: Device,
    buffer: Buffer,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateBufferView = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const BufferViewCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_view: *mut BufferView,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyBufferView = unsafe extern "system" fn(
    device: Device,
    buffer_view: BufferView,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateImage = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const ImageCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_image: *mut Image,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyImage = unsafe extern "system" fn(
    device: Device,
    image: Image,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetImageSubresourceLayout = unsafe extern "system" fn(
    device: Device,
    image: Image,
    p_subresource: *const ImageSubresource,
    p_layout: *mut SubresourceLayout,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateImageView = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const ImageViewCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_view: *mut ImageView,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyImageView = unsafe extern "system" fn(
    device: Device,
    image_view: ImageView,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateShaderModule = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const ShaderModuleCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_shader_module: *mut ShaderModule,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyShaderModule = unsafe extern "system" fn(
    device: Device,
    shader_module: ShaderModule,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreatePipelineCache = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const PipelineCacheCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_pipeline_cache: *mut PipelineCache,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyPipelineCache = unsafe extern "system" fn(
    device: Device,
    pipeline_cache: PipelineCache,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetPipelineCacheData = unsafe extern "system" fn(
    device: Device,
    pipeline_cache: PipelineCache,
    p_data_size: *mut usize,
    p_data: *mut c_void,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkMergePipelineCaches = unsafe extern "system" fn(
    device: Device,
    dst_cache: PipelineCache,
    src_cache_count: u32,
    p_src_caches: *const PipelineCache,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateGraphicsPipelines = unsafe extern "system" fn(
    device: Device,
    pipeline_cache: PipelineCache,
    create_info_count: u32,
    p_create_infos: *const GraphicsPipelineCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_pipelines: *mut Pipeline,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCreateComputePipelines = unsafe extern "system" fn(
    device: Device,
    pipeline_cache: PipelineCache,
    create_info_count: u32,
    p_create_infos: *const ComputePipelineCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_pipelines: *mut Pipeline,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyPipeline = unsafe extern "system" fn(
    device: Device,
    pipeline: Pipeline,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreatePipelineLayout = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const PipelineLayoutCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_pipeline_layout: *mut PipelineLayout,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyPipelineLayout = unsafe extern "system" fn(
    device: Device,
    pipeline_layout: PipelineLayout,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateSampler = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const SamplerCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_sampler: *mut Sampler,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroySampler = unsafe extern "system" fn(
    device: Device,
    sampler: Sampler,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDescriptorSetLayout = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const DescriptorSetLayoutCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_set_layout: *mut DescriptorSetLayout,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDescriptorSetLayout = unsafe extern "system" fn(
    device: Device,
    descriptor_set_layout: DescriptorSetLayout,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateDescriptorPool = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const DescriptorPoolCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_descriptor_pool: *mut DescriptorPool,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyDescriptorPool = unsafe extern "system" fn(
    device: Device,
    descriptor_pool: DescriptorPool,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkResetDescriptorPool = unsafe extern "system" fn(
    device: Device,
    descriptor_pool: DescriptorPool,
    flags: DescriptorPoolResetFlags,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAllocateDescriptorSets = unsafe extern "system" fn(
    device: Device,
    p_allocate_info: *const DescriptorSetAllocateInfo,
    p_descriptor_sets: *mut DescriptorSet,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkFreeDescriptorSets = unsafe extern "system" fn(
    device: Device,
    descriptor_pool: DescriptorPool,
    descriptor_set_count: u32,
    p_descriptor_sets: *const DescriptorSet,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkUpdateDescriptorSets = unsafe extern "system" fn(
    device: Device,
    descriptor_write_count: u32,
    p_descriptor_writes: *const WriteDescriptorSet,
    descriptor_copy_count: u32,
    p_descriptor_copies: *const CopyDescriptorSet,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateFramebuffer = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const FramebufferCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_framebuffer: *mut Framebuffer,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyFramebuffer = unsafe extern "system" fn(
    device: Device,
    framebuffer: Framebuffer,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateRenderPass = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const RenderPassCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_render_pass: *mut RenderPass,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyRenderPass = unsafe extern "system" fn(
    device: Device,
    render_pass: RenderPass,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkGetRenderAreaGranularity = unsafe extern "system" fn(
    device: Device,
    render_pass: RenderPass,
    p_granularity: *mut Extent2D,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCreateCommandPool = unsafe extern "system" fn(
    device: Device,
    p_create_info: *const CommandPoolCreateInfo,
    p_allocator: *const AllocationCallbacks,
    p_command_pool: *mut CommandPool,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkDestroyCommandPool = unsafe extern "system" fn(
    device: Device,
    command_pool: CommandPool,
    p_allocator: *const AllocationCallbacks,
);
#[allow(non_camel_case_types)]
pub type PFN_vkResetCommandPool = unsafe extern "system" fn(
    device: Device,
    command_pool: CommandPool,
    flags: CommandPoolResetFlags,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkAllocateCommandBuffers = unsafe extern "system" fn(
    device: Device,
    p_allocate_info: *const CommandBufferAllocateInfo,
    p_command_buffers: *mut CommandBuffer,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkFreeCommandBuffers = unsafe extern "system" fn(
    device: Device,
    command_pool: CommandPool,
    command_buffer_count: u32,
    p_command_buffers: *const CommandBuffer,
);
#[allow(non_camel_case_types)]
pub type PFN_vkBeginCommandBuffer = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_begin_info: *const CommandBufferBeginInfo,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkEndCommandBuffer =
    unsafe extern "system" fn(command_buffer: CommandBuffer) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkResetCommandBuffer = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    flags: CommandBufferResetFlags,
) -> Result;
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindPipeline = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_bind_point: PipelineBindPoint,
    pipeline: Pipeline,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetViewport = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_viewport: u32,
    viewport_count: u32,
    p_viewports: *const Viewport,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetScissor = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_scissor: u32,
    scissor_count: u32,
    p_scissors: *const Rect2D,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetLineWidth =
    unsafe extern "system" fn(command_buffer: CommandBuffer, line_width: f32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthBias = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    depth_bias_constant_factor: f32,
    depth_bias_clamp: f32,
    depth_bias_slope_factor: f32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetBlendConstants =
    unsafe extern "system" fn(command_buffer: CommandBuffer, blend_constants: *const [f32; 4]);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetDepthBounds = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    min_depth_bounds: f32,
    max_depth_bounds: f32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetStencilCompareMask = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    face_mask: StencilFaceFlags,
    compare_mask: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetStencilWriteMask = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    face_mask: StencilFaceFlags,
    write_mask: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetStencilReference = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    face_mask: StencilFaceFlags,
    reference: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindDescriptorSets = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_bind_point: PipelineBindPoint,
    layout: PipelineLayout,
    first_set: u32,
    descriptor_set_count: u32,
    p_descriptor_sets: *const DescriptorSet,
    dynamic_offset_count: u32,
    p_dynamic_offsets: *const u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindIndexBuffer = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    index_type: IndexType,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBindVertexBuffers = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    first_binding: u32,
    binding_count: u32,
    p_buffers: *const Buffer,
    p_offsets: *const DeviceSize,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDraw = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    vertex_count: u32,
    instance_count: u32,
    first_vertex: u32,
    first_instance: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndexed = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    index_count: u32,
    instance_count: u32,
    first_index: u32,
    vertex_offset: i32,
    first_instance: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndirect = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    draw_count: u32,
    stride: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDrawIndexedIndirect = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    buffer: Buffer,
    offset: DeviceSize,
    draw_count: u32,
    stride: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDispatch = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    group_count_x: u32,
    group_count_y: u32,
    group_count_z: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdDispatchIndirect =
    unsafe extern "system" fn(command_buffer: CommandBuffer, buffer: Buffer, offset: DeviceSize);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyBuffer = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    src_buffer: Buffer,
    dst_buffer: Buffer,
    region_count: u32,
    p_regions: *const BufferCopy,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyImage = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    src_image: Image,
    src_image_layout: ImageLayout,
    dst_image: Image,
    dst_image_layout: ImageLayout,
    region_count: u32,
    p_regions: *const ImageCopy,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBlitImage = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    src_image: Image,
    src_image_layout: ImageLayout,
    dst_image: Image,
    dst_image_layout: ImageLayout,
    region_count: u32,
    p_regions: *const ImageBlit,
    filter: Filter,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyBufferToImage = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    src_buffer: Buffer,
    dst_image: Image,
    dst_image_layout: ImageLayout,
    region_count: u32,
    p_regions: *const BufferImageCopy,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyImageToBuffer = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    src_image: Image,
    src_image_layout: ImageLayout,
    dst_buffer: Buffer,
    region_count: u32,
    p_regions: *const BufferImageCopy,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdUpdateBuffer = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    dst_buffer: Buffer,
    dst_offset: DeviceSize,
    data_size: DeviceSize,
    p_data: *const c_void,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdFillBuffer = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    dst_buffer: Buffer,
    dst_offset: DeviceSize,
    size: DeviceSize,
    data: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdClearColorImage = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    image: Image,
    image_layout: ImageLayout,
    p_color: *const ClearColorValue,
    range_count: u32,
    p_ranges: *const ImageSubresourceRange,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdClearDepthStencilImage = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    image: Image,
    image_layout: ImageLayout,
    p_depth_stencil: *const ClearDepthStencilValue,
    range_count: u32,
    p_ranges: *const ImageSubresourceRange,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdClearAttachments = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    attachment_count: u32,
    p_attachments: *const ClearAttachment,
    rect_count: u32,
    p_rects: *const ClearRect,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdResolveImage = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    src_image: Image,
    src_image_layout: ImageLayout,
    dst_image: Image,
    dst_image_layout: ImageLayout,
    region_count: u32,
    p_regions: *const ImageResolve,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdSetEvent = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    event: Event,
    stage_mask: PipelineStageFlags,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdResetEvent = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    event: Event,
    stage_mask: PipelineStageFlags,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWaitEvents = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    event_count: u32,
    p_events: *const Event,
    src_stage_mask: PipelineStageFlags,
    dst_stage_mask: PipelineStageFlags,
    memory_barrier_count: u32,
    p_memory_barriers: *const MemoryBarrier,
    buffer_memory_barrier_count: u32,
    p_buffer_memory_barriers: *const BufferMemoryBarrier,
    image_memory_barrier_count: u32,
    p_image_memory_barriers: *const ImageMemoryBarrier,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPipelineBarrier = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    src_stage_mask: PipelineStageFlags,
    dst_stage_mask: PipelineStageFlags,
    dependency_flags: DependencyFlags,
    memory_barrier_count: u32,
    p_memory_barriers: *const MemoryBarrier,
    buffer_memory_barrier_count: u32,
    p_buffer_memory_barriers: *const BufferMemoryBarrier,
    image_memory_barrier_count: u32,
    p_image_memory_barriers: *const ImageMemoryBarrier,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginQuery = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    query_pool: QueryPool,
    query: u32,
    flags: QueryControlFlags,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndQuery =
    unsafe extern "system" fn(command_buffer: CommandBuffer, query_pool: QueryPool, query: u32);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdResetQueryPool = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    query_pool: QueryPool,
    first_query: u32,
    query_count: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdWriteTimestamp = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    pipeline_stage: PipelineStageFlags,
    query_pool: QueryPool,
    query: u32,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdCopyQueryPoolResults = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    query_pool: QueryPool,
    first_query: u32,
    query_count: u32,
    dst_buffer: Buffer,
    dst_offset: DeviceSize,
    stride: DeviceSize,
    flags: QueryResultFlags,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdPushConstants = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    layout: PipelineLayout,
    stage_flags: ShaderStageFlags,
    offset: u32,
    size: u32,
    p_values: *const c_void,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdBeginRenderPass = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    p_render_pass_begin: *const RenderPassBeginInfo,
    contents: SubpassContents,
);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdNextSubpass =
    unsafe extern "system" fn(command_buffer: CommandBuffer, contents: SubpassContents);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdEndRenderPass = unsafe extern "system" fn(command_buffer: CommandBuffer);
#[allow(non_camel_case_types)]
pub type PFN_vkCmdExecuteCommands = unsafe extern "system" fn(
    command_buffer: CommandBuffer,
    command_buffer_count: u32,
    p_command_buffers: *const CommandBuffer,
);
#[derive(Clone)]
pub struct DeviceFnV1_0 {
    pub destroy_device: PFN_vkDestroyDevice,
    pub get_device_queue: PFN_vkGetDeviceQueue,
    pub queue_submit: PFN_vkQueueSubmit,
    pub queue_wait_idle: PFN_vkQueueWaitIdle,
    pub device_wait_idle: PFN_vkDeviceWaitIdle,
    pub allocate_memory: PFN_vkAllocateMemory,
    pub free_memory: PFN_vkFreeMemory,
    pub map_memory: PFN_vkMapMemory,
    pub unmap_memory: PFN_vkUnmapMemory,
    pub flush_mapped_memory_ranges: PFN_vkFlushMappedMemoryRanges,
    pub invalidate_mapped_memory_ranges: PFN_vkInvalidateMappedMemoryRanges,
    pub get_device_memory_commitment: PFN_vkGetDeviceMemoryCommitment,
    pub bind_buffer_memory: PFN_vkBindBufferMemory,
    pub bind_image_memory: PFN_vkBindImageMemory,
    pub get_buffer_memory_requirements: PFN_vkGetBufferMemoryRequirements,
    pub get_image_memory_requirements: PFN_vkGetImageMemoryRequirements,
    pub get_image_sparse_memory_requirements: PFN_vkGetImageSparseMemoryRequirements,
    pub queue_bind_sparse: PFN_vkQueueBindSparse,
    pub create_fence: PFN_vkCreateFence,
    pub destroy_fence: PFN_vkDestroyFence,
    pub reset_fences: PFN_vkResetFences,
    pub get_fence_status: PFN_vkGetFenceStatus,
    pub wait_for_fences: PFN_vkWaitForFences,
    pub create_semaphore: PFN_vkCreateSemaphore,
    pub destroy_semaphore: PFN_vkDestroySemaphore,
    pub create_event: PFN_vkCreateEvent,
    pub destroy_event: PFN_vkDestroyEvent,
    pub get_event_status: PFN_vkGetEventStatus,
    pub set_event: PFN_vkSetEvent,
    pub reset_event: PFN_vkResetEvent,
    pub create_query_pool: PFN_vkCreateQueryPool,
    pub destroy_query_pool: PFN_vkDestroyQueryPool,
    pub get_query_pool_results: PFN_vkGetQueryPoolResults,
    pub create_buffer: PFN_vkCreateBuffer,
    pub destroy_buffer: PFN_vkDestroyBuffer,
    pub create_buffer_view: PFN_vkCreateBufferView,
    pub destroy_buffer_view: PFN_vkDestroyBufferView,
    pub create_image: PFN_vkCreateImage,
    pub destroy_image: PFN_vkDestroyImage,
    pub get_image_subresource_layout: PFN_vkGetImageSubresourceLayout,
    pub create_image_view: PFN_vkCreateImageView,
    pub destroy_image_view: PFN_vkDestroyImageView,
    pub create_shader_module: PFN_vkCreateShaderModule,
    pub destroy_shader_module: PFN_vkDestroyShaderModule,
    pub create_pipeline_cache: PFN_vkCreatePipelineCache,
    pub destroy_pipeline_cache: PFN_vkDestroyPipelineCache,
    pub get_pipeline_cache_data: PFN_vkGetPipelineCacheData,
    pub merge_pipeline_caches: PFN_vkMergePipelineCaches,
    pub create_graphics_pipelines: PFN_vkCreateGraphicsPipelines,
    pub create_compute_pipelines: PFN_vkCreateComputePipelines,
    pub destroy_pipeline: PFN_vkDestroyPipeline,
    pub create_pipeline_layout: PFN_vkCreatePipelineLayout,
    pub destroy_pipeline_layout: PFN_vkDestroyPipelineLayout,
    pub create_sampler: PFN_vkCreateSampler,
    pub destroy_sampler: PFN_vkDestroySampler,
    pub create_descriptor_set_layout: PFN_vkCreateDescriptorSetLayout,
    pub destroy_descriptor_set_layout: PFN_vkDestroyDescriptorSetLayout,
    pub create_descriptor_pool: PFN_vkCreateDescriptorPool,
    pub destroy_descriptor_pool: PFN_vkDestroyDescriptorPool,
    pub reset_descriptor_pool: PFN_vkResetDescriptorPool,
    pub allocate_descriptor_sets: PFN_vkAllocateDescriptorSets,
    pub free_descriptor_sets: PFN_vkFreeDescriptorSets,
    pub update_descriptor_sets: PFN_vkUpdateDescriptorSets,
    pub create_framebuffer: PFN_vkCreateFramebuffer,
    pub destroy_framebuffer: PFN_vkDestroyFramebuffer,
    pub create_render_pass: PFN_vkCreateRenderPass,
    pub destroy_render_pass: PFN_vkDestroyRenderPass,
    pub get_render_area_granularity: PFN_vkGetRenderAreaGranularity,
    pub create_command_pool: PFN_vkCreateCommandPool,
    pub destroy_command_pool: PFN_vkDestroyCommandPool,
    pub reset_command_pool: PFN_vkResetCommandPool,
    pub allocate_command_buffers: PFN_vkAllocateCommandBuffers,
    pub free_command_buffers: PFN_vkFreeCommandBuffers,
    pub begin_command_buffer: PFN_vkBeginCommandBuffer,
    pub end_command_buffer: PFN_vkEndCommandBuffer,
    pub reset_command_buffer: PFN_vkResetCommandBuffer,
    pub cmd_bind_pipeline: PFN_vkCmdBindPipeline,
    pub cmd_set_viewport: PFN_vkCmdSetViewport,
    pub cmd_set_scissor: PFN_vkCmdSetScissor,
    pub cmd_set_line_width: PFN_vkCmdSetLineWidth,
    pub cmd_set_depth_bias: PFN_vkCmdSetDepthBias,
    pub cmd_set_blend_constants: PFN_vkCmdSetBlendConstants,
    pub cmd_set_depth_bounds: PFN_vkCmdSetDepthBounds,
    pub cmd_set_stencil_compare_mask: PFN_vkCmdSetStencilCompareMask,
    pub cmd_set_stencil_write_mask: PFN_vkCmdSetStencilWriteMask,
    pub cmd_set_stencil_reference: PFN_vkCmdSetStencilReference,
    pub cmd_bind_descriptor_sets: PFN_vkCmdBindDescriptorSets,
    pub cmd_bind_index_buffer: PFN_vkCmdBindIndexBuffer,
    pub cmd_bind_vertex_buffers: PFN_vkCmdBindVertexBuffers,
    pub cmd_draw: PFN_vkCmdDraw,
    pub cmd_draw_indexed: PFN_vkCmdDrawIndexed,
    pub cmd_draw_indirect: PFN_vkCmdDrawIndirect,
    pub cmd_draw_indexed_indirect: PFN_vkCmdDrawIndexedIndirect,
    pub cmd_dispatch: PFN_vkCmdDispatch,
    pub cmd_dispatch_indirect: PFN_vkCmdDispatchIndirect,
    pub cmd_copy_buffer: PFN_vkCmdCopyBuffer,
    pub cmd_copy_image: PFN_vkCmdCopyImage,
    pub cmd_blit_image: PFN_vkCmdBlitImage,
    pub cmd_copy_buffer_to_image: PFN_vkCmdCopyBufferToImage,
    pub cmd_copy_image_to_buffer: PFN_vkCmdCopyImageToBuffer,
    pub cmd_update_buffer: PFN_vkCmdUpdateBuffer,
    pub cmd_fill_buffer: PFN_vkCmdFillBuffer,
    pub cmd_clear_color_image: PFN_vkCmdClearColorImage,
    pub cmd_clear_depth_stencil_image: PFN_vkCmdClearDepthStencilImage,
    pub cmd_clear_attachments: PFN_vkCmdClearAttachments,
    pub cmd_resolve_image: PFN_vkCmdResolveImage,
    pub cmd_set_event: PFN_vkCmdSetEvent,
    pub cmd_reset_event: PFN_vkCmdResetEvent,
    pub cmd_wait_events: PFN_vkCmdWaitEvents,
    pub cmd_pipeline_barrier: PFN_vkCmdPipelineBarrier,
    pub cmd_begin_query: PFN_vkCmdBeginQuery,
    pub cmd_end_query: PFN_vkCmdEndQuery,
    pub cmd_reset_query_pool: PFN_vkCmdResetQueryPool,
    pub cmd_write_timestamp: PFN_vkCmdWriteTimestamp,
    pub cmd_copy_query_pool_results: PFN_vkCmdCopyQueryPoolResults,
    pub cmd_push_constants: PFN_vkCmdPushConstants,
    pub cmd_begin_render_pass: PFN_vkCmdBeginRenderPass,
    pub cmd_next_subpass: PFN_vkCmdNextSubpass,
    pub cmd_end_render_pass: PFN_vkCmdEndRenderPass,
    pub cmd_execute_commands: PFN_vkCmdExecuteCommands,
}
unsafe impl Send for DeviceFnV1_0 {}
unsafe impl Sync for DeviceFnV1_0 {}
impl DeviceFnV1_0 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            destroy_device: unsafe {
                unsafe extern "system" fn destroy_device(
                    _device: Device,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_device)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyDevice\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_device
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_queue: unsafe {
                unsafe extern "system" fn get_device_queue(
                    _device: Device,
                    _queue_family_index: u32,
                    _queue_index: u32,
                    _p_queue: *mut Queue,
                ) {
                    panic!(concat!("Unable to load ", stringify!(get_device_queue)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetDeviceQueue\0");
                let val = _f(cname);
                if val.is_null() {
                    get_device_queue
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_submit: unsafe {
                unsafe extern "system" fn queue_submit(
                    _queue: Queue,
                    _submit_count: u32,
                    _p_submits: *const SubmitInfo,
                    _fence: Fence,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(queue_submit)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkQueueSubmit\0");
                let val = _f(cname);
                if val.is_null() {
                    queue_submit
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_wait_idle: unsafe {
                unsafe extern "system" fn queue_wait_idle(_queue: Queue) -> Result {
                    panic!(concat!("Unable to load ", stringify!(queue_wait_idle)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkQueueWaitIdle\0");
                let val = _f(cname);
                if val.is_null() {
                    queue_wait_idle
                } else {
                    ::std::mem::transmute(val)
                }
            },
            device_wait_idle: unsafe {
                unsafe extern "system" fn device_wait_idle(_device: Device) -> Result {
                    panic!(concat!("Unable to load ", stringify!(device_wait_idle)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDeviceWaitIdle\0");
                let val = _f(cname);
                if val.is_null() {
                    device_wait_idle
                } else {
                    ::std::mem::transmute(val)
                }
            },
            allocate_memory: unsafe {
                unsafe extern "system" fn allocate_memory(
                    _device: Device,
                    _p_allocate_info: *const MemoryAllocateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_memory: *mut DeviceMemory,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(allocate_memory)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAllocateMemory\0");
                let val = _f(cname);
                if val.is_null() {
                    allocate_memory
                } else {
                    ::std::mem::transmute(val)
                }
            },
            free_memory: unsafe {
                unsafe extern "system" fn free_memory(
                    _device: Device,
                    _memory: DeviceMemory,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(free_memory)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkFreeMemory\0");
                let val = _f(cname);
                if val.is_null() {
                    free_memory
                } else {
                    ::std::mem::transmute(val)
                }
            },
            map_memory: unsafe {
                unsafe extern "system" fn map_memory(
                    _device: Device,
                    _memory: DeviceMemory,
                    _offset: DeviceSize,
                    _size: DeviceSize,
                    _flags: MemoryMapFlags,
                    _pp_data: *mut *mut c_void,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(map_memory)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkMapMemory\0");
                let val = _f(cname);
                if val.is_null() {
                    map_memory
                } else {
                    ::std::mem::transmute(val)
                }
            },
            unmap_memory: unsafe {
                unsafe extern "system" fn unmap_memory(_device: Device, _memory: DeviceMemory) {
                    panic!(concat!("Unable to load ", stringify!(unmap_memory)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkUnmapMemory\0");
                let val = _f(cname);
                if val.is_null() {
                    unmap_memory
                } else {
                    ::std::mem::transmute(val)
                }
            },
            flush_mapped_memory_ranges: unsafe {
                unsafe extern "system" fn flush_mapped_memory_ranges(
                    _device: Device,
                    _memory_range_count: u32,
                    _p_memory_ranges: *const MappedMemoryRange,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(flush_mapped_memory_ranges)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkFlushMappedMemoryRanges\0");
                let val = _f(cname);
                if val.is_null() {
                    flush_mapped_memory_ranges
                } else {
                    ::std::mem::transmute(val)
                }
            },
            invalidate_mapped_memory_ranges: unsafe {
                unsafe extern "system" fn invalidate_mapped_memory_ranges(
                    _device: Device,
                    _memory_range_count: u32,
                    _p_memory_ranges: *const MappedMemoryRange,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(invalidate_mapped_memory_ranges)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkInvalidateMappedMemoryRanges\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    invalidate_mapped_memory_ranges
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_memory_commitment: unsafe {
                unsafe extern "system" fn get_device_memory_commitment(
                    _device: Device,
                    _memory: DeviceMemory,
                    _p_committed_memory_in_bytes: *mut DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_memory_commitment)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceMemoryCommitment\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_memory_commitment
                } else {
                    ::std::mem::transmute(val)
                }
            },
            bind_buffer_memory: unsafe {
                unsafe extern "system" fn bind_buffer_memory(
                    _device: Device,
                    _buffer: Buffer,
                    _memory: DeviceMemory,
                    _memory_offset: DeviceSize,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(bind_buffer_memory)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkBindBufferMemory\0");
                let val = _f(cname);
                if val.is_null() {
                    bind_buffer_memory
                } else {
                    ::std::mem::transmute(val)
                }
            },
            bind_image_memory: unsafe {
                unsafe extern "system" fn bind_image_memory(
                    _device: Device,
                    _image: Image,
                    _memory: DeviceMemory,
                    _memory_offset: DeviceSize,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(bind_image_memory)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkBindImageMemory\0");
                let val = _f(cname);
                if val.is_null() {
                    bind_image_memory
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_buffer_memory_requirements: unsafe {
                unsafe extern "system" fn get_buffer_memory_requirements(
                    _device: Device,
                    _buffer: Buffer,
                    _p_memory_requirements: *mut MemoryRequirements,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_memory_requirements)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetBufferMemoryRequirements\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_memory_requirements
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_image_memory_requirements: unsafe {
                unsafe extern "system" fn get_image_memory_requirements(
                    _device: Device,
                    _image: Image,
                    _p_memory_requirements: *mut MemoryRequirements,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_memory_requirements)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageMemoryRequirements\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_memory_requirements
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_image_sparse_memory_requirements: unsafe {
                unsafe extern "system" fn get_image_sparse_memory_requirements(
                    _device: Device,
                    _image: Image,
                    _p_sparse_memory_requirement_count: *mut u32,
                    _p_sparse_memory_requirements: *mut SparseImageMemoryRequirements,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_sparse_memory_requirements)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageSparseMemoryRequirements\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_sparse_memory_requirements
                } else {
                    ::std::mem::transmute(val)
                }
            },
            queue_bind_sparse: unsafe {
                unsafe extern "system" fn queue_bind_sparse(
                    _queue: Queue,
                    _bind_info_count: u32,
                    _p_bind_info: *const BindSparseInfo,
                    _fence: Fence,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(queue_bind_sparse)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkQueueBindSparse\0");
                let val = _f(cname);
                if val.is_null() {
                    queue_bind_sparse
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_fence: unsafe {
                unsafe extern "system" fn create_fence(
                    _device: Device,
                    _p_create_info: *const FenceCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_fence: *mut Fence,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_fence)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateFence\0");
                let val = _f(cname);
                if val.is_null() {
                    create_fence
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_fence: unsafe {
                unsafe extern "system" fn destroy_fence(
                    _device: Device,
                    _fence: Fence,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_fence)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyFence\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_fence
                } else {
                    ::std::mem::transmute(val)
                }
            },
            reset_fences: unsafe {
                unsafe extern "system" fn reset_fences(
                    _device: Device,
                    _fence_count: u32,
                    _p_fences: *const Fence,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(reset_fences)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkResetFences\0");
                let val = _f(cname);
                if val.is_null() {
                    reset_fences
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_fence_status: unsafe {
                unsafe extern "system" fn get_fence_status(
                    _device: Device,
                    _fence: Fence,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_fence_status)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetFenceStatus\0");
                let val = _f(cname);
                if val.is_null() {
                    get_fence_status
                } else {
                    ::std::mem::transmute(val)
                }
            },
            wait_for_fences: unsafe {
                unsafe extern "system" fn wait_for_fences(
                    _device: Device,
                    _fence_count: u32,
                    _p_fences: *const Fence,
                    _wait_all: Bool32,
                    _timeout: u64,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(wait_for_fences)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkWaitForFences\0");
                let val = _f(cname);
                if val.is_null() {
                    wait_for_fences
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_semaphore: unsafe {
                unsafe extern "system" fn create_semaphore(
                    _device: Device,
                    _p_create_info: *const SemaphoreCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_semaphore: *mut Semaphore,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_semaphore)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateSemaphore\0");
                let val = _f(cname);
                if val.is_null() {
                    create_semaphore
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_semaphore: unsafe {
                unsafe extern "system" fn destroy_semaphore(
                    _device: Device,
                    _semaphore: Semaphore,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_semaphore)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroySemaphore\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_semaphore
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_event: unsafe {
                unsafe extern "system" fn create_event(
                    _device: Device,
                    _p_create_info: *const EventCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_event: *mut Event,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_event)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateEvent\0");
                let val = _f(cname);
                if val.is_null() {
                    create_event
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_event: unsafe {
                unsafe extern "system" fn destroy_event(
                    _device: Device,
                    _event: Event,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_event)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyEvent\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_event
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_event_status: unsafe {
                unsafe extern "system" fn get_event_status(
                    _device: Device,
                    _event: Event,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(get_event_status)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetEventStatus\0");
                let val = _f(cname);
                if val.is_null() {
                    get_event_status
                } else {
                    ::std::mem::transmute(val)
                }
            },
            set_event: unsafe {
                unsafe extern "system" fn set_event(_device: Device, _event: Event) -> Result {
                    panic!(concat!("Unable to load ", stringify!(set_event)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkSetEvent\0");
                let val = _f(cname);
                if val.is_null() {
                    set_event
                } else {
                    ::std::mem::transmute(val)
                }
            },
            reset_event: unsafe {
                unsafe extern "system" fn reset_event(_device: Device, _event: Event) -> Result {
                    panic!(concat!("Unable to load ", stringify!(reset_event)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkResetEvent\0");
                let val = _f(cname);
                if val.is_null() {
                    reset_event
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_query_pool: unsafe {
                unsafe extern "system" fn create_query_pool(
                    _device: Device,
                    _p_create_info: *const QueryPoolCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_query_pool: *mut QueryPool,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_query_pool)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateQueryPool\0");
                let val = _f(cname);
                if val.is_null() {
                    create_query_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_query_pool: unsafe {
                unsafe extern "system" fn destroy_query_pool(
                    _device: Device,
                    _query_pool: QueryPool,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_query_pool)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyQueryPool\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_query_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_query_pool_results: unsafe {
                unsafe extern "system" fn get_query_pool_results(
                    _device: Device,
                    _query_pool: QueryPool,
                    _first_query: u32,
                    _query_count: u32,
                    _data_size: usize,
                    _p_data: *mut c_void,
                    _stride: DeviceSize,
                    _flags: QueryResultFlags,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_query_pool_results)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetQueryPoolResults\0");
                let val = _f(cname);
                if val.is_null() {
                    get_query_pool_results
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_buffer: unsafe {
                unsafe extern "system" fn create_buffer(
                    _device: Device,
                    _p_create_info: *const BufferCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_buffer: *mut Buffer,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_buffer)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    create_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_buffer: unsafe {
                unsafe extern "system" fn destroy_buffer(
                    _device: Device,
                    _buffer: Buffer,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_buffer)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_buffer_view: unsafe {
                unsafe extern "system" fn create_buffer_view(
                    _device: Device,
                    _p_create_info: *const BufferViewCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_view: *mut BufferView,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_buffer_view)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateBufferView\0");
                let val = _f(cname);
                if val.is_null() {
                    create_buffer_view
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_buffer_view: unsafe {
                unsafe extern "system" fn destroy_buffer_view(
                    _device: Device,
                    _buffer_view: BufferView,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_buffer_view)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyBufferView\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_buffer_view
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_image: unsafe {
                unsafe extern "system" fn create_image(
                    _device: Device,
                    _p_create_info: *const ImageCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_image: *mut Image,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_image)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateImage\0");
                let val = _f(cname);
                if val.is_null() {
                    create_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_image: unsafe {
                unsafe extern "system" fn destroy_image(
                    _device: Device,
                    _image: Image,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_image)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyImage\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_image_subresource_layout: unsafe {
                unsafe extern "system" fn get_image_subresource_layout(
                    _device: Device,
                    _image: Image,
                    _p_subresource: *const ImageSubresource,
                    _p_layout: *mut SubresourceLayout,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_subresource_layout)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageSubresourceLayout\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_subresource_layout
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_image_view: unsafe {
                unsafe extern "system" fn create_image_view(
                    _device: Device,
                    _p_create_info: *const ImageViewCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_view: *mut ImageView,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_image_view)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateImageView\0");
                let val = _f(cname);
                if val.is_null() {
                    create_image_view
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_image_view: unsafe {
                unsafe extern "system" fn destroy_image_view(
                    _device: Device,
                    _image_view: ImageView,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_image_view)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyImageView\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_image_view
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_shader_module: unsafe {
                unsafe extern "system" fn create_shader_module(
                    _device: Device,
                    _p_create_info: *const ShaderModuleCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_shader_module: *mut ShaderModule,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_shader_module)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateShaderModule\0");
                let val = _f(cname);
                if val.is_null() {
                    create_shader_module
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_shader_module: unsafe {
                unsafe extern "system" fn destroy_shader_module(
                    _device: Device,
                    _shader_module: ShaderModule,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_shader_module)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyShaderModule\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_shader_module
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_pipeline_cache: unsafe {
                unsafe extern "system" fn create_pipeline_cache(
                    _device: Device,
                    _p_create_info: *const PipelineCacheCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_pipeline_cache: *mut PipelineCache,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_pipeline_cache)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreatePipelineCache\0");
                let val = _f(cname);
                if val.is_null() {
                    create_pipeline_cache
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_pipeline_cache: unsafe {
                unsafe extern "system" fn destroy_pipeline_cache(
                    _device: Device,
                    _pipeline_cache: PipelineCache,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_pipeline_cache)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyPipelineCache\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_pipeline_cache
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_pipeline_cache_data: unsafe {
                unsafe extern "system" fn get_pipeline_cache_data(
                    _device: Device,
                    _pipeline_cache: PipelineCache,
                    _p_data_size: *mut usize,
                    _p_data: *mut c_void,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_pipeline_cache_data)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetPipelineCacheData\0");
                let val = _f(cname);
                if val.is_null() {
                    get_pipeline_cache_data
                } else {
                    ::std::mem::transmute(val)
                }
            },
            merge_pipeline_caches: unsafe {
                unsafe extern "system" fn merge_pipeline_caches(
                    _device: Device,
                    _dst_cache: PipelineCache,
                    _src_cache_count: u32,
                    _p_src_caches: *const PipelineCache,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(merge_pipeline_caches)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkMergePipelineCaches\0");
                let val = _f(cname);
                if val.is_null() {
                    merge_pipeline_caches
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_graphics_pipelines: unsafe {
                unsafe extern "system" fn create_graphics_pipelines(
                    _device: Device,
                    _pipeline_cache: PipelineCache,
                    _create_info_count: u32,
                    _p_create_infos: *const GraphicsPipelineCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_pipelines: *mut Pipeline,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_graphics_pipelines)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateGraphicsPipelines\0");
                let val = _f(cname);
                if val.is_null() {
                    create_graphics_pipelines
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_compute_pipelines: unsafe {
                unsafe extern "system" fn create_compute_pipelines(
                    _device: Device,
                    _pipeline_cache: PipelineCache,
                    _create_info_count: u32,
                    _p_create_infos: *const ComputePipelineCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_pipelines: *mut Pipeline,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_compute_pipelines)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateComputePipelines\0");
                let val = _f(cname);
                if val.is_null() {
                    create_compute_pipelines
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_pipeline: unsafe {
                unsafe extern "system" fn destroy_pipeline(
                    _device: Device,
                    _pipeline: Pipeline,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_pipeline)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyPipeline\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_pipeline
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_pipeline_layout: unsafe {
                unsafe extern "system" fn create_pipeline_layout(
                    _device: Device,
                    _p_create_info: *const PipelineLayoutCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_pipeline_layout: *mut PipelineLayout,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_pipeline_layout)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreatePipelineLayout\0");
                let val = _f(cname);
                if val.is_null() {
                    create_pipeline_layout
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_pipeline_layout: unsafe {
                unsafe extern "system" fn destroy_pipeline_layout(
                    _device: Device,
                    _pipeline_layout: PipelineLayout,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_pipeline_layout)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyPipelineLayout\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_pipeline_layout
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_sampler: unsafe {
                unsafe extern "system" fn create_sampler(
                    _device: Device,
                    _p_create_info: *const SamplerCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_sampler: *mut Sampler,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_sampler)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateSampler\0");
                let val = _f(cname);
                if val.is_null() {
                    create_sampler
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_sampler: unsafe {
                unsafe extern "system" fn destroy_sampler(
                    _device: Device,
                    _sampler: Sampler,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_sampler)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroySampler\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_sampler
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_descriptor_set_layout: unsafe {
                unsafe extern "system" fn create_descriptor_set_layout(
                    _device: Device,
                    _p_create_info: *const DescriptorSetLayoutCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_set_layout: *mut DescriptorSetLayout,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_descriptor_set_layout)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDescriptorSetLayout\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_descriptor_set_layout
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_descriptor_set_layout: unsafe {
                unsafe extern "system" fn destroy_descriptor_set_layout(
                    _device: Device,
                    _descriptor_set_layout: DescriptorSetLayout,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_descriptor_set_layout)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyDescriptorSetLayout\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_descriptor_set_layout
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_descriptor_pool: unsafe {
                unsafe extern "system" fn create_descriptor_pool(
                    _device: Device,
                    _p_create_info: *const DescriptorPoolCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_descriptor_pool: *mut DescriptorPool,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_descriptor_pool)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateDescriptorPool\0");
                let val = _f(cname);
                if val.is_null() {
                    create_descriptor_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_descriptor_pool: unsafe {
                unsafe extern "system" fn destroy_descriptor_pool(
                    _device: Device,
                    _descriptor_pool: DescriptorPool,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_descriptor_pool)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyDescriptorPool\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_descriptor_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            reset_descriptor_pool: unsafe {
                unsafe extern "system" fn reset_descriptor_pool(
                    _device: Device,
                    _descriptor_pool: DescriptorPool,
                    _flags: DescriptorPoolResetFlags,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(reset_descriptor_pool)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkResetDescriptorPool\0");
                let val = _f(cname);
                if val.is_null() {
                    reset_descriptor_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            allocate_descriptor_sets: unsafe {
                unsafe extern "system" fn allocate_descriptor_sets(
                    _device: Device,
                    _p_allocate_info: *const DescriptorSetAllocateInfo,
                    _p_descriptor_sets: *mut DescriptorSet,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(allocate_descriptor_sets)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAllocateDescriptorSets\0");
                let val = _f(cname);
                if val.is_null() {
                    allocate_descriptor_sets
                } else {
                    ::std::mem::transmute(val)
                }
            },
            free_descriptor_sets: unsafe {
                unsafe extern "system" fn free_descriptor_sets(
                    _device: Device,
                    _descriptor_pool: DescriptorPool,
                    _descriptor_set_count: u32,
                    _p_descriptor_sets: *const DescriptorSet,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(free_descriptor_sets)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkFreeDescriptorSets\0");
                let val = _f(cname);
                if val.is_null() {
                    free_descriptor_sets
                } else {
                    ::std::mem::transmute(val)
                }
            },
            update_descriptor_sets: unsafe {
                unsafe extern "system" fn update_descriptor_sets(
                    _device: Device,
                    _descriptor_write_count: u32,
                    _p_descriptor_writes: *const WriteDescriptorSet,
                    _descriptor_copy_count: u32,
                    _p_descriptor_copies: *const CopyDescriptorSet,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(update_descriptor_sets)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkUpdateDescriptorSets\0");
                let val = _f(cname);
                if val.is_null() {
                    update_descriptor_sets
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_framebuffer: unsafe {
                unsafe extern "system" fn create_framebuffer(
                    _device: Device,
                    _p_create_info: *const FramebufferCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_framebuffer: *mut Framebuffer,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_framebuffer)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateFramebuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    create_framebuffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_framebuffer: unsafe {
                unsafe extern "system" fn destroy_framebuffer(
                    _device: Device,
                    _framebuffer: Framebuffer,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_framebuffer)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyFramebuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_framebuffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_render_pass: unsafe {
                unsafe extern "system" fn create_render_pass(
                    _device: Device,
                    _p_create_info: *const RenderPassCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_render_pass: *mut RenderPass,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_render_pass)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateRenderPass\0");
                let val = _f(cname);
                if val.is_null() {
                    create_render_pass
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_render_pass: unsafe {
                unsafe extern "system" fn destroy_render_pass(
                    _device: Device,
                    _render_pass: RenderPass,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_render_pass)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyRenderPass\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_render_pass
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_render_area_granularity: unsafe {
                unsafe extern "system" fn get_render_area_granularity(
                    _device: Device,
                    _render_pass: RenderPass,
                    _p_granularity: *mut Extent2D,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_render_area_granularity)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetRenderAreaGranularity\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_render_area_granularity
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_command_pool: unsafe {
                unsafe extern "system" fn create_command_pool(
                    _device: Device,
                    _p_create_info: *const CommandPoolCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_command_pool: *mut CommandPool,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_command_pool)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateCommandPool\0");
                let val = _f(cname);
                if val.is_null() {
                    create_command_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_command_pool: unsafe {
                unsafe extern "system" fn destroy_command_pool(
                    _device: Device,
                    _command_pool: CommandPool,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!("Unable to load ", stringify!(destroy_command_pool)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkDestroyCommandPool\0");
                let val = _f(cname);
                if val.is_null() {
                    destroy_command_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            reset_command_pool: unsafe {
                unsafe extern "system" fn reset_command_pool(
                    _device: Device,
                    _command_pool: CommandPool,
                    _flags: CommandPoolResetFlags,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(reset_command_pool)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkResetCommandPool\0");
                let val = _f(cname);
                if val.is_null() {
                    reset_command_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            allocate_command_buffers: unsafe {
                unsafe extern "system" fn allocate_command_buffers(
                    _device: Device,
                    _p_allocate_info: *const CommandBufferAllocateInfo,
                    _p_command_buffers: *mut CommandBuffer,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(allocate_command_buffers)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkAllocateCommandBuffers\0");
                let val = _f(cname);
                if val.is_null() {
                    allocate_command_buffers
                } else {
                    ::std::mem::transmute(val)
                }
            },
            free_command_buffers: unsafe {
                unsafe extern "system" fn free_command_buffers(
                    _device: Device,
                    _command_pool: CommandPool,
                    _command_buffer_count: u32,
                    _p_command_buffers: *const CommandBuffer,
                ) {
                    panic!(concat!("Unable to load ", stringify!(free_command_buffers)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkFreeCommandBuffers\0");
                let val = _f(cname);
                if val.is_null() {
                    free_command_buffers
                } else {
                    ::std::mem::transmute(val)
                }
            },
            begin_command_buffer: unsafe {
                unsafe extern "system" fn begin_command_buffer(
                    _command_buffer: CommandBuffer,
                    _p_begin_info: *const CommandBufferBeginInfo,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(begin_command_buffer)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkBeginCommandBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    begin_command_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            end_command_buffer: unsafe {
                unsafe extern "system" fn end_command_buffer(
                    _command_buffer: CommandBuffer,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(end_command_buffer)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkEndCommandBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    end_command_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            reset_command_buffer: unsafe {
                unsafe extern "system" fn reset_command_buffer(
                    _command_buffer: CommandBuffer,
                    _flags: CommandBufferResetFlags,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(reset_command_buffer)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkResetCommandBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    reset_command_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_bind_pipeline: unsafe {
                unsafe extern "system" fn cmd_bind_pipeline(
                    _command_buffer: CommandBuffer,
                    _pipeline_bind_point: PipelineBindPoint,
                    _pipeline: Pipeline,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_bind_pipeline)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBindPipeline\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_pipeline
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_viewport: unsafe {
                unsafe extern "system" fn cmd_set_viewport(
                    _command_buffer: CommandBuffer,
                    _first_viewport: u32,
                    _viewport_count: u32,
                    _p_viewports: *const Viewport,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_viewport)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetViewport\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_viewport
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_scissor: unsafe {
                unsafe extern "system" fn cmd_set_scissor(
                    _command_buffer: CommandBuffer,
                    _first_scissor: u32,
                    _scissor_count: u32,
                    _p_scissors: *const Rect2D,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_scissor)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetScissor\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_scissor
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_line_width: unsafe {
                unsafe extern "system" fn cmd_set_line_width(
                    _command_buffer: CommandBuffer,
                    _line_width: f32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_line_width)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetLineWidth\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_line_width
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_depth_bias: unsafe {
                unsafe extern "system" fn cmd_set_depth_bias(
                    _command_buffer: CommandBuffer,
                    _depth_bias_constant_factor: f32,
                    _depth_bias_clamp: f32,
                    _depth_bias_slope_factor: f32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_depth_bias)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetDepthBias\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_depth_bias
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_blend_constants: unsafe {
                unsafe extern "system" fn cmd_set_blend_constants(
                    _command_buffer: CommandBuffer,
                    _blend_constants: *const [f32; 4],
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_blend_constants)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetBlendConstants\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_blend_constants
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_depth_bounds: unsafe {
                unsafe extern "system" fn cmd_set_depth_bounds(
                    _command_buffer: CommandBuffer,
                    _min_depth_bounds: f32,
                    _max_depth_bounds: f32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_depth_bounds)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetDepthBounds\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_depth_bounds
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_stencil_compare_mask: unsafe {
                unsafe extern "system" fn cmd_set_stencil_compare_mask(
                    _command_buffer: CommandBuffer,
                    _face_mask: StencilFaceFlags,
                    _compare_mask: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_stencil_compare_mask)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdSetStencilCompareMask\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_stencil_compare_mask
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_stencil_write_mask: unsafe {
                unsafe extern "system" fn cmd_set_stencil_write_mask(
                    _command_buffer: CommandBuffer,
                    _face_mask: StencilFaceFlags,
                    _write_mask: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_stencil_write_mask)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetStencilWriteMask\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_stencil_write_mask
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_stencil_reference: unsafe {
                unsafe extern "system" fn cmd_set_stencil_reference(
                    _command_buffer: CommandBuffer,
                    _face_mask: StencilFaceFlags,
                    _reference: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_set_stencil_reference)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetStencilReference\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_stencil_reference
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_bind_descriptor_sets: unsafe {
                unsafe extern "system" fn cmd_bind_descriptor_sets(
                    _command_buffer: CommandBuffer,
                    _pipeline_bind_point: PipelineBindPoint,
                    _layout: PipelineLayout,
                    _first_set: u32,
                    _descriptor_set_count: u32,
                    _p_descriptor_sets: *const DescriptorSet,
                    _dynamic_offset_count: u32,
                    _p_dynamic_offsets: *const u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_descriptor_sets)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBindDescriptorSets\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_descriptor_sets
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_bind_index_buffer: unsafe {
                unsafe extern "system" fn cmd_bind_index_buffer(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _index_type: IndexType,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_index_buffer)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBindIndexBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_index_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_bind_vertex_buffers: unsafe {
                unsafe extern "system" fn cmd_bind_vertex_buffers(
                    _command_buffer: CommandBuffer,
                    _first_binding: u32,
                    _binding_count: u32,
                    _p_buffers: *const Buffer,
                    _p_offsets: *const DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_bind_vertex_buffers)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBindVertexBuffers\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_bind_vertex_buffers
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw: unsafe {
                unsafe extern "system" fn cmd_draw(
                    _command_buffer: CommandBuffer,
                    _vertex_count: u32,
                    _instance_count: u32,
                    _first_vertex: u32,
                    _first_instance: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_draw)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDraw\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_indexed: unsafe {
                unsafe extern "system" fn cmd_draw_indexed(
                    _command_buffer: CommandBuffer,
                    _index_count: u32,
                    _instance_count: u32,
                    _first_index: u32,
                    _vertex_offset: i32,
                    _first_instance: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_draw_indexed)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawIndexed\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indexed
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_indirect: unsafe {
                unsafe extern "system" fn cmd_draw_indirect(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_draw_indirect)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawIndirect\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indirect
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_indexed_indirect: unsafe {
                unsafe extern "system" fn cmd_draw_indexed_indirect(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                    _draw_count: u32,
                    _stride: u32,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_draw_indexed_indirect)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawIndexedIndirect\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indexed_indirect
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_dispatch: unsafe {
                unsafe extern "system" fn cmd_dispatch(
                    _command_buffer: CommandBuffer,
                    _group_count_x: u32,
                    _group_count_y: u32,
                    _group_count_z: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_dispatch)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDispatch\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_dispatch
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_dispatch_indirect: unsafe {
                unsafe extern "system" fn cmd_dispatch_indirect(
                    _command_buffer: CommandBuffer,
                    _buffer: Buffer,
                    _offset: DeviceSize,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_dispatch_indirect)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDispatchIndirect\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_dispatch_indirect
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_buffer: unsafe {
                unsafe extern "system" fn cmd_copy_buffer(
                    _command_buffer: CommandBuffer,
                    _src_buffer: Buffer,
                    _dst_buffer: Buffer,
                    _region_count: u32,
                    _p_regions: *const BufferCopy,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_copy_buffer)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCopyBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_image: unsafe {
                unsafe extern "system" fn cmd_copy_image(
                    _command_buffer: CommandBuffer,
                    _src_image: Image,
                    _src_image_layout: ImageLayout,
                    _dst_image: Image,
                    _dst_image_layout: ImageLayout,
                    _region_count: u32,
                    _p_regions: *const ImageCopy,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_copy_image)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCopyImage\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_blit_image: unsafe {
                unsafe extern "system" fn cmd_blit_image(
                    _command_buffer: CommandBuffer,
                    _src_image: Image,
                    _src_image_layout: ImageLayout,
                    _dst_image: Image,
                    _dst_image_layout: ImageLayout,
                    _region_count: u32,
                    _p_regions: *const ImageBlit,
                    _filter: Filter,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_blit_image)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBlitImage\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_blit_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_buffer_to_image: unsafe {
                unsafe extern "system" fn cmd_copy_buffer_to_image(
                    _command_buffer: CommandBuffer,
                    _src_buffer: Buffer,
                    _dst_image: Image,
                    _dst_image_layout: ImageLayout,
                    _region_count: u32,
                    _p_regions: *const BufferImageCopy,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_buffer_to_image)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCopyBufferToImage\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_buffer_to_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_image_to_buffer: unsafe {
                unsafe extern "system" fn cmd_copy_image_to_buffer(
                    _command_buffer: CommandBuffer,
                    _src_image: Image,
                    _src_image_layout: ImageLayout,
                    _dst_buffer: Buffer,
                    _region_count: u32,
                    _p_regions: *const BufferImageCopy,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_image_to_buffer)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCopyImageToBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_image_to_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_update_buffer: unsafe {
                unsafe extern "system" fn cmd_update_buffer(
                    _command_buffer: CommandBuffer,
                    _dst_buffer: Buffer,
                    _dst_offset: DeviceSize,
                    _data_size: DeviceSize,
                    _p_data: *const c_void,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_update_buffer)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdUpdateBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_update_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_fill_buffer: unsafe {
                unsafe extern "system" fn cmd_fill_buffer(
                    _command_buffer: CommandBuffer,
                    _dst_buffer: Buffer,
                    _dst_offset: DeviceSize,
                    _size: DeviceSize,
                    _data: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_fill_buffer)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdFillBuffer\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_fill_buffer
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_clear_color_image: unsafe {
                unsafe extern "system" fn cmd_clear_color_image(
                    _command_buffer: CommandBuffer,
                    _image: Image,
                    _image_layout: ImageLayout,
                    _p_color: *const ClearColorValue,
                    _range_count: u32,
                    _p_ranges: *const ImageSubresourceRange,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_clear_color_image)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdClearColorImage\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_clear_color_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_clear_depth_stencil_image: unsafe {
                unsafe extern "system" fn cmd_clear_depth_stencil_image(
                    _command_buffer: CommandBuffer,
                    _image: Image,
                    _image_layout: ImageLayout,
                    _p_depth_stencil: *const ClearDepthStencilValue,
                    _range_count: u32,
                    _p_ranges: *const ImageSubresourceRange,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_clear_depth_stencil_image)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdClearDepthStencilImage\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_clear_depth_stencil_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_clear_attachments: unsafe {
                unsafe extern "system" fn cmd_clear_attachments(
                    _command_buffer: CommandBuffer,
                    _attachment_count: u32,
                    _p_attachments: *const ClearAttachment,
                    _rect_count: u32,
                    _p_rects: *const ClearRect,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_clear_attachments)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdClearAttachments\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_clear_attachments
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_resolve_image: unsafe {
                unsafe extern "system" fn cmd_resolve_image(
                    _command_buffer: CommandBuffer,
                    _src_image: Image,
                    _src_image_layout: ImageLayout,
                    _dst_image: Image,
                    _dst_image_layout: ImageLayout,
                    _region_count: u32,
                    _p_regions: *const ImageResolve,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_resolve_image)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdResolveImage\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_resolve_image
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_event: unsafe {
                unsafe extern "system" fn cmd_set_event(
                    _command_buffer: CommandBuffer,
                    _event: Event,
                    _stage_mask: PipelineStageFlags,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_event)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetEvent\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_event
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_reset_event: unsafe {
                unsafe extern "system" fn cmd_reset_event(
                    _command_buffer: CommandBuffer,
                    _event: Event,
                    _stage_mask: PipelineStageFlags,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_reset_event)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdResetEvent\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_reset_event
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_wait_events: unsafe {
                unsafe extern "system" fn cmd_wait_events(
                    _command_buffer: CommandBuffer,
                    _event_count: u32,
                    _p_events: *const Event,
                    _src_stage_mask: PipelineStageFlags,
                    _dst_stage_mask: PipelineStageFlags,
                    _memory_barrier_count: u32,
                    _p_memory_barriers: *const MemoryBarrier,
                    _buffer_memory_barrier_count: u32,
                    _p_buffer_memory_barriers: *const BufferMemoryBarrier,
                    _image_memory_barrier_count: u32,
                    _p_image_memory_barriers: *const ImageMemoryBarrier,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_wait_events)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdWaitEvents\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_wait_events
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_pipeline_barrier: unsafe {
                unsafe extern "system" fn cmd_pipeline_barrier(
                    _command_buffer: CommandBuffer,
                    _src_stage_mask: PipelineStageFlags,
                    _dst_stage_mask: PipelineStageFlags,
                    _dependency_flags: DependencyFlags,
                    _memory_barrier_count: u32,
                    _p_memory_barriers: *const MemoryBarrier,
                    _buffer_memory_barrier_count: u32,
                    _p_buffer_memory_barriers: *const BufferMemoryBarrier,
                    _image_memory_barrier_count: u32,
                    _p_image_memory_barriers: *const ImageMemoryBarrier,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_pipeline_barrier)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdPipelineBarrier\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_pipeline_barrier
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_query: unsafe {
                unsafe extern "system" fn cmd_begin_query(
                    _command_buffer: CommandBuffer,
                    _query_pool: QueryPool,
                    _query: u32,
                    _flags: QueryControlFlags,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_begin_query)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBeginQuery\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_query
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_query: unsafe {
                unsafe extern "system" fn cmd_end_query(
                    _command_buffer: CommandBuffer,
                    _query_pool: QueryPool,
                    _query: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_end_query)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdEndQuery\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_query
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_reset_query_pool: unsafe {
                unsafe extern "system" fn cmd_reset_query_pool(
                    _command_buffer: CommandBuffer,
                    _query_pool: QueryPool,
                    _first_query: u32,
                    _query_count: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_reset_query_pool)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdResetQueryPool\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_reset_query_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_write_timestamp: unsafe {
                unsafe extern "system" fn cmd_write_timestamp(
                    _command_buffer: CommandBuffer,
                    _pipeline_stage: PipelineStageFlags,
                    _query_pool: QueryPool,
                    _query: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_write_timestamp)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdWriteTimestamp\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_write_timestamp
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_copy_query_pool_results: unsafe {
                unsafe extern "system" fn cmd_copy_query_pool_results(
                    _command_buffer: CommandBuffer,
                    _query_pool: QueryPool,
                    _first_query: u32,
                    _query_count: u32,
                    _dst_buffer: Buffer,
                    _dst_offset: DeviceSize,
                    _stride: DeviceSize,
                    _flags: QueryResultFlags,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_copy_query_pool_results)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdCopyQueryPoolResults\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_copy_query_pool_results
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_push_constants: unsafe {
                unsafe extern "system" fn cmd_push_constants(
                    _command_buffer: CommandBuffer,
                    _layout: PipelineLayout,
                    _stage_flags: ShaderStageFlags,
                    _offset: u32,
                    _size: u32,
                    _p_values: *const c_void,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_push_constants)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdPushConstants\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_push_constants
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_render_pass: unsafe {
                unsafe extern "system" fn cmd_begin_render_pass(
                    _command_buffer: CommandBuffer,
                    _p_render_pass_begin: *const RenderPassBeginInfo,
                    _contents: SubpassContents,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_render_pass)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBeginRenderPass\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_render_pass
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_next_subpass: unsafe {
                unsafe extern "system" fn cmd_next_subpass(
                    _command_buffer: CommandBuffer,
                    _contents: SubpassContents,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_next_subpass)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdNextSubpass\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_next_subpass
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_render_pass: unsafe {
                unsafe extern "system" fn cmd_end_render_pass(_command_buffer: CommandBuffer) {
                    panic!(concat!("Unable to load ", stringify!(cmd_end_render_pass)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdEndRenderPass\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_render_pass
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_execute_commands: unsafe {
                unsafe extern "system" fn cmd_execute_commands(
                    _command_buffer: CommandBuffer,
                    _command_buffer_count: u32,
                    _p_command_buffers: *const CommandBuffer,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_execute_commands)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdExecuteCommands\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_execute_commands
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDevice.html>"]
    pub unsafe fn destroy_device(&self, device: Device, p_allocator: *const AllocationCallbacks) {
        (self.destroy_device)(device, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceQueue.html>"]
    pub unsafe fn get_device_queue(
        &self,
        device: Device,
        queue_family_index: u32,
        queue_index: u32,
        p_queue: *mut Queue,
    ) {
        (self.get_device_queue)(device, queue_family_index, queue_index, p_queue)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueSubmit.html>"]
    pub unsafe fn queue_submit(
        &self,
        queue: Queue,
        submit_count: u32,
        p_submits: *const SubmitInfo,
        fence: Fence,
    ) -> Result {
        (self.queue_submit)(queue, submit_count, p_submits, fence)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueWaitIdle.html>"]
    pub unsafe fn queue_wait_idle(&self, queue: Queue) -> Result {
        (self.queue_wait_idle)(queue)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDeviceWaitIdle.html>"]
    pub unsafe fn device_wait_idle(&self, device: Device) -> Result {
        (self.device_wait_idle)(device)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAllocateMemory.html>"]
    pub unsafe fn allocate_memory(
        &self,
        device: Device,
        p_allocate_info: *const MemoryAllocateInfo,
        p_allocator: *const AllocationCallbacks,
        p_memory: *mut DeviceMemory,
    ) -> Result {
        (self.allocate_memory)(device, p_allocate_info, p_allocator, p_memory)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFreeMemory.html>"]
    pub unsafe fn free_memory(
        &self,
        device: Device,
        memory: DeviceMemory,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.free_memory)(device, memory, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkMapMemory.html>"]
    pub unsafe fn map_memory(
        &self,
        device: Device,
        memory: DeviceMemory,
        offset: DeviceSize,
        size: DeviceSize,
        flags: MemoryMapFlags,
        pp_data: *mut *mut c_void,
    ) -> Result {
        (self.map_memory)(device, memory, offset, size, flags, pp_data)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUnmapMemory.html>"]
    pub unsafe fn unmap_memory(&self, device: Device, memory: DeviceMemory) {
        (self.unmap_memory)(device, memory)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFlushMappedMemoryRanges.html>"]
    pub unsafe fn flush_mapped_memory_ranges(
        &self,
        device: Device,
        memory_range_count: u32,
        p_memory_ranges: *const MappedMemoryRange,
    ) -> Result {
        (self.flush_mapped_memory_ranges)(device, memory_range_count, p_memory_ranges)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkInvalidateMappedMemoryRanges.html>"]
    pub unsafe fn invalidate_mapped_memory_ranges(
        &self,
        device: Device,
        memory_range_count: u32,
        p_memory_ranges: *const MappedMemoryRange,
    ) -> Result {
        (self.invalidate_mapped_memory_ranges)(device, memory_range_count, p_memory_ranges)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceMemoryCommitment.html>"]
    pub unsafe fn get_device_memory_commitment(
        &self,
        device: Device,
        memory: DeviceMemory,
        p_committed_memory_in_bytes: *mut DeviceSize,
    ) {
        (self.get_device_memory_commitment)(device, memory, p_committed_memory_in_bytes)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindBufferMemory.html>"]
    pub unsafe fn bind_buffer_memory(
        &self,
        device: Device,
        buffer: Buffer,
        memory: DeviceMemory,
        memory_offset: DeviceSize,
    ) -> Result {
        (self.bind_buffer_memory)(device, buffer, memory, memory_offset)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindImageMemory.html>"]
    pub unsafe fn bind_image_memory(
        &self,
        device: Device,
        image: Image,
        memory: DeviceMemory,
        memory_offset: DeviceSize,
    ) -> Result {
        (self.bind_image_memory)(device, image, memory, memory_offset)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferMemoryRequirements.html>"]
    pub unsafe fn get_buffer_memory_requirements(
        &self,
        device: Device,
        buffer: Buffer,
        p_memory_requirements: *mut MemoryRequirements,
    ) {
        (self.get_buffer_memory_requirements)(device, buffer, p_memory_requirements)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageMemoryRequirements.html>"]
    pub unsafe fn get_image_memory_requirements(
        &self,
        device: Device,
        image: Image,
        p_memory_requirements: *mut MemoryRequirements,
    ) {
        (self.get_image_memory_requirements)(device, image, p_memory_requirements)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageSparseMemoryRequirements.html>"]
    pub unsafe fn get_image_sparse_memory_requirements(
        &self,
        device: Device,
        image: Image,
        p_sparse_memory_requirement_count: *mut u32,
        p_sparse_memory_requirements: *mut SparseImageMemoryRequirements,
    ) {
        (self.get_image_sparse_memory_requirements)(
            device,
            image,
            p_sparse_memory_requirement_count,
            p_sparse_memory_requirements,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkQueueBindSparse.html>"]
    pub unsafe fn queue_bind_sparse(
        &self,
        queue: Queue,
        bind_info_count: u32,
        p_bind_info: *const BindSparseInfo,
        fence: Fence,
    ) -> Result {
        (self.queue_bind_sparse)(queue, bind_info_count, p_bind_info, fence)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateFence.html>"]
    pub unsafe fn create_fence(
        &self,
        device: Device,
        p_create_info: *const FenceCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_fence: *mut Fence,
    ) -> Result {
        (self.create_fence)(device, p_create_info, p_allocator, p_fence)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyFence.html>"]
    pub unsafe fn destroy_fence(
        &self,
        device: Device,
        fence: Fence,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_fence)(device, fence, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetFences.html>"]
    pub unsafe fn reset_fences(
        &self,
        device: Device,
        fence_count: u32,
        p_fences: *const Fence,
    ) -> Result {
        (self.reset_fences)(device, fence_count, p_fences)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetFenceStatus.html>"]
    pub unsafe fn get_fence_status(&self, device: Device, fence: Fence) -> Result {
        (self.get_fence_status)(device, fence)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitForFences.html>"]
    pub unsafe fn wait_for_fences(
        &self,
        device: Device,
        fence_count: u32,
        p_fences: *const Fence,
        wait_all: Bool32,
        timeout: u64,
    ) -> Result {
        (self.wait_for_fences)(device, fence_count, p_fences, wait_all, timeout)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSemaphore.html>"]
    pub unsafe fn create_semaphore(
        &self,
        device: Device,
        p_create_info: *const SemaphoreCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_semaphore: *mut Semaphore,
    ) -> Result {
        (self.create_semaphore)(device, p_create_info, p_allocator, p_semaphore)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySemaphore.html>"]
    pub unsafe fn destroy_semaphore(
        &self,
        device: Device,
        semaphore: Semaphore,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_semaphore)(device, semaphore, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateEvent.html>"]
    pub unsafe fn create_event(
        &self,
        device: Device,
        p_create_info: *const EventCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_event: *mut Event,
    ) -> Result {
        (self.create_event)(device, p_create_info, p_allocator, p_event)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyEvent.html>"]
    pub unsafe fn destroy_event(
        &self,
        device: Device,
        event: Event,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_event)(device, event, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetEventStatus.html>"]
    pub unsafe fn get_event_status(&self, device: Device, event: Event) -> Result {
        (self.get_event_status)(device, event)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSetEvent.html>"]
    pub unsafe fn set_event(&self, device: Device, event: Event) -> Result {
        (self.set_event)(device, event)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetEvent.html>"]
    pub unsafe fn reset_event(&self, device: Device, event: Event) -> Result {
        (self.reset_event)(device, event)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateQueryPool.html>"]
    pub unsafe fn create_query_pool(
        &self,
        device: Device,
        p_create_info: *const QueryPoolCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_query_pool: *mut QueryPool,
    ) -> Result {
        (self.create_query_pool)(device, p_create_info, p_allocator, p_query_pool)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyQueryPool.html>"]
    pub unsafe fn destroy_query_pool(
        &self,
        device: Device,
        query_pool: QueryPool,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_query_pool)(device, query_pool, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetQueryPoolResults.html>"]
    pub unsafe fn get_query_pool_results(
        &self,
        device: Device,
        query_pool: QueryPool,
        first_query: u32,
        query_count: u32,
        data_size: usize,
        p_data: *mut c_void,
        stride: DeviceSize,
        flags: QueryResultFlags,
    ) -> Result {
        (self.get_query_pool_results)(
            device,
            query_pool,
            first_query,
            query_count,
            data_size,
            p_data,
            stride,
            flags,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateBuffer.html>"]
    pub unsafe fn create_buffer(
        &self,
        device: Device,
        p_create_info: *const BufferCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_buffer: *mut Buffer,
    ) -> Result {
        (self.create_buffer)(device, p_create_info, p_allocator, p_buffer)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyBuffer.html>"]
    pub unsafe fn destroy_buffer(
        &self,
        device: Device,
        buffer: Buffer,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_buffer)(device, buffer, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateBufferView.html>"]
    pub unsafe fn create_buffer_view(
        &self,
        device: Device,
        p_create_info: *const BufferViewCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_view: *mut BufferView,
    ) -> Result {
        (self.create_buffer_view)(device, p_create_info, p_allocator, p_view)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyBufferView.html>"]
    pub unsafe fn destroy_buffer_view(
        &self,
        device: Device,
        buffer_view: BufferView,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_buffer_view)(device, buffer_view, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateImage.html>"]
    pub unsafe fn create_image(
        &self,
        device: Device,
        p_create_info: *const ImageCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_image: *mut Image,
    ) -> Result {
        (self.create_image)(device, p_create_info, p_allocator, p_image)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyImage.html>"]
    pub unsafe fn destroy_image(
        &self,
        device: Device,
        image: Image,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_image)(device, image, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageSubresourceLayout.html>"]
    pub unsafe fn get_image_subresource_layout(
        &self,
        device: Device,
        image: Image,
        p_subresource: *const ImageSubresource,
        p_layout: *mut SubresourceLayout,
    ) {
        (self.get_image_subresource_layout)(device, image, p_subresource, p_layout)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateImageView.html>"]
    pub unsafe fn create_image_view(
        &self,
        device: Device,
        p_create_info: *const ImageViewCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_view: *mut ImageView,
    ) -> Result {
        (self.create_image_view)(device, p_create_info, p_allocator, p_view)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyImageView.html>"]
    pub unsafe fn destroy_image_view(
        &self,
        device: Device,
        image_view: ImageView,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_image_view)(device, image_view, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateShaderModule.html>"]
    pub unsafe fn create_shader_module(
        &self,
        device: Device,
        p_create_info: *const ShaderModuleCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_shader_module: *mut ShaderModule,
    ) -> Result {
        (self.create_shader_module)(device, p_create_info, p_allocator, p_shader_module)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyShaderModule.html>"]
    pub unsafe fn destroy_shader_module(
        &self,
        device: Device,
        shader_module: ShaderModule,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_shader_module)(device, shader_module, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreatePipelineCache.html>"]
    pub unsafe fn create_pipeline_cache(
        &self,
        device: Device,
        p_create_info: *const PipelineCacheCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_pipeline_cache: *mut PipelineCache,
    ) -> Result {
        (self.create_pipeline_cache)(device, p_create_info, p_allocator, p_pipeline_cache)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyPipelineCache.html>"]
    pub unsafe fn destroy_pipeline_cache(
        &self,
        device: Device,
        pipeline_cache: PipelineCache,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_pipeline_cache)(device, pipeline_cache, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineCacheData.html>"]
    pub unsafe fn get_pipeline_cache_data(
        &self,
        device: Device,
        pipeline_cache: PipelineCache,
        p_data_size: *mut usize,
        p_data: *mut c_void,
    ) -> Result {
        (self.get_pipeline_cache_data)(device, pipeline_cache, p_data_size, p_data)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkMergePipelineCaches.html>"]
    pub unsafe fn merge_pipeline_caches(
        &self,
        device: Device,
        dst_cache: PipelineCache,
        src_cache_count: u32,
        p_src_caches: *const PipelineCache,
    ) -> Result {
        (self.merge_pipeline_caches)(device, dst_cache, src_cache_count, p_src_caches)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateGraphicsPipelines.html>"]
    pub unsafe fn create_graphics_pipelines(
        &self,
        device: Device,
        pipeline_cache: PipelineCache,
        create_info_count: u32,
        p_create_infos: *const GraphicsPipelineCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_pipelines: *mut Pipeline,
    ) -> Result {
        (self.create_graphics_pipelines)(
            device,
            pipeline_cache,
            create_info_count,
            p_create_infos,
            p_allocator,
            p_pipelines,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateComputePipelines.html>"]
    pub unsafe fn create_compute_pipelines(
        &self,
        device: Device,
        pipeline_cache: PipelineCache,
        create_info_count: u32,
        p_create_infos: *const ComputePipelineCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_pipelines: *mut Pipeline,
    ) -> Result {
        (self.create_compute_pipelines)(
            device,
            pipeline_cache,
            create_info_count,
            p_create_infos,
            p_allocator,
            p_pipelines,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyPipeline.html>"]
    pub unsafe fn destroy_pipeline(
        &self,
        device: Device,
        pipeline: Pipeline,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_pipeline)(device, pipeline, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreatePipelineLayout.html>"]
    pub unsafe fn create_pipeline_layout(
        &self,
        device: Device,
        p_create_info: *const PipelineLayoutCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_pipeline_layout: *mut PipelineLayout,
    ) -> Result {
        (self.create_pipeline_layout)(device, p_create_info, p_allocator, p_pipeline_layout)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyPipelineLayout.html>"]
    pub unsafe fn destroy_pipeline_layout(
        &self,
        device: Device,
        pipeline_layout: PipelineLayout,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_pipeline_layout)(device, pipeline_layout, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSampler.html>"]
    pub unsafe fn create_sampler(
        &self,
        device: Device,
        p_create_info: *const SamplerCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_sampler: *mut Sampler,
    ) -> Result {
        (self.create_sampler)(device, p_create_info, p_allocator, p_sampler)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySampler.html>"]
    pub unsafe fn destroy_sampler(
        &self,
        device: Device,
        sampler: Sampler,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_sampler)(device, sampler, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDescriptorSetLayout.html>"]
    pub unsafe fn create_descriptor_set_layout(
        &self,
        device: Device,
        p_create_info: *const DescriptorSetLayoutCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_set_layout: *mut DescriptorSetLayout,
    ) -> Result {
        (self.create_descriptor_set_layout)(device, p_create_info, p_allocator, p_set_layout)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDescriptorSetLayout.html>"]
    pub unsafe fn destroy_descriptor_set_layout(
        &self,
        device: Device,
        descriptor_set_layout: DescriptorSetLayout,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_descriptor_set_layout)(device, descriptor_set_layout, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDescriptorPool.html>"]
    pub unsafe fn create_descriptor_pool(
        &self,
        device: Device,
        p_create_info: *const DescriptorPoolCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_descriptor_pool: *mut DescriptorPool,
    ) -> Result {
        (self.create_descriptor_pool)(device, p_create_info, p_allocator, p_descriptor_pool)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDescriptorPool.html>"]
    pub unsafe fn destroy_descriptor_pool(
        &self,
        device: Device,
        descriptor_pool: DescriptorPool,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_descriptor_pool)(device, descriptor_pool, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetDescriptorPool.html>"]
    pub unsafe fn reset_descriptor_pool(
        &self,
        device: Device,
        descriptor_pool: DescriptorPool,
        flags: DescriptorPoolResetFlags,
    ) -> Result {
        (self.reset_descriptor_pool)(device, descriptor_pool, flags)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAllocateDescriptorSets.html>"]
    pub unsafe fn allocate_descriptor_sets(
        &self,
        device: Device,
        p_allocate_info: *const DescriptorSetAllocateInfo,
        p_descriptor_sets: *mut DescriptorSet,
    ) -> Result {
        (self.allocate_descriptor_sets)(device, p_allocate_info, p_descriptor_sets)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFreeDescriptorSets.html>"]
    pub unsafe fn free_descriptor_sets(
        &self,
        device: Device,
        descriptor_pool: DescriptorPool,
        descriptor_set_count: u32,
        p_descriptor_sets: *const DescriptorSet,
    ) -> Result {
        (self.free_descriptor_sets)(
            device,
            descriptor_pool,
            descriptor_set_count,
            p_descriptor_sets,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUpdateDescriptorSets.html>"]
    pub unsafe fn update_descriptor_sets(
        &self,
        device: Device,
        descriptor_write_count: u32,
        p_descriptor_writes: *const WriteDescriptorSet,
        descriptor_copy_count: u32,
        p_descriptor_copies: *const CopyDescriptorSet,
    ) {
        (self.update_descriptor_sets)(
            device,
            descriptor_write_count,
            p_descriptor_writes,
            descriptor_copy_count,
            p_descriptor_copies,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateFramebuffer.html>"]
    pub unsafe fn create_framebuffer(
        &self,
        device: Device,
        p_create_info: *const FramebufferCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_framebuffer: *mut Framebuffer,
    ) -> Result {
        (self.create_framebuffer)(device, p_create_info, p_allocator, p_framebuffer)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyFramebuffer.html>"]
    pub unsafe fn destroy_framebuffer(
        &self,
        device: Device,
        framebuffer: Framebuffer,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_framebuffer)(device, framebuffer, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRenderPass.html>"]
    pub unsafe fn create_render_pass(
        &self,
        device: Device,
        p_create_info: *const RenderPassCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_render_pass: *mut RenderPass,
    ) -> Result {
        (self.create_render_pass)(device, p_create_info, p_allocator, p_render_pass)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyRenderPass.html>"]
    pub unsafe fn destroy_render_pass(
        &self,
        device: Device,
        render_pass: RenderPass,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_render_pass)(device, render_pass, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetRenderAreaGranularity.html>"]
    pub unsafe fn get_render_area_granularity(
        &self,
        device: Device,
        render_pass: RenderPass,
        p_granularity: *mut Extent2D,
    ) {
        (self.get_render_area_granularity)(device, render_pass, p_granularity)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateCommandPool.html>"]
    pub unsafe fn create_command_pool(
        &self,
        device: Device,
        p_create_info: *const CommandPoolCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_command_pool: *mut CommandPool,
    ) -> Result {
        (self.create_command_pool)(device, p_create_info, p_allocator, p_command_pool)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyCommandPool.html>"]
    pub unsafe fn destroy_command_pool(
        &self,
        device: Device,
        command_pool: CommandPool,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_command_pool)(device, command_pool, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetCommandPool.html>"]
    pub unsafe fn reset_command_pool(
        &self,
        device: Device,
        command_pool: CommandPool,
        flags: CommandPoolResetFlags,
    ) -> Result {
        (self.reset_command_pool)(device, command_pool, flags)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkAllocateCommandBuffers.html>"]
    pub unsafe fn allocate_command_buffers(
        &self,
        device: Device,
        p_allocate_info: *const CommandBufferAllocateInfo,
        p_command_buffers: *mut CommandBuffer,
    ) -> Result {
        (self.allocate_command_buffers)(device, p_allocate_info, p_command_buffers)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkFreeCommandBuffers.html>"]
    pub unsafe fn free_command_buffers(
        &self,
        device: Device,
        command_pool: CommandPool,
        command_buffer_count: u32,
        p_command_buffers: *const CommandBuffer,
    ) {
        (self.free_command_buffers)(
            device,
            command_pool,
            command_buffer_count,
            p_command_buffers,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBeginCommandBuffer.html>"]
    pub unsafe fn begin_command_buffer(
        &self,
        command_buffer: CommandBuffer,
        p_begin_info: *const CommandBufferBeginInfo,
    ) -> Result {
        (self.begin_command_buffer)(command_buffer, p_begin_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEndCommandBuffer.html>"]
    pub unsafe fn end_command_buffer(&self, command_buffer: CommandBuffer) -> Result {
        (self.end_command_buffer)(command_buffer)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetCommandBuffer.html>"]
    pub unsafe fn reset_command_buffer(
        &self,
        command_buffer: CommandBuffer,
        flags: CommandBufferResetFlags,
    ) -> Result {
        (self.reset_command_buffer)(command_buffer, flags)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindPipeline.html>"]
    pub unsafe fn cmd_bind_pipeline(
        &self,
        command_buffer: CommandBuffer,
        pipeline_bind_point: PipelineBindPoint,
        pipeline: Pipeline,
    ) {
        (self.cmd_bind_pipeline)(command_buffer, pipeline_bind_point, pipeline)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetViewport.html>"]
    pub unsafe fn cmd_set_viewport(
        &self,
        command_buffer: CommandBuffer,
        first_viewport: u32,
        viewport_count: u32,
        p_viewports: *const Viewport,
    ) {
        (self.cmd_set_viewport)(command_buffer, first_viewport, viewport_count, p_viewports)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetScissor.html>"]
    pub unsafe fn cmd_set_scissor(
        &self,
        command_buffer: CommandBuffer,
        first_scissor: u32,
        scissor_count: u32,
        p_scissors: *const Rect2D,
    ) {
        (self.cmd_set_scissor)(command_buffer, first_scissor, scissor_count, p_scissors)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetLineWidth.html>"]
    pub unsafe fn cmd_set_line_width(&self, command_buffer: CommandBuffer, line_width: f32) {
        (self.cmd_set_line_width)(command_buffer, line_width)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBias.html>"]
    pub unsafe fn cmd_set_depth_bias(
        &self,
        command_buffer: CommandBuffer,
        depth_bias_constant_factor: f32,
        depth_bias_clamp: f32,
        depth_bias_slope_factor: f32,
    ) {
        (self.cmd_set_depth_bias)(
            command_buffer,
            depth_bias_constant_factor,
            depth_bias_clamp,
            depth_bias_slope_factor,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetBlendConstants.html>"]
    pub unsafe fn cmd_set_blend_constants(
        &self,
        command_buffer: CommandBuffer,
        blend_constants: *const [f32; 4],
    ) {
        (self.cmd_set_blend_constants)(command_buffer, blend_constants)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBounds.html>"]
    pub unsafe fn cmd_set_depth_bounds(
        &self,
        command_buffer: CommandBuffer,
        min_depth_bounds: f32,
        max_depth_bounds: f32,
    ) {
        (self.cmd_set_depth_bounds)(command_buffer, min_depth_bounds, max_depth_bounds)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilCompareMask.html>"]
    pub unsafe fn cmd_set_stencil_compare_mask(
        &self,
        command_buffer: CommandBuffer,
        face_mask: StencilFaceFlags,
        compare_mask: u32,
    ) {
        (self.cmd_set_stencil_compare_mask)(command_buffer, face_mask, compare_mask)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilWriteMask.html>"]
    pub unsafe fn cmd_set_stencil_write_mask(
        &self,
        command_buffer: CommandBuffer,
        face_mask: StencilFaceFlags,
        write_mask: u32,
    ) {
        (self.cmd_set_stencil_write_mask)(command_buffer, face_mask, write_mask)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetStencilReference.html>"]
    pub unsafe fn cmd_set_stencil_reference(
        &self,
        command_buffer: CommandBuffer,
        face_mask: StencilFaceFlags,
        reference: u32,
    ) {
        (self.cmd_set_stencil_reference)(command_buffer, face_mask, reference)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindDescriptorSets.html>"]
    pub unsafe fn cmd_bind_descriptor_sets(
        &self,
        command_buffer: CommandBuffer,
        pipeline_bind_point: PipelineBindPoint,
        layout: PipelineLayout,
        first_set: u32,
        descriptor_set_count: u32,
        p_descriptor_sets: *const DescriptorSet,
        dynamic_offset_count: u32,
        p_dynamic_offsets: *const u32,
    ) {
        (self.cmd_bind_descriptor_sets)(
            command_buffer,
            pipeline_bind_point,
            layout,
            first_set,
            descriptor_set_count,
            p_descriptor_sets,
            dynamic_offset_count,
            p_dynamic_offsets,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindIndexBuffer.html>"]
    pub unsafe fn cmd_bind_index_buffer(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        index_type: IndexType,
    ) {
        (self.cmd_bind_index_buffer)(command_buffer, buffer, offset, index_type)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBindVertexBuffers.html>"]
    pub unsafe fn cmd_bind_vertex_buffers(
        &self,
        command_buffer: CommandBuffer,
        first_binding: u32,
        binding_count: u32,
        p_buffers: *const Buffer,
        p_offsets: *const DeviceSize,
    ) {
        (self.cmd_bind_vertex_buffers)(
            command_buffer,
            first_binding,
            binding_count,
            p_buffers,
            p_offsets,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDraw.html>"]
    pub unsafe fn cmd_draw(
        &self,
        command_buffer: CommandBuffer,
        vertex_count: u32,
        instance_count: u32,
        first_vertex: u32,
        first_instance: u32,
    ) {
        (self.cmd_draw)(
            command_buffer,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexed.html>"]
    pub unsafe fn cmd_draw_indexed(
        &self,
        command_buffer: CommandBuffer,
        index_count: u32,
        instance_count: u32,
        first_index: u32,
        vertex_offset: i32,
        first_instance: u32,
    ) {
        (self.cmd_draw_indexed)(
            command_buffer,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirect.html>"]
    pub unsafe fn cmd_draw_indirect(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indirect)(command_buffer, buffer, offset, draw_count, stride)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexedIndirect.html>"]
    pub unsafe fn cmd_draw_indexed_indirect(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indexed_indirect)(command_buffer, buffer, offset, draw_count, stride)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDispatch.html>"]
    pub unsafe fn cmd_dispatch(
        &self,
        command_buffer: CommandBuffer,
        group_count_x: u32,
        group_count_y: u32,
        group_count_z: u32,
    ) {
        (self.cmd_dispatch)(command_buffer, group_count_x, group_count_y, group_count_z)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDispatchIndirect.html>"]
    pub unsafe fn cmd_dispatch_indirect(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
    ) {
        (self.cmd_dispatch_indirect)(command_buffer, buffer, offset)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyBuffer.html>"]
    pub unsafe fn cmd_copy_buffer(
        &self,
        command_buffer: CommandBuffer,
        src_buffer: Buffer,
        dst_buffer: Buffer,
        region_count: u32,
        p_regions: *const BufferCopy,
    ) {
        (self.cmd_copy_buffer)(
            command_buffer,
            src_buffer,
            dst_buffer,
            region_count,
            p_regions,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyImage.html>"]
    pub unsafe fn cmd_copy_image(
        &self,
        command_buffer: CommandBuffer,
        src_image: Image,
        src_image_layout: ImageLayout,
        dst_image: Image,
        dst_image_layout: ImageLayout,
        region_count: u32,
        p_regions: *const ImageCopy,
    ) {
        (self.cmd_copy_image)(
            command_buffer,
            src_image,
            src_image_layout,
            dst_image,
            dst_image_layout,
            region_count,
            p_regions,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBlitImage.html>"]
    pub unsafe fn cmd_blit_image(
        &self,
        command_buffer: CommandBuffer,
        src_image: Image,
        src_image_layout: ImageLayout,
        dst_image: Image,
        dst_image_layout: ImageLayout,
        region_count: u32,
        p_regions: *const ImageBlit,
        filter: Filter,
    ) {
        (self.cmd_blit_image)(
            command_buffer,
            src_image,
            src_image_layout,
            dst_image,
            dst_image_layout,
            region_count,
            p_regions,
            filter,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyBufferToImage.html>"]
    pub unsafe fn cmd_copy_buffer_to_image(
        &self,
        command_buffer: CommandBuffer,
        src_buffer: Buffer,
        dst_image: Image,
        dst_image_layout: ImageLayout,
        region_count: u32,
        p_regions: *const BufferImageCopy,
    ) {
        (self.cmd_copy_buffer_to_image)(
            command_buffer,
            src_buffer,
            dst_image,
            dst_image_layout,
            region_count,
            p_regions,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyImageToBuffer.html>"]
    pub unsafe fn cmd_copy_image_to_buffer(
        &self,
        command_buffer: CommandBuffer,
        src_image: Image,
        src_image_layout: ImageLayout,
        dst_buffer: Buffer,
        region_count: u32,
        p_regions: *const BufferImageCopy,
    ) {
        (self.cmd_copy_image_to_buffer)(
            command_buffer,
            src_image,
            src_image_layout,
            dst_buffer,
            region_count,
            p_regions,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdUpdateBuffer.html>"]
    pub unsafe fn cmd_update_buffer(
        &self,
        command_buffer: CommandBuffer,
        dst_buffer: Buffer,
        dst_offset: DeviceSize,
        data_size: DeviceSize,
        p_data: *const c_void,
    ) {
        (self.cmd_update_buffer)(command_buffer, dst_buffer, dst_offset, data_size, p_data)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdFillBuffer.html>"]
    pub unsafe fn cmd_fill_buffer(
        &self,
        command_buffer: CommandBuffer,
        dst_buffer: Buffer,
        dst_offset: DeviceSize,
        size: DeviceSize,
        data: u32,
    ) {
        (self.cmd_fill_buffer)(command_buffer, dst_buffer, dst_offset, size, data)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdClearColorImage.html>"]
    pub unsafe fn cmd_clear_color_image(
        &self,
        command_buffer: CommandBuffer,
        image: Image,
        image_layout: ImageLayout,
        p_color: *const ClearColorValue,
        range_count: u32,
        p_ranges: *const ImageSubresourceRange,
    ) {
        (self.cmd_clear_color_image)(
            command_buffer,
            image,
            image_layout,
            p_color,
            range_count,
            p_ranges,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdClearDepthStencilImage.html>"]
    pub unsafe fn cmd_clear_depth_stencil_image(
        &self,
        command_buffer: CommandBuffer,
        image: Image,
        image_layout: ImageLayout,
        p_depth_stencil: *const ClearDepthStencilValue,
        range_count: u32,
        p_ranges: *const ImageSubresourceRange,
    ) {
        (self.cmd_clear_depth_stencil_image)(
            command_buffer,
            image,
            image_layout,
            p_depth_stencil,
            range_count,
            p_ranges,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdClearAttachments.html>"]
    pub unsafe fn cmd_clear_attachments(
        &self,
        command_buffer: CommandBuffer,
        attachment_count: u32,
        p_attachments: *const ClearAttachment,
        rect_count: u32,
        p_rects: *const ClearRect,
    ) {
        (self.cmd_clear_attachments)(
            command_buffer,
            attachment_count,
            p_attachments,
            rect_count,
            p_rects,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResolveImage.html>"]
    pub unsafe fn cmd_resolve_image(
        &self,
        command_buffer: CommandBuffer,
        src_image: Image,
        src_image_layout: ImageLayout,
        dst_image: Image,
        dst_image_layout: ImageLayout,
        region_count: u32,
        p_regions: *const ImageResolve,
    ) {
        (self.cmd_resolve_image)(
            command_buffer,
            src_image,
            src_image_layout,
            dst_image,
            dst_image_layout,
            region_count,
            p_regions,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetEvent.html>"]
    pub unsafe fn cmd_set_event(
        &self,
        command_buffer: CommandBuffer,
        event: Event,
        stage_mask: PipelineStageFlags,
    ) {
        (self.cmd_set_event)(command_buffer, event, stage_mask)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResetEvent.html>"]
    pub unsafe fn cmd_reset_event(
        &self,
        command_buffer: CommandBuffer,
        event: Event,
        stage_mask: PipelineStageFlags,
    ) {
        (self.cmd_reset_event)(command_buffer, event, stage_mask)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWaitEvents.html>"]
    pub unsafe fn cmd_wait_events(
        &self,
        command_buffer: CommandBuffer,
        event_count: u32,
        p_events: *const Event,
        src_stage_mask: PipelineStageFlags,
        dst_stage_mask: PipelineStageFlags,
        memory_barrier_count: u32,
        p_memory_barriers: *const MemoryBarrier,
        buffer_memory_barrier_count: u32,
        p_buffer_memory_barriers: *const BufferMemoryBarrier,
        image_memory_barrier_count: u32,
        p_image_memory_barriers: *const ImageMemoryBarrier,
    ) {
        (self.cmd_wait_events)(
            command_buffer,
            event_count,
            p_events,
            src_stage_mask,
            dst_stage_mask,
            memory_barrier_count,
            p_memory_barriers,
            buffer_memory_barrier_count,
            p_buffer_memory_barriers,
            image_memory_barrier_count,
            p_image_memory_barriers,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPipelineBarrier.html>"]
    pub unsafe fn cmd_pipeline_barrier(
        &self,
        command_buffer: CommandBuffer,
        src_stage_mask: PipelineStageFlags,
        dst_stage_mask: PipelineStageFlags,
        dependency_flags: DependencyFlags,
        memory_barrier_count: u32,
        p_memory_barriers: *const MemoryBarrier,
        buffer_memory_barrier_count: u32,
        p_buffer_memory_barriers: *const BufferMemoryBarrier,
        image_memory_barrier_count: u32,
        p_image_memory_barriers: *const ImageMemoryBarrier,
    ) {
        (self.cmd_pipeline_barrier)(
            command_buffer,
            src_stage_mask,
            dst_stage_mask,
            dependency_flags,
            memory_barrier_count,
            p_memory_barriers,
            buffer_memory_barrier_count,
            p_buffer_memory_barriers,
            image_memory_barrier_count,
            p_image_memory_barriers,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginQuery.html>"]
    pub unsafe fn cmd_begin_query(
        &self,
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        query: u32,
        flags: QueryControlFlags,
    ) {
        (self.cmd_begin_query)(command_buffer, query_pool, query, flags)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndQuery.html>"]
    pub unsafe fn cmd_end_query(
        &self,
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        query: u32,
    ) {
        (self.cmd_end_query)(command_buffer, query_pool, query)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdResetQueryPool.html>"]
    pub unsafe fn cmd_reset_query_pool(
        &self,
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        first_query: u32,
        query_count: u32,
    ) {
        (self.cmd_reset_query_pool)(command_buffer, query_pool, first_query, query_count)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdWriteTimestamp.html>"]
    pub unsafe fn cmd_write_timestamp(
        &self,
        command_buffer: CommandBuffer,
        pipeline_stage: PipelineStageFlags,
        query_pool: QueryPool,
        query: u32,
    ) {
        (self.cmd_write_timestamp)(command_buffer, pipeline_stage, query_pool, query)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdCopyQueryPoolResults.html>"]
    pub unsafe fn cmd_copy_query_pool_results(
        &self,
        command_buffer: CommandBuffer,
        query_pool: QueryPool,
        first_query: u32,
        query_count: u32,
        dst_buffer: Buffer,
        dst_offset: DeviceSize,
        stride: DeviceSize,
        flags: QueryResultFlags,
    ) {
        (self.cmd_copy_query_pool_results)(
            command_buffer,
            query_pool,
            first_query,
            query_count,
            dst_buffer,
            dst_offset,
            stride,
            flags,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushConstants.html>"]
    pub unsafe fn cmd_push_constants(
        &self,
        command_buffer: CommandBuffer,
        layout: PipelineLayout,
        stage_flags: ShaderStageFlags,
        offset: u32,
        size: u32,
        p_values: *const c_void,
    ) {
        (self.cmd_push_constants)(command_buffer, layout, stage_flags, offset, size, p_values)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginRenderPass.html>"]
    pub unsafe fn cmd_begin_render_pass(
        &self,
        command_buffer: CommandBuffer,
        p_render_pass_begin: *const RenderPassBeginInfo,
        contents: SubpassContents,
    ) {
        (self.cmd_begin_render_pass)(command_buffer, p_render_pass_begin, contents)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdNextSubpass.html>"]
    pub unsafe fn cmd_next_subpass(
        &self,
        command_buffer: CommandBuffer,
        contents: SubpassContents,
    ) {
        (self.cmd_next_subpass)(command_buffer, contents)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndRenderPass.html>"]
    pub unsafe fn cmd_end_render_pass(&self, command_buffer: CommandBuffer) {
        (self.cmd_end_render_pass)(command_buffer)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdExecuteCommands.html>"]
    pub unsafe fn cmd_execute_commands(
        &self,
        command_buffer: CommandBuffer,
        command_buffer_count: u32,
        p_command_buffers: *const CommandBuffer,
    ) {
        (self.cmd_execute_commands)(command_buffer, command_buffer_count, p_command_buffers)
    }
}
#[allow(non_camel_case_types)]
pub type PFN_vkEnumerateInstanceVersion =
    unsafe extern "system" fn(p_api_version: *mut u32) -> Result;
#[derive(Clone)]
pub struct EntryFnV1_1 {
    pub enumerate_instance_version: PFN_vkEnumerateInstanceVersion,
}
unsafe impl Send for EntryFnV1_1 {}
unsafe impl Sync for EntryFnV1_1 {}
impl EntryFnV1_1 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            enumerate_instance_version: unsafe {
                unsafe extern "system" fn enumerate_instance_version(
                    _p_api_version: *mut u32,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_instance_version)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumerateInstanceVersion\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_instance_version
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceVersion.html>"]
    pub unsafe fn enumerate_instance_version(&self, p_api_version: *mut u32) -> Result {
        (self.enumerate_instance_version)(p_api_version)
    }
}
#[derive(Clone)]
pub struct InstanceFnV1_1 {
    pub enumerate_physical_device_groups: crate::vk::PFN_vkEnumeratePhysicalDeviceGroups,
    pub get_physical_device_features2: crate::vk::PFN_vkGetPhysicalDeviceFeatures2,
    pub get_physical_device_properties2: crate::vk::PFN_vkGetPhysicalDeviceProperties2,
    pub get_physical_device_format_properties2: crate::vk::PFN_vkGetPhysicalDeviceFormatProperties2,
    pub get_physical_device_image_format_properties2:
        crate::vk::PFN_vkGetPhysicalDeviceImageFormatProperties2,
    pub get_physical_device_queue_family_properties2:
        crate::vk::PFN_vkGetPhysicalDeviceQueueFamilyProperties2,
    pub get_physical_device_memory_properties2: crate::vk::PFN_vkGetPhysicalDeviceMemoryProperties2,
    pub get_physical_device_sparse_image_format_properties2:
        crate::vk::PFN_vkGetPhysicalDeviceSparseImageFormatProperties2,
    pub get_physical_device_external_buffer_properties:
        crate::vk::PFN_vkGetPhysicalDeviceExternalBufferProperties,
    pub get_physical_device_external_fence_properties:
        crate::vk::PFN_vkGetPhysicalDeviceExternalFenceProperties,
    pub get_physical_device_external_semaphore_properties:
        crate::vk::PFN_vkGetPhysicalDeviceExternalSemaphoreProperties,
}
unsafe impl Send for InstanceFnV1_1 {}
unsafe impl Sync for InstanceFnV1_1 {}
impl InstanceFnV1_1 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            enumerate_physical_device_groups: unsafe {
                unsafe extern "system" fn enumerate_physical_device_groups(
                    _instance: Instance,
                    _p_physical_device_group_count: *mut u32,
                    _p_physical_device_group_properties: *mut PhysicalDeviceGroupProperties,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(enumerate_physical_device_groups)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkEnumeratePhysicalDeviceGroups\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    enumerate_physical_device_groups
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_features2: unsafe {
                unsafe extern "system" fn get_physical_device_features2(
                    _physical_device: PhysicalDevice,
                    _p_features: *mut PhysicalDeviceFeatures2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_features2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceFeatures2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_features2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_properties2: unsafe {
                unsafe extern "system" fn get_physical_device_properties2(
                    _physical_device: PhysicalDevice,
                    _p_properties: *mut PhysicalDeviceProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_properties2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceProperties2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_properties2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_format_properties2: unsafe {
                unsafe extern "system" fn get_physical_device_format_properties2(
                    _physical_device: PhysicalDevice,
                    _format: Format,
                    _p_format_properties: *mut FormatProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_format_properties2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceFormatProperties2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_format_properties2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_image_format_properties2: unsafe {
                unsafe extern "system" fn get_physical_device_image_format_properties2(
                    _physical_device: PhysicalDevice,
                    _p_image_format_info: *const PhysicalDeviceImageFormatInfo2,
                    _p_image_format_properties: *mut ImageFormatProperties2,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_image_format_properties2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceImageFormatProperties2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_image_format_properties2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_queue_family_properties2: unsafe {
                unsafe extern "system" fn get_physical_device_queue_family_properties2(
                    _physical_device: PhysicalDevice,
                    _p_queue_family_property_count: *mut u32,
                    _p_queue_family_properties: *mut QueueFamilyProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_queue_family_properties2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceQueueFamilyProperties2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_queue_family_properties2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_memory_properties2: unsafe {
                unsafe extern "system" fn get_physical_device_memory_properties2(
                    _physical_device: PhysicalDevice,
                    _p_memory_properties: *mut PhysicalDeviceMemoryProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_memory_properties2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceMemoryProperties2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_memory_properties2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_sparse_image_format_properties2: unsafe {
                unsafe extern "system" fn get_physical_device_sparse_image_format_properties2(
                    _physical_device: PhysicalDevice,
                    _p_format_info: *const PhysicalDeviceSparseImageFormatInfo2,
                    _p_property_count: *mut u32,
                    _p_properties: *mut SparseImageFormatProperties2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_sparse_image_format_properties2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceSparseImageFormatProperties2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_sparse_image_format_properties2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_external_buffer_properties: unsafe {
                unsafe extern "system" fn get_physical_device_external_buffer_properties(
                    _physical_device: PhysicalDevice,
                    _p_external_buffer_info: *const PhysicalDeviceExternalBufferInfo,
                    _p_external_buffer_properties: *mut ExternalBufferProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_external_buffer_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceExternalBufferProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_external_buffer_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_external_fence_properties: unsafe {
                unsafe extern "system" fn get_physical_device_external_fence_properties(
                    _physical_device: PhysicalDevice,
                    _p_external_fence_info: *const PhysicalDeviceExternalFenceInfo,
                    _p_external_fence_properties: *mut ExternalFenceProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_external_fence_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceExternalFenceProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_external_fence_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_physical_device_external_semaphore_properties: unsafe {
                unsafe extern "system" fn get_physical_device_external_semaphore_properties(
                    _physical_device: PhysicalDevice,
                    _p_external_semaphore_info: *const PhysicalDeviceExternalSemaphoreInfo,
                    _p_external_semaphore_properties: *mut ExternalSemaphoreProperties,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_physical_device_external_semaphore_properties)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetPhysicalDeviceExternalSemaphoreProperties\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_physical_device_external_semaphore_properties
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumeratePhysicalDeviceGroups.html>"]
    pub unsafe fn enumerate_physical_device_groups(
        &self,
        instance: Instance,
        p_physical_device_group_count: *mut u32,
        p_physical_device_group_properties: *mut PhysicalDeviceGroupProperties,
    ) -> Result {
        (self.enumerate_physical_device_groups)(
            instance,
            p_physical_device_group_count,
            p_physical_device_group_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFeatures2.html>"]
    pub unsafe fn get_physical_device_features2(
        &self,
        physical_device: PhysicalDevice,
        p_features: *mut PhysicalDeviceFeatures2,
    ) {
        (self.get_physical_device_features2)(physical_device, p_features)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceProperties2.html>"]
    pub unsafe fn get_physical_device_properties2(
        &self,
        physical_device: PhysicalDevice,
        p_properties: *mut PhysicalDeviceProperties2,
    ) {
        (self.get_physical_device_properties2)(physical_device, p_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceFormatProperties2.html>"]
    pub unsafe fn get_physical_device_format_properties2(
        &self,
        physical_device: PhysicalDevice,
        format: Format,
        p_format_properties: *mut FormatProperties2,
    ) {
        (self.get_physical_device_format_properties2)(physical_device, format, p_format_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties2.html>"]
    pub unsafe fn get_physical_device_image_format_properties2(
        &self,
        physical_device: PhysicalDevice,
        p_image_format_info: *const PhysicalDeviceImageFormatInfo2,
        p_image_format_properties: *mut ImageFormatProperties2,
    ) -> Result {
        (self.get_physical_device_image_format_properties2)(
            physical_device,
            p_image_format_info,
            p_image_format_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceQueueFamilyProperties2.html>"]
    pub unsafe fn get_physical_device_queue_family_properties2(
        &self,
        physical_device: PhysicalDevice,
        p_queue_family_property_count: *mut u32,
        p_queue_family_properties: *mut QueueFamilyProperties2,
    ) {
        (self.get_physical_device_queue_family_properties2)(
            physical_device,
            p_queue_family_property_count,
            p_queue_family_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceMemoryProperties2.html>"]
    pub unsafe fn get_physical_device_memory_properties2(
        &self,
        physical_device: PhysicalDevice,
        p_memory_properties: *mut PhysicalDeviceMemoryProperties2,
    ) {
        (self.get_physical_device_memory_properties2)(physical_device, p_memory_properties)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceSparseImageFormatProperties2.html>"]
    pub unsafe fn get_physical_device_sparse_image_format_properties2(
        &self,
        physical_device: PhysicalDevice,
        p_format_info: *const PhysicalDeviceSparseImageFormatInfo2,
        p_property_count: *mut u32,
        p_properties: *mut SparseImageFormatProperties2,
    ) {
        (self.get_physical_device_sparse_image_format_properties2)(
            physical_device,
            p_format_info,
            p_property_count,
            p_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalBufferProperties.html>"]
    pub unsafe fn get_physical_device_external_buffer_properties(
        &self,
        physical_device: PhysicalDevice,
        p_external_buffer_info: *const PhysicalDeviceExternalBufferInfo,
        p_external_buffer_properties: *mut ExternalBufferProperties,
    ) {
        (self.get_physical_device_external_buffer_properties)(
            physical_device,
            p_external_buffer_info,
            p_external_buffer_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalFenceProperties.html>"]
    pub unsafe fn get_physical_device_external_fence_properties(
        &self,
        physical_device: PhysicalDevice,
        p_external_fence_info: *const PhysicalDeviceExternalFenceInfo,
        p_external_fence_properties: *mut ExternalFenceProperties,
    ) {
        (self.get_physical_device_external_fence_properties)(
            physical_device,
            p_external_fence_info,
            p_external_fence_properties,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceExternalSemaphoreProperties.html>"]
    pub unsafe fn get_physical_device_external_semaphore_properties(
        &self,
        physical_device: PhysicalDevice,
        p_external_semaphore_info: *const PhysicalDeviceExternalSemaphoreInfo,
        p_external_semaphore_properties: *mut ExternalSemaphoreProperties,
    ) {
        (self.get_physical_device_external_semaphore_properties)(
            physical_device,
            p_external_semaphore_info,
            p_external_semaphore_properties,
        )
    }
}
#[allow(non_camel_case_types)]
pub type PFN_vkGetDeviceQueue2 = unsafe extern "system" fn(
    device: Device,
    p_queue_info: *const DeviceQueueInfo2,
    p_queue: *mut Queue,
);
#[derive(Clone)]
pub struct DeviceFnV1_1 {
    pub bind_buffer_memory2: crate::vk::PFN_vkBindBufferMemory2,
    pub bind_image_memory2: crate::vk::PFN_vkBindImageMemory2,
    pub get_device_group_peer_memory_features: crate::vk::PFN_vkGetDeviceGroupPeerMemoryFeatures,
    pub cmd_set_device_mask: crate::vk::PFN_vkCmdSetDeviceMask,
    pub cmd_dispatch_base: crate::vk::PFN_vkCmdDispatchBase,
    pub get_image_memory_requirements2: crate::vk::PFN_vkGetImageMemoryRequirements2,
    pub get_buffer_memory_requirements2: crate::vk::PFN_vkGetBufferMemoryRequirements2,
    pub get_image_sparse_memory_requirements2: crate::vk::PFN_vkGetImageSparseMemoryRequirements2,
    pub trim_command_pool: crate::vk::PFN_vkTrimCommandPool,
    pub get_device_queue2: PFN_vkGetDeviceQueue2,
    pub create_sampler_ycbcr_conversion: crate::vk::PFN_vkCreateSamplerYcbcrConversion,
    pub destroy_sampler_ycbcr_conversion: crate::vk::PFN_vkDestroySamplerYcbcrConversion,
    pub create_descriptor_update_template: crate::vk::PFN_vkCreateDescriptorUpdateTemplate,
    pub destroy_descriptor_update_template: crate::vk::PFN_vkDestroyDescriptorUpdateTemplate,
    pub update_descriptor_set_with_template: crate::vk::PFN_vkUpdateDescriptorSetWithTemplate,
    pub get_descriptor_set_layout_support: crate::vk::PFN_vkGetDescriptorSetLayoutSupport,
}
unsafe impl Send for DeviceFnV1_1 {}
unsafe impl Sync for DeviceFnV1_1 {}
impl DeviceFnV1_1 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            bind_buffer_memory2: unsafe {
                unsafe extern "system" fn bind_buffer_memory2(
                    _device: Device,
                    _bind_info_count: u32,
                    _p_bind_infos: *const BindBufferMemoryInfo,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(bind_buffer_memory2)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkBindBufferMemory2\0");
                let val = _f(cname);
                if val.is_null() {
                    bind_buffer_memory2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            bind_image_memory2: unsafe {
                unsafe extern "system" fn bind_image_memory2(
                    _device: Device,
                    _bind_info_count: u32,
                    _p_bind_infos: *const BindImageMemoryInfo,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(bind_image_memory2)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkBindImageMemory2\0");
                let val = _f(cname);
                if val.is_null() {
                    bind_image_memory2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_group_peer_memory_features: unsafe {
                unsafe extern "system" fn get_device_group_peer_memory_features(
                    _device: Device,
                    _heap_index: u32,
                    _local_device_index: u32,
                    _remote_device_index: u32,
                    _p_peer_memory_features: *mut PeerMemoryFeatureFlags,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_group_peer_memory_features)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceGroupPeerMemoryFeatures\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_group_peer_memory_features
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_set_device_mask: unsafe {
                unsafe extern "system" fn cmd_set_device_mask(
                    _command_buffer: CommandBuffer,
                    _device_mask: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_set_device_mask)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdSetDeviceMask\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_set_device_mask
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_dispatch_base: unsafe {
                unsafe extern "system" fn cmd_dispatch_base(
                    _command_buffer: CommandBuffer,
                    _base_group_x: u32,
                    _base_group_y: u32,
                    _base_group_z: u32,
                    _group_count_x: u32,
                    _group_count_y: u32,
                    _group_count_z: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_dispatch_base)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDispatchBase\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_dispatch_base
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_image_memory_requirements2: unsafe {
                unsafe extern "system" fn get_image_memory_requirements2(
                    _device: Device,
                    _p_info: *const ImageMemoryRequirementsInfo2,
                    _p_memory_requirements: *mut MemoryRequirements2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_memory_requirements2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageMemoryRequirements2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_memory_requirements2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_buffer_memory_requirements2: unsafe {
                unsafe extern "system" fn get_buffer_memory_requirements2(
                    _device: Device,
                    _p_info: *const BufferMemoryRequirementsInfo2,
                    _p_memory_requirements: *mut MemoryRequirements2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_memory_requirements2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetBufferMemoryRequirements2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_memory_requirements2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_image_sparse_memory_requirements2: unsafe {
                unsafe extern "system" fn get_image_sparse_memory_requirements2(
                    _device: Device,
                    _p_info: *const ImageSparseMemoryRequirementsInfo2,
                    _p_sparse_memory_requirement_count: *mut u32,
                    _p_sparse_memory_requirements: *mut SparseImageMemoryRequirements2,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_image_sparse_memory_requirements2)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetImageSparseMemoryRequirements2\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_image_sparse_memory_requirements2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            trim_command_pool: unsafe {
                unsafe extern "system" fn trim_command_pool(
                    _device: Device,
                    _command_pool: CommandPool,
                    _flags: CommandPoolTrimFlags,
                ) {
                    panic!(concat!("Unable to load ", stringify!(trim_command_pool)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkTrimCommandPool\0");
                let val = _f(cname);
                if val.is_null() {
                    trim_command_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_queue2: unsafe {
                unsafe extern "system" fn get_device_queue2(
                    _device: Device,
                    _p_queue_info: *const DeviceQueueInfo2,
                    _p_queue: *mut Queue,
                ) {
                    panic!(concat!("Unable to load ", stringify!(get_device_queue2)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetDeviceQueue2\0");
                let val = _f(cname);
                if val.is_null() {
                    get_device_queue2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_sampler_ycbcr_conversion: unsafe {
                unsafe extern "system" fn create_sampler_ycbcr_conversion(
                    _device: Device,
                    _p_create_info: *const SamplerYcbcrConversionCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_ycbcr_conversion: *mut SamplerYcbcrConversion,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_sampler_ycbcr_conversion)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateSamplerYcbcrConversion\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_sampler_ycbcr_conversion
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_sampler_ycbcr_conversion: unsafe {
                unsafe extern "system" fn destroy_sampler_ycbcr_conversion(
                    _device: Device,
                    _ycbcr_conversion: SamplerYcbcrConversion,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_sampler_ycbcr_conversion)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroySamplerYcbcrConversion\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_sampler_ycbcr_conversion
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_descriptor_update_template: unsafe {
                unsafe extern "system" fn create_descriptor_update_template(
                    _device: Device,
                    _p_create_info: *const DescriptorUpdateTemplateCreateInfo,
                    _p_allocator: *const AllocationCallbacks,
                    _p_descriptor_update_template: *mut DescriptorUpdateTemplate,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(create_descriptor_update_template)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCreateDescriptorUpdateTemplate\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    create_descriptor_update_template
                } else {
                    ::std::mem::transmute(val)
                }
            },
            destroy_descriptor_update_template: unsafe {
                unsafe extern "system" fn destroy_descriptor_update_template(
                    _device: Device,
                    _descriptor_update_template: DescriptorUpdateTemplate,
                    _p_allocator: *const AllocationCallbacks,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(destroy_descriptor_update_template)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkDestroyDescriptorUpdateTemplate\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    destroy_descriptor_update_template
                } else {
                    ::std::mem::transmute(val)
                }
            },
            update_descriptor_set_with_template: unsafe {
                unsafe extern "system" fn update_descriptor_set_with_template(
                    _device: Device,
                    _descriptor_set: DescriptorSet,
                    _descriptor_update_template: DescriptorUpdateTemplate,
                    _p_data: *const c_void,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(update_descriptor_set_with_template)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkUpdateDescriptorSetWithTemplate\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    update_descriptor_set_with_template
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_descriptor_set_layout_support: unsafe {
                unsafe extern "system" fn get_descriptor_set_layout_support(
                    _device: Device,
                    _p_create_info: *const DescriptorSetLayoutCreateInfo,
                    _p_support: *mut DescriptorSetLayoutSupport,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_descriptor_set_layout_support)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDescriptorSetLayoutSupport\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_descriptor_set_layout_support
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindBufferMemory2.html>"]
    pub unsafe fn bind_buffer_memory2(
        &self,
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindBufferMemoryInfo,
    ) -> Result {
        (self.bind_buffer_memory2)(device, bind_info_count, p_bind_infos)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkBindImageMemory2.html>"]
    pub unsafe fn bind_image_memory2(
        &self,
        device: Device,
        bind_info_count: u32,
        p_bind_infos: *const BindImageMemoryInfo,
    ) -> Result {
        (self.bind_image_memory2)(device, bind_info_count, p_bind_infos)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceGroupPeerMemoryFeatures.html>"]
    pub unsafe fn get_device_group_peer_memory_features(
        &self,
        device: Device,
        heap_index: u32,
        local_device_index: u32,
        remote_device_index: u32,
        p_peer_memory_features: *mut PeerMemoryFeatureFlags,
    ) {
        (self.get_device_group_peer_memory_features)(
            device,
            heap_index,
            local_device_index,
            remote_device_index,
            p_peer_memory_features,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDeviceMask.html>"]
    pub unsafe fn cmd_set_device_mask(&self, command_buffer: CommandBuffer, device_mask: u32) {
        (self.cmd_set_device_mask)(command_buffer, device_mask)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDispatchBase.html>"]
    pub unsafe fn cmd_dispatch_base(
        &self,
        command_buffer: CommandBuffer,
        base_group_x: u32,
        base_group_y: u32,
        base_group_z: u32,
        group_count_x: u32,
        group_count_y: u32,
        group_count_z: u32,
    ) {
        (self.cmd_dispatch_base)(
            command_buffer,
            base_group_x,
            base_group_y,
            base_group_z,
            group_count_x,
            group_count_y,
            group_count_z,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageMemoryRequirements2.html>"]
    pub unsafe fn get_image_memory_requirements2(
        &self,
        device: Device,
        p_info: *const ImageMemoryRequirementsInfo2,
        p_memory_requirements: *mut MemoryRequirements2,
    ) {
        (self.get_image_memory_requirements2)(device, p_info, p_memory_requirements)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferMemoryRequirements2.html>"]
    pub unsafe fn get_buffer_memory_requirements2(
        &self,
        device: Device,
        p_info: *const BufferMemoryRequirementsInfo2,
        p_memory_requirements: *mut MemoryRequirements2,
    ) {
        (self.get_buffer_memory_requirements2)(device, p_info, p_memory_requirements)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetImageSparseMemoryRequirements2.html>"]
    pub unsafe fn get_image_sparse_memory_requirements2(
        &self,
        device: Device,
        p_info: *const ImageSparseMemoryRequirementsInfo2,
        p_sparse_memory_requirement_count: *mut u32,
        p_sparse_memory_requirements: *mut SparseImageMemoryRequirements2,
    ) {
        (self.get_image_sparse_memory_requirements2)(
            device,
            p_info,
            p_sparse_memory_requirement_count,
            p_sparse_memory_requirements,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkTrimCommandPool.html>"]
    pub unsafe fn trim_command_pool(
        &self,
        device: Device,
        command_pool: CommandPool,
        flags: CommandPoolTrimFlags,
    ) {
        (self.trim_command_pool)(device, command_pool, flags)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceQueue2.html>"]
    pub unsafe fn get_device_queue2(
        &self,
        device: Device,
        p_queue_info: *const DeviceQueueInfo2,
        p_queue: *mut Queue,
    ) {
        (self.get_device_queue2)(device, p_queue_info, p_queue)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateSamplerYcbcrConversion.html>"]
    pub unsafe fn create_sampler_ycbcr_conversion(
        &self,
        device: Device,
        p_create_info: *const SamplerYcbcrConversionCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_ycbcr_conversion: *mut SamplerYcbcrConversion,
    ) -> Result {
        (self.create_sampler_ycbcr_conversion)(
            device,
            p_create_info,
            p_allocator,
            p_ycbcr_conversion,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroySamplerYcbcrConversion.html>"]
    pub unsafe fn destroy_sampler_ycbcr_conversion(
        &self,
        device: Device,
        ycbcr_conversion: SamplerYcbcrConversion,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_sampler_ycbcr_conversion)(device, ycbcr_conversion, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateDescriptorUpdateTemplate.html>"]
    pub unsafe fn create_descriptor_update_template(
        &self,
        device: Device,
        p_create_info: *const DescriptorUpdateTemplateCreateInfo,
        p_allocator: *const AllocationCallbacks,
        p_descriptor_update_template: *mut DescriptorUpdateTemplate,
    ) -> Result {
        (self.create_descriptor_update_template)(
            device,
            p_create_info,
            p_allocator,
            p_descriptor_update_template,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkDestroyDescriptorUpdateTemplate.html>"]
    pub unsafe fn destroy_descriptor_update_template(
        &self,
        device: Device,
        descriptor_update_template: DescriptorUpdateTemplate,
        p_allocator: *const AllocationCallbacks,
    ) {
        (self.destroy_descriptor_update_template)(device, descriptor_update_template, p_allocator)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkUpdateDescriptorSetWithTemplate.html>"]
    pub unsafe fn update_descriptor_set_with_template(
        &self,
        device: Device,
        descriptor_set: DescriptorSet,
        descriptor_update_template: DescriptorUpdateTemplate,
        p_data: *const c_void,
    ) {
        (self.update_descriptor_set_with_template)(
            device,
            descriptor_set,
            descriptor_update_template,
            p_data,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDescriptorSetLayoutSupport.html>"]
    pub unsafe fn get_descriptor_set_layout_support(
        &self,
        device: Device,
        p_create_info: *const DescriptorSetLayoutCreateInfo,
        p_support: *mut DescriptorSetLayoutSupport,
    ) {
        (self.get_descriptor_set_layout_support)(device, p_create_info, p_support)
    }
}
#[derive(Clone)]
pub struct EntryFnV1_2 {}
unsafe impl Send for EntryFnV1_2 {}
unsafe impl Sync for EntryFnV1_2 {}
impl EntryFnV1_2 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {}
    }
}
#[derive(Clone)]
pub struct InstanceFnV1_2 {}
unsafe impl Send for InstanceFnV1_2 {}
unsafe impl Sync for InstanceFnV1_2 {}
impl InstanceFnV1_2 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {}
    }
}
#[derive(Clone)]
pub struct DeviceFnV1_2 {
    pub cmd_draw_indirect_count: crate::vk::PFN_vkCmdDrawIndirectCount,
    pub cmd_draw_indexed_indirect_count: crate::vk::PFN_vkCmdDrawIndexedIndirectCount,
    pub create_render_pass2: crate::vk::PFN_vkCreateRenderPass2,
    pub cmd_begin_render_pass2: crate::vk::PFN_vkCmdBeginRenderPass2,
    pub cmd_next_subpass2: crate::vk::PFN_vkCmdNextSubpass2,
    pub cmd_end_render_pass2: crate::vk::PFN_vkCmdEndRenderPass2,
    pub reset_query_pool: crate::vk::PFN_vkResetQueryPool,
    pub get_semaphore_counter_value: crate::vk::PFN_vkGetSemaphoreCounterValue,
    pub wait_semaphores: crate::vk::PFN_vkWaitSemaphores,
    pub signal_semaphore: crate::vk::PFN_vkSignalSemaphore,
    pub get_buffer_device_address: crate::vk::PFN_vkGetBufferDeviceAddress,
    pub get_buffer_opaque_capture_address: crate::vk::PFN_vkGetBufferOpaqueCaptureAddress,
    pub get_device_memory_opaque_capture_address:
        crate::vk::PFN_vkGetDeviceMemoryOpaqueCaptureAddress,
}
unsafe impl Send for DeviceFnV1_2 {}
unsafe impl Sync for DeviceFnV1_2 {}
impl DeviceFnV1_2 {
    pub fn load<F>(mut _f: F) -> Self
    where
        F: FnMut(&::std::ffi::CStr) -> *const c_void,
    {
        Self {
            cmd_draw_indirect_count: unsafe {
                unsafe extern "system" fn cmd_draw_indirect_count(
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
                        stringify!(cmd_draw_indirect_count)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdDrawIndirectCount\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indirect_count
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_draw_indexed_indirect_count: unsafe {
                unsafe extern "system" fn cmd_draw_indexed_indirect_count(
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
                        stringify!(cmd_draw_indexed_indirect_count)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkCmdDrawIndexedIndirectCount\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    cmd_draw_indexed_indirect_count
                } else {
                    ::std::mem::transmute(val)
                }
            },
            create_render_pass2: unsafe {
                unsafe extern "system" fn create_render_pass2(
                    _device: Device,
                    _p_create_info: *const RenderPassCreateInfo2,
                    _p_allocator: *const AllocationCallbacks,
                    _p_render_pass: *mut RenderPass,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(create_render_pass2)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCreateRenderPass2\0");
                let val = _f(cname);
                if val.is_null() {
                    create_render_pass2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_begin_render_pass2: unsafe {
                unsafe extern "system" fn cmd_begin_render_pass2(
                    _command_buffer: CommandBuffer,
                    _p_render_pass_begin: *const RenderPassBeginInfo,
                    _p_subpass_begin_info: *const SubpassBeginInfo,
                ) {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(cmd_begin_render_pass2)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdBeginRenderPass2\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_begin_render_pass2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_next_subpass2: unsafe {
                unsafe extern "system" fn cmd_next_subpass2(
                    _command_buffer: CommandBuffer,
                    _p_subpass_begin_info: *const SubpassBeginInfo,
                    _p_subpass_end_info: *const SubpassEndInfo,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_next_subpass2)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdNextSubpass2\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_next_subpass2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            cmd_end_render_pass2: unsafe {
                unsafe extern "system" fn cmd_end_render_pass2(
                    _command_buffer: CommandBuffer,
                    _p_subpass_end_info: *const SubpassEndInfo,
                ) {
                    panic!(concat!("Unable to load ", stringify!(cmd_end_render_pass2)))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkCmdEndRenderPass2\0");
                let val = _f(cname);
                if val.is_null() {
                    cmd_end_render_pass2
                } else {
                    ::std::mem::transmute(val)
                }
            },
            reset_query_pool: unsafe {
                unsafe extern "system" fn reset_query_pool(
                    _device: Device,
                    _query_pool: QueryPool,
                    _first_query: u32,
                    _query_count: u32,
                ) {
                    panic!(concat!("Unable to load ", stringify!(reset_query_pool)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkResetQueryPool\0");
                let val = _f(cname);
                if val.is_null() {
                    reset_query_pool
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_semaphore_counter_value: unsafe {
                unsafe extern "system" fn get_semaphore_counter_value(
                    _device: Device,
                    _semaphore: Semaphore,
                    _p_value: *mut u64,
                ) -> Result {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_semaphore_counter_value)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetSemaphoreCounterValue\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_semaphore_counter_value
                } else {
                    ::std::mem::transmute(val)
                }
            },
            wait_semaphores: unsafe {
                unsafe extern "system" fn wait_semaphores(
                    _device: Device,
                    _p_wait_info: *const SemaphoreWaitInfo,
                    _timeout: u64,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(wait_semaphores)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkWaitSemaphores\0");
                let val = _f(cname);
                if val.is_null() {
                    wait_semaphores
                } else {
                    ::std::mem::transmute(val)
                }
            },
            signal_semaphore: unsafe {
                unsafe extern "system" fn signal_semaphore(
                    _device: Device,
                    _p_signal_info: *const SemaphoreSignalInfo,
                ) -> Result {
                    panic!(concat!("Unable to load ", stringify!(signal_semaphore)))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkSignalSemaphore\0");
                let val = _f(cname);
                if val.is_null() {
                    signal_semaphore
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_buffer_device_address: unsafe {
                unsafe extern "system" fn get_buffer_device_address(
                    _device: Device,
                    _p_info: *const BufferDeviceAddressInfo,
                ) -> DeviceAddress {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_device_address)
                    ))
                }
                let cname =
                    ::std::ffi::CStr::from_bytes_with_nul_unchecked(b"vkGetBufferDeviceAddress\0");
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_device_address
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_buffer_opaque_capture_address: unsafe {
                unsafe extern "system" fn get_buffer_opaque_capture_address(
                    _device: Device,
                    _p_info: *const BufferDeviceAddressInfo,
                ) -> u64 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_buffer_opaque_capture_address)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetBufferOpaqueCaptureAddress\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_buffer_opaque_capture_address
                } else {
                    ::std::mem::transmute(val)
                }
            },
            get_device_memory_opaque_capture_address: unsafe {
                unsafe extern "system" fn get_device_memory_opaque_capture_address(
                    _device: Device,
                    _p_info: *const DeviceMemoryOpaqueCaptureAddressInfo,
                ) -> u64 {
                    panic!(concat!(
                        "Unable to load ",
                        stringify!(get_device_memory_opaque_capture_address)
                    ))
                }
                let cname = ::std::ffi::CStr::from_bytes_with_nul_unchecked(
                    b"vkGetDeviceMemoryOpaqueCaptureAddress\0",
                );
                let val = _f(cname);
                if val.is_null() {
                    get_device_memory_opaque_capture_address
                } else {
                    ::std::mem::transmute(val)
                }
            },
        }
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirectCount.html>"]
    pub unsafe fn cmd_draw_indirect_count(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indirect_count)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexedIndirectCount.html>"]
    pub unsafe fn cmd_draw_indexed_indirect_count(
        &self,
        command_buffer: CommandBuffer,
        buffer: Buffer,
        offset: DeviceSize,
        count_buffer: Buffer,
        count_buffer_offset: DeviceSize,
        max_draw_count: u32,
        stride: u32,
    ) {
        (self.cmd_draw_indexed_indirect_count)(
            command_buffer,
            buffer,
            offset,
            count_buffer,
            count_buffer_offset,
            max_draw_count,
            stride,
        )
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCreateRenderPass2.html>"]
    pub unsafe fn create_render_pass2(
        &self,
        device: Device,
        p_create_info: *const RenderPassCreateInfo2,
        p_allocator: *const AllocationCallbacks,
        p_render_pass: *mut RenderPass,
    ) -> Result {
        (self.create_render_pass2)(device, p_create_info, p_allocator, p_render_pass)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdBeginRenderPass2.html>"]
    pub unsafe fn cmd_begin_render_pass2(
        &self,
        command_buffer: CommandBuffer,
        p_render_pass_begin: *const RenderPassBeginInfo,
        p_subpass_begin_info: *const SubpassBeginInfo,
    ) {
        (self.cmd_begin_render_pass2)(command_buffer, p_render_pass_begin, p_subpass_begin_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdNextSubpass2.html>"]
    pub unsafe fn cmd_next_subpass2(
        &self,
        command_buffer: CommandBuffer,
        p_subpass_begin_info: *const SubpassBeginInfo,
        p_subpass_end_info: *const SubpassEndInfo,
    ) {
        (self.cmd_next_subpass2)(command_buffer, p_subpass_begin_info, p_subpass_end_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdEndRenderPass2.html>"]
    pub unsafe fn cmd_end_render_pass2(
        &self,
        command_buffer: CommandBuffer,
        p_subpass_end_info: *const SubpassEndInfo,
    ) {
        (self.cmd_end_render_pass2)(command_buffer, p_subpass_end_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkResetQueryPool.html>"]
    pub unsafe fn reset_query_pool(
        &self,
        device: Device,
        query_pool: QueryPool,
        first_query: u32,
        query_count: u32,
    ) {
        (self.reset_query_pool)(device, query_pool, first_query, query_count)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetSemaphoreCounterValue.html>"]
    pub unsafe fn get_semaphore_counter_value(
        &self,
        device: Device,
        semaphore: Semaphore,
        p_value: *mut u64,
    ) -> Result {
        (self.get_semaphore_counter_value)(device, semaphore, p_value)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkWaitSemaphores.html>"]
    pub unsafe fn wait_semaphores(
        &self,
        device: Device,
        p_wait_info: *const SemaphoreWaitInfo,
        timeout: u64,
    ) -> Result {
        (self.wait_semaphores)(device, p_wait_info, timeout)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkSignalSemaphore.html>"]
    pub unsafe fn signal_semaphore(
        &self,
        device: Device,
        p_signal_info: *const SemaphoreSignalInfo,
    ) -> Result {
        (self.signal_semaphore)(device, p_signal_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferDeviceAddress.html>"]
    pub unsafe fn get_buffer_device_address(
        &self,
        device: Device,
        p_info: *const BufferDeviceAddressInfo,
    ) -> DeviceAddress {
        (self.get_buffer_device_address)(device, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetBufferOpaqueCaptureAddress.html>"]
    pub unsafe fn get_buffer_opaque_capture_address(
        &self,
        device: Device,
        p_info: *const BufferDeviceAddressInfo,
    ) -> u64 {
        (self.get_buffer_opaque_capture_address)(device, p_info)
    }
    #[doc = "<https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetDeviceMemoryOpaqueCaptureAddress.html>"]
    pub unsafe fn get_device_memory_opaque_capture_address(
        &self,
        device: Device,
        p_info: *const DeviceMemoryOpaqueCaptureAddressInfo,
    ) -> u64 {
        (self.get_device_memory_opaque_capture_address)(device, p_info)
    }
}
