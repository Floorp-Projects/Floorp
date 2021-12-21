use core::ffi::c_void;
use core::ptr;

/// Raw window handle for Android NDK.
///
/// ## Construction
/// ```
/// # use raw_window_handle::AndroidNdkHandle;
/// let mut handle = AndroidNdkHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AndroidNdkHandle {
    /// A pointer to an `ANativeWindow`.
    pub a_native_window: *mut c_void,
}

impl AndroidNdkHandle {
    pub fn empty() -> Self {
        Self {
            a_native_window: ptr::null_mut(),
        }
    }
}
