/*!
# Vulkan backend internals.

## Stack memory

Most of the code just passes the data through. The only problem
that affects all the pieces is related to memory allocation:
Vulkan expects slices, but the API gives us `Iterator`.
So we end up using a lot of `inplace_it` to get things collected on stack.

## Framebuffers

One part that has actual logic is related to framebuffers. HAL is modelled
after image-less framebuffers. If the the Vulkan implementation supports it,
we map it 1:1, and everything is great. If it doesn't expose
`KHR_imageless_framebuffer`, however, than we have to keep all the created
framebuffers internally in an internally-synchronized map, per `B::Framebuffer`.
!*/

#![allow(non_snake_case)]

#[macro_use]
extern crate log;

#[cfg(target_os = "macos")]
#[macro_use]
extern crate objc;

#[cfg(not(feature = "use-rtld-next"))]
use ash::Entry;
#[cfg(feature = "use-rtld-next")]
type Entry = ash::EntryCustom<()>;
use ash::{
    extensions::{ext, khr, nv::MeshShader},
    version::{DeviceV1_0, EntryV1_0, InstanceV1_0},
    vk,
};

use hal::{
    adapter,
    device::{DeviceLost, OutOfMemory},
    display, image, memory,
    pso::PipelineStage,
    queue,
    window::{OutOfDate, PresentError, Suboptimal, SurfaceLost},
    Features,
};

use std::{
    borrow::Cow,
    cmp,
    ffi::{CStr, CString},
    fmt, slice,
    sync::Arc,
    thread, unreachable,
};

mod command;
mod conv;
mod device;
mod info;
mod native;
mod physical_device;
mod pool;
mod window;

pub use physical_device::*;

// Sets up the maximum count we expect in most cases, but maybe not all of them.
const ROUGH_MAX_ATTACHMENT_COUNT: usize = 5;

pub struct RawInstance {
    inner: ash::Instance,
    handle_is_external: bool,
    debug_messenger: Option<DebugMessenger>,
    get_physical_device_properties: Option<ExtensionFn<vk::KhrGetPhysicalDeviceProperties2Fn>>,
    display: Option<khr::Display>,
    external_memory_capabilities: Option<ExtensionFn<vk::KhrExternalMemoryCapabilitiesFn>>,
}

pub enum DebugMessenger {
    Utils(ext::DebugUtils, vk::DebugUtilsMessengerEXT),
    #[allow(deprecated)] // `DebugReport`
    Report(ext::DebugReport, vk::DebugReportCallbackEXT),
}

impl Drop for RawInstance {
    fn drop(&mut self) {
        unsafe {
            match self.debug_messenger {
                Some(DebugMessenger::Utils(ref ext, callback)) => {
                    ext.destroy_debug_utils_messenger(callback, None)
                }
                #[allow(deprecated)] // `DebugReport`
                Some(DebugMessenger::Report(ref ext, callback)) => {
                    ext.destroy_debug_report_callback(callback, None)
                }
                None => {}
            }

            if !self.handle_is_external {
                self.inner.destroy_instance(None);
            }
        }
    }
}

/// Helper wrapper around `vk::make_version`.
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub struct Version(u32);

impl Version {
    pub const V1_0: Version = Self(vk::make_version(1, 0, 0));
    pub const V1_1: Version = Self(vk::make_version(1, 1, 0));
    pub const V1_2: Version = Self(vk::make_version(1, 2, 0));

    pub const fn major(self) -> u32 {
        vk::version_major(self.0)
    }

    pub const fn minor(self) -> u32 {
        vk::version_minor(self.0)
    }

    pub const fn patch(self) -> u32 {
        vk::version_patch(self.0)
    }
}

impl fmt::Debug for Version {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ApiVersion")
            .field("major", &self.major())
            .field("minor", &self.minor())
            .field("patch", &self.patch())
            .finish()
    }
}

impl Into<u32> for Version {
    fn into(self) -> u32 {
        self.0
    }
}

impl Into<Version> for u32 {
    fn into(self) -> Version {
        Version(self)
    }
}

pub struct Instance {
    pub raw: Arc<RawInstance>,

    /// Supported extensions of this instance.
    pub extensions: Vec<&'static CStr>,

    pub entry: Entry,
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

