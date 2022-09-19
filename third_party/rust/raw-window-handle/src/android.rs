use core::ffi::c_void;
use core::ptr;

/// Raw display handle for Android.
///
/// ## Construction
/// ```
/// # use raw_window_handle::AndroidDisplayHandle;
/// let mut display_handle = AndroidDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AndroidDisplayHandle;

impl AndroidDisplayHandle {
    pub fn empty() -> Self {
        Self {}
    }
}

/// Raw window handle for Android NDK.
///
/// ## Construction
/// ```
/// # use raw_window_handle::AndroidNdkWindowHandle;
/// let mut window_handle = AndroidNdkWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AndroidNdkWindowHandle {
    /// A pointer to an `ANativeWindow`.
    pub a_native_window: *mut c_void,
}

impl AndroidNdkWindowHandle {
    pub fn empty() -> Self {
        Self {
            a_native_window: ptr::null_mut(),
        }
    }
}
