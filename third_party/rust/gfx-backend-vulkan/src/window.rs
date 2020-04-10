use std::{
    borrow::Borrow,
    fmt,
    hash,
    os::raw::c_void,
    ptr,
    sync::{Arc, Mutex},
    time::Instant,
};

use ash::{extensions::khr, version::DeviceV1_0 as _, vk};
use hal::{format::Format, window as w};
use smallvec::SmallVec;

use crate::{conv, native};
use crate::{
    Backend,
    Device,
    Instance,
    PhysicalDevice,
    QueueFamily,
    RawDevice,
    RawInstance,
    VK_ENTRY,
};


#[derive(Debug, Default)]
pub struct FramebufferCache {
    // We expect exactly one framebuffer per frame, but can support more.
    pub framebuffers: SmallVec<[vk::Framebuffer; 1]>,
}

#[derive(Debug, Default)]
pub struct FramebufferCachePtr(pub Arc<Mutex<FramebufferCache>>);

impl hash::Hash for FramebufferCachePtr {
    fn hash<H: hash::Hasher>(&self, hasher: &mut H) {
        (self.0.as_ref() as *const Mutex<FramebufferCache>).hash(hasher)
    }
}
impl PartialEq for FramebufferCachePtr {
    fn eq(&self, other: &Self) -> bool {
        Arc::ptr_eq(&self.0, &other.0)
    }
}
impl Eq for FramebufferCachePtr {}

#[derive(Debug)]
struct SurfaceFrame {
    image: vk::Image,
    view: vk::ImageView,
    framebuffers: FramebufferCachePtr,
}

#[derive(Debug)]
pub struct SurfaceSwapchain {
    pub(crate) swapchain: Swapchain,
    device: Arc<RawDevice>,
    fence: native::Fence,
    pub(crate) semaphore: native::Semaphore,
    frames: Vec<SurfaceFrame>,
}

impl SurfaceSwapchain {
    unsafe fn release_resources(self, device: &ash::Device) -> Swapchain {
        let _ = device.device_wait_idle();
        device.destroy_fence(self.fence.0, None);
        device.destroy_semaphore(self.semaphore.0, None);
        for frame in self.frames {
            device.destroy_image_view(frame.view, None);
            for framebuffer in frame.framebuffers.0.lock().unwrap().framebuffers.drain() {
                device.destroy_framebuffer(framebuffer, None);
            }
        }
        self.swapchain
    }
}

pub struct Surface {
    // Vk (EXT) specs [29.2.7 Platform-Independent Information]
    // For vkDestroySurfaceKHR: Host access to surface must be externally synchronized
    pub(crate) raw: Arc<RawSurface>,
    pub(crate) swapchain: Option<SurfaceSwapchain>,
}

impl fmt::Debug for Surface {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Surface")
    }
}

pub struct RawSurface {
    pub(crate) handle: vk::SurfaceKHR,
    pub(crate) functor: khr::Surface,
    pub(crate) instance: Arc<RawInstance>,
}