    //TODO: use color field of vk::DebugUtilsLabelExt in a meaningful way?
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
                        "(type: {:?}, hndl: 0x{:x}, name: {})",
                        obj_info.object_type, obj_info.object_handle, name
                    ),
                    None => format!(
                        "(type: {:?}, hndl: 0x{:x})",
                        obj_info.object_type, obj_info.object_handle
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
    if thread::panicking() {
        return vk::FALSE;
    }
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
            "\n{} [{} (0x{:x})] : {}",
            message_type, message_id_name, message_id_number, message
        );

        for &(info_label, ref info) in additional_info.iter() {
            if let Some(ref data) = *info {
                msg = format!("{}\n{}: {}", msg, info_label, data);
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
    if thread::panicking() {
        return vk::FALSE;
    }

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

impl Instance {
    pub fn required_extensions(
        entry: &Entry,
        driver_api_version: Version,
    ) -> Result<Vec<&'static CStr>, hal::UnsupportedBackend> {
        let instance_extensions = entry
            .enumerate_instance_extension_properties()
            .map_err(|e| {
                info!("Unable to enumerate instance extensions: {:?}", e);
                hal::UnsupportedBackend
            })?;

        // Check our extensions against the available extensions
        let mut extensions: Vec<&'static CStr> = Vec::new();
        extensions.push(khr::Surface::name());

        // Platform-specific WSI extensions
        if cfg!(all(
            unix,
            not(target_os = "android"),
            not(target_os = "macos")
        )) {
            extensions.push(khr::XlibSurface::name());
            extensions.push(khr::XcbSurface::name());
            extensions.push(khr::WaylandSurface::name());
        }
        if cfg!(target_os = "android") {
            extensions.push(khr::AndroidSurface::name());
        }
        if cfg!(target_os = "windows") {
            extensions.push(khr::Win32Surface::name());
        }
        if cfg!(target_os = "macos") {
            extensions.push(ash::extensions::mvk::MacOSSurface::name());
        }

        extensions.push(ext::DebugUtils::name());
        if cfg!(debug_assertions) {
            #[allow(deprecated)]
            extensions.push(ext::DebugReport::name());
        }

        extensions.push(vk::KhrGetPhysicalDeviceProperties2Fn::name());

        // VK_KHR_storage_buffer_storage_class required for `Naga` on Vulkan 1.0 devices
        if driver_api_version == Version::V1_0 {
            extensions.push(vk::KhrStorageBufferStorageClassFn::name());
        }

        extensions.push(vk::ExtDisplaySurfaceCounterFn::name());
        extensions.push(khr::Display::name());

        if driver_api_version < Version::V1_1 {
            extensions.push(vk::KhrExternalMemoryCapabilitiesFn::name());
        }

        // VK_KHR_storage_buffer_storage_class required for `Naga` on Vulkan 1.0 devices
        if driver_api_version == Version::V1_0 {
            extensions.push(vk::KhrStorageBufferStorageClassFn::name());
        }

        // Only keep available extensions.
        extensions.retain(|&ext| {
            if instance_extensions
                .iter()
                .find(|inst_ext| unsafe { CStr::from_ptr(inst_ext.extension_name.as_ptr()) == ext })
                .is_some()
            {
                true
            } else {
                info!("Unable to find extension: {}", ext.to_string_lossy());
                false
            }
        });
        Ok(extensions)
    }

    pub fn required_layers(entry: &Entry) -> Result<Vec<&'static CStr>, hal::UnsupportedBackend> {
        let instance_layers = entry.enumerate_instance_layer_properties().map_err(|e| {
            info!("Unable to enumerate instance layers: {:?}", e);
            hal::UnsupportedBackend
        })?;

        // Check requested layers against the available layers
        let mut layers: Vec<&'static CStr> = Vec::new();
        if cfg!(debug_assertions) {
            layers.push(CStr::from_bytes_with_nul(b"VK_LAYER_KHRONOS_validation\0").unwrap());
        }

        // Only keep available layers.
        layers.retain(|&layer| {
            if instance_layers
                .iter()
                .find(|inst_layer| unsafe {
                    CStr::from_ptr(inst_layer.layer_name.as_ptr()) == layer
                })
                .is_some()
            {
                true
            } else {
                warn!("Unable to find layer: {}", layer.to_string_lossy());
                false
            }
        });
        Ok(layers)
    }

    /// # Safety
    /// `raw_instance` must be created using at least the extensions provided by `Instance::required_extensions()`
    /// and the layers provided by `Instance::required_extensions()`.
    /// `driver_api_version` must match the version used to create `raw_instance`.
    /// `extensions` must match the extensions used to create `raw_instance`.
    /// `raw_instance` must be manually destroyed *after* gfx-hal Instance has been dropped.
    pub unsafe fn from_raw(
        entry: Entry,
        raw_instance: ash::Instance,
        driver_api_version: Version,
        extensions: Vec<&'static CStr>,
    ) -> Result<Self, hal::UnsupportedBackend> {
        Instance::inner_create(entry, raw_instance, true, driver_api_version, extensions)
    }

    fn inner_create(
        entry: Entry,
        instance: ash::Instance,
        handle_is_external: bool,
        driver_api_version: Version,
        extensions: Vec<&'static CStr>,
    ) -> Result<Self, hal::UnsupportedBackend> {
        if driver_api_version == Version::V1_0
            && !extensions.contains(&vk::KhrStorageBufferStorageClassFn::name())
        {
            warn!("Unable to create Vulkan instance. Required VK_KHR_storage_buffer_storage_class extension is not supported");
            return Err(hal::UnsupportedBackend);
        }

        let instance_extensions = entry
            .enumerate_instance_extension_properties()
            .map_err(|e| {
                info!("Unable to enumerate instance extensions: {:?}", e);
                hal::UnsupportedBackend
            })?;

        let get_physical_device_properties = if driver_api_version >= Version::V1_1 {
            Some(ExtensionFn::Promoted)
        } else {
            extensions
                .iter()
                .find(|&&ext| ext == vk::KhrGetPhysicalDeviceProperties2Fn::name())
                .map(|_| {
                    ExtensionFn::Extension(vk::KhrGetPhysicalDeviceProperties2Fn::load(
                        |name| unsafe {
                            std::mem::transmute(
                                entry.get_instance_proc_addr(instance.handle(), name.as_ptr()),
                            )
                        },
                    ))
                })
        };

        let display = extensions
            .iter()
            .find(|&&ext| ext == khr::Display::name())
            .map(|_| khr::Display::new(&entry, &instance));

        let external_memory_capabilities = if driver_api_version >= Version::V1_1 {
            Some(ExtensionFn::Promoted)
        } else {
            extensions
                .iter()
                .find(|&&ext| ext == vk::KhrExternalMemoryCapabilitiesFn::name())
                .map(|_| {
                    ExtensionFn::Extension(vk::KhrExternalMemoryCapabilitiesFn::load(
                        |name| unsafe {
                            std::mem::transmute(
                                entry.get_instance_proc_addr(instance.handle(), name.as_ptr()),
                            )
                        },
                    ))
                })
        };

        #[allow(deprecated)] // `DebugReport`
        let debug_messenger = {
            // make sure VK_EXT_debug_utils is available
            if instance_extensions.iter().any(|props| unsafe {
                CStr::from_ptr(props.extension_name.as_ptr()) == ext::DebugUtils::name()
            }) {
                let ext = ext::DebugUtils::new(&entry, &instance);
                let info = vk::DebugUtilsMessengerCreateInfoEXT::builder()
                    .flags(vk::DebugUtilsMessengerCreateFlagsEXT::empty())
                    .message_severity(vk::DebugUtilsMessageSeverityFlagsEXT::all())
                    .message_type(vk::DebugUtilsMessageTypeFlagsEXT::all())
                    .pfn_user_callback(Some(debug_utils_messenger_callback));
                let handle = unsafe { ext.create_debug_utils_messenger(&info, None) }.unwrap();
                Some(DebugMessenger::Utils(ext, handle))
            } else if cfg!(debug_assertions)
                && instance_extensions.iter().any(|props| unsafe {
                    CStr::from_ptr(props.extension_name.as_ptr()) == ext::DebugReport::name()
                })
            {
                let ext = ext::DebugReport::new(&entry, &instance);
                let info = vk::DebugReportCallbackCreateInfoEXT::builder()
                    .flags(vk::DebugReportFlagsEXT::all())
                    .pfn_callback(Some(debug_report_callback));
                let handle = unsafe { ext.create_debug_report_callback(&info, None) }.unwrap();
                Some(DebugMessenger::Report(ext, handle))
            } else {
                None
            }
        };

        Ok(Instance {
            raw: Arc::new(RawInstance {
                inner: instance,
                handle_is_external,
                debug_messenger,
                get_physical_device_properties,
                display,
                external_memory_capabilities,
            }),
            extensions,
            entry,
        })
    }

    /// # Safety
    /// `raw_physical_device` must be created from `self` (or from the inner raw handle)
    pub unsafe fn adapter_from_raw(
        &self,
        raw_physical_device: vk::PhysicalDevice,
    ) -> adapter::Adapter<Backend> {
        physical_device::load_adapter(&self.raw, raw_physical_device)
    }
}

