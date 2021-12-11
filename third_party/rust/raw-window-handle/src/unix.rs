use core::ptr;
use libc::{c_ulong, c_void};

/// Raw window handle for Xlib.
///
/// ## Construction
/// ```
/// # use raw_window_handle::unix::XlibHandle;
/// let handle = XlibHandle {
///     /* fields */
///     ..XlibHandle::empty()
/// };
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct XlibHandle {
    /// An Xlib `Window`.
    pub window: c_ulong,
    /// A pointer to an Xlib `Display`.
    pub display: *mut c_void,
    #[doc(hidden)]
    #[deprecated = "This field is used to ensure that this struct is non-exhaustive, so that it may be extended in the future. Do not refer to this field."]
    pub _non_exhaustive_do_not_use: crate::seal::Seal,
}

/// Raw window handle for Xcb.
///
/// ## Construction
/// ```
/// # use raw_window_handle::unix::XcbHandle;
/// let handle = XcbHandle {
///     /* fields */
///     ..XcbHandle::empty()
/// };
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct XcbHandle {
    /// An X11 `xcb_window_t`.
    pub window: u32, // Based on xproto.h
    /// A pointer to an X server `xcb_connection_t`.
    pub connection: *mut c_void,
    #[doc(hidden)]
    #[deprecated = "This field is used to ensure that this struct is non-exhaustive, so that it may be extended in the future. Do not refer to this field."]
    pub _non_exhaustive_do_not_use: crate::seal::Seal,
}

/// Raw window handle for Wayland.
///
/// ## Construction
/// ```
/// # use raw_window_handle::unix::WaylandHandle;
/// let handle = WaylandHandle {
///     /* fields */
///     ..WaylandHandle::empty()
/// };
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WaylandHandle {
    /// A pointer to a `wl_surface`.
    pub surface: *mut c_void,
    /// A pointer to a `wl_display`.
    pub display: *mut c_void,
    #[doc(hidden)]
    #[deprecated = "This field is used to ensure that this struct is non-exhaustive, so that it may be extended in the future. Do not refer to this field."]
    pub _non_exhaustive_do_not_use: crate::seal::Seal,
}

impl XlibHandle {
    pub fn empty() -> XlibHandle {
        #[allow(deprecated)]
        XlibHandle {
            window: 0,
            display: ptr::null_mut(),
            _non_exhaustive_do_not_use: crate::seal::Seal,
        }
    }
}

impl XcbHandle {
    pub fn empty() -> XcbHandle {
        #[allow(deprecated)]
        XcbHandle {
            window: 0,
            connection: ptr::null_mut(),
            _non_exhaustive_do_not_use: crate::seal::Seal,
        }
    }
}

impl WaylandHandle {
    pub fn empty() -> WaylandHandle {
        #[allow(deprecated)]
        WaylandHandle {
            surface: ptr::null_mut(),
            display: ptr::null_mut(),
            _non_exhaustive_do_not_use: crate::seal::Seal,
        }
    }
}
