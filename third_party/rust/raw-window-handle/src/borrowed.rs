//! Borrowable window handles based on the ones in this crate.
//!
//! These should be 100% safe to pass around and use, no possibility of dangling or invalidity.

#[cfg(all(not(feature = "std"), target_os = "android"))]
compile_error!("Using borrowed handles on Android requires the `std` feature to be enabled.");

use core::fmt;
use core::hash::{Hash, Hasher};
use core::marker::PhantomData;

use crate::{HasRawDisplayHandle, HasRawWindowHandle, RawDisplayHandle, RawWindowHandle};

/// Keeps track of whether the application is currently active.
///
/// On certain platforms (e.g. Android), it is possible for the application to enter a "suspended"
/// state. While in this state, all previously valid window handles become invalid. Therefore, in
/// order for window handles to be valid, the application must be active.
///
/// On platforms where the graphical user interface is always active, this type is a ZST and all
/// of its methods are noops. On Android, this type acts as a reference counter that keeps track
/// of all currently active window handles. Before the application enters the suspended state, it
/// blocks until all of the currently active window handles are dropped.
///
/// ## Explanation
///
/// On Android, there is an [Activity]-global [`ANativeWindow`] object that is used for drawing. This
/// handle is used [within the `RawWindowHandle` type] for Android NDK, since it is necessary for GFX
/// APIs to draw to the screen.
///
/// However, the [`ANativeWindow`] type can be arbitrarily invalidated by the underlying Android runtime.
/// The reasoning for this is complicated, but this idea is exposed to native code through the
/// [`onNativeWindowCreated`] and [`onNativeWindowDestroyed`] callbacks. To save you a click, the
/// conditions associated with these callbacks are:
///
/// - [`onNativeWindowCreated`] provides a valid [`ANativeWindow`] pointer that can be used for drawing.
/// - [`onNativeWindowDestroyed`] indicates that the previous [`ANativeWindow`] pointer is no longer
///   valid. The documentation clarifies that, *once the function returns*, the [`ANativeWindow`] pointer
///   can no longer be used for drawing without resulting in undefined behavior.
///
/// In [`winit`], these are exposed via the [`Resumed`] and [`Suspended`] events, respectively. Therefore,
/// between the last [`Suspended`] event and the next [`Resumed`] event, it is undefined behavior to use
/// the raw window handle. This condition makes it tricky to define an API that safely wraps the raw
/// window handles, since an existing window handle can be made invalid at any time.
///
/// The Android docs specifies that the [`ANativeWindow`] pointer is still valid while the application
/// is still in the [`onNativeWindowDestroyed`] block, and suggests that synchronization needs to take
/// place to ensure that the pointer has been invalidated before the function returns. `Active` aims
/// to be the solution to this problem. It keeps track of all currently active window handles, and
/// blocks until all of them are dropped before allowing the application to enter the suspended state.
///
/// [Activity]: https://developer.android.com/reference/android/app/Activity
/// [`ANativeWindow`]: https://developer.android.com/ndk/reference/group/a-native-window
/// [within the `RawWindowHandle` type]: struct.AndroidNdkWindowHandle.html#structfield.a_native_window
/// [`onNativeWindowCreated`]: https://developer.android.com/ndk/reference/struct/a-native-activity-callbacks#onnativewindowcreated
/// [`onNativeWindowDestroyed`]: https://developer.android.com/ndk/reference/struct/a-native-activity-callbacks#onnativewindowdestroyed
/// [`Resumed`]: https://docs.rs/winit/latest/winit/event/enum.Event.html#variant.Resumed
/// [`Suspended`]: https://docs.rs/winit/latest/winit/event/enum.Event.html#variant.Suspended
/// [`sdl2`]: https://crates.io/crates/sdl2
/// [`RawWindowHandle`]: https://docs.rs/raw-window-handle/latest/raw_window_handle/enum.RawWindowHandle.html
/// [`HasRawWindowHandle`]: https://docs.rs/raw-window-handle/latest/raw_window_handle/trait.HasRawWindowHandle.html
/// [`winit`]: https://crates.io/crates/winit
pub struct Active(imp::Active);

impl fmt::Debug for Active {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("Active { .. }")
    }
}