impl hal::Instance<Backend> for Instance {
    fn create(name: &str, version: u32) -> Result<Self, hal::UnsupportedBackend> {
        #[cfg(not(feature = "use-rtld-next"))]
        let entry = match unsafe { Entry::new() } {
            Ok(entry) => entry,
            Err(err) => {
                info!("Missing Vulkan entry points: {:?}", err);
                return Err(hal::UnsupportedBackend);
            }
        };

        #[cfg(feature = "use-rtld-next")]
        let entry = Entry::new_custom((), |_, name| unsafe {
            libc::dlsym(libc::RTLD_NEXT, name.as_ptr())
        });

        let driver_api_version = match entry.try_enumerate_instance_version() {
            // Vulkan 1.1+
            Ok(Some(version)) => version.into(),

            // Vulkan 1.0
            Ok(None) => Version::V1_0,

            // Ignore out of memory since it's unlikely to happen and `Instance::create` doesn't have a way to express it in the return value.
            Err(err) if err == vk::Result::ERROR_OUT_OF_HOST_MEMORY => {
                warn!("vkEnumerateInstanceVersion returned VK_ERROR_OUT_OF_HOST_MEMORY");
                return Err(hal::UnsupportedBackend);
            }

            Err(_) => unreachable!(),
        };

        let app_name = CString::new(name).unwrap();
        let app_info = vk::ApplicationInfo::builder()
            .application_name(app_name.as_c_str())
            .application_version(version)
            .engine_name(CStr::from_bytes_with_nul(b"gfx-rs\0").unwrap())
            .engine_version(1)
            .api_version({
                // Pick the latest API version available, but don't go later than the SDK version used by `gfx_backend_vulkan`.
                cmp::min(driver_api_version, {
                    // This is the max Vulkan API version supported by `gfx_backend_vulkan`.
                    //
                    // If we want to increment this, there are some things that must be done first:
                    //  - Audit the behavioral differences between the previous and new API versions.
                    //  - Audit all extensions used by this backend:
                    //    - If any were promoted in the new API version and the behavior has changed, we must handle the new behavior in addition to the old behavior.
                    //    - If any were obsoleted in the new API version, we must implement a fallback for the new API version
                    //    - If any are non-KHR-vendored, we must ensure the new behavior is still correct (since backwards-compatibility is not guaranteed).
                    //
                    // TODO: This should be replaced by `vk::HEADER_VERSION_COMPLETE` (added in `ash@6f488cd`) and this comment moved to either `README.md` or `Cargo.toml`.
                    Version::V1_2
                })
                .into()
            });

        let extensions = Instance::required_extensions(&entry, driver_api_version)?;

        let layers = Instance::required_layers(&entry)?;

        let instance = {
            let str_pointers = layers
                .iter()
                .chain(extensions.iter())
                .map(|&s| {
                    // Safe because `layers` and `extensions` entries have static lifetime.
                    s.as_ptr()
                })
                .collect::<Vec<_>>();

            let create_info = vk::InstanceCreateInfo::builder()
                .flags(vk::InstanceCreateFlags::empty())
                .application_info(&app_info)
                .enabled_layer_names(&str_pointers[..layers.len()])
                .enabled_extension_names(&str_pointers[layers.len()..]);

            unsafe { entry.create_instance(&create_info, None) }.map_err(|e| {
                warn!("Unable to create Vulkan instance: {:?}", e);
                hal::UnsupportedBackend
            })?
        };
        Instance::inner_create(entry, instance, false, driver_api_version, extensions)
    }

