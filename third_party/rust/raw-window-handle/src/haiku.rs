use core::ffi::c_void;
use core::ptr::NonNull;

/// Raw display handle for Haiku.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct HaikuDisplayHandle {}

impl HaikuDisplayHandle {
    /// Create a new empty display handle.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use raw_window_handle::HaikuDisplayHandle;
    /// let handle = HaikuDisplayHandle::new();
    /// ```
    pub fn new() -> Self {
        Self {}
    }
}

/// Raw window handle for Haiku.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct HaikuWindowHandle {
    /// A pointer to a BWindow object
    pub b_window: NonNull<c_void>,
    /// A pointer to a BDirectWindow object that might be null
    pub b_direct_window: Option<NonNull<c_void>>,
}

impl HaikuWindowHandle {
    /// Create a new handle to a window.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use core::ptr::NonNull;
    /// # use raw_window_handle::HaikuWindowHandle;
    /// # type BWindow = ();
    /// #
    /// let b_window: NonNull<BWindow>;
    /// # b_window = NonNull::from(&());
    /// let mut handle = HaikuWindowHandle::new(b_window.cast());
    /// // Optionally set `b_direct_window`.
    /// handle.b_direct_window = None;
    /// ```
    pub fn new(b_window: NonNull<c_void>) -> Self {
        Self {
            b_window,
            b_direct_window: None,
        }
    }
}