/// Represents a live window handle.
///
/// This is carried around by the [`Active`] type, and is used to ensure that the application doesn't
/// enter the suspended state while there are still live window handles. See documentation on the
/// [`Active`] type for more information.
///
/// On non-Android platforms, this is a ZST. On Android, this is a reference counted handle that
/// keeps the application active while it is alive.
#[derive(Clone)]
pub struct ActiveHandle<'a>(imp::ActiveHandle<'a>);

impl<'a> fmt::Debug for ActiveHandle<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("ActiveHandle { .. }")
    }
}

impl Active {
    /// Create a new `Active` tracker.
    ///
    /// Only one of these should exist per display connection.
    ///
    /// # Example
    ///
    /// ```
    /// use raw_window_handle::Active;
    /// let active = Active::new();
    /// ```
    pub const fn new() -> Self {
        Self(imp::Active::new())
    }

    /// Get a live window handle.
    ///
    /// This function returns an active handle if the application is active, and `None` otherwise.
    ///
    /// # Example
    ///
    /// ```
    /// use raw_window_handle::Active;
    ///
    /// // Set the application to be active.
    /// let active = Active::new();
    /// unsafe { active.set_active() };
    ///
    /// // Get a live window handle.
    /// let handle = active.handle();
    ///
    /// // Drop it and set the application to be inactive.
    /// drop(handle);
    /// active.set_inactive();
    /// ```
    pub fn handle(&self) -> Option<ActiveHandle<'_>> {
        self.0.handle().map(ActiveHandle)
    }

    /// Set the application to be inactive.
    ///
    /// This function may block until there are no more active handles.
    ///
    /// # Example
    ///
    /// ```
    /// use raw_window_handle::Active;
    ///
    /// // Set the application to be active.
    /// let active = Active::new();
    /// unsafe { active.set_active() };
    ///
    /// // Set the application to be inactive.
    /// active.set_inactive();
    /// ```
    pub fn set_inactive(&self) {
        self.0.set_inactive()
    }

    /// Set the application to be active.
    ///
    /// # Safety
    ///
    /// The application must actually be active. Setting to active when the application is not active
    /// will result in undefined behavior.
    ///
    /// # Example
    ///
    /// ```
    /// use raw_window_handle::Active;
    ///
    /// // Set the application to be active.
    /// let active = Active::new();
    /// unsafe { active.set_active() };
    ///
    /// // Set the application to be inactive.
    /// active.set_inactive();
    /// ```
    pub unsafe fn set_active(&self) {
        self.0.set_active()
    }
}

impl ActiveHandle<'_> {
    /// Create a new freestanding active handle.
    ///
    /// This function acts as an "escape hatch" to allow the user to create a live window handle
    /// without having to go through the [`Active`] type. This is useful if the user *knows* that the
    /// application is active, and wants to create a live window handle without having to go through
    /// the [`Active`] type.
    ///
    /// # Safety
    ///
    /// The application must actually be active.
    ///
    /// # Example
    ///
    /// ```
    /// use raw_window_handle::ActiveHandle;
    ///
    /// // Create a freestanding active handle.
    /// // SAFETY: The application must actually be active.
    /// let handle = unsafe { ActiveHandle::new_unchecked() };
    /// ```
    pub unsafe fn new_unchecked() -> Self {
        Self(imp::ActiveHandle::new_unchecked())
    }
}

/// A display that acts as a wrapper around a display handle.
///
/// Objects that implement this trait should be able to return a [`DisplayHandle`] for the display
/// that they are associated with. This handle should last for the lifetime of the object, and should
/// return an error if the application is inactive.
///
/// Implementors of this trait will be windowing systems, like [`winit`] and [`sdl2`]. These windowing
/// systems should implement this trait on types that already implement [`HasRawDisplayHandle`]. It
/// should be implemented by tying the lifetime of the [`DisplayHandle`] to the lifetime of the
/// display object.
///
/// Users of this trait will include graphics libraries, like [`wgpu`] and [`glutin`]. These APIs
/// should be generic over a type that implements `HasDisplayHandle`, and should use the
/// [`DisplayHandle`] type to access the display handle.
///
/// # Safety
///
/// The safety requirements of [`HasRawDisplayHandle`] apply here as well. To reiterate, the
/// [`DisplayHandle`] must contain a valid window handle for its lifetime.
///
/// It is not possible to invalidate a [`DisplayHandle`] on any platform without additional unsafe code.
///
/// Note that these requirements are not enforced on `HasDisplayHandle`, rather, they are enforced on the
/// constructors of [`DisplayHandle`]. This is because the `HasDisplayHandle` trait is safe to implement.
///
/// [`HasRawDisplayHandle`]: crate::HasRawDisplayHandle
/// [`winit`]: https://crates.io/crates/winit
/// [`sdl2`]: https://crates.io/crates/sdl2
/// [`wgpu`]: https://crates.io/crates/wgpu
/// [`glutin`]: https://crates.io/crates/glutin
pub trait HasDisplayHandle {
    /// Get a handle to the display controller of the windowing system.
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError>;
}