    fn enumerate_adapters(&self) -> Vec<adapter::Adapter<Backend>> {
        let devices = match unsafe { self.raw.inner.enumerate_physical_devices() } {
            Ok(devices) => devices,
            Err(err) => {
                error!("Could not enumerate physical devices! {}", err);
                vec![]
            }
        };

        devices
            .into_iter()
            .map(|device| physical_device::load_adapter(&self.raw, device))
            .collect()
    }

    unsafe fn create_surface(
        &self,
        has_handle: &impl raw_window_handle::HasRawWindowHandle,
    ) -> Result<window::Surface, hal::window::InitError> {
        use raw_window_handle::RawWindowHandle;

        match has_handle.raw_window_handle() {
            #[cfg(all(
                unix,
                not(target_os = "android"),
                not(target_os = "macos"),
                not(target_os = "solaris")
            ))]
            RawWindowHandle::Wayland(handle)
                if self.extensions.contains(&khr::WaylandSurface::name()) =>
            {
                Ok(self.create_surface_from_wayland(handle.display, handle.surface))
            }
            #[cfg(all(
                unix,
                not(target_os = "android"),
                not(target_os = "macos"),
                not(target_os = "solaris")
            ))]
            RawWindowHandle::Xlib(handle)
                if self.extensions.contains(&khr::XlibSurface::name()) =>
            {
                Ok(self.create_surface_from_xlib(handle.display as *mut _, handle.window))
            }
            #[cfg(all(
                unix,
                not(target_os = "android"),
                not(target_os = "macos"),
                not(target_os = "ios")
            ))]
            RawWindowHandle::Xcb(handle) if self.extensions.contains(&khr::XcbSurface::name()) => {
                Ok(self.create_surface_from_xcb(handle.connection as *mut _, handle.window))
            }
            #[cfg(target_os = "android")]
            RawWindowHandle::Android(handle) => {
                Ok(self.create_surface_android(handle.a_native_window))
            }
            #[cfg(windows)]
            RawWindowHandle::Windows(handle) => {
                use winapi::um::libloaderapi::GetModuleHandleW;

                let hinstance = GetModuleHandleW(std::ptr::null());
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

    unsafe fn create_display_plane_surface(
        &self,
        display_plane: &hal::display::DisplayPlane<Backend>,
        plane_stack_index: u32,
        transformation: hal::display::SurfaceTransform,
        alpha: hal::display::DisplayPlaneAlpha,
        image_extent: hal::window::Extent2D,
    ) -> Result<window::Surface, hal::display::DisplayPlaneSurfaceError> {
        let display_extension = match &self.raw.display {
            Some(display_extension) => display_extension,
            None => {
                error!("Direct display feature not supported");
                return Err(display::DisplayPlaneSurfaceError::UnsupportedFeature);
            }
        };
        let surface_transform_flags = hal::display::SurfaceTransformFlags::from(transformation);
        let vk_surface_transform_flags =
            vk::SurfaceTransformFlagsKHR::from_raw(surface_transform_flags.bits());

        let display_surface_ci = {
            let builder = vk::DisplaySurfaceCreateInfoKHR::builder()
                .display_mode(display_plane.display_mode.handle.0)
                .plane_index(display_plane.plane.handle)
                .plane_stack_index(plane_stack_index)
                .image_extent(vk::Extent2D {
                    width: image_extent.width,
                    height: image_extent.height,
                })
                .transform(vk_surface_transform_flags);

            match alpha {
                hal::display::DisplayPlaneAlpha::Opaque => {
                    builder.alpha_mode(vk::DisplayPlaneAlphaFlagsKHR::OPAQUE)
                }
                hal::display::DisplayPlaneAlpha::Global(value) => builder
                    .alpha_mode(vk::DisplayPlaneAlphaFlagsKHR::GLOBAL)
                    .global_alpha(value),
                hal::display::DisplayPlaneAlpha::PerPixel => {
                    builder.alpha_mode(vk::DisplayPlaneAlphaFlagsKHR::PER_PIXEL)
                }
                hal::display::DisplayPlaneAlpha::PerPixelPremultiplied => {
                    builder.alpha_mode(vk::DisplayPlaneAlphaFlagsKHR::PER_PIXEL_PREMULTIPLIED)
                }
            }
            .build()
        };

        let surface = match display_extension
            .create_display_plane_surface(&display_surface_ci, None)
        {
            Ok(surface) => surface,
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => return Err(OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => return Err(OutOfMemory::Device.into()),
            err => panic!(
                "Unexpected error on `create_display_plane_surface`: {:#?}",
                err
            ),
        };

        Ok(self.create_surface_from_vk_surface_khr(surface))
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
    fn supports_sparse_binding(&self) -> bool {
        self.properties
            .queue_flags
            .contains(vk::QueueFlags::SPARSE_BINDING)
    }
}

struct DeviceExtensionFunctions {
    mesh_shaders: Option<ExtensionFn<MeshShader>>,
    draw_indirect_count: Option<ExtensionFn<khr::DrawIndirectCount>>,
    display_control: Option<vk::ExtDisplayControlFn>,
    memory_requirements2: Option<ExtensionFn<vk::KhrGetMemoryRequirements2Fn>>,
    // The extension does not have its own functions.
    dedicated_allocation: Option<ExtensionFn<()>>,
    // The extension does not have its own functions.
    external_memory: Option<ExtensionFn<()>>,
    external_memory_host: Option<vk::ExtExternalMemoryHostFn>,
    #[cfg(windows)]
    external_memory_win32: Option<vk::KhrExternalMemoryWin32Fn>,
    #[cfg(unix)]
    external_memory_fd: Option<khr::ExternalMemoryFd>,
    #[cfg(any(target_os = "linux", target_os = "android"))]
    // The extension does not have its own functions.
    external_memory_dma_buf: Option<()>,
    #[cfg(any(target_os = "linux", target_os = "android"))]
    image_drm_format_modifier: Option<vk::ExtImageDrmFormatModifierFn>,
}

// TODO there's no reason why this can't be unified--the function pointers should all be the same--it's not clear how to do this with `ash`.
enum ExtensionFn<T> {
    /// The loaded function pointer struct for an extension.
    Extension(T),
    /// The extension was promoted to a core version of Vulkan and the functions on `ash`'s `DeviceV1_x` traits should be used.
    Promoted,
}

impl<T> ExtensionFn<T> {
    /// Expect `self` to be `ExtensionFn::Extension` and return the inner value.
    fn unwrap_extension(&self) -> &T {
        if let ExtensionFn::Extension(t) = self {
            t
        } else {
            panic!()
        }
    }
}

#[doc(hidden)]
pub struct RawDevice {
    raw: ash::Device,
    handle_is_external: bool,
    features: Features,
    instance: Arc<RawInstance>,
    extension_fns: DeviceExtensionFunctions,
    /// The `hal::Features::NDC_Y_UP` flag is implemented with either `VK_AMD_negative_viewport_height` or `VK_KHR_maintenance1`/1.1+. The AMD extension for negative viewport height does not require a Y shift.
    ///
    /// This flag is `true` if the device has `VK_KHR_maintenance1`/1.1+ and `false` otherwise (i.e. in the case of `VK_AMD_negative_viewport_height`).
    flip_y_requires_shift: bool,
    imageless_framebuffers: bool,
    image_view_usage: bool,
    timestamp_period: f32,
}

impl fmt::Debug for RawDevice {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "RawDevice") // TODO: Real Debug impl
    }
}
impl Drop for RawDevice {
    fn drop(&mut self) {
        if !self.handle_is_external {
            unsafe {
                self.raw.destroy_device(None);
            }
        }
    }
}

