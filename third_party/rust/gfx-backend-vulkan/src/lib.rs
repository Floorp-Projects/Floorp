#![allow(non_snake_case)]

#[macro_use]
extern crate log;
#[macro_use]
extern crate lazy_static;

#[cfg(target_os = "macos")]
#[macro_use]
extern crate objc;

use ash::extensions::{
    self,
    ext::{DebugReport, DebugUtils},
};
use ash::version::{DeviceV1_0, EntryV1_0, InstanceV1_0};
use ash::vk;
#[cfg(not(feature = "use-rtld-next"))]
use ash::{Entry, LoadingError};

use hal::{
    adapter,
    device::{CreationError as DeviceCreationError, DeviceLost, OutOfMemory, SurfaceLost},
    format,
    image,
    memory,
    pso::{PatchSize, PipelineStage},
    queue,
    window::{PresentError, Suboptimal, SwapImageIndex},
    Features,
    Hints,
    Limits,
};

use std::borrow::{Borrow, Cow};
use std::ffi::{CStr, CString};
use std::sync::Arc;
use std::{fmt, mem, ptr, slice};

#[cfg(feature = "use-rtld-next")]
use ash::{EntryCustom, LoadingError};
#[cfg(feature = "use-rtld-next")]
use shared_library::dynamic_library::{DynamicLibrary, SpecialHandles};

mod command;
mod conv;
mod device;
mod info;
mod native;
mod pool;
mod window;

// CStr's cannot be constant yet, until const fn lands we need to use a lazy_static
lazy_static! {
    static ref LAYERS: Vec<&'static CStr> = if cfg!(all(target_os = "android", debug_assertions)) {
        vec![
            CStr::from_bytes_with_nul(b"VK_LAYER_LUNARG_core_validation\0").unwrap(),
            CStr::from_bytes_with_nul(b"VK_LAYER_LUNARG_object_tracker\0").unwrap(),
            CStr::from_bytes_with_nul(b"VK_LAYER_LUNARG_parameter_validation\0").unwrap(),
            CStr::from_bytes_with_nul(b"VK_LAYER_GOOGLE_threading\0").unwrap(),
            CStr::from_bytes_with_nul(b"VK_LAYER_GOOGLE_unique_objects\0").unwrap(),
        ]
    } else if cfg!(debug_assertions) {
        vec![CStr::from_bytes_with_nul(b"VK_LAYER_LUNARG_standard_validation\0").unwrap()]
    } else {
        vec![]
    };
    static ref EXTENSIONS: Vec<&'static CStr> = if cfg!(debug_assertions) {
        vec![
            DebugUtils::name(),
            DebugReport::name(),
        ]
    } else {
        vec![]
    };
    static ref DEVICE_EXTENSIONS: Vec<&'static CStr> = vec![extensions::khr::Swapchain::name()];
    static ref SURFACE_EXTENSIONS: Vec<&'static CStr> = vec![
        extensions::khr::Surface::name(),
        // Platform-specific WSI extensions
        #[cfg(all(unix, not(target_os = "android"), not(target_os = "macos")))]
        extensions::khr::XlibSurface::name(),
        #[cfg(all(unix, not(target_os = "android"), not(target_os = "macos")))]
        extensions::khr::XcbSurface::name(),
        #[cfg(all(unix, not(target_os = "android"), not(target_os = "macos")))]
        extensions::khr::WaylandSurface::name(),
        #[cfg(target_os = "android")]
        extensions::khr::AndroidSurface::name(),
        #[cfg(target_os = "windows")]
        extensions::khr::Win32Surface::name(),
        #[cfg(target_os = "macos")]
        extensions::mvk::MacOSSurface::name(),
    ];
    static ref AMD_NEGATIVE_VIEWPORT_HEIGHT: &'static CStr =
        CStr::from_bytes_with_nul(b"VK_AMD_negative_viewport_height\0").unwrap();
    static ref KHR_MAINTENANCE1: &'static CStr =
        CStr::from_bytes_with_nul(b"VK_KHR_maintenance1\0").unwrap();
    static ref KHR_SAMPLER_MIRROR_MIRROR_CLAMP_TO_EDGE : &'static CStr =
        CStr::from_bytes_with_nul(b"VK_KHR_sampler_mirror_clamp_to_edge\0").unwrap();
}

#[cfg(not(feature = "use-rtld-next"))]
lazy_static! {
    // Entry function pointers
    pub static ref VK_ENTRY: Result<Entry, LoadingError> = Entry::new();
}

#[cfg(feature = "use-rtld-next")]
lazy_static! {
    // Entry function pointers
    pub static ref VK_ENTRY: Result<EntryCustom<V1_0, ()>, LoadingError>
        = EntryCustom::new_custom(
            || Ok(()),
            |_, name| unsafe {
                DynamicLibrary::symbol_special(SpecialHandles::Next, &*name.to_string_lossy())
                    .unwrap_or(ptr::null_mut())
            }
        );
}

pub struct RawInstance(ash::Instance, Option<DebugMessenger>);

pub enum DebugMessenger {
    Utils(DebugUtils, vk::DebugUtilsMessengerEXT),
    Report(DebugReport, vk::DebugReportCallbackEXT),
}

impl Drop for RawInstance {
    fn drop(&mut self) {
        unsafe {
            #[cfg(debug_assertions)]
            {
                match self.1 {
                    Some(DebugMessenger::Utils(ref ext, callback)) => {
                        ext.destroy_debug_utils_messenger(callback, None)
                    }
                    Some(DebugMessenger::Report(ref ext, callback)) => {
                        ext.destroy_debug_report_callback(callback, None)
                    }
                    None => {}
                }
            }

            self.0.destroy_instance(None);
        }
    }
}

pub struct Instance {
    pub raw: Arc<RawInstance>,

    /// Supported extensions of this instance.
    pub extensions: Vec<&'static CStr>,
}

impl fmt::Debug for Instance {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Instance")
    }
}

fn map_queue_type(flags: vk::QueueFlags) -> queue::QueueType {
    if flags.contains(vk::QueueFlags::GRAPHICS | vk::QueueFlags::COMPUTE) {
        // TRANSFER_BIT optional
        queue::QueueType::General
    } else if flags.contains(vk::QueueFlags::GRAPHICS) {
        // TRANSFER_BIT optional
        queue::QueueType::Graphics
    } else if flags.contains(vk::QueueFlags::COMPUTE) {
        // TRANSFER_BIT optional
        queue::QueueType::Compute
    } else if flags.contains(vk::QueueFlags::TRANSFER) {
        queue::QueueType::Transfer
    } else {
        // TODO: present only queues?
        unimplemented!()
    }
}