impl<H: HasDisplayHandle + ?Sized> HasDisplayHandle for &H {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        (**self).display_handle()
    }
}

#[cfg(feature = "alloc")]
impl<H: HasDisplayHandle + ?Sized> HasDisplayHandle for alloc::boxed::Box<H> {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        (**self).display_handle()
    }
}

#[cfg(feature = "alloc")]
impl<H: HasDisplayHandle + ?Sized> HasDisplayHandle for alloc::rc::Rc<H> {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        (**self).display_handle()
    }
}

#[cfg(feature = "alloc")]
impl<H: HasDisplayHandle + ?Sized> HasDisplayHandle for alloc::sync::Arc<H> {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        (**self).display_handle()
    }
}

/// The handle to the display controller of the windowing system.
///
/// This is the primary return type of the [`HasDisplayHandle`] trait. It is guaranteed to contain
/// a valid platform-specific display handle for its lifetime.
///
/// Get the underlying raw display handle with the [`HasRawDisplayHandle`] trait.
#[repr(transparent)]
#[derive(PartialEq, Eq, Hash)]
pub struct DisplayHandle<'a> {
    raw: RawDisplayHandle,
    _marker: PhantomData<&'a *const ()>,
}

impl fmt::Debug for DisplayHandle<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("DisplayHandle").field(&self.raw).finish()
    }
}

impl<'a> Clone for DisplayHandle<'a> {
    fn clone(&self) -> Self {
        Self {
            raw: self.raw,
            _marker: PhantomData,
        }
    }
}

impl<'a> DisplayHandle<'a> {
    /// Create a `DisplayHandle` from a [`RawDisplayHandle`].
    ///
    /// # Safety
    ///
    /// The `RawDisplayHandle` must be valid for the lifetime.
    pub unsafe fn borrow_raw(raw: RawDisplayHandle) -> Self {
        Self {
            raw,
            _marker: PhantomData,
        }
    }
}

unsafe impl HasRawDisplayHandle for DisplayHandle<'_> {
    fn raw_display_handle(&self) -> RawDisplayHandle {
        self.raw
    }
}

impl<'a> HasDisplayHandle for DisplayHandle<'a> {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        Ok(self.clone())
    }
}

