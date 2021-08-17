//! Windowing system interoperability.
//!
//! Screen presentation (fullscreen or window) of images requires two objects:
//!
//! * [Surface][Surface] is an abstraction of a native screen or window, for graphics use.
//!     It hosts a chain of multiple images, which can be presented on a surface ("swapchain").
//!
//! ## Window
//!
//! `gfx-hal` does not provide any methods for creating a native window or screen.
//! This is handled exeternally, either by managing your own window or by using a
//! library such as [winit](https://github.com/rust-windowing/winit), and providing
//! the [raw window handle](https://github.com/rust-windowing/raw-window-handle).
//!
//! ## Surface
//!
//! Once you have a window handle, you need to [create a surface][crate::Instance::create_surface]
//! compatible with the [instance][crate::Instance] of the graphics API you currently use.
//!
//! ## PresentationSurface
//!
//! A surface has an implicit swapchain in it.
//!
//! The most interesting part of a swapchain are the contained presentable images/backbuffers.
//! Presentable images are specialized images, which can be presented on the screen. They are
//! 2D color images with optionally associated depth-stencil images.
//!
//! The common steps for presentation of a frame are acquisition and presentation:
//!
//! ```no_run
//! # extern crate gfx_backend_empty as empty;
//! # extern crate gfx_hal;
//! # fn main() {
//! # use gfx_hal::prelude::*;
//!
//! # let mut surface: empty::Surface = return;
//! # let device: empty::Device = return;
//! # let mut present_queue: empty::Queue = return;
//! # unsafe {
//! let mut render_semaphore = device.create_semaphore().unwrap();
//!
//! let (frame, suboptimal) = surface.acquire_image(!0).unwrap();
//! // render the scene..
//! // `render_semaphore` will be signalled once rendering has been finished
//! present_queue.present(&mut surface, frame, Some(&mut render_semaphore));
//! # }}
//! ```
//!
//! Queues need to synchronize with the presentation engine, usually done via signalling a semaphore
//! once a frame is available for rendering and waiting on a separate semaphore until scene rendering
//! has finished.
//!
//! ### Recreation
//!
//! DOC TODO

use crate::{device, format::Format, image, Backend};

use std::{
    any::Any,
    borrow::Borrow,
    cmp::{max, min},
    fmt,
    ops::RangeInclusive,
};

/// Default image usage for the swapchain.
pub const DEFAULT_USAGE: image::Usage = image::Usage::COLOR_ATTACHMENT;
/// Default image count for the swapchain.
pub const DEFAULT_IMAGE_COUNT: SwapImageIndex = 3;

/// Error occurred caused surface to be lost.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
#[error("Surface lost")]
pub struct SurfaceLost;

