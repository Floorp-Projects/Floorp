use core::ptr;
use libc::c_void;

/// Raw window handle for iOS.
///
/// ## Construction
/// ```
/// # use raw_window_handle::ios::IOSHandle;
/// let handle = IOSHandle {
///     /* fields */
///     ..IOSHandle::empty()
/// };
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct IOSHandle {
    pub ui_window: *mut c_void,
    pub ui_view: *mut c_void,
    pub ui_view_controller: *mut c_void,
    #[doc(hidden)]
    #[deprecated = "This field is used to ensure that this struct is non-exhaustive, so that it may be extended in the future. Do not refer to this field."]
    pub _non_exhaustive_do_not_use: crate::seal::Seal,
}

impl IOSHandle {
    pub fn empty() -> IOSHandle {
        #[allow(deprecated)]
        IOSHandle {
            ui_window: ptr::null_mut(),
            ui_view: ptr::null_mut(),
            ui_view_controller: ptr::null_mut(),
            _non_exhaustive_do_not_use: crate::seal::Seal,
        }
    }
}