impl RawDevice {
    fn debug_messenger(&self) -> Option<&DebugMessenger> {
        self.instance.debug_messenger.as_ref()
    }

    fn map_viewport(&self, rect: &hal::pso::Viewport) -> vk::Viewport {
        let flip_y = self.features.contains(hal::Features::NDC_Y_UP);
        let shift_y = flip_y && self.flip_y_requires_shift;
        conv::map_viewport(rect, flip_y, shift_y)
    }

    unsafe fn set_object_name(
        &self,
        object_type: vk::ObjectType,
        object: impl vk::Handle,
        name: &str,
    ) {
        let instance = &self.instance;
        if let Some(DebugMessenger::Utils(ref debug_utils_ext, _)) = instance.debug_messenger {
            // Keep variables outside the if-else block to ensure they do not
            // go out of scope while we hold a pointer to them
            let mut buffer: [u8; 64] = [0u8; 64];
            let buffer_vec: Vec<u8>;

            // Append a null terminator to the string
            let name_cstr = if name.len() < 64 {
                // Common case, string is very small. Allocate a copy on the stack.
                std::ptr::copy_nonoverlapping(name.as_ptr(), buffer.as_mut_ptr(), name.len());
                // Add null terminator
                buffer[name.len()] = 0;
                CStr::from_bytes_with_nul(&buffer[..name.len() + 1]).unwrap()
            } else {
                // Less common case, the string is large.
                // This requires a heap allocation.
                buffer_vec = name
                    .as_bytes()
                    .iter()
                    .cloned()
                    .chain(std::iter::once(0))
                    .collect::<Vec<u8>>();
                CStr::from_bytes_with_nul(&buffer_vec).unwrap()
            };
            let _result = debug_utils_ext.debug_utils_set_object_name(
                self.raw.handle(),
                &vk::DebugUtilsObjectNameInfoEXT::builder()
                    .object_type(object_type)
                    .object_handle(object.as_raw())
                    .object_name(name_cstr),
            );
        }
    }
}

