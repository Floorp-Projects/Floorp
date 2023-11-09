use core::ffi::c_void;
use core::ptr::NonNull;

/// Raw display handle for the Redox operating system.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct OrbitalDisplayHandle {}

impl OrbitalDisplayHandle {
    /// Create a new empty display handle.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use raw_window_handle::OrbitalDisplayHandle;
    /// let handle = OrbitalDisplayHandle::new();
    /// ```
    pub fn new() -> Self {
        Self {}
    }
}

/// Raw window handle for the Redox operating system.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct OrbitalWindowHandle {
    /// A pointer to an orbclient window.
    // TODO(madsmtm): I think this is a file descriptor, so perhaps it should
    // actually use `std::os::fd::RawFd`, or some sort of integer instead?
    pub window: NonNull<c_void>,
}

impl OrbitalWindowHandle {
    /// Create a new handle to a window.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use core::ptr::NonNull;
    /// # use raw_window_handle::OrbitalWindowHandle;
    /// # type Window = ();
    /// #
    /// let window: NonNull<Window>;
    /// # window = NonNull::from(&());
    /// let mut handle = OrbitalWindowHandle::new(window.cast());
    /// ```
    pub fn new(window: NonNull<c_void>) -> Self {
        Self { window }
    }
}