unsafe fn display_debug_utils_label_ext(
    label_structs: *mut vk::DebugUtilsLabelEXT,
    count: usize,
) -> Option<String> {
    if count == 0 {
        return None;
    }

    Some(
        slice::from_raw_parts::<vk::DebugUtilsLabelEXT>(label_structs, count)
            .iter()
            .flat_map(|dul_obj| {
                dul_obj
                    .p_label_name
                    .as_ref()
                    .map(|lbl| CStr::from_ptr(lbl).to_string_lossy().into_owned())
            })
            .collect::<Vec<String>>()
            .join(", "),
    )
}

unsafe fn display_debug_utils_object_name_info_ext(
    info_structs: *mut vk::DebugUtilsObjectNameInfoEXT,
    count: usize,
) -> Option<String> {
    if count == 0 {
        return None;
    }

    //TODO: use color field of vk::DebugUtilsLabelsExt in a meaningful way?
    Some(
        slice::from_raw_parts::<vk::DebugUtilsObjectNameInfoEXT>(info_structs, count)
            .iter()
            .map(|obj_info| {
                let object_name = obj_info
                    .p_object_name
                    .as_ref()
                    .map(|name| CStr::from_ptr(name).to_string_lossy().into_owned());

                match object_name {
                    Some(name) => format!(
                        "(type: {:?}, hndl: {}, name: {})",
                        obj_info.object_type,
                        &obj_info.object_handle.to_string(),
                        name
                    ),
                    None => format!(
                        "(type: {:?}, hndl: {})",
                        obj_info.object_type,
                        &obj_info.object_handle.to_string()
                    ),
                }
            })
            .collect::<Vec<String>>()
            .join(", "),
    )
}

unsafe extern "system" fn debug_utils_messenger_callback(
    message_severity: vk::DebugUtilsMessageSeverityFlagsEXT,
    message_type: vk::DebugUtilsMessageTypeFlagsEXT,
    p_callback_data: *const vk::DebugUtilsMessengerCallbackDataEXT,
    _user_data: *mut std::os::raw::c_void,
) -> vk::Bool32 {
    let callback_data = *p_callback_data;

    let message_severity = match message_severity {
        vk::DebugUtilsMessageSeverityFlagsEXT::ERROR => log::Level::Error,
        vk::DebugUtilsMessageSeverityFlagsEXT::WARNING => log::Level::Warn,
        vk::DebugUtilsMessageSeverityFlagsEXT::INFO => log::Level::Info,
        vk::DebugUtilsMessageSeverityFlagsEXT::VERBOSE => log::Level::Trace,
        _ => log::Level::Warn,
    };
    let message_type = &format!("{:?}", message_type);
    let message_id_number: i32 = callback_data.message_id_number as i32;

    let message_id_name = if callback_data.p_message_id_name.is_null() {
        Cow::from("")
    } else {
        CStr::from_ptr(callback_data.p_message_id_name).to_string_lossy()
    };

    let message = if callback_data.p_message.is_null() {
        Cow::from("")
    } else {
        CStr::from_ptr(callback_data.p_message).to_string_lossy()
    };

    let additional_info: [(&str, Option<String>); 3] = [
        (
            "queue info",
            display_debug_utils_label_ext(
                callback_data.p_queue_labels as *mut _,
                callback_data.queue_label_count as usize,
            ),
        ),
        (
            "cmd buf info",
            display_debug_utils_label_ext(
                callback_data.p_cmd_buf_labels as *mut _,
                callback_data.cmd_buf_label_count as usize,
            ),
        ),
        (
            "object info",
            display_debug_utils_object_name_info_ext(
                callback_data.p_objects as *mut _,
                callback_data.object_count as usize,
            ),
        ),
    ];

    log!(message_severity, "{}\n", {
        let mut msg = format!(
            "\n{} [{} ({})] : {}",
            message_type,
            message_id_name,
            &message_id_number.to_string(),
            message
        );

        #[allow(array_into_iter)]
        for (info_label, info) in additional_info.into_iter() {
            match info {
                Some(data) => {
                    msg = format!("{}\n{}: {}", msg, info_label, data);
                }
                None => {}
            }
        }

        msg
    });

    vk::FALSE
}

unsafe extern "system" fn debug_report_callback(
    type_: vk::DebugReportFlagsEXT,
    _: vk::DebugReportObjectTypeEXT,
    _object: u64,
    _location: usize,
    _msg_code: i32,
    layer_prefix: *const std::os::raw::c_char,
    description: *const std::os::raw::c_char,
    _user_data: *mut std::os::raw::c_void,
) -> vk::Bool32 {
    let level = match type_ {
        vk::DebugReportFlagsEXT::ERROR => log::Level::Error,
        vk::DebugReportFlagsEXT::WARNING => log::Level::Warn,
        vk::DebugReportFlagsEXT::INFORMATION => log::Level::Info,
        vk::DebugReportFlagsEXT::DEBUG => log::Level::Debug,
        _ => log::Level::Warn,
    };

    let layer_prefix = CStr::from_ptr(layer_prefix).to_str().unwrap();
    let description = CStr::from_ptr(description).to_str().unwrap();
    log!(level, "[{}] {}", layer_prefix, description);
    vk::FALSE
}

