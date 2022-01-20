use core::ptr;
use libc::c_void;

/// Raw window handle for macOS.
///
/// ## Construction
/// ```
/// # use raw_window_handle::macos::MacOSHandle;
/// let handle = MacOSHandle {
///     /* fields */
///     ..MacOSHandle::empty()
/// };
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct MacOSHandle {
    pub ns_window: *mut c_void,
    pub ns_view: *mut c_void,
    // TODO: WHAT ABOUT ns_window_controller and ns_view_controller?
    #[doc(hidden)]
    #[deprecated = "This field is used to ensure that this struct is non-exhaustive, so that it may be extended in the future. Do not refer to this field."]
    pub _non_exhaustive_do_not_use: crate::seal::Seal,
}

impl MacOSHandle {
    pub fn empty() -> MacOSHandle {
        #[allow(deprecated)]
        MacOSHandle {
            ns_window: ptr::null_mut(),
            ns_view: ptr::null_mut(),
            _non_exhaustive_do_not_use: crate::seal::Seal,
        }
    }
}