impl Instance {
    #[cfg(all(
        feature = "x11",
        unix,
        not(target_os = "android"),
        not(target_os = "macos")
    ))]
    pub fn create_surface_from_xlib(&self, dpy: *mut vk::Display, window: vk::Window) -> Surface {
        let entry = VK_ENTRY
            .as_ref()
            .expect("Unable to load Vulkan entry points");

        if !self.extensions.contains(&khr::XlibSurface::name()) {
            panic!("Vulkan driver does not support VK_KHR_XLIB_SURFACE");
        }

        let surface = {
            let xlib_loader = khr::XlibSurface::new(entry, &self.raw.0);
            let info = vk::XlibSurfaceCreateInfoKHR {
                s_type: vk::StructureType::XLIB_SURFACE_CREATE_INFO_KHR,
                p_next: ptr::null(),
                flags: vk::XlibSurfaceCreateFlagsKHR::empty(),
                window,
                dpy,
            };

            unsafe { xlib_loader.create_xlib_surface(&info, None) }
                .expect("XlibSurface::create_xlib_surface() failed")
        };

        self.create_surface_from_vk_surface_khr(surface)
    }

    #[cfg(all(
        feature = "xcb",
        unix,
        not(target_os = "android"),
        not(target_os = "macos")
    ))]
    pub fn create_surface_from_xcb(
        &self,
        connection: *mut vk::xcb_connection_t,
        window: vk::xcb_window_t,
    ) -> Surface {
        let entry = VK_ENTRY
            .as_ref()
            .expect("Unable to load Vulkan entry points");

        if !self.extensions.contains(&khr::XcbSurface::name()) {
            panic!("Vulkan driver does not support VK_KHR_XCB_SURFACE");
        }

        let surface = {
            let xcb_loader = khr::XcbSurface::new(entry, &self.raw.0);
            let info = vk::XcbSurfaceCreateInfoKHR {
                s_type: vk::StructureType::XCB_SURFACE_CREATE_INFO_KHR,
                p_next: ptr::null(),
                flags: vk::XcbSurfaceCreateFlagsKHR::empty(),
                window,
                connection,
            };

            unsafe { xcb_loader.create_xcb_surface(&info, None) }
                .expect("XcbSurface::create_xcb_surface() failed")
        };

        self.create_surface_from_vk_surface_khr(surface)
    }

    #[cfg(all(unix, not(target_os = "android")))]
    pub fn create_surface_from_wayland(
        &self,
        display: *mut c_void,
        surface: *mut c_void,
    ) -> Surface {
        let entry = VK_ENTRY
            .as_ref()
            .expect("Unable to load Vulkan entry points");

        if !self.extensions.contains(&khr::WaylandSurface::name()) {
            panic!("Vulkan driver does not support VK_KHR_WAYLAND_SURFACE");
        }

        let surface = {
            let w_loader = khr::WaylandSurface::new(entry, &self.raw.0);
            let info = vk::WaylandSurfaceCreateInfoKHR {
                s_type: vk::StructureType::WAYLAND_SURFACE_CREATE_INFO_KHR,
                p_next: ptr::null(),
                flags: vk::WaylandSurfaceCreateFlagsKHR::empty(),
                display: display as *mut _,
                surface: surface as *mut _,
            };

            unsafe { w_loader.create_wayland_surface(&info, None) }.expect("WaylandSurface failed")
        };

        self.create_surface_from_vk_surface_khr(surface)
    }

    #[cfg(target_os = "android")]
    pub fn create_surface_android(&self, window: *const c_void) -> Surface {
        let entry = VK_ENTRY
            .as_ref()
            .expect("Unable to load Vulkan entry points");

        let surface = {
            let loader = khr::AndroidSurface::new(entry, &self.raw.0);
            let info = vk::AndroidSurfaceCreateInfoKHR {
                s_type: vk::StructureType::ANDROID_SURFACE_CREATE_INFO_KHR,
                p_next: ptr::null(),
                flags: vk::AndroidSurfaceCreateFlagsKHR::empty(),
                window: window as *const _ as *mut _,
            };

            unsafe { loader.create_android_surface(&info, None) }.expect("AndroidSurface failed")
        };

        self.create_surface_from_vk_surface_khr(surface)
    }

    #[cfg(windows)]
    pub fn create_surface_from_hwnd(&self, hinstance: *mut c_void, hwnd: *mut c_void) -> Surface {
        let entry = VK_ENTRY
            .as_ref()
            .expect("Unable to load Vulkan entry points");

        if !self.extensions.contains(&khr::Win32Surface::name()) {
            panic!("Vulkan driver does not support VK_KHR_WIN32_SURFACE");
        }

        let surface = {
            let info = vk::Win32SurfaceCreateInfoKHR {
                s_type: vk::StructureType::WIN32_SURFACE_CREATE_INFO_KHR,
                p_next: ptr::null(),
                flags: vk::Win32SurfaceCreateFlagsKHR::empty(),
                hinstance: hinstance as *mut _,
                hwnd: hwnd as *mut _,
            };
            let win32_loader = khr::Win32Surface::new(entry, &self.raw.0);
            unsafe {
                win32_loader
                    .create_win32_surface(&info, None)
                    .expect("Unable to create Win32 surface")
            }
        };

        self.create_surface_from_vk_surface_khr(surface)
    }

    #[cfg(target_os = "macos")]
    pub fn create_surface_from_ns_view(&self, view: *mut c_void) -> Surface {
        use ash::extensions::mvk;
        use core_graphics::{base::CGFloat, geometry::CGRect};
        use objc::runtime::{Object, BOOL, YES};

        // TODO: this logic is duplicated from gfx-backend-metal, refactor?
        unsafe {
            let view = view as *mut Object;
            let existing: *mut Object = msg_send![view, layer];
            let class = class!(CAMetalLayer);

            let use_current = if existing.is_null() {
                false
            } else {
                let result: BOOL = msg_send![existing, isKindOfClass: class];
                result == YES
            };

            if !use_current {
                let layer: *mut Object = msg_send![class, new];
                let () = msg_send![view, setLayer: layer];
                let bounds: CGRect = msg_send![view, bounds];
                let () = msg_send![layer, setBounds: bounds];

                let window: *mut Object = msg_send![view, window];
                if !window.is_null() {
                    let scale_factor: CGFloat = msg_send![window, backingScaleFactor];
                    let () = msg_send![layer, setContentsScale: scale_factor];
                }
            }
        }

        let entry = VK_ENTRY
            .as_ref()
            .expect("Unable to load Vulkan entry points");

        if !self.extensions.contains(&mvk::MacOSSurface::name()) {
            panic!("Vulkan driver does not support VK_MVK_MACOS_SURFACE");
        }

        let surface = {
            let mac_os_loader = mvk::MacOSSurface::new(entry, &self.raw.0);
            let info = vk::MacOSSurfaceCreateInfoMVK {
                s_type: vk::StructureType::MACOS_SURFACE_CREATE_INFO_M,
                p_next: ptr::null(),
                flags: vk::MacOSSurfaceCreateFlagsMVK::empty(),
                p_view: view,
            };

            unsafe {
                mac_os_loader
                    .create_mac_os_surface_mvk(&info, None)
                    .expect("Unable to create macOS surface")
            }
        };

        self.create_surface_from_vk_surface_khr(surface)
    }

    pub fn create_surface_from_vk_surface_khr(&self, surface: vk::SurfaceKHR) -> Surface {
        let entry = VK_ENTRY
            .as_ref()
            .expect("Unable to load Vulkan entry points");

        let functor = khr::Surface::new(entry, &self.raw.0);

        let raw = Arc::new(RawSurface {
            handle: surface,
            functor,
            instance: self.raw.clone(),
        });

        Surface {
            raw,
            swapchain: None,
        }
    }
}

