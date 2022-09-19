use core::ffi::c_void;
use core::ptr;

/// Raw display handle for Haiku.
///
/// ## Construction
/// ```
/// # use raw_window_handle::HaikuDisplayHandle;
/// let mut display_handle = HaikuDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct HaikuDisplayHandle;

impl HaikuDisplayHandle {
    pub fn empty() -> Self {
        Self {}
    }
}

/// Raw window handle for Haiku.
///
/// ## Construction
/// ```
/// # use raw_window_handle::HaikuWindowHandle;
/// let mut window_handle = HaikuWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct HaikuWindowHandle {
    /// A pointer to a BWindow object
    pub b_window: *mut c_void,
    /// A pointer to a BDirectWindow object that might be null
    pub b_direct_window: *mut c_void,
}

impl HaikuWindowHandle {
    pub fn empty() -> Self {
        Self {
            b_window: ptr::null_mut(),
            b_direct_window: ptr::null_mut(),
        }
    }
}
