use core::ptr;
use libc::c_void;

/// Raw window handle for Android.
///
/// ## Construction
/// ```
/// # use raw_window_handle::android::AndroidHandle;
/// let handle = AndroidHandle {
///     /* fields */
///     ..AndroidHandle::empty()
/// };
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AndroidHandle {
    /// A pointer to an ANativeWindow.
    pub a_native_window: *mut c_void,
    #[doc(hidden)]
    #[deprecated = "This field is used to ensure that this struct is non-exhaustive, so that it may be extended in the future. Do not refer to this field."]
    pub _non_exhaustive_do_not_use: crate::seal::Seal,
}

impl AndroidHandle {
    pub fn empty() -> AndroidHandle {
        #[allow(deprecated)]
        AndroidHandle {
            a_native_window: ptr::null_mut(),
            _non_exhaustive_do_not_use: crate::seal::Seal,
        }
    }
}