impl hal::Instance<Backend> for Instance {
    fn create(name: &str, version: u32) -> Result<Self, hal::UnsupportedBackend> {
        // TODO: return errors instead of panic
        let entry = VK_ENTRY.as_ref().map_err(|e| {
            info!("Missing Vulkan entry points: {:?}", e);
            hal::UnsupportedBackend
        })?;

        let app_name = CString::new(name).unwrap();
        let app_info = vk::ApplicationInfo {
            s_type: vk::StructureType::APPLICATION_INFO,
            p_next: ptr::null(),
            p_application_name: app_name.as_ptr(),
            application_version: version,
            p_engine_name: b"gfx-rs\0".as_ptr() as *const _,
            engine_version: 1,
            api_version: vk::make_version(1, 0, 0),
        };

        let instance_extensions = entry
            .enumerate_instance_extension_properties()
            .expect("Unable to enumerate instance extensions");

        let instance_layers = entry
            .enumerate_instance_layer_properties()
            .expect("Unable to enumerate instance layers");

        // Check our extensions against the available extensions
        let extensions = SURFACE_EXTENSIONS
            .iter()
            .chain(EXTENSIONS.iter())
            .filter_map(|&ext| {
                instance_extensions
                    .iter()
                    .find(|inst_ext| unsafe {
                        CStr::from_ptr(inst_ext.extension_name.as_ptr()) == ext
                    })
                    .map(|_| ext)
                    .or_else(|| {
                        warn!("Unable to find extension: {}", ext.to_string_lossy());
                        None
                    })
            })
            .collect::<Vec<&CStr>>();

        // Check requested layers against the available layers
        let layers = LAYERS
            .iter()
            .filter_map(|&layer| {
                instance_layers
                    .iter()
                    .find(|inst_layer| unsafe {
                        CStr::from_ptr(inst_layer.layer_name.as_ptr()) == layer
                    })
                    .map(|_| layer)
                    .or_else(|| {
                        warn!("Unable to find layer: {}", layer.to_string_lossy());
                        None
                    })
            })
            .collect::<Vec<&CStr>>();

        let instance = {
            let cstrings = layers
                .iter()
                .chain(extensions.iter())
                .map(|&s| CString::from(s))
                .collect::<Vec<_>>();

            let str_pointers = cstrings.iter().map(|s| s.as_ptr()).collect::<Vec<_>>();

            let create_info = vk::InstanceCreateInfo {
                s_type: vk::StructureType::INSTANCE_CREATE_INFO,
                p_next: ptr::null(),
                flags: vk::InstanceCreateFlags::empty(),
                p_application_info: &app_info,
                enabled_layer_count: layers.len() as _,
                pp_enabled_layer_names: str_pointers.as_ptr(),
                enabled_extension_count: extensions.len() as _,
                pp_enabled_extension_names: str_pointers[layers.len() ..].as_ptr(),
            };

            unsafe { entry.create_instance(&create_info, None) }.map_err(|e| {
                warn!("Unable to create Vulkan instance: {:?}", e);
                hal::UnsupportedBackend
            })?
        };

        #[cfg(debug_assertions)]
        let debug_messenger = {
            // make sure VK_EXT_debug_utils is available
            if instance_extensions.iter().any(|props| unsafe {
                CStr::from_ptr(props.extension_name.as_ptr()) == DebugUtils::name()
            }) {
                let ext = DebugUtils::new(entry, &instance);
                let info = vk::DebugUtilsMessengerCreateInfoEXT {
                    s_type: vk::StructureType::DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                    p_next: ptr::null(),
                    flags: vk::DebugUtilsMessengerCreateFlagsEXT::empty(),
                    message_severity: vk::DebugUtilsMessageSeverityFlagsEXT::all(),
                    message_type: vk::DebugUtilsMessageTypeFlagsEXT::all(),
                    pfn_user_callback: Some(debug_utils_messenger_callback),
                    p_user_data: ptr::null_mut(),
                };
                let handle = unsafe { ext.create_debug_utils_messenger(&info, None) }.unwrap();
                Some(DebugMessenger::Utils(ext, handle))
            } else if instance_extensions.iter().any(|props| unsafe {
                CStr::from_ptr(props.extension_name.as_ptr()) == DebugReport::name()
            }) {
                let ext = DebugReport::new(entry, &instance);
                let info = vk::DebugReportCallbackCreateInfoEXT {
                    s_type: vk::StructureType::DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
                    p_next: ptr::null(),
                    flags: vk::DebugReportFlagsEXT::all(),
                    pfn_callback: Some(debug_report_callback),
                    p_user_data: ptr::null_mut(),
                };
                let handle = unsafe { ext.create_debug_report_callback(&info, None) }.unwrap();
                Some(DebugMessenger::Report(ext, handle))
            } else {
                None
            }
        };
        #[cfg(not(debug_assertions))]
        let debug_messenger = None;

        Ok(Instance {
            raw: Arc::new(RawInstance(instance, debug_messenger)),
            extensions,
        })
    }

    fn enumerate_adapters(&self) -> Vec<adapter::Adapter<Backend>> {
        let devices = match unsafe { self.raw.0.enumerate_physical_devices() } {
            Ok(devices) => devices,
            Err(err) => {
                error!("Could not enumerate physical devices! {}", err);
                vec![]
            }
        };

        devices
            .into_iter()
            .map(|device| {
                let extensions =
                    unsafe { self.raw.0.enumerate_device_extension_properties(device) }.unwrap();
                let properties = unsafe { self.raw.0.get_physical_device_properties(device) };
                let info = adapter::AdapterInfo {
                    name: unsafe {
                        CStr::from_ptr(properties.device_name.as_ptr())
                            .to_str()
                            .unwrap_or("Unknown")
                            .to_owned()
                    },
                    vendor: properties.vendor_id as usize,
                    device: properties.device_id as usize,
                    device_type: match properties.device_type {
                        ash::vk::PhysicalDeviceType::OTHER => adapter::DeviceType::Other,
                        ash::vk::PhysicalDeviceType::INTEGRATED_GPU => {
                            adapter::DeviceType::IntegratedGpu
                        }
                        ash::vk::PhysicalDeviceType::DISCRETE_GPU => {
                            adapter::DeviceType::DiscreteGpu
                        }
                        ash::vk::PhysicalDeviceType::VIRTUAL_GPU => adapter::DeviceType::VirtualGpu,
                        ash::vk::PhysicalDeviceType::CPU => adapter::DeviceType::Cpu,
                        _ => adapter::DeviceType::Other,
                    },
                };
                let physical_device = PhysicalDevice {
                    instance: self.raw.clone(),
                    handle: device,
                    extensions,
                    properties,
                };
                let queue_families = unsafe {
                    self.raw
                        .0
                        .get_physical_device_queue_family_properties(device)
                        .into_iter()
                        .enumerate()
                        .map(|(i, properties)| QueueFamily {
                            properties,
                            device,
                            index: i as u32,
                        })
                        .collect()
                };

                adapter::Adapter {
                    info,
                    physical_device,
                    queue_families,
                }
            })
            .collect()
    }

    unsafe fn create_surface(
        &self,
        has_handle: &impl raw_window_handle::HasRawWindowHandle,
    ) -> Result<window::Surface, hal::window::InitError> {
        use raw_window_handle::RawWindowHandle;

        match has_handle.raw_window_handle() {
            #[cfg(all(unix, not(target_os = "android"), not(target_os = "macos")))]
            RawWindowHandle::Wayland(handle)
                if self
                    .extensions
                    .contains(&extensions::khr::WaylandSurface::name()) =>
            {
                Ok(self.create_surface_from_wayland(handle.display, handle.surface))
            }
            #[cfg(all(
                feature = "x11",
                unix,
                not(target_os = "android"),
                not(target_os = "macos")
            ))]
            RawWindowHandle::Xlib(handle)
                if self
                    .extensions
                    .contains(&extensions::khr::XlibSurface::name()) =>
            {
                Ok(self.create_surface_from_xlib(handle.display as *mut _, handle.window))
            }
            #[cfg(all(
                feature = "xcb",
                unix,
                not(target_os = "android"),
                not(target_os = "macos"),
                not(target_os = "ios")
            ))]
            RawWindowHandle::Xcb(handle)
                if self
                    .extensions
                    .contains(&extensions::khr::XcbSurface::name()) =>
            {
                Ok(self.create_surface_from_xcb(handle.connection as *mut _, handle.window))
            }
            #[cfg(target_os = "android")]
            RawWindowHandle::Android(handle) => {
                Ok(self.create_surface_android(handle.a_native_window))
            }
            #[cfg(windows)]
            RawWindowHandle::Windows(handle) => {
                use winapi::um::libloaderapi::GetModuleHandleW;

                let hinstance = GetModuleHandleW(ptr::null());
                Ok(self.create_surface_from_hwnd(hinstance as *mut _, handle.hwnd))
            }
            #[cfg(target_os = "macos")]
            RawWindowHandle::MacOS(handle) => Ok(self.create_surface_from_ns_view(handle.ns_view)),
            _ => Err(hal::window::InitError::UnsupportedWindowHandle),
        }
    }

    unsafe fn destroy_surface(&self, surface: window::Surface) {
        surface
            .raw
            .functor
            .destroy_surface(surface.raw.handle, None);
    }
}