/// Error occurred during swapchain configuration.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum SwapchainError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] device::OutOfMemory),
    /// Device is lost
    #[error(transparent)]
    DeviceLost(#[from] device::DeviceLost),
    /// Surface is lost
    #[error(transparent)]
    SurfaceLost(#[from] SurfaceLost),
    /// Window in use
    #[error("Window is in use")]
    WindowInUse,
    /// Accecssing the underlying NSView from wrong thread https://github.com/gfx-rs/gfx/issues/3704
    #[error("Accecssing NSView from wrong thread")]
    WrongThread,
    /// Unknown error.
    #[error("Swapchain can't be created for an unknown reason")]
    Unknown,
}

/// An extent describes the size of a rectangle, such as
/// a window or texture. It is not used for referring to a
/// sub-rectangle; for that see `command::Rect`.
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Extent2D {
    /// Width
    pub width: image::Size,
    /// Height
    pub height: image::Size,
}

impl From<image::Extent> for Extent2D {
    fn from(ex: image::Extent) -> Self {
        Extent2D {
            width: ex.width,
            height: ex.height,
        }
    }
}

impl From<(image::Size, image::Size)> for Extent2D {
    fn from(tuple: (image::Size, image::Size)) -> Self {
        Extent2D {
            width: tuple.0,
            height: tuple.1,
        }
    }
}

impl From<Extent2D> for (image::Size, image::Size) {
    fn from(extent: Extent2D) -> Self {
        (extent.width, extent.height)
    }
}

impl Extent2D {
    /// Convert into a regular image extent.
    pub fn to_extent(&self) -> image::Extent {
        image::Extent {
            width: self.width,
            height: self.height,
            depth: 1,
        }
    }
}

/// An offset describes the position of a rectangle, such as
/// a window or texture.
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Offset2D {
    /// X
    pub x: image::TexelCoordinate,
    /// Y
    pub y: image::TexelCoordinate,
}

impl From<(image::TexelCoordinate, image::TexelCoordinate)> for Offset2D {
    fn from(tuple: (image::TexelCoordinate, image::TexelCoordinate)) -> Self {
        Offset2D {
            x: tuple.0,
            y: tuple.1,
        }
    }
}

/// Describes information about what a `Surface`'s properties are.
/// Fetch this with [Surface::capabilities].
#[derive(Debug, Clone)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct SurfaceCapabilities {
    /// Number of presentable images supported by the adapter for a swapchain
    /// created from this surface.
    ///
    /// - `image_count.start` must be at least 1.
    /// - `image_count.end` must be larger or equal to `image_count.start`.
    pub image_count: RangeInclusive<SwapImageIndex>,

    /// Current extent of the surface.
    ///
    /// `None` if the surface has no explicit size, depending on the swapchain extent.
    pub current_extent: Option<Extent2D>,

    /// Range of supported extents.
    ///
    /// `current_extent` must be inside this range.
    pub extents: RangeInclusive<Extent2D>,

    /// Maximum number of layers supported for presentable images.
    ///
    /// Must be at least 1.
    pub max_image_layers: image::Layer,

    /// Supported image usage flags.
    pub usage: image::Usage,

    /// A bitmask of supported presentation modes.
    pub present_modes: PresentMode,

    /// A bitmask of supported alpha composition modes.
    pub composite_alpha_modes: CompositeAlphaMode,
}

impl SurfaceCapabilities {
    fn clamped_extent(&self, default_extent: Extent2D) -> Extent2D {
        match self.current_extent {
            Some(current) => current,
            None => {
                let (min_width, max_width) = (self.extents.start().width, self.extents.end().width);
                let (min_height, max_height) =
                    (self.extents.start().height, self.extents.end().height);

                // clamp the default_extent to within the allowed surface sizes
                let width = min(max_width, max(default_extent.width, min_width));
                let height = min(max_height, max(default_extent.height, min_height));

                Extent2D { width, height }
            }
        }
    }
}

/// A `Surface` abstracts the surface of a native window.
pub trait Surface<B: Backend>: fmt::Debug + Any + Send + Sync {
    /// Check if the queue family supports presentation to this surface.
    fn supports_queue_family(&self, family: &B::QueueFamily) -> bool;

    /// Query surface capabilities for this physical device.
    ///
    /// Use this function for configuring swapchain creation.
    fn capabilities(&self, physical_device: &B::PhysicalDevice) -> SurfaceCapabilities;

    /// Query surface formats for this physical device.
    ///
    /// This function may be slow. It's typically used during the initialization only.
    ///
    /// Note: technically the surface support formats may change at the point
    /// where an application needs to recreate the swapchain, e.g. when the window
    /// is moved to a different monitor.
    ///
    /// If `None` is returned then the surface has no preferred format and the
    /// application may use any desired format.
    fn supported_formats(&self, physical_device: &B::PhysicalDevice) -> Option<Vec<Format>>;
}

/// A surface trait that exposes the ability to present images on the
/// associtated swap chain.
pub trait PresentationSurface<B: Backend>: Surface<B> {
    /// An opaque type wrapping the swapchain image.
    type SwapchainImage: Borrow<B::Image> + Borrow<B::ImageView> + fmt::Debug + Send + Sync;

    /// Set up the swapchain associated with the surface to have the given format.
    unsafe fn configure_swapchain(
        &mut self,
        device: &B::Device,
        config: SwapchainConfig,
    ) -> Result<(), SwapchainError>;

    /// Remove the associated swapchain from this surface.
    ///
    /// This has to be done before the surface is dropped.
    unsafe fn unconfigure_swapchain(&mut self, device: &B::Device);

    /// Acquire a new swapchain image for rendering.
    ///
    /// May fail according to one of the reasons indicated in `AcquireError` enum.
    ///
    /// # Synchronization
    ///
    /// The acquired image is available to render. No synchronization is required.
    unsafe fn acquire_image(
        &mut self,
        timeout_ns: u64,
    ) -> Result<(Self::SwapchainImage, Option<Suboptimal>), AcquireError>;
}