/// A handle to a window.
///
/// Objects that implement this trait should be able to return a [`WindowHandle`] for the window
/// that they are associated with. This handle should last for the lifetime of the object, and should
/// return an error if the application is inactive.
///
/// Implementors of this trait will be windowing systems, like [`winit`] and [`sdl2`]. These windowing
/// systems should implement this trait on types that already implement [`HasRawWindowHandle`]. First,
/// it should be made sure that the display type contains a unique [`Active`] ref-counted handle.
/// To create a [`WindowHandle`], the [`Active`] should be used to create an [`ActiveHandle`] that is
/// then used to create a [`WindowHandle`]. Finally, the raw window handle should be retrieved from
/// the type and used to create a [`WindowHandle`].
///
/// Users of this trait will include graphics libraries, like [`wgpu`] and [`glutin`]. These APIs
/// should be generic over a type that implements `HasWindowHandle`, and should use the
/// [`WindowHandle`] type to access the window handle. The window handle should be acquired and held
/// while the window is being used, in order to ensure that the window is not deleted while it is in
/// use.
///
/// # Safety
///
/// All pointers within the resulting [`WindowHandle`] must be valid and not dangling for the lifetime of
/// the handle.
///
/// Note that this guarantee only applies to *pointers*, and not any window ID types in the handle.
/// This includes Window IDs (XIDs) from X11 and the window ID for web platforms. There is no way for
/// Rust to enforce any kind of invariant on these types, since:
///
/// - For all three listed platforms, it is possible for safe code in the same process to delete
///   the window.
/// - For X11, it is possible for code in a different process to delete the window. In fact, it is
///   possible for code on a different *machine* to delete the window.
///
/// It is *also* possible for the window to be replaced with another, valid-but-different window. User
/// code should be aware of this possibility, and should be ready to soundly handle the possible error
/// conditions that can arise from this.
///
/// In addition, the window handle must not be invalidated for the duration of the [`ActiveHandle`] token.
///
/// Note that these requirements are not enforced on `HasWindowHandle`, rather, they are enforced on the
/// constructors of [`WindowHandle`]. This is because the `HasWindowHandle` trait is safe to implement.
///
/// [`winit`]: https://crates.io/crates/winit
/// [`sdl2`]: https://crates.io/crates/sdl2
/// [`wgpu`]: https://crates.io/crates/wgpu
/// [`glutin`]: https://crates.io/crates/glutin
pub trait HasWindowHandle {
    /// Get a handle to the window.
    fn window_handle(&self) -> Result<WindowHandle<'_>, HandleError>;
}

impl<H: HasWindowHandle + ?Sized> HasWindowHandle for &H {
    fn window_handle(&self) -> Result<WindowHandle<'_>, HandleError> {
        (**self).window_handle()
    }
}

#[cfg(feature = "alloc")]
impl<H: HasWindowHandle + ?Sized> HasWindowHandle for alloc::boxed::Box<H> {
    fn window_handle(&self) -> Result<WindowHandle<'_>, HandleError> {
        (**self).window_handle()
    }
}

#[cfg(feature = "alloc")]
impl<H: HasWindowHandle + ?Sized> HasWindowHandle for alloc::rc::Rc<H> {
    fn window_handle(&self) -> Result<WindowHandle<'_>, HandleError> {
        (**self).window_handle()
    }
}

#[cfg(feature = "alloc")]
impl<H: HasWindowHandle + ?Sized> HasWindowHandle for alloc::sync::Arc<H> {
    fn window_handle(&self) -> Result<WindowHandle<'_>, HandleError> {
        (**self).window_handle()
    }
}

/// The handle to a window.
///
/// This is the primary return type of the [`HasWindowHandle`] trait. All *pointers* within this type
/// are guaranteed to be valid and not dangling for the lifetime of the handle. This excludes window IDs
/// like XIDs and the window ID for web platforms. See the documentation on the [`HasWindowHandle`]
/// trait for more information about these safety requirements.
///
/// This handle is guaranteed to be safe and valid. Get the underlying raw window handle with the
/// [`HasRawWindowHandle`] trait.
#[derive(Clone)]
pub struct WindowHandle<'a> {
    raw: RawWindowHandle,
    _active: ActiveHandle<'a>,
    _marker: PhantomData<&'a *const ()>,
}

impl fmt::Debug for WindowHandle<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("WindowHandle").field(&self.raw).finish()
    }
}

impl PartialEq for WindowHandle<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.raw == other.raw
    }
}

impl Eq for WindowHandle<'_> {}

impl Hash for WindowHandle<'_> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.raw.hash(state);
    }
}

impl<'a> WindowHandle<'a> {
    /// Borrow a `WindowHandle` from a [`RawWindowHandle`].
    ///
    /// # Safety
    ///
    /// The [`RawWindowHandle`] must be valid for the lifetime and the application must not be
    /// suspended. The [`Active`] object that the [`ActiveHandle`] was created from must be
    /// associated directly with the display that the window handle is associated with.
    pub unsafe fn borrow_raw(raw: RawWindowHandle, active: ActiveHandle<'a>) -> Self {
        Self {
            raw,
            _active: active,
            _marker: PhantomData,
        }
    }
}

unsafe impl HasRawWindowHandle for WindowHandle<'_> {
    fn raw_window_handle(&self) -> RawWindowHandle {
        self.raw
    }
}

