use core::ffi::{c_int, c_ulong, c_void};
use core::ptr;

/// Raw display handle for Xlib.
///
/// ## Construction
/// ```
/// # use raw_window_handle::XlibDisplayHandle;
/// let display_handle = XlibDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct XlibDisplayHandle {
    /// A pointer to an Xlib `Display`.
    pub display: *mut c_void,

    /// An X11 screen to use with this display handle.
    ///
    /// Note, that X11 could have multiple screens, however
    /// graphics APIs could work only with one screen at the time,
    /// given that multiple screens usually reside on different GPUs.
    pub screen: c_int,
}

/// Raw window handle for Xlib.
///
/// ## Construction
/// ```
/// # use raw_window_handle::XlibWindowHandle;
/// let window_handle = XlibWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct XlibWindowHandle {
    /// An Xlib `Window`.
    pub window: c_ulong,
    /// An Xlib visual ID, or 0 if unknown.
    pub visual_id: c_ulong,
}

/// Raw display handle for Xcb.
///
/// ## Construction
/// ```
/// # use raw_window_handle::XcbDisplayHandle;
/// let display_handle = XcbDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct XcbDisplayHandle {
    /// A pointer to an X server `xcb_connection_t`.
    pub connection: *mut c_void,

    /// An X11 screen to use with this display handle.
    ///
    /// Note, that X11 could have multiple screens, however
    /// graphics APIs could work only with one screen at the time,
    /// given that multiple screens usually reside on different GPUs.
    pub screen: c_int,
}

/// Raw window handle for Xcb.
///
/// ## Construction
/// ```
/// # use raw_window_handle::XcbWindowHandle;
/// let window_handle = XcbWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct XcbWindowHandle {
    /// An X11 `xcb_window_t`.
    pub window: u32, // Based on xproto.h
    /// An X11 `xcb_visualid_t`, or 0 if unknown.
    pub visual_id: u32,
}

/// Raw display handle for Wayland.
///
/// ## Construction
/// ```
/// # use raw_window_handle::WaylandDisplayHandle;
/// let display_handle = WaylandDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WaylandDisplayHandle {
    /// A pointer to a `wl_display`.
    pub display: *mut c_void,
}

/// Raw window handle for Wayland.
///
/// ## Construction
/// ```
/// # use raw_window_handle::WaylandWindowHandle;
/// let window_handle = WaylandWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WaylandWindowHandle {
    /// A pointer to a `wl_surface`.
    pub surface: *mut c_void,
}

/// Raw display handle for the Linux Kernel Mode Set/Direct Rendering Manager.
///
/// ## Construction
/// ```
/// # use raw_window_handle::DrmDisplayHandle;
/// let display_handle = DrmDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct DrmDisplayHandle {
    /// The drm file descriptor.
    pub fd: i32,
}

/// Raw window handle for the Linux Kernel Mode Set/Direct Rendering Manager.
///
/// ## Construction
/// ```
/// # use raw_window_handle::DrmWindowHandle;
/// let handle = DrmWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct DrmWindowHandle {
    /// The primary drm plane handle.
    pub plane: u32,
}

/// Raw display handle for the Linux Generic Buffer Manager.
///
/// ## Construction
/// ```
/// # use raw_window_handle::GbmDisplayHandle;
/// let display_handle = GbmDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct GbmDisplayHandle {
    /// The gbm device.
    pub gbm_device: *mut c_void,
}

/// Raw window handle for the Linux Generic Buffer Manager.
///
/// ## Construction
/// ```
/// # use raw_window_handle::GbmWindowHandle;
/// let handle = GbmWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct GbmWindowHandle {
    /// The gbm surface.
    pub gbm_surface: *mut c_void,
}

impl XlibDisplayHandle {
    pub fn empty() -> Self {
        Self {
            display: ptr::null_mut(),
            screen: 0,
        }
    }
}

impl XlibWindowHandle {
    pub fn empty() -> Self {
        Self {
            window: 0,
            visual_id: 0,
        }
    }
}

impl XcbDisplayHandle {
    pub fn empty() -> Self {
        Self {
            connection: ptr::null_mut(),
            screen: 0,
        }
    }
}

impl XcbWindowHandle {
    pub fn empty() -> Self {
        Self {
            window: 0,
            visual_id: 0,
        }
    }
}

impl WaylandDisplayHandle {
    pub fn empty() -> Self {
        Self {
            display: ptr::null_mut(),
        }
    }
}

impl WaylandWindowHandle {
    pub fn empty() -> Self {
        Self {
            surface: ptr::null_mut(),
        }
    }
}

impl DrmDisplayHandle {
    pub fn empty() -> Self {
        Self { fd: 0 }
    }
}

impl DrmWindowHandle {
    pub fn empty() -> Self {
        Self { plane: 0 }
    }
}

impl GbmDisplayHandle {
    pub fn empty() -> Self {
        Self {
            gbm_device: ptr::null_mut(),
        }
    }
}

impl GbmWindowHandle {
    pub fn empty() -> Self {
        Self {
            gbm_surface: ptr::null_mut(),
        }
    }
}