/// Index of an image in the swapchain.
///
/// The swapchain is a series of one or more images, usually
/// with one being drawn on while the other is displayed by
/// the GPU (aka double-buffering). A `SwapImageIndex` refers
/// to a particular image in the swapchain.
pub type SwapImageIndex = u32;

bitflags!(
    /// Specifies the mode regulating how a swapchain presents frames.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct PresentMode: u32 {
        /// Don't ever wait for v-sync.
        const IMMEDIATE = 0x1;
        /// Wait for v-sync, overwrite the last rendered frame.
        const MAILBOX = 0x2;
        /// Present frames in the same order they are rendered.
        const FIFO = 0x4;
        /// Don't wait for the next v-sync if we just missed it.
        const RELAXED = 0x8;
    }
);

bitflags!(
    /// Specifies how the alpha channel of the images should be handled during
    /// compositing.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct CompositeAlphaMode: u32 {
        /// The alpha channel, if it exists, of the images is ignored in the
        /// compositing process. Instead, the image is treated as if it has a
        /// constant alpha of 1.0.
        const OPAQUE = 0x1;
        /// The alpha channel, if it exists, of the images is respected in the
        /// compositing process. The non-alpha channels of the image are
        /// expected to already be multiplied by the alpha channel by the
        /// application.
        const PREMULTIPLIED = 0x2;
        /// The alpha channel, if it exists, of the images is respected in the
        /// compositing process. The non-alpha channels of the image are not
        /// expected to already be multiplied by the alpha channel by the
        /// application; instead, the compositor will multiply the non-alpha
        /// channels of the image by the alpha channel during compositing.
        const POSTMULTIPLIED = 0x4;
        /// The way in which the presentation engine treats the alpha channel in
        /// the images is unknown to gfx-hal. Instead, the application is
        /// responsible for setting the composite alpha blending mode using
        /// native window system commands. If the application does not set the
        /// blending mode using native window system commands, then a
        /// platform-specific default will be used.
        const INHERIT = 0x8;
    }
);

/// Contains all the data necessary to create a new `Swapchain`:
/// color, depth, and number of images.
///
/// # Examples
///
/// This type implements the builder pattern, method calls can be
/// easily chained.
///
/// ```no_run
/// # extern crate gfx_hal;
/// # fn main() {
/// # use gfx_hal::window::SwapchainConfig;
/// # use gfx_hal::format::Format;
/// let config = SwapchainConfig::new(100, 100, Format::Bgra8Unorm, 2);
/// # }
/// ```
#[derive(Debug, Clone)]
pub struct SwapchainConfig {
    /// Presentation mode.
    pub present_mode: PresentMode,
    /// Alpha composition mode.
    pub composite_alpha_mode: CompositeAlphaMode,
    /// Format of the backbuffer images.
    pub format: Format,
    /// Requested image extent. Must be in
    /// `SurfaceCapabilities::extents` range.
    pub extent: Extent2D,
    /// Number of images in the swapchain. Must be in
    /// `SurfaceCapabilities::image_count` range.
    pub image_count: SwapImageIndex,
    /// Number of image layers. Must be lower or equal to
    /// `SurfaceCapabilities::max_image_layers`.
    pub image_layers: image::Layer,
    /// Image usage of the backbuffer images.
    pub image_usage: image::Usage,
}

impl SwapchainConfig {
    /// Create a new default configuration (color images only).
    pub fn new(width: u32, height: u32, format: Format, image_count: SwapImageIndex) -> Self {
        SwapchainConfig {
            present_mode: PresentMode::FIFO,
            composite_alpha_mode: CompositeAlphaMode::OPAQUE,
            format,
            extent: Extent2D { width, height },
            image_count,
            image_layers: 1,
            image_usage: DEFAULT_USAGE,
        }
    }

    /// Return the framebuffer attachment corresponding to the swapchain image views.
    pub fn framebuffer_attachment(&self) -> image::FramebufferAttachment {
        image::FramebufferAttachment {
            usage: self.image_usage,
            view_caps: image::ViewCapabilities::empty(),
            format: self.format,
        }
    }