impl HasWindowHandle for WindowHandle<'_> {
    fn window_handle(&self) -> Result<Self, HandleError> {
        Ok(self.clone())
    }
}

/// The error type returned when a handle cannot be obtained.
#[derive(Debug)]
#[non_exhaustive]
pub enum HandleError {
    /// The handle is not currently active.
    ///
    /// See documentation on [`Active`] for more information.
    Inactive,
}

impl fmt::Display for HandleError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Inactive => write!(f, "the handle is not currently active"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for HandleError {}

/// ```compile_fail
/// use raw_window_handle::{Active, DisplayHandle, WindowHandle};
/// fn _assert<T: Send + Sync>() {}
/// _assert::<Active<'static>>();
/// _assert::<DisplayHandle<'static>>();
/// _assert::<WindowHandle<'static>>();
/// ```
fn _not_send_or_sync() {}

#[cfg(not(any(target_os = "android", raw_window_handle_force_refcount)))]
#[cfg_attr(docsrs, doc(cfg(not(target_os = "android"))))]
mod imp {
    //! We don't need to refcount the handles, so we can just use no-ops.

    use core::cell::UnsafeCell;
    use core::marker::PhantomData;

    pub(super) struct Active;

    #[derive(Clone)]
    pub(super) struct ActiveHandle<'a> {
        _marker: PhantomData<&'a UnsafeCell<()>>,
    }

    impl Active {
        pub(super) const fn new() -> Self {
            Self
        }

        pub(super) fn handle(&self) -> Option<ActiveHandle<'_>> {
            // SAFETY: The handle is always active.
            Some(unsafe { ActiveHandle::new_unchecked() })
        }

        pub(super) unsafe fn set_active(&self) {}

        pub(super) fn set_inactive(&self) {}
    }

    impl ActiveHandle<'_> {
        pub(super) unsafe fn new_unchecked() -> Self {
            Self {
                _marker: PhantomData,
            }
        }
    }

    impl Drop for ActiveHandle<'_> {
        fn drop(&mut self) {
            // Done for consistency with the refcounted version.
        }
    }

    impl super::ActiveHandle<'_> {
        /// Create a new `ActiveHandle`.
        ///
        /// This is safe because the handle is always active.
        ///
        /// # Example
        ///
        /// ```
        /// use raw_window_handle::ActiveHandle;
        /// let handle = ActiveHandle::new();
        /// ```
        #[allow(clippy::new_without_default)]
        pub fn new() -> Self {
            // SAFETY: The handle is always active.
            unsafe { super::ActiveHandle::new_unchecked() }
        }
    }
}

#[cfg(any(target_os = "android", raw_window_handle_force_refcount))]
#[cfg_attr(docsrs, doc(cfg(any(target_os = "android"))))]
mod imp {
    //! We need to refcount the handles, so we use an `RwLock` to do so.

    use std::sync::{RwLock, RwLockReadGuard};

    pub(super) struct Active {
        active: RwLock<bool>,
    }

    pub(super) struct ActiveHandle<'a> {
        inner: Option<Inner<'a>>,
    }

    struct Inner<'a> {
        _read_guard: RwLockReadGuard<'a, bool>,
        active: &'a Active,
    }

    impl Clone for ActiveHandle<'_> {
        fn clone(&self) -> Self {
            Self {
                inner: self.inner.as_ref().map(|inner| Inner {
                    _read_guard: inner.active.active.read().unwrap(),
                    active: inner.active,
                }),
            }
        }
    }

    impl Active {
        pub(super) const fn new() -> Self {
            Self {
                active: RwLock::new(false),
            }
        }

        pub(super) fn handle(&self) -> Option<ActiveHandle<'_>> {
            let active = self.active.read().ok()?;
            if !*active {
                return None;
            }

            Some(ActiveHandle {
                inner: Some(Inner {
                    _read_guard: active,
                    active: self,
                }),
            })
        }

        pub(super) unsafe fn set_active(&self) {
            *self.active.write().unwrap() = true;
        }

        pub(super) fn set_inactive(&self) {
            *self.active.write().unwrap() = false;
        }
    }

    impl ActiveHandle<'_> {
        pub(super) unsafe fn new_unchecked() -> Self {
            Self { inner: None }
        }
    }
}
