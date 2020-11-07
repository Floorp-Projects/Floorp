use crate::{
    device::{Device, PhysicalDevice},
    native, Backend, QueueFamily, Shared,
};

use hal::{format, image, window as w};

use crate::CGRect;
use foreign_types::ForeignType;
use metal::{CGFloat, CGSize, CoreAnimationDrawable};
use objc::rc::autoreleasepool;
use objc::runtime::Object;
use parking_lot::Mutex;

use std::borrow::Borrow;
use std::ptr::NonNull;
use std::sync::Arc;
use std::thread;

#[derive(Debug)]
pub struct Surface {
    inner: Arc<SurfaceInner>,
    swapchain_format: metal::MTLPixelFormat,
    main_thread_id: thread::ThreadId,
}

#[derive(Debug)]
pub(crate) struct SurfaceInner {
    view: Option<NonNull<Object>>,
    render_layer: Mutex<metal::CoreAnimationLayer>,
}

unsafe impl Send for SurfaceInner {}
unsafe impl Sync for SurfaceInner {}

impl Drop for SurfaceInner {
    fn drop(&mut self) {
        if let Some(view) = self.view {
            let () = unsafe { msg_send![view.as_ptr(), release] };
        }
    }
}

#[derive(Debug)]
struct FrameNotFound {
    drawable: metal::Drawable,
    texture: metal::Texture,
}

impl SurfaceInner {
    pub fn new(view: Option<NonNull<Object>>, layer: metal::CoreAnimationLayer) -> Self {
        SurfaceInner {
            view,
            render_layer: Mutex::new(layer),
        }
    }

    pub fn into_surface(self) -> Surface {
        Surface {
            inner: Arc::new(self),
            swapchain_format: metal::MTLPixelFormat::Invalid,
            main_thread_id: thread::current().id(),
        }
    }

    fn configure(&self, shared: &Shared, config: &w::SwapchainConfig) -> metal::MTLPixelFormat {
        info!("build swapchain {:?}", config);

        let caps = &shared.private_caps;
        let mtl_format = caps
            .map_format(config.format)
            .expect("unsupported backbuffer format");

        let render_layer_borrow = self.render_layer.lock();
        let render_layer = render_layer_borrow.as_ref();
        let framebuffer_only = config.image_usage == image::Usage::COLOR_ATTACHMENT;
        let display_sync = config.present_mode != w::PresentMode::IMMEDIATE;
        let is_mac = caps.os_is_mac;
        let can_set_next_drawable_timeout = if is_mac {
            caps.has_version_at_least(10, 13)
        } else {
            caps.has_version_at_least(11, 0)
        };
        let can_set_display_sync = is_mac && caps.has_version_at_least(10, 13);
        let drawable_size = CGSize::new(config.extent.width as f64, config.extent.height as f64);

        let device_raw = shared.device.lock().as_ptr();
        unsafe {
            // On iOS, unless the user supplies a view with a CAMetalLayer, we
            // create one as a sublayer. However, when the view changes size,
            // its sublayers are not automatically resized, and we must resize
            // it here. The drawable size and the layer size don't correlate
            #[cfg(target_os = "ios")]
            {
                if let Some(view) = self.view {
                    let main_layer: *mut Object = msg_send![view.as_ptr(), layer];
                    let bounds: CGRect = msg_send![main_layer, bounds];
                    let () = msg_send![render_layer, setFrame: bounds];
                }
            }
            let () = msg_send![render_layer, setDevice: device_raw];
            let () = msg_send![render_layer, setPixelFormat: mtl_format];
            let () = msg_send![render_layer, setFramebufferOnly: framebuffer_only];

            // this gets ignored on iOS for certain OS/device combinations (iphone5s iOS 10.3)
            let () = msg_send![render_layer, setMaximumDrawableCount: config.image_count as u64];

            let () = msg_send![render_layer, setDrawableSize: drawable_size];
            if can_set_next_drawable_timeout {
                let () = msg_send![render_layer, setAllowsNextDrawableTimeout:false];
            }
            if can_set_display_sync {
                let () = msg_send![render_layer, setDisplaySyncEnabled: display_sync];
            }
        };

        mtl_format
    }