#[derive(Debug, Clone)]
pub struct QueueFamily {
    properties: vk::QueueFamilyProperties,
    device: vk::PhysicalDevice,
    index: u32,
}

impl queue::QueueFamily for QueueFamily {
    fn queue_type(&self) -> queue::QueueType {
        map_queue_type(self.properties.queue_flags)
    }
    fn max_queues(&self) -> usize {
        self.properties.queue_count as _
    }
    fn id(&self) -> queue::QueueFamilyId {
        queue::QueueFamilyId(self.index as _)
    }
}

pub struct PhysicalDevice {
    instance: Arc<RawInstance>,
    handle: vk::PhysicalDevice,
    extensions: Vec<vk::ExtensionProperties>,
    properties: vk::PhysicalDeviceProperties,
}

impl PhysicalDevice {
    fn supports_extension(&self, extension: &CStr) -> bool {
        self.extensions
            .iter()
            .any(|ep| unsafe { CStr::from_ptr(ep.extension_name.as_ptr()) } == extension)
    }
}

impl fmt::Debug for PhysicalDevice {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("PhysicalDevice")
    }
}

impl adapter::PhysicalDevice<Backend> for PhysicalDevice {
    unsafe fn open(
        &self,
        families: &[(&QueueFamily, &[queue::QueuePriority])],
        requested_features: Features,
    ) -> Result<adapter::Gpu<Backend>, DeviceCreationError> {
        let family_infos = families
            .iter()
            .map(|&(family, priorities)| vk::DeviceQueueCreateInfo {
                s_type: vk::StructureType::DEVICE_QUEUE_CREATE_INFO,
                p_next: ptr::null(),
                flags: vk::DeviceQueueCreateFlags::empty(),
                queue_family_index: family.index,
                queue_count: priorities.len() as _,
                p_queue_priorities: priorities.as_ptr(),
            })
            .collect::<Vec<_>>();

        if !self.features().contains(requested_features) {
            return Err(DeviceCreationError::MissingFeature);
        }

        let maintenance_level = if self.supports_extension(*KHR_MAINTENANCE1) { 1 } else { 0 };
        let enabled_features = conv::map_device_features(requested_features);
        let enabled_extensions = DEVICE_EXTENSIONS
            .iter()
            .cloned()
            .chain(
                if requested_features.contains(Features::NDC_Y_UP) && maintenance_level == 0 {
                    Some(*AMD_NEGATIVE_VIEWPORT_HEIGHT)
                } else {
                    None
                },
            )
            .chain(
                match maintenance_level {
                    0 => None,
                    1 => Some(*KHR_MAINTENANCE1),
                    _ => unreachable!(),
                }
            );

        // Create device
        let device_raw = {
            let cstrings = enabled_extensions.map(CString::from).collect::<Vec<_>>();

            let str_pointers = cstrings.iter().map(|s| s.as_ptr()).collect::<Vec<_>>();

            let info = vk::DeviceCreateInfo {
                s_type: vk::StructureType::DEVICE_CREATE_INFO,
                p_next: ptr::null(),
                flags: vk::DeviceCreateFlags::empty(),
                queue_create_info_count: family_infos.len() as u32,
                p_queue_create_infos: family_infos.as_ptr(),
                enabled_layer_count: 0,
                pp_enabled_layer_names: ptr::null(),
                enabled_extension_count: str_pointers.len() as u32,
                pp_enabled_extension_names: str_pointers.as_ptr(),
                p_enabled_features: &enabled_features,
            };

            match self.instance.0.create_device(self.handle, &info, None) {
                Ok(device) => device,
                Err(e) => {
                    return Err(match e {
                        vk::Result::ERROR_OUT_OF_HOST_MEMORY => {
                            DeviceCreationError::OutOfMemory(OutOfMemory::Host)
                        }
                        vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => {
                            DeviceCreationError::OutOfMemory(OutOfMemory::Device)
                        }
                        vk::Result::ERROR_INITIALIZATION_FAILED => {
                            DeviceCreationError::InitializationFailed
                        }
                        vk::Result::ERROR_DEVICE_LOST => DeviceCreationError::DeviceLost,
                        vk::Result::ERROR_TOO_MANY_OBJECTS => DeviceCreationError::TooManyObjects,
                        _ => unreachable!(),
                    })
                }
            }
        };

        let swapchain_fn = vk::KhrSwapchainFn::load(|name| {
            mem::transmute(
                self.instance
                    .0
                    .get_device_proc_addr(device_raw.handle(), name.as_ptr()),
            )
        });

        let device = Device {
            shared: Arc::new(RawDevice {
                raw: device_raw,
                features: requested_features,
                instance: Arc::clone(&self.instance),
                maintenance_level,
            }),
            vendor_id: self.properties.vendor_id,
        };

        let device_arc = Arc::clone(&device.shared);
        let queue_groups = families
            .into_iter()
            .map(|&(family, ref priorities)| {
                let mut family_raw =
                    queue::QueueGroup::new(queue::QueueFamilyId(family.index as usize));
                for id in 0 .. priorities.len() {
                    let queue_raw = device_arc.raw.get_device_queue(family.index, id as _);
                    family_raw.add_queue(CommandQueue {
                        raw: Arc::new(queue_raw),
                        device: device_arc.clone(),
                        swapchain_fn: swapchain_fn.clone(),
                    });
                }
                family_raw
            })
            .collect();

        Ok(adapter::Gpu {
            device,
            queue_groups,
        })
    }

    fn format_properties(&self, format: Option<format::Format>) -> format::Properties {
        let properties = unsafe {
            self.instance.0.get_physical_device_format_properties(
                self.handle,
                format.map_or(vk::Format::UNDEFINED, conv::map_format),
            )
        };

        format::Properties {
            linear_tiling: conv::map_image_features(properties.linear_tiling_features),
            optimal_tiling: conv::map_image_features(properties.optimal_tiling_features),
            buffer_features: conv::map_buffer_features(properties.buffer_features),
        }
    }

