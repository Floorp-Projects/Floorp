use core::ffi::c_void;
use core::num::NonZeroIsize;
use core::ptr::NonNull;

/// Raw display handle for Windows.
///
/// It can be used regardless of Windows window backend.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WindowsDisplayHandle {}

impl WindowsDisplayHandle {
    /// Create a new empty display handle.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use raw_window_handle::WindowsDisplayHandle;
    /// let handle = WindowsDisplayHandle::new();
    /// ```
    pub fn new() -> Self {
        Self {}
    }
}

/// Raw window handle for Win32.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Win32WindowHandle {
    /// A Win32 `HWND` handle.
    pub hwnd: NonZeroIsize,
    /// The `GWLP_HINSTANCE` associated with this type's `HWND`.
    pub hinstance: Option<NonZeroIsize>,
}

impl Win32WindowHandle {
    /// Create a new handle to a window.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use core::num::NonZeroIsize;
    /// # use raw_window_handle::Win32WindowHandle;
    /// # struct HWND(isize);
    /// #
    /// let window: HWND;
    /// # window = HWND(1);
    /// let mut handle = Win32WindowHandle::new(NonZeroIsize::new(window.0).unwrap());
    /// // Optionally set the GWLP_HINSTANCE.
    /// # #[cfg(only_for_showcase)]
    /// let hinstance = NonZeroIsize::new(unsafe { GetWindowLongPtrW(window, GWLP_HINSTANCE) }).unwrap();
    /// # let hinstance = None;
    /// handle.hinstance = hinstance;
    /// ```
    pub fn new(hwnd: NonZeroIsize) -> Self {
        Self {
            hwnd,
            hinstance: None,
        }
    }
}

/// Raw window handle for WinRT.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WinRtWindowHandle {
    /// A WinRT `CoreWindow` handle.
    pub core_window: NonNull<c_void>,
}

impl WinRtWindowHandle {
    /// Create a new handle to a window.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use core::ptr::NonNull;
    /// # use raw_window_handle::WinRtWindowHandle;
    /// # type CoreWindow = ();
    /// #
    /// let window: NonNull<CoreWindow>;
    /// # window = NonNull::from(&());
    /// let handle = WinRtWindowHandle::new(window.cast());
    /// ```
    pub fn new(core_window: NonNull<c_void>) -> Self {
        Self { core_window }
    }
}
