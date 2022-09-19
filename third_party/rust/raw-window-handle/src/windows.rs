use core::ffi::c_void;
use core::ptr;

/// Raw display handle for Windows.
///
/// It could be used regardless of Windows window backend.
///
/// ## Construction
/// ```
/// # use raw_window_handle::WindowsDisplayHandle;
/// let mut display_handle = WindowsDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WindowsDisplayHandle;

impl WindowsDisplayHandle {
    pub fn empty() -> Self {
        Self
    }
}

/// Raw window handle for Win32.
///
/// ## Construction
/// ```
/// # use raw_window_handle::Win32WindowHandle;
/// let mut window_handle = Win32WindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Win32WindowHandle {
    /// A Win32 `HWND` handle.
    pub hwnd: *mut c_void,
    /// The `HINSTANCE` associated with this type's `HWND`.
    pub hinstance: *mut c_void,
}

impl Win32WindowHandle {
    pub fn empty() -> Self {
        Self {
            hwnd: ptr::null_mut(),
            hinstance: ptr::null_mut(),
        }
    }
}

/// Raw window handle for WinRT.
///
/// ## Construction
/// ```
/// # use raw_window_handle::WinRtWindowHandle;
/// let mut window_handle = WinRtWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WinRtWindowHandle {
    /// A WinRT `CoreWindow` handle.
    pub core_window: *mut c_void,
}

impl WinRtWindowHandle {
    pub fn empty() -> Self {
        Self {
            core_window: ptr::null_mut(),
        }
    }
}