    /// Create a swapchain configuration based on the capabilities
    /// returned from a physical device query. If the surface does not
    /// specify a current size, default_extent is clamped and used instead.
    ///
    /// The default values are taken from `DEFAULT_USAGE` and `DEFAULT_IMAGE_COUNT`.
    pub fn from_caps(caps: &SurfaceCapabilities, format: Format, default_extent: Extent2D) -> Self {
        let composite_alpha_mode = if caps
            .composite_alpha_modes
            .contains(CompositeAlphaMode::INHERIT)
        {
            CompositeAlphaMode::INHERIT
        } else if caps
            .composite_alpha_modes
            .contains(CompositeAlphaMode::OPAQUE)
        {
            CompositeAlphaMode::OPAQUE
        } else {
            panic!("neither INHERIT or OPAQUE CompositeAlphaMode(s) are supported")
        };
        let present_mode = if caps.present_modes.contains(PresentMode::MAILBOX) {
            PresentMode::MAILBOX
        } else if caps.present_modes.contains(PresentMode::FIFO) {
            PresentMode::FIFO
        } else {
            panic!("FIFO PresentMode is not supported")
        };

        SwapchainConfig {
            present_mode,
            composite_alpha_mode,
            format,
            extent: caps.clamped_extent(default_extent),
            image_count: DEFAULT_IMAGE_COUNT
                .max(*caps.image_count.start())
                .min(*caps.image_count.end()),
            image_layers: 1,
            image_usage: DEFAULT_USAGE,
        }
    }

    /// Specify the presentation mode.
    pub fn with_present_mode(mut self, mode: PresentMode) -> Self {
        self.present_mode = mode;
        self
    }

    /// Specify the presentation mode.
    pub fn with_composite_alpha_mode(mut self, mode: CompositeAlphaMode) -> Self {
        self.composite_alpha_mode = mode;
        self
    }

    /// Specify the usage of backbuffer images.
    pub fn with_image_usage(mut self, usage: image::Usage) -> Self {
        self.image_usage = usage;
        self
    }

    /// Specify the count of backbuffer image.
    pub fn with_image_count(mut self, count: SwapImageIndex) -> Self {
        self.image_count = count;
        self
    }

    // TODO: depth-only, stencil-only, swapchain size, present modes, etc.
}

/// Marker value returned if the swapchain no longer matches the surface properties exactly,
/// but can still be used to present to the surface successfully.
#[derive(Debug)]
pub struct Suboptimal;

/// Error occurred caused surface to be lost.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
#[error("Swapchain is out of date and needs to be re-created")]
pub struct OutOfDate;

/// Error on acquiring the next image from a swapchain.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum AcquireError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] device::OutOfMemory),
    /// No image was ready and no timeout was specified.
    #[error("No image ready (timeout: {timeout:})")]
    NotReady {
        /// Time has ran out.
        timeout: bool,
    },
    /// The swapchain is no longer in sync with the surface, needs to be re-created.
    #[error(transparent)]
    OutOfDate(#[from] OutOfDate),
    /// The surface was lost, and the swapchain is no longer usable.
    #[error(transparent)]
    SurfaceLost(#[from] SurfaceLost),
    /// Device is lost
    #[error(transparent)]
    DeviceLost(#[from] device::DeviceLost),
}

/// Error on acquiring the next image from a swapchain.
#[derive(Clone, Debug, PartialEq, thiserror::Error)]
pub enum PresentError {
    /// Out of either host or device memory.
    #[error(transparent)]
    OutOfMemory(#[from] device::OutOfMemory),
    /// The swapchain is no longer in sync with the surface, needs to be re-created.
    #[error(transparent)]
    OutOfDate(#[from] OutOfDate),
    /// The surface was lost, and the swapchain is no longer usable.
    #[error(transparent)]
    SurfaceLost(#[from] SurfaceLost),
    /// Device is lost
    #[error(transparent)]
    DeviceLost(#[from] device::DeviceLost),
}

/// Error occurred during surface creation.
#[derive(Clone, Copy, Debug, PartialEq, Eq, thiserror::Error)]
pub enum InitError {
    /// Window handle is not supported by the backend.
    #[error("Specified window handle is unsupported")]
    UnsupportedWindowHandle,
}