impl w::Surface<Backend> for Surface {
    fn supports_queue_family(&self, queue_family: &QueueFamily) -> bool {
        unsafe {
            self.raw.functor.get_physical_device_surface_support(
                queue_family.device,
                queue_family.index,
                self.raw.handle,
            )
        }
    }

    fn capabilities(&self, physical_device: &PhysicalDevice) -> w::SurfaceCapabilities {
        // Capabilities
        let caps = unsafe {
            self.raw
                .functor
                .get_physical_device_surface_capabilities(physical_device.handle, self.raw.handle)
        }
        .expect("Unable to query surface capabilities");

        // If image count is 0, the support number of images is unlimited.
        let max_images = if caps.max_image_count == 0 {
            !0
        } else {
            caps.max_image_count
        };

        // `0xFFFFFFFF` indicates that the extent depends on the created swapchain.
        let current_extent = if caps.current_extent.width != !0 && caps.current_extent.height != !0
        {
            Some(w::Extent2D {
                width: caps.current_extent.width,
                height: caps.current_extent.height,
            })
        } else {
            None
        };

        let min_extent = w::Extent2D {
            width: caps.min_image_extent.width,
            height: caps.min_image_extent.height,
        };

        let max_extent = w::Extent2D {
            width: caps.max_image_extent.width,
            height: caps.max_image_extent.height,
        };

        let raw_present_modes = unsafe {
            self.raw
                .functor
                .get_physical_device_surface_present_modes(physical_device.handle, self.raw.handle)
        }
        .expect("Unable to query present modes");

        w::SurfaceCapabilities {
            present_modes: raw_present_modes
                .into_iter()
                .fold(w::PresentMode::empty(), |u, m| { u | conv::map_vk_present_mode(m) }),
            composite_alpha_modes: conv::map_vk_composite_alpha(caps.supported_composite_alpha),
            image_count: caps.min_image_count ..= max_images,
            current_extent,
            extents: min_extent ..= max_extent,
            max_image_layers: caps.max_image_array_layers as _,
            usage: conv::map_vk_image_usage(caps.supported_usage_flags),
        }
    }