    fn image_format_properties(
        &self,
        format: format::Format,
        dimensions: u8,
        tiling: image::Tiling,
        usage: image::Usage,
        view_caps: image::ViewCapabilities,
    ) -> Option<image::FormatProperties> {
        let format_properties = unsafe {
            self.instance.0.get_physical_device_image_format_properties(
                self.handle,
                conv::map_format(format),
                match dimensions {
                    1 => vk::ImageType::TYPE_1D,
                    2 => vk::ImageType::TYPE_2D,
                    3 => vk::ImageType::TYPE_3D,
                    _ => panic!("Unexpected image dimensionality: {}", dimensions),
                },
                conv::map_tiling(tiling),
                conv::map_image_usage(usage),
                conv::map_view_capabilities(view_caps),
            )
        };

        match format_properties {
            Ok(props) => Some(image::FormatProperties {
                max_extent: image::Extent {
                    width: props.max_extent.width,
                    height: props.max_extent.height,
                    depth: props.max_extent.depth,
                },
                max_levels: props.max_mip_levels as _,
                max_layers: props.max_array_layers as _,
                sample_count_mask: props.sample_counts.as_raw() as _,
                max_resource_size: props.max_resource_size as _,
            }),
            Err(vk::Result::ERROR_FORMAT_NOT_SUPPORTED) => None,
            Err(other) => {
                error!("Unexpected error in `image_format_properties`: {:?}", other);
                None
            }
        }
    }

    fn memory_properties(&self) -> adapter::MemoryProperties {
        let mem_properties = unsafe {
            self.instance
                .0
                .get_physical_device_memory_properties(self.handle)
        };
        let memory_heaps = mem_properties.memory_heaps
            [.. mem_properties.memory_heap_count as usize]
            .iter()
            .map(|mem| mem.size)
            .collect();
        let memory_types = mem_properties.memory_types
            [.. mem_properties.memory_type_count as usize]
            .iter()
            .map(|mem| {
                use crate::memory::Properties;
                let mut type_flags = Properties::empty();

                if mem
                    .property_flags
                    .intersects(vk::MemoryPropertyFlags::DEVICE_LOCAL)
                {
                    type_flags |= Properties::DEVICE_LOCAL;
                }
                if mem
                    .property_flags
                    .intersects(vk::MemoryPropertyFlags::HOST_VISIBLE)
                {
                    type_flags |= Properties::CPU_VISIBLE;
                }
                if mem
                    .property_flags
                    .intersects(vk::MemoryPropertyFlags::HOST_COHERENT)
                {
                    type_flags |= Properties::COHERENT;
                }
                if mem
                    .property_flags
                    .intersects(vk::MemoryPropertyFlags::HOST_CACHED)
                {
                    type_flags |= Properties::CPU_CACHED;
                }
                if mem
                    .property_flags
                    .intersects(vk::MemoryPropertyFlags::LAZILY_ALLOCATED)
                {
                    type_flags |= Properties::LAZILY_ALLOCATED;
                }

                adapter::MemoryType {
                    properties: type_flags,
                    heap_index: mem.heap_index as usize,
                }
            })
            .collect();

        adapter::MemoryProperties {
            memory_heaps,
            memory_types,
        }
    }

