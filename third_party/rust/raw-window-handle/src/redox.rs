use core::ffi::c_void;
use core::ptr;

/// Raw display handle for the Redox operating system.
///
/// ## Construction
/// ```
/// # use raw_window_handle::OrbitalDisplayHandle;
/// let mut display_handle = OrbitalDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct OrbitalDisplayHandle;

impl OrbitalDisplayHandle {
    pub fn empty() -> Self {
        Self {}
    }
}

/// Raw window handle for the Redox operating system.
///
/// ## Construction
/// ```
/// # use raw_window_handle::OrbitalWindowHandle;
/// let mut window_handle = OrbitalWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct OrbitalWindowHandle {
    /// A pointer to an orbclient window.
    pub window: *mut c_void,
}

impl OrbitalWindowHandle {
    pub fn empty() -> Self {
        Self {
            window: ptr::null_mut(),
        }
    }
}
