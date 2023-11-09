use core::ffi::c_void;
use core::ptr::NonNull;

/// Raw display handle for Android.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AndroidDisplayHandle {}

impl AndroidDisplayHandle {
    /// Create a new empty display handle.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use raw_window_handle::AndroidDisplayHandle;
    /// let handle = AndroidDisplayHandle::new();
    /// ```
    pub fn new() -> Self {
        Self {}
    }
}

/// Raw window handle for Android NDK.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct AndroidNdkWindowHandle {
    /// A pointer to an `ANativeWindow`.
    pub a_native_window: NonNull<c_void>,
}

impl AndroidNdkWindowHandle {
    /// Create a new handle to an `ANativeWindow`.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use core::ptr::NonNull;
    /// # use raw_window_handle::AndroidNdkWindowHandle;
    /// # type ANativeWindow = ();
    /// #
    /// let ptr: NonNull<ANativeWindow>;
    /// # ptr = NonNull::from(&());
    /// let handle = AndroidNdkWindowHandle::new(ptr.cast());
    /// ```
    pub fn new(a_native_window: NonNull<c_void>) -> Self {
        Self { a_native_window }
    }
}