    fn features(&self) -> Features {
        // see https://github.com/gfx-rs/gfx/issues/1930
        let is_windows_intel_dual_src_bug = cfg!(windows)
            && self.properties.vendor_id == info::intel::VENDOR
            && (self.properties.device_id & info::intel::DEVICE_KABY_LAKE_MASK
                == info::intel::DEVICE_KABY_LAKE_MASK
                || self.properties.device_id & info::intel::DEVICE_SKY_LAKE_MASK
                    == info::intel::DEVICE_SKY_LAKE_MASK);

        let features = unsafe { self.instance.0.get_physical_device_features(self.handle) };
        let mut bits = Features::empty()
            | Features::TRIANGLE_FAN
            | Features::SEPARATE_STENCIL_REF_VALUES
            | Features::SAMPLER_MIP_LOD_BIAS;

        if self.supports_extension(*AMD_NEGATIVE_VIEWPORT_HEIGHT)
            || self.supports_extension(*KHR_MAINTENANCE1)
        {
            bits |= Features::NDC_Y_UP;
        }
        if self.supports_extension(*KHR_SAMPLER_MIRROR_MIRROR_CLAMP_TO_EDGE) {
            bits |= Features::SAMPLER_MIRROR_CLAMP_EDGE;
        }

        if features.robust_buffer_access != 0 {
            bits |= Features::ROBUST_BUFFER_ACCESS;
        }
        if features.full_draw_index_uint32 != 0 {
            bits |= Features::FULL_DRAW_INDEX_U32;
        }
        if features.image_cube_array != 0 {
            bits |= Features::IMAGE_CUBE_ARRAY;
        }
        if features.independent_blend != 0 {
            bits |= Features::INDEPENDENT_BLENDING;
        }
        if features.geometry_shader != 0 {
            bits |= Features::GEOMETRY_SHADER;
        }
        if features.tessellation_shader != 0 {
            bits |= Features::TESSELLATION_SHADER;
        }
        if features.sample_rate_shading != 0 {
            bits |= Features::SAMPLE_RATE_SHADING;
        }
        if features.dual_src_blend != 0 && !is_windows_intel_dual_src_bug {
            bits |= Features::DUAL_SRC_BLENDING;
        }
        if features.logic_op != 0 {
            bits |= Features::LOGIC_OP;
        }
        if features.multi_draw_indirect != 0 {
            bits |= Features::MULTI_DRAW_INDIRECT;
        }
        if features.draw_indirect_first_instance != 0 {
            bits |= Features::DRAW_INDIRECT_FIRST_INSTANCE;
        }
        if features.depth_clamp != 0 {
            bits |= Features::DEPTH_CLAMP;
        }
        if features.depth_bias_clamp != 0 {
            bits |= Features::DEPTH_BIAS_CLAMP;
        }
        if features.fill_mode_non_solid != 0 {
            bits |= Features::NON_FILL_POLYGON_MODE;
        }
        if features.depth_bounds != 0 {
            bits |= Features::DEPTH_BOUNDS;
        }
        if features.wide_lines != 0 {
            bits |= Features::LINE_WIDTH;
        }
        if features.large_points != 0 {
            bits |= Features::POINT_SIZE;
        }
        if features.alpha_to_one != 0 {
            bits |= Features::ALPHA_TO_ONE;
        }
        if features.multi_viewport != 0 {
            bits |= Features::MULTI_VIEWPORTS;
        }
        if features.sampler_anisotropy != 0 {
            bits |= Features::SAMPLER_ANISOTROPY;
        }
        if features.texture_compression_etc2 != 0 {
            bits |= Features::FORMAT_ETC2;
        }
        if features.texture_compression_astc_ldr != 0 {
            bits |= Features::FORMAT_ASTC_LDR;
        }
        if features.texture_compression_bc != 0 {
            bits |= Features::FORMAT_BC;
        }
        if features.occlusion_query_precise != 0 {
            bits |= Features::PRECISE_OCCLUSION_QUERY;
        }
        if features.pipeline_statistics_query != 0 {
            bits |= Features::PIPELINE_STATISTICS_QUERY;
        }
        if features.vertex_pipeline_stores_and_atomics != 0 {
            bits |= Features::VERTEX_STORES_AND_ATOMICS;
        }
        if features.fragment_stores_and_atomics != 0 {
            bits |= Features::FRAGMENT_STORES_AND_ATOMICS;
        }
        if features.shader_tessellation_and_geometry_point_size != 0 {
            bits |= Features::SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE;
        }
        if features.shader_image_gather_extended != 0 {
            bits |= Features::SHADER_IMAGE_GATHER_EXTENDED;
        }
        if features.shader_storage_image_extended_formats != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_EXTENDED_FORMATS;
        }
        if features.shader_storage_image_multisample != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_MULTISAMPLE;
        }
        if features.shader_storage_image_read_without_format != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT;
        }
        if features.shader_storage_image_write_without_format != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT;
        }
        if features.shader_uniform_buffer_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING;
        }
        if features.shader_sampled_image_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING;
        }
        if features.shader_storage_buffer_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING;
        }
        if features.shader_storage_image_array_dynamic_indexing != 0 {
            bits |= Features::SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING;
        }
        if features.shader_clip_distance != 0 {
            bits |= Features::SHADER_CLIP_DISTANCE;
        }
        if features.shader_cull_distance != 0 {
            bits |= Features::SHADER_CULL_DISTANCE;
        }
        if features.shader_float64 != 0 {
            bits |= Features::SHADER_FLOAT64;
        }
        if features.shader_int64 != 0 {
            bits |= Features::SHADER_INT64;
        }
        if features.shader_int16 != 0 {
            bits |= Features::SHADER_INT16;
        }
        if features.shader_resource_residency != 0 {
            bits |= Features::SHADER_RESOURCE_RESIDENCY;
        }
        if features.shader_resource_min_lod != 0 {
            bits |= Features::SHADER_RESOURCE_MIN_LOD;
        }
        if features.sparse_binding != 0 {
            bits |= Features::SPARSE_BINDING;
        }
        if features.sparse_residency_buffer != 0 {
            bits |= Features::SPARSE_RESIDENCY_BUFFER;
        }
        if features.sparse_residency_image2_d != 0 {
            bits |= Features::SPARSE_RESIDENCY_IMAGE_2D;
        }
        if features.sparse_residency_image3_d != 0 {
            bits |= Features::SPARSE_RESIDENCY_IMAGE_3D;
        }
        if features.sparse_residency2_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_2_SAMPLES;
        }
        if features.sparse_residency4_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_4_SAMPLES;
        }
        if features.sparse_residency8_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_8_SAMPLES;
        }
        if features.sparse_residency16_samples != 0 {
            bits |= Features::SPARSE_RESIDENCY_16_SAMPLES;
        }
        if features.sparse_residency_aliased != 0 {
            bits |= Features::SPARSE_RESIDENCY_ALIASED;
        }
        if features.variable_multisample_rate != 0 {
            bits |= Features::VARIABLE_MULTISAMPLE_RATE;
        }
        if features.inherited_queries != 0 {
            bits |= Features::INHERITED_QUERIES;
        }

        bits
    }

    fn hints(&self) -> Hints {
        Hints::BASE_VERTEX_INSTANCE_DRAWING
    }

    fn limits(&self) -> Limits {
        let limits = &self.properties.limits;
        let max_group_count = limits.max_compute_work_group_count;
        let max_group_size = limits.max_compute_work_group_size;

        Limits {
            max_image_1d_size: limits.max_image_dimension1_d,
            max_image_2d_size: limits.max_image_dimension2_d,
            max_image_3d_size: limits.max_image_dimension3_d,
            max_image_cube_size: limits.max_image_dimension_cube,
            max_image_array_layers: limits.max_image_array_layers as _,
            max_texel_elements: limits.max_texel_buffer_elements as _,
            max_patch_size: limits.max_tessellation_patch_size as PatchSize,
            max_viewports: limits.max_viewports as _,
            max_viewport_dimensions: limits.max_viewport_dimensions,
            max_framebuffer_extent: image::Extent {
                width: limits.max_framebuffer_width,
                height: limits.max_framebuffer_height,
                depth: limits.max_framebuffer_layers,
            },
            max_compute_work_group_count: [
                max_group_count[0] as _,
                max_group_count[1] as _,
                max_group_count[2] as _,
            ],
            max_compute_work_group_size: [
                max_group_size[0] as _,
                max_group_size[1] as _,
                max_group_size[2] as _,
            ],
            max_vertex_input_attributes: limits.max_vertex_input_attributes as _,
            max_vertex_input_bindings: limits.max_vertex_input_bindings as _,
            max_vertex_input_attribute_offset: limits.max_vertex_input_attribute_offset as _,
            max_vertex_input_binding_stride: limits.max_vertex_input_binding_stride as _,
            max_vertex_output_components: limits.max_vertex_output_components as _,
            optimal_buffer_copy_offset_alignment: limits.optimal_buffer_copy_offset_alignment as _,
            optimal_buffer_copy_pitch_alignment: limits.optimal_buffer_copy_row_pitch_alignment
                as _,
            min_texel_buffer_offset_alignment: limits.min_texel_buffer_offset_alignment as _,
            min_uniform_buffer_offset_alignment: limits.min_uniform_buffer_offset_alignment as _,
            min_storage_buffer_offset_alignment: limits.min_storage_buffer_offset_alignment as _,
            framebuffer_color_sample_counts: limits.framebuffer_color_sample_counts.as_raw() as _,
            framebuffer_depth_sample_counts: limits.framebuffer_depth_sample_counts.as_raw() as _,
            framebuffer_stencil_sample_counts: limits.framebuffer_stencil_sample_counts.as_raw()
                as _,
            max_color_attachments: limits.max_color_attachments as _,
            buffer_image_granularity: limits.buffer_image_granularity,
            non_coherent_atom_size: limits.non_coherent_atom_size as _,
            max_sampler_anisotropy: limits.max_sampler_anisotropy,
            min_vertex_input_binding_stride_alignment: 1,
            max_bound_descriptor_sets: limits.max_bound_descriptor_sets as _,
            max_compute_shared_memory_size: limits.max_compute_shared_memory_size as _,
            max_compute_work_group_invocations: limits.max_compute_work_group_invocations as _,
            max_descriptor_set_input_attachments: limits.max_descriptor_set_input_attachments as _,
            max_descriptor_set_sampled_images: limits.max_descriptor_set_sampled_images as _,
            max_descriptor_set_samplers: limits.max_descriptor_set_samplers as _,
            max_descriptor_set_storage_buffers: limits.max_descriptor_set_storage_buffers as _,
            max_descriptor_set_storage_buffers_dynamic: limits
                .max_descriptor_set_storage_buffers_dynamic
                as _,
            max_descriptor_set_storage_images: limits.max_descriptor_set_storage_images as _,
            max_descriptor_set_uniform_buffers: limits.max_descriptor_set_uniform_buffers as _,
            max_descriptor_set_uniform_buffers_dynamic: limits
                .max_descriptor_set_uniform_buffers_dynamic
                as _,
            max_draw_indexed_index_value: limits.max_draw_indexed_index_value,
            max_draw_indirect_count: limits.max_draw_indirect_count,
            max_fragment_combined_output_resources: limits.max_fragment_combined_output_resources
                as _,
            max_fragment_dual_source_attachments: limits.max_fragment_dual_src_attachments as _,
            max_fragment_input_components: limits.max_fragment_input_components as _,
            max_fragment_output_attachments: limits.max_fragment_output_attachments as _,
            max_framebuffer_layers: limits.max_framebuffer_layers as _,
            max_geometry_input_components: limits.max_geometry_input_components as _,
            max_geometry_output_components: limits.max_geometry_output_components as _,
            max_geometry_output_vertices: limits.max_geometry_output_vertices as _,
            max_geometry_shader_invocations: limits.max_geometry_shader_invocations as _,
            max_geometry_total_output_components: limits.max_geometry_total_output_components as _,
            max_memory_allocation_count: limits.max_memory_allocation_count as _,
            max_per_stage_descriptor_input_attachments: limits
                .max_per_stage_descriptor_input_attachments
                as _,
            max_per_stage_descriptor_sampled_images: limits.max_per_stage_descriptor_sampled_images
                as _,
            max_per_stage_descriptor_samplers: limits.max_per_stage_descriptor_samplers as _,
            max_per_stage_descriptor_storage_buffers: limits
                .max_per_stage_descriptor_storage_buffers
                as _,
            max_per_stage_descriptor_storage_images: limits.max_per_stage_descriptor_storage_images
                as _,
            max_per_stage_descriptor_uniform_buffers: limits
                .max_per_stage_descriptor_uniform_buffers
                as _,
            max_per_stage_resources: limits.max_per_stage_resources as _,
            max_push_constants_size: limits.max_push_constants_size as _,
            max_sampler_allocation_count: limits.max_sampler_allocation_count as _,
            max_sampler_lod_bias: limits.max_sampler_lod_bias as _,
            max_storage_buffer_range: limits.max_storage_buffer_range as _,
            max_uniform_buffer_range: limits.max_uniform_buffer_range as _,
            min_memory_map_alignment: limits.min_memory_map_alignment,
            standard_sample_locations: limits.standard_sample_locations == ash::vk::TRUE,
        }
    }

    fn is_valid_cache(&self, cache: &[u8]) -> bool {
        const HEADER_SIZE: usize = 16 + vk::UUID_SIZE;

        if cache.len() < HEADER_SIZE {
            warn!("Bad cache data length {:?}", cache.len());
            return false;
        }

        let header_len = u32::from_le_bytes([cache[0], cache[1], cache[2], cache[3]]);
        let header_version = u32::from_le_bytes([cache[4], cache[5], cache[6], cache[7]]);
        let vendor_id = u32::from_le_bytes([cache[8], cache[9], cache[10], cache[11]]);
        let device_id = u32::from_le_bytes([cache[12], cache[13], cache[14], cache[15]]);

        // header length
        if (header_len as usize) < HEADER_SIZE {
            warn!("Bad header length {:?}", header_len);
            return false;
        }

        // cache header version
        if header_version != vk::PipelineCacheHeaderVersion::ONE.as_raw() as u32 {
            warn!("Unsupported cache header version: {:?}", header_version);
            return false;
        }

        // vendor id
        if vendor_id != self.properties.vendor_id {
            warn!(
                "Vendor ID mismatch. Device: {:?}, cache: {:?}.",
                self.properties.vendor_id, vendor_id,
            );
            return false;
        }

        // device id
        if device_id != self.properties.device_id {
            warn!(
                "Device ID mismatch. Device: {:?}, cache: {:?}.",
                self.properties.device_id, device_id,
            );
            return false;
        }

        if self.properties.pipeline_cache_uuid != cache[16 .. 16 + vk::UUID_SIZE] {
            warn!(
                "Pipeline cache UUID mismatch. Device: {:?}, cache: {:?}.",
                self.properties.pipeline_cache_uuid,
                &cache[16 .. 16 + vk::UUID_SIZE],
            );
            return false;
        }
        true
    }
}