    fn dimensions(&self) -> w::Extent2D {
        let (size, scale): (CGSize, CGFloat) = match self.view {
            Some(view) if !cfg!(target_os = "macos") => unsafe {
                let bounds: CGRect = msg_send![view.as_ptr(), bounds];
                let window: Option<NonNull<Object>> = msg_send![view.as_ptr(), window];
                let screen = window.and_then(|window| -> Option<NonNull<Object>> {
                    msg_send![window.as_ptr(), screen]
                });
                match screen {
                    Some(screen) => {
                        let screen_space: *mut Object = msg_send![screen.as_ptr(), coordinateSpace];
                        let rect: CGRect = msg_send![view.as_ptr(), convertRect:bounds toCoordinateSpace:screen_space];
                        let scale_factor: CGFloat = msg_send![screen.as_ptr(), nativeScale];
                        (rect.size, scale_factor)
                    }
                    None => (bounds.size, 1.0),
                }
            },
            _ => unsafe {
                let render_layer_borrow = self.render_layer.lock();
                let render_layer = render_layer_borrow.as_ref();
                let bounds: CGRect = msg_send![render_layer, bounds];
                let contents_scale: CGFloat = msg_send![render_layer, contentsScale];
                (bounds.size, contents_scale)
            },
        };
        w::Extent2D {
            width: (size.width * scale) as u32,
            height: (size.height * scale) as u32,
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum AcquireMode {
    Wait,
    Oldest,
}

impl Default for AcquireMode {
    fn default() -> Self {
        AcquireMode::Oldest
    }
}

#[derive(Debug)]
pub struct SwapchainImage {
    surface: Arc<SurfaceInner>,
    index: w::SwapImageIndex,
}

#[derive(Debug)]
pub struct SurfaceImage {
    view: native::ImageView,
    drawable: metal::CoreAnimationDrawable,
}

unsafe impl Send for SurfaceImage {}
unsafe impl Sync for SurfaceImage {}

impl SurfaceImage {
    pub(crate) fn into_drawable(self) -> CoreAnimationDrawable {
        self.drawable
    }
}

impl Borrow<native::ImageView> for SurfaceImage {
    fn borrow(&self) -> &native::ImageView {
        &self.view
    }
}

impl w::Surface<Backend> for Surface {
    fn supports_queue_family(&self, _queue_family: &QueueFamily) -> bool {
        // we only expose one family atm, so it's compatible
        true
    }

    fn capabilities(&self, physical_device: &PhysicalDevice) -> w::SurfaceCapabilities {
        let current_extent = if self.main_thread_id == thread::current().id() {
            Some(self.inner.dimensions())
        } else {
            warn!("Unable to get the current view dimensions on a non-main thread");
            None
        };

        let device_caps = &physical_device.shared.private_caps;

        let can_set_maximum_drawables_count =
            device_caps.os_is_mac || device_caps.has_version_at_least(11, 2);
        let can_set_display_sync =
            device_caps.os_is_mac && device_caps.has_version_at_least(10, 13);

        w::SurfaceCapabilities {
            present_modes: if can_set_display_sync {
                w::PresentMode::FIFO | w::PresentMode::IMMEDIATE
            } else {
                w::PresentMode::FIFO
            },
            composite_alpha_modes: w::CompositeAlphaMode::OPAQUE, //TODO
            //Note: this is hardcoded in `CAMetalLayer` documentation
            image_count: if can_set_maximum_drawables_count {
                2..=3
            } else {
                // 3 is the default in `CAMetalLayer` documentation
                // iOS 10.3 was tested to use 3 on iphone5s
                3..=3
            },
            current_extent,
            extents: w::Extent2D {
                width: 4,
                height: 4,
            }..=w::Extent2D {
                width: 4096,
                height: 4096,
            },
            max_image_layers: 1,
            usage: image::Usage::COLOR_ATTACHMENT
                | image::Usage::SAMPLED
                | image::Usage::TRANSFER_SRC
                | image::Usage::TRANSFER_DST,
        }
    }

    fn supported_formats(&self, _physical_device: &PhysicalDevice) -> Option<Vec<format::Format>> {
        Some(vec![
            format::Format::Bgra8Unorm,
            format::Format::Bgra8Srgb,
            format::Format::Rgba16Sfloat,
        ])
    }
}

impl w::PresentationSurface<Backend> for Surface {
    type SwapchainImage = SurfaceImage;

    unsafe fn configure_swapchain(
        &mut self,
        device: &Device,
        config: w::SwapchainConfig,
    ) -> Result<(), w::CreationError> {
        assert!(image::Usage::COLOR_ATTACHMENT.contains(config.image_usage));
        self.swapchain_format = self.inner.configure(&device.shared, &config);
        Ok(())
    }

    unsafe fn unconfigure_swapchain(&mut self, _device: &Device) {
        self.swapchain_format = metal::MTLPixelFormat::Invalid;
    }

    unsafe fn acquire_image(
        &mut self,
        _timeout_ns: u64, //TODO: use the timeout
    ) -> Result<(Self::SwapchainImage, Option<w::Suboptimal>), w::AcquireError> {
        let render_layer_borrow = self.inner.render_layer.lock();
        let (drawable, texture) = autoreleasepool(|| {
            let drawable = render_layer_borrow.next_drawable().unwrap();
            (drawable.to_owned(), drawable.texture().to_owned())
        });

        let image = SurfaceImage {
            view: native::ImageView {
                texture,
                mtl_format: self.swapchain_format,
            },
            drawable,
        };
        Ok((image, None))
    }
}
