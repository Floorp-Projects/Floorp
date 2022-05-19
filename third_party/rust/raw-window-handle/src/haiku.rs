use core::ffi::c_void;
use core::ptr;

/// Raw window handle for Haiku.
///
/// ## Construction
/// ```
/// # use raw_window_handle::HaikuHandle;
/// let mut handle = HaikuHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct HaikuHandle {
    /// A pointer to a BWindow object
    pub b_window: *mut c_void,
    /// A pointer to a BDirectWindow object that might be null
    pub b_direct_window: *mut c_void,
}

impl HaikuHandle {
    pub fn empty() -> Self {
        Self {
            b_window: ptr::null_mut(),
            b_direct_window: ptr::null_mut(),
        }
    }
}