#[doc(hidden)]
pub struct RawDevice {
    raw: ash::Device,
    features: Features,
    instance: Arc<RawInstance>,
    maintenance_level: u8,
}

impl fmt::Debug for RawDevice {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "RawDevice") // TODO: Real Debug impl
    }
}
impl Drop for RawDevice {
    fn drop(&mut self) {
        unsafe {
            self.raw.destroy_device(None);
        }
    }
}

impl RawDevice {
    fn debug_messenger(&self) -> Option<&DebugMessenger> {
        self.instance.1.as_ref()
    }

    fn map_viewport(&self, rect: &hal::pso::Viewport) -> vk::Viewport {
        let flip_y = self.features.contains(hal::Features::NDC_Y_UP);
        let shift_y = flip_y && self.maintenance_level != 0;
        conv::map_viewport(rect, flip_y, shift_y)
    }
}

// Need to explicitly synchronize on submission and present.
pub type RawCommandQueue = Arc<vk::Queue>;

pub struct CommandQueue {
    raw: RawCommandQueue,
    device: Arc<RawDevice>,
    swapchain_fn: vk::KhrSwapchainFn,
}

impl fmt::Debug for CommandQueue {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("CommandQueue")
    }
}

impl queue::CommandQueue<Backend> for CommandQueue {
    unsafe fn submit<'a, T, Ic, S, Iw, Is>(
        &mut self,
        submission: queue::Submission<Ic, Iw, Is>,
        fence: Option<&native::Fence>,
    ) where
        T: 'a + Borrow<command::CommandBuffer>,
        Ic: IntoIterator<Item = &'a T>,
        S: 'a + Borrow<native::Semaphore>,
        Iw: IntoIterator<Item = (&'a S, PipelineStage)>,
        Is: IntoIterator<Item = &'a S>,
    {
        //TODO: avoid heap allocations
        let mut waits = Vec::new();
        let mut stages = Vec::new();

        let buffers = submission
            .command_buffers
            .into_iter()
            .map(|cmd| cmd.borrow().raw)
            .collect::<Vec<_>>();
        for (semaphore, stage) in submission.wait_semaphores {
            waits.push(semaphore.borrow().0);
            stages.push(conv::map_pipeline_stage(stage));
        }
        let signals = submission
            .signal_semaphores
            .into_iter()
            .map(|semaphore| semaphore.borrow().0)
            .collect::<Vec<_>>();

        let info = vk::SubmitInfo {
            s_type: vk::StructureType::SUBMIT_INFO,
            p_next: ptr::null(),
            wait_semaphore_count: waits.len() as u32,
            p_wait_semaphores: waits.as_ptr(),
            // If count is zero, AMD driver crashes if nullptr is not set for stage masks
            p_wait_dst_stage_mask: if stages.is_empty() {
                ptr::null()
            } else {
                stages.as_ptr()
            },
            command_buffer_count: buffers.len() as u32,
            p_command_buffers: buffers.as_ptr(),
            signal_semaphore_count: signals.len() as u32,
            p_signal_semaphores: signals.as_ptr(),
        };

        let fence_raw = fence.map(|fence| fence.0).unwrap_or(vk::Fence::null());

        let result = self.device.raw.queue_submit(*self.raw, &[info], fence_raw);
        assert_eq!(Ok(()), result);
    }

    unsafe fn present<'a, W, Is, S, Iw>(
        &mut self,
        swapchains: Is,
        wait_semaphores: Iw,
    ) -> Result<Option<Suboptimal>, PresentError>
    where
        W: 'a + Borrow<window::Swapchain>,
        Is: IntoIterator<Item = (&'a W, SwapImageIndex)>,
        S: 'a + Borrow<native::Semaphore>,
        Iw: IntoIterator<Item = &'a S>,
    {
        let semaphores = wait_semaphores
            .into_iter()
            .map(|sem| sem.borrow().0)
            .collect::<Vec<_>>();

        let mut frames = Vec::new();
        let mut vk_swapchains = Vec::new();
        for (swapchain, index) in swapchains {
            vk_swapchains.push(swapchain.borrow().raw);
            frames.push(index);
        }

        let info = vk::PresentInfoKHR {
            s_type: vk::StructureType::PRESENT_INFO_KHR,
            p_next: ptr::null(),
            wait_semaphore_count: semaphores.len() as _,
            p_wait_semaphores: semaphores.as_ptr(),
            swapchain_count: vk_swapchains.len() as _,
            p_swapchains: vk_swapchains.as_ptr(),
            p_image_indices: frames.as_ptr(),
            p_results: ptr::null_mut(),
        };

        match self.swapchain_fn.queue_present_khr(*self.raw, &info) {
            vk::Result::SUCCESS => Ok(None),
            vk::Result::SUBOPTIMAL_KHR => Ok(Some(Suboptimal)),
            vk::Result::ERROR_OUT_OF_HOST_MEMORY => {
                Err(PresentError::OutOfMemory(OutOfMemory::Host))
            }
            vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => {
                Err(PresentError::OutOfMemory(OutOfMemory::Device))
            }
            vk::Result::ERROR_DEVICE_LOST => Err(PresentError::DeviceLost(DeviceLost)),
            vk::Result::ERROR_OUT_OF_DATE_KHR => Err(PresentError::OutOfDate),
            vk::Result::ERROR_SURFACE_LOST_KHR => Err(PresentError::SurfaceLost(SurfaceLost)),
            _ => panic!("Failed to present frame"),
        }
    }

    unsafe fn present_surface(
        &mut self,
        surface: &mut window::Surface,
        image: window::SurfaceImage,
        wait_semaphore: Option<&native::Semaphore>,
    ) -> Result<Option<Suboptimal>, PresentError> {
        let ssc = surface.swapchain.as_ref().unwrap();
        let p_wait_semaphores = if let Some(wait_semaphore) = wait_semaphore {
            &wait_semaphore.0
        } else {
            let submit_info = vk::SubmitInfo {
                s_type: vk::StructureType::SUBMIT_INFO,
                p_next: ptr::null(),
                wait_semaphore_count: 0,
                p_wait_semaphores: ptr::null(),
                p_wait_dst_stage_mask: &vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT,
                command_buffer_count: 0,
                p_command_buffers: ptr::null(),
                signal_semaphore_count: 1,
                p_signal_semaphores: &ssc.semaphore.0,
            };
            self.device
                .raw
                .queue_submit(*self.raw, &[submit_info], vk::Fence::null())
                .unwrap();
            &ssc.semaphore.0
        };
        let present_info = vk::PresentInfoKHR {
            s_type: vk::StructureType::PRESENT_INFO_KHR,
            p_next: ptr::null(),
            wait_semaphore_count: 1,
            p_wait_semaphores,
            swapchain_count: 1,
            p_swapchains: &ssc.swapchain.raw,
            p_image_indices: &image.index,
            p_results: ptr::null_mut(),
        };

        match self
            .swapchain_fn
            .queue_present_khr(*self.raw, &present_info)
        {
            vk::Result::SUCCESS => Ok(None),
            vk::Result::SUBOPTIMAL_KHR => Ok(Some(Suboptimal)),
            vk::Result::ERROR_OUT_OF_HOST_MEMORY => {
                Err(PresentError::OutOfMemory(OutOfMemory::Host))
            }
            vk::Result::ERROR_OUT_OF_DEVICE_MEMORY => {
                Err(PresentError::OutOfMemory(OutOfMemory::Device))
            }
            vk::Result::ERROR_DEVICE_LOST => Err(PresentError::DeviceLost(DeviceLost)),
            vk::Result::ERROR_OUT_OF_DATE_KHR => Err(PresentError::OutOfDate),
            vk::Result::ERROR_SURFACE_LOST_KHR => Err(PresentError::SurfaceLost(SurfaceLost)),
            _ => panic!("Failed to present frame"),
        }
    }

    fn wait_idle(&self) -> Result<(), OutOfMemory> {
        match unsafe { self.device.raw.queue_wait_idle(*self.raw) } {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(OutOfMemory::Device),
            Err(_) => unreachable!(),
        }
    }
}

