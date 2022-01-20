use core::ffi::c_void;
use core::ptr;

/// Raw window handle for the Redox operating system.
///
/// ## Construction
/// ```
/// # use raw_window_handle::OrbitalHandle;
/// let mut handle = OrbitalHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct OrbitalHandle {
    /// A pointer to an orbclient window.
    pub window: *mut c_void,
}

impl OrbitalHandle {
    pub fn empty() -> Self {
        Self {
            window: ptr::null_mut(),
        }
    }
}