// Need to explicitly synchronize on submission and present.
pub type RawCommandQueue = Arc<vk::Queue>;

pub struct Queue {
    raw: RawCommandQueue,
    device: Arc<RawDevice>,
    swapchain_fn: khr::Swapchain,
}

impl fmt::Debug for Queue {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Queue")
    }
}

impl queue::Queue<Backend> for Queue {
    unsafe fn submit<'a, Ic, Iw, Is>(
        &mut self,
        command_buffers: Ic,
        wait_semaphores: Iw,
        signal_semaphores: Is,
        fence: Option<&mut native::Fence>,
    ) where
        Ic: Iterator<Item = &'a command::CommandBuffer>,
        Iw: Iterator<Item = (&'a native::Semaphore, PipelineStage)>,
        Is: Iterator<Item = &'a native::Semaphore>,
    {
        //TODO: avoid heap allocations
        let mut waits = Vec::new();
        let mut stages = Vec::new();

        let buffers = command_buffers.map(|cmd| cmd.raw).collect::<Vec<_>>();
        for (semaphore, stage) in wait_semaphores {
            waits.push(semaphore.0);
            stages.push(conv::map_pipeline_stage(stage));
        }
        let signals = signal_semaphores
            .map(|semaphore| semaphore.0)
            .collect::<Vec<_>>();

        let mut info = vk::SubmitInfo::builder()
            .wait_semaphores(&waits)
            .command_buffers(&buffers)
            .signal_semaphores(&signals);
        // If count is zero, AMD driver crashes if nullptr is not set for stage masks
        if !stages.is_empty() {
            info = info.wait_dst_stage_mask(&stages);
        }

        let fence_raw = fence.map(|fence| fence.0).unwrap_or(vk::Fence::null());

        let result = self.device.raw.queue_submit(*self.raw, &[*info], fence_raw);
        if let Err(e) = result {
            error!("Submit resulted in {:?}", e);
        }
    }

    unsafe fn bind_sparse<'a, Iw, Is, Ibi, Ib, Iii, Io, Ii>(
        &mut self,
        wait_semaphores: Iw,
        signal_semaphores: Is,
        buffer_memory_binds: Ib,
        image_opaque_memory_binds: Io,
        image_memory_binds: Ii,
        device: &Device,
        fence: Option<&native::Fence>,
    ) where
        Ibi: Iterator<Item = &'a memory::SparseBind<&'a native::Memory>>,
        Ib: Iterator<Item = (&'a mut native::Buffer, Ibi)>,
        Iii: Iterator<Item = &'a memory::SparseImageBind<&'a native::Memory>>,
        Io: Iterator<Item = (&'a mut native::Image, Ibi)>,
        Ii: Iterator<Item = (&'a mut native::Image, Iii)>,
        Iw: Iterator<Item = &'a native::Semaphore>,
        Is: Iterator<Item = &'a native::Semaphore>,
    {
        //TODO: avoid heap allocations
        let mut waits = Vec::new();

        for semaphore in wait_semaphores {
            waits.push(semaphore.0);
        }
        let signals = signal_semaphores
            .map(|semaphore| semaphore.0)
            .collect::<Vec<_>>();

        let mut buffer_memory_binds_vec = Vec::new();
        let mut buffer_binds = buffer_memory_binds
            .map(|(buffer, bind_iter)| {
                let binds_before = buffer_memory_binds_vec.len();
                buffer_memory_binds_vec.extend(bind_iter.into_iter().map(|bind| {
                    let mut bind_builder = vk::SparseMemoryBind::builder()
                        .resource_offset(bind.resource_offset as u64)
                        .size(bind.size as u64);
                    if let Some((memory, memory_offset)) = bind.memory {
                        bind_builder = bind_builder.memory(memory.raw);
                        bind_builder = bind_builder.memory_offset(memory_offset as u64);
                    }

                    bind_builder.build()
                }));

                vk::SparseBufferMemoryBindInfo {
                    buffer: buffer.raw,
                    bind_count: (buffer_memory_binds_vec.len() - binds_before) as u32,
                    p_binds: std::ptr::null(),
                }
            })
            .collect::<Vec<_>>();
        // Set buffer bindings
        buffer_binds.iter_mut().fold(0u32, |idx, bind| {
            (*bind).p_binds = &buffer_memory_binds_vec[idx as usize];
            idx + bind.bind_count
        });

        let mut image_opaque_memory_binds_vec = Vec::new();
        let mut image_opaque_binds = image_opaque_memory_binds
            .map(|(image, bind_iter)| {
                let binds_before = image_opaque_memory_binds_vec.len();
                image_opaque_memory_binds_vec.extend(bind_iter.into_iter().map(|bind| {
                    let mut bind_builder = vk::SparseMemoryBind::builder()
                        .resource_offset(bind.resource_offset as u64)
                        .size(bind.size as u64);
                    if let Some((memory, memory_offset)) = bind.memory {
                        bind_builder = bind_builder.memory(memory.raw);
                        bind_builder = bind_builder.memory_offset(memory_offset as u64);
                    }

                    bind_builder.build()
                }));

                vk::SparseImageOpaqueMemoryBindInfo {
                    image: image.raw,
                    bind_count: (image_opaque_memory_binds_vec.len() - binds_before) as u32,
                    p_binds: std::ptr::null(),
                }
            })
            .collect::<Vec<_>>();
        // Set opaque image bindings
        image_opaque_binds.iter_mut().fold(0u32, |idx, bind| {
            (*bind).p_binds = &image_opaque_memory_binds_vec[idx as usize];
            idx + bind.bind_count
        });

        let mut image_memory_binds_vec = Vec::new();
        let mut image_binds = image_memory_binds
            .map(|(image, bind_iter)| {
                let binds_before = image_memory_binds_vec.len();
                image_memory_binds_vec.extend(bind_iter.into_iter().map(|bind| {
                    let mut bind_builder = vk::SparseImageMemoryBind::builder()
                        .subresource(conv::map_subresource(&bind.subresource))
                        .offset(conv::map_offset(bind.offset))
                        .extent(conv::map_extent(bind.extent));
                    if let Some((memory, memory_offset)) = bind.memory {
                        bind_builder = bind_builder.memory(memory.raw);
                        bind_builder = bind_builder.memory_offset(memory_offset as u64);
                    }

                    bind_builder.build()
                }));

                vk::SparseImageMemoryBindInfo {
                    image: image.raw,
                    bind_count: (image_memory_binds_vec.len() - binds_before) as u32,
                    p_binds: std::ptr::null(),
                }
            })
            .collect::<Vec<_>>();
        // Set image bindings
        image_binds.iter_mut().fold(0u32, |idx, bind| {
            (*bind).p_binds = &image_memory_binds_vec[idx as usize];
            idx + bind.bind_count
        });

        let info = vk::BindSparseInfo::builder()
            .wait_semaphores(&waits)
            .signal_semaphores(&signals)
            .buffer_binds(&buffer_binds)
            .image_opaque_binds(&image_opaque_binds)
            .image_binds(&image_binds);

        let info = info.build();
        let fence_raw = fence.map(|fence| fence.0).unwrap_or(vk::Fence::null());

        // TODO temporary hack as method is not yet exposed, https://github.com/MaikKlein/ash/issues/342
        assert_eq!(
            vk::Result::SUCCESS,
            device
                .shared
                .raw
                .fp_v1_0()
                .queue_bind_sparse(*self.raw, 1, &info, fence_raw)
        );
    }

    unsafe fn present(
        &mut self,
        surface: &mut window::Surface,
        image: window::SurfaceImage,
        wait_semaphore: Option<&mut native::Semaphore>,
    ) -> Result<Option<Suboptimal>, PresentError> {
        let ssc = surface.swapchain.as_ref().unwrap();
        let wait_semaphore = if let Some(wait_semaphore) = wait_semaphore {
            wait_semaphore.0
        } else {
            let signals = &[ssc.semaphore.0];
            let submit_info = vk::SubmitInfo::builder().signal_semaphores(signals);
            self.device
                .raw
                .queue_submit(*self.raw, &[*submit_info], vk::Fence::null())
                .unwrap();
            ssc.semaphore.0
        };

        let wait_semaphores = &[wait_semaphore];
        let swapchains = &[ssc.swapchain.raw];
        let image_indices = &[image.index];
        let present_info = vk::PresentInfoKHR::builder()
            .wait_semaphores(wait_semaphores)
            .swapchains(swapchains)
            .image_indices(image_indices);

        match self.swapchain_fn.queue_present(*self.raw, &present_info) {
            Ok(false) => Ok(None),
            Ok(true) => Ok(Some(Suboptimal)),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(OutOfMemory::Host.into()),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(OutOfMemory::Device.into()),
            Err(vk::Result::ERROR_DEVICE_LOST) => Err(DeviceLost.into()),
            Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => Err(OutOfDate.into()),
            Err(vk::Result::ERROR_SURFACE_LOST_KHR) => Err(SurfaceLost.into()),
            _ => panic!("Failed to present frame"),
        }
    }

    fn wait_idle(&mut self) -> Result<(), OutOfMemory> {
        match unsafe { self.device.raw.queue_wait_idle(*self.raw) } {
            Ok(()) => Ok(()),
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(OutOfMemory::Host),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(OutOfMemory::Device),
            Err(_) => unreachable!(),
        }
    }

    fn timestamp_period(&self) -> f32 {
        self.device.timestamp_period
    }
}

#[derive(Debug)]
pub struct Device {
    shared: Arc<RawDevice>,
    vendor_id: u32,
    valid_ash_memory_types: u32,
    render_doc: gfx_renderdoc::RenderDoc,
    #[cfg(feature = "naga")]
    naga_options: naga::back::spv::Options,
}

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
pub enum Backend {}
impl hal::Backend for Backend {
    type Instance = Instance;
    type PhysicalDevice = PhysicalDevice;
    type Device = Device;
    type Surface = window::Surface;

    type QueueFamily = QueueFamily;
    type Queue = Queue;
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

    type Display = native::Display;
    type DisplayMode = native::DisplayMode;
}