#[derive(Debug)]
pub struct Device {
    shared: Arc<RawDevice>,
    vendor_id: u32,
}

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub enum Backend {}
impl hal::Backend for Backend {
    type Instance = Instance;
    type PhysicalDevice = PhysicalDevice;
    type Device = Device;

    type Surface = window::Surface;
    type Swapchain = window::Swapchain;

    type QueueFamily = QueueFamily;
    type CommandQueue = CommandQueue;
    type CommandBuffer = command::CommandBuffer;

    type Memory = native::Memory;
    type CommandPool = pool::RawCommandPool;

    type ShaderModule = native::ShaderModule;
    type RenderPass = native::RenderPass;
    type Framebuffer = native::Framebuffer;

    type Buffer = native::Buffer;
    type BufferView = native::BufferView;
    type Image = native::Image;
    type ImageView = native::ImageView;
    type Sampler = native::Sampler;

    type ComputePipeline = native::ComputePipeline;
    type GraphicsPipeline = native::GraphicsPipeline;
    type PipelineLayout = native::PipelineLayout;
    type PipelineCache = native::PipelineCache;
    type DescriptorSetLayout = native::DescriptorSetLayout;
    type DescriptorPool = native::DescriptorPool;
    type DescriptorSet = native::DescriptorSet;

    type Fence = native::Fence;
    type Semaphore = native::Semaphore;
    type Event = native::Event;
    type QueryPool = native::QueryPool;
}