    fn supported_formats(&self, physical_device: &PhysicalDevice) -> Option<Vec<Format>> {
        // Swapchain formats
        let raw_formats = unsafe {
            self.raw
                .functor
                .get_physical_device_surface_formats(physical_device.handle, self.raw.handle)
        }
        .expect("Unable to query surface formats");

        match raw_formats[0].format {
            // If pSurfaceFormats includes just one entry, whose value for format is
            // VK_FORMAT_UNDEFINED, surface has no preferred format. In this case, the application
            // can use any valid VkFormat value.
            vk::Format::UNDEFINED => None,
            _ => Some(
                raw_formats
                    .into_iter()
                    .filter_map(|sf| conv::map_vk_format(sf.format))
                    .collect(),
            ),
        }
    }
}

#[derive(Debug)]
pub struct SurfaceImage {
    pub(crate) index: w::SwapImageIndex,
    view: native::ImageView,
}

impl Borrow<native::ImageView> for SurfaceImage {
    fn borrow(&self) -> &native::ImageView {
        &self.view
    }
}

impl w::PresentationSurface<Backend> for Surface {
    type SwapchainImage = SurfaceImage;

    unsafe fn configure_swapchain(
        &mut self,
        device: &Device,
        config: w::SwapchainConfig,
    ) -> Result<(), w::CreationError> {
        use hal::device::Device as _;

        let format = config.format;
        let old = self
            .swapchain
            .take()
            .map(|ssc| ssc.release_resources(&device.raw.0));

        let (swapchain, images) = device.create_swapchain(self, config, old)?;

        self.swapchain = Some(SurfaceSwapchain {
            swapchain,
            device: Arc::clone(&device.raw),
            fence: device.create_fence(false).unwrap(),
            semaphore: device.create_semaphore().unwrap(),
            frames: images
                .iter()
                .map(|image| {
                    let view = device
                        .create_image_view(
                            image,
                            hal::image::ViewKind::D2,
                            format,
                            hal::format::Swizzle::NO,
                            hal::image::SubresourceRange {
                                aspects: hal::format::Aspects::COLOR,
                                layers: 0 .. 1,
                                levels: 0 .. 1,
                            },
                        )
                        .unwrap();
                    SurfaceFrame {
                        image: view.image,
                        view: view.view,
                        framebuffers: Default::default(),
                    }
                })
                .collect(),
        });

        Ok(())
    }

    unsafe fn unconfigure_swapchain(&mut self, device: &Device) {
        if let Some(ssc) = self.swapchain.take() {
            let swapchain = ssc.release_resources(&device.raw.0);
            swapchain.functor.destroy_swapchain(swapchain.raw, None);
        }
    }

