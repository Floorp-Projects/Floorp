use core::ffi::c_void;
use core::ptr::NonNull;

/// Raw display handle for UIKit.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct UiKitDisplayHandle {}

impl UiKitDisplayHandle {
    /// Create a new empty display handle.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use raw_window_handle::UiKitDisplayHandle;
    /// let handle = UiKitDisplayHandle::new();
    /// ```
    pub fn new() -> Self {
        Self {}
    }
}

/// Raw window handle for UIKit.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct UiKitWindowHandle {
    /// A pointer to an `UIView` object.
    pub ui_view: NonNull<c_void>,
    /// A pointer to an `UIViewController` object, if the view has one.
    pub ui_view_controller: Option<NonNull<c_void>>,
}

impl UiKitWindowHandle {
    /// Create a new handle to a view.
    ///
    ///
    /// # Example
    ///
    /// ```
    /// # use core::ptr::NonNull;
    /// # use raw_window_handle::UiKitWindowHandle;
    /// # type UIView = ();
    /// #
    /// let view: &UIView;
    /// # view = &();
    /// let mut handle = UiKitWindowHandle::new(NonNull::from(view).cast());
    /// // Optionally set the view controller.
    /// handle.ui_view_controller = None;
    /// ```
    pub fn new(ui_view: NonNull<c_void>) -> Self {
        Self {
            ui_view,
            ui_view_controller: None,
        }
    }
}
