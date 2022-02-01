use core::ffi::c_void;
use core::ptr;

/// Raw window handle for AppKit.
///
/// ## Construction
/// ```
/// # use raw_window_handle::AppKitHandle;
/// let mut handle = AppKitHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AppKitHandle {
    /// A pointer to an `NSWindow` object.
    pub ns_window: *mut c_void,
    /// A pointer to an `NSView` object.
    pub ns_view: *mut c_void,
    // TODO: WHAT ABOUT ns_window_controller and ns_view_controller?
}

impl AppKitHandle {
    pub fn empty() -> Self {
        Self {
            ns_window: ptr::null_mut(),
            ns_view: ptr::null_mut(),
        }
    }
}