    unsafe fn acquire_image(
        &mut self,
        mut timeout_ns: u64,
    ) -> Result<(Self::SwapchainImage, Option<w::Suboptimal>), w::AcquireError> {
        use hal::window::Swapchain as _;

        let ssc = self.swapchain.as_mut().unwrap();
        let moment = Instant::now();
        let (index, suboptimal) =
            ssc.swapchain
                .acquire_image(timeout_ns, None, Some(&ssc.fence))?;
        timeout_ns = timeout_ns.saturating_sub(moment.elapsed().as_nanos() as u64);
        let fences = &[ssc.fence.0];

        match ssc.device.0.wait_for_fences(fences, true, timeout_ns) {
            Ok(()) => {
                ssc.device.0.reset_fences(fences).unwrap();
                let frame = &ssc.frames[index as usize];
                // We have just waited for the frame to be fully available on CPU.
                // All the associated framebuffers are expected to be destroyed by now.
                for framebuffer in frame.framebuffers.0.lock().unwrap().framebuffers.drain() {
                    ssc.device.0.destroy_framebuffer(framebuffer, None);
                }
                let image = Self::SwapchainImage {
                    index,
                    view: native::ImageView {
                        image: frame.image,
                        view: frame.view,
                        range: hal::image::SubresourceRange {
                            aspects: hal::format::Aspects::COLOR,
                            layers: 0 .. 1,
                            levels: 0 .. 1,
                        },
                        owner: native::ImageViewOwner::Surface(FramebufferCachePtr(Arc::clone(
                            &frame.framebuffers.0,
                        ))),
                    },
                };
                Ok((image, suboptimal))
            }
            Err(vk::Result::NOT_READY) => Err(w::AcquireError::NotReady),
            Err(vk::Result::TIMEOUT) => Err(w::AcquireError::Timeout),
            Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => Err(w::AcquireError::OutOfDate),
            Err(vk::Result::ERROR_SURFACE_LOST_KHR) => {
                Err(w::AcquireError::SurfaceLost(hal::device::SurfaceLost))
            }
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(w::AcquireError::OutOfMemory(
                hal::device::OutOfMemory::Host,
            )),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(w::AcquireError::OutOfMemory(
                hal::device::OutOfMemory::Device,
            )),
            Err(vk::Result::ERROR_DEVICE_LOST) => {
                Err(w::AcquireError::DeviceLost(hal::device::DeviceLost))
            }
            _ => unreachable!(),
        }
    }
}

pub struct Swapchain {
    pub(crate) raw: vk::SwapchainKHR,
    pub(crate) functor: khr::Swapchain,
}

impl fmt::Debug for Swapchain {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_str("Swapchain")
    }
}

impl w::Swapchain<Backend> for Swapchain {
    unsafe fn acquire_image(
        &mut self,
        timeout_ns: u64,
        semaphore: Option<&native::Semaphore>,
        fence: Option<&native::Fence>,
    ) -> Result<(w::SwapImageIndex, Option<w::Suboptimal>), w::AcquireError> {
        let semaphore = semaphore.map_or(vk::Semaphore::null(), |s| s.0);
        let fence = fence.map_or(vk::Fence::null(), |f| f.0);

        // will block if no image is available
        let index = self
            .functor
            .acquire_next_image(self.raw, timeout_ns, semaphore, fence);

        match index {
            Ok((i, true)) => Ok((i, Some(w::Suboptimal))),
            Ok((i, false)) => Ok((i, None)),
            Err(vk::Result::NOT_READY) => Err(w::AcquireError::NotReady),
            Err(vk::Result::TIMEOUT) => Err(w::AcquireError::Timeout),
            Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => Err(w::AcquireError::OutOfDate),
            Err(vk::Result::ERROR_SURFACE_LOST_KHR) => {
                Err(w::AcquireError::SurfaceLost(hal::device::SurfaceLost))
            }
            Err(vk::Result::ERROR_OUT_OF_HOST_MEMORY) => Err(w::AcquireError::OutOfMemory(
                hal::device::OutOfMemory::Host,
            )),
            Err(vk::Result::ERROR_OUT_OF_DEVICE_MEMORY) => Err(w::AcquireError::OutOfMemory(
                hal::device::OutOfMemory::Device,
            )),
            Err(vk::Result::ERROR_DEVICE_LOST) => {
                Err(w::AcquireError::DeviceLost(hal::device::DeviceLost))
            }
            _ => panic!("Failed to acquire image."),
        }
    }
}
