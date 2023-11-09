use core::ffi::c_void;
use core::ptr::NonNull;

/// Raw display handle for AppKit.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AppKitDisplayHandle {}

impl AppKitDisplayHandle {
    /// Create a new empty display handle.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use raw_window_handle::AppKitDisplayHandle;
    /// let handle = AppKitDisplayHandle::new();
    /// ```
    pub fn new() -> Self {
        Self {}
    }
}

/// Raw window handle for AppKit.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AppKitWindowHandle {
    /// A pointer to an `NSView` object.
    pub ns_view: NonNull<c_void>,
}

impl AppKitWindowHandle {
    /// Create a new handle to a view.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use core::ptr::NonNull;
    /// # use raw_window_handle::AppKitWindowHandle;
    /// # type NSView = ();
    /// #
    /// let view: &NSView;
    /// # view = &();
    /// let handle = AppKitWindowHandle::new(NonNull::from(view).cast());
    /// ```
    pub fn new(ns_view: NonNull<c_void>) -> Self {
        Self { ns_view }
    }
}
