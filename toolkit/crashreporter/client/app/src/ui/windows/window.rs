/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Types and helpers relating to windows and window classes.

use super::Font;
use super::WideString;
use std::cell::RefCell;
use windows_sys::Win32::{
    Foundation::{HINSTANCE, HWND, LPARAM, LRESULT, WPARAM},
    Graphics::Gdi::{self, HBRUSH},
    UI::WindowsAndMessaging::{self as win, HCURSOR, HICON},
};

/// Types representing a window class.
pub trait WindowClass: Sized + 'static {
    fn class_name() -> WideString;

    fn builder(self, module: HINSTANCE) -> WindowBuilder<'static, Self> {
        WindowBuilder {
            name: None,
            style: None,
            x: 0,
            y: 0,
            width: 0,
            height: 0,
            parent: None,
            child_id: 0,
            module,
            data: self,
        }
    }
}

/// Window classes which have their own message handler.
///
/// A type implementing this trait provides its data to a single window.
///
/// `register` must be called before use.
pub trait CustomWindowClass: WindowClass {
    /// Handle a message.
    fn message(
        data: &RefCell<Self>,
        hwnd: HWND,
        umsg: u32,
        wparam: WPARAM,
        lparam: LPARAM,
    ) -> Option<LRESULT>;

    /// The class's default background brush.
    fn background() -> HBRUSH {
        (Gdi::COLOR_3DFACE + 1) as HBRUSH
    }

    /// The class's default cursor.
    fn cursor() -> HCURSOR {
        unsafe { win::LoadCursorW(0, win::IDC_ARROW) }
    }

    /// The class's default icon.
    fn icon() -> HICON {
        0
    }

    /// Register the class.
    fn register(module: HINSTANCE) -> anyhow::Result<()> {
        unsafe extern "system" fn wnd_proc<W: CustomWindowClass>(
            hwnd: HWND,
            umsg: u32,
            wparam: WPARAM,
            lparam: LPARAM,
        ) -> LRESULT {
            if umsg == win::WM_CREATE {
                let create_struct = &*(lparam as *const win::CREATESTRUCTW);
                success!(lasterror win::SetWindowLongPtrW(hwnd, 0, create_struct.lpCreateParams as _));
                // Changes made with SetWindowLongPtr don't take effect until SetWindowPos is called...
                success!(nonzero win::SetWindowPos(
                    hwnd,
                    win::HWND_TOP,
                    0,
                    0,
                    0,
                    0,
                    win::SWP_NOMOVE | win::SWP_NOSIZE | win::SWP_NOZORDER | win::SWP_FRAMECHANGED,
                ));

                // For reasons that don't make much sense, icons scale poorly unless set with
                // `WM_SETICON` (see bug 1891920).
                let icon = W::icon();
                if icon != 0 {
                    // The return value doesn't indicate failures.
                    win::SendMessageW(hwnd, win::WM_SETICON, win::ICON_SMALL as _, icon);
                    win::SendMessageW(hwnd, win::WM_SETICON, win::ICON_BIG as _, icon);
                }
            }

            let result = unsafe { W::get(hwnd).as_ref() }
                .and_then(|data| W::message(data, hwnd, umsg, wparam, lparam));
            if umsg == win::WM_DESTROY {
                drop(Box::from_raw(
                    win::GetWindowLongPtrW(hwnd, 0) as *mut RefCell<W>
                ));
            }
            result.unwrap_or_else(|| win::DefWindowProcW(hwnd, umsg, wparam, lparam))
        }

        let class_name = Self::class_name();
        let window_class = win::WNDCLASSW {
            lpfnWndProc: Some(wnd_proc::<Self>),
            hInstance: module,
            lpszClassName: class_name.pcwstr(),
            hbrBackground: Self::background(),
            hIcon: Self::icon(),
            hCursor: Self::cursor(),
            cbWndExtra: std::mem::size_of::<isize>() as i32,
            ..unsafe { std::mem::zeroed() }
        };

        if unsafe { win::RegisterClassW(&window_class) } == 0 {
            anyhow::bail!("RegisterClassW failed")
        }
        Ok(())
    }

    /// Get the window data from a window created with this class.
    ///
    /// # Safety
    /// This must only be called on window handles which were created with this class.
    unsafe fn get(hwnd: HWND) -> *const RefCell<Self> {
        win::GetWindowLongPtrW(hwnd, 0) as *const RefCell<Self>
    }
}

/// Types that can be stored as associated window data.
pub trait WindowData: Sized {
    fn to_ptr(self) -> *mut RefCell<Self> {
        std::ptr::null_mut()
    }
}

impl<T: CustomWindowClass> WindowData for T {
    fn to_ptr(self) -> *mut RefCell<Self> {
        Box::into_raw(Box::new(RefCell::new(self)))
    }
}

macro_rules! basic_window_classes {
    () => {};
    ( $(#[$attr:meta])* struct $name:ident => $class:expr; $($rest:tt)* ) => {
        #[derive(Default)]
        $(#[$attr])*
        struct $name;

        impl $crate::ui::ui_impl::window::WindowClass for $name {
            fn class_name() -> $crate::ui::ui_impl::WideString {
                $crate::ui::ui_impl::WideString::new($class)
            }
        }

        impl $crate::ui::ui_impl::window::WindowData for $name {}

        $crate::ui::ui_impl::window::basic_window_classes!($($rest)*);
    }
}

pub(crate) use basic_window_classes;

pub struct WindowBuilder<'a, W> {
    name: Option<&'a WideString>,
    style: Option<u32>,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    parent: Option<HWND>,
    child_id: i32,
    module: HINSTANCE,
    data: W,
}

impl<'a, W> WindowBuilder<'a, W> {
    #[must_use]
    pub fn name<'b>(self, s: &'b WideString) -> WindowBuilder<'b, W> {
        WindowBuilder {
            name: Some(s),
            style: self.style,
            x: self.x,
            y: self.y,
            width: self.width,
            height: self.height,
            parent: self.parent,
            child_id: self.child_id,
            module: self.module,
            data: self.data,
        }
    }

    #[must_use]
    pub fn style(mut self, d: u32) -> Self {
        self.style = Some(d);
        self
    }

    #[must_use]
    pub fn add_style(mut self, d: u32) -> Self {
        *self.style.get_or_insert(0) |= d;
        self
    }

    #[must_use]
    pub fn pos(mut self, x: i32, y: i32) -> Self {
        self.x = x;
        self.y = y;
        self
    }

    #[must_use]
    pub fn size(mut self, width: i32, height: i32) -> Self {
        self.width = width;
        self.height = height;
        self
    }

    #[must_use]
    pub fn parent(mut self, parent: HWND) -> Self {
        self.parent = Some(parent);
        self
    }

    #[must_use]
    pub fn child_id(mut self, id: i32) -> Self {
        self.child_id = id;
        self
    }

    pub fn create(self) -> Window<W>
    where
        W: WindowClass + WindowData,
    {
        let class_name = W::class_name();
        let handle = unsafe {
            win::CreateWindowExW(
                0,
                class_name.pcwstr(),
                self.name.map(|n| n.pcwstr()).unwrap_or(std::ptr::null()),
                self.style.unwrap_or_default(),
                self.x,
                self.y,
                self.width,
                self.height,
                self.parent.unwrap_or_default(),
                self.child_id as _,
                self.module,
                self.data.to_ptr() as _,
            )
        };
        assert!(handle != 0);

        Window {
            handle,
            child_id: self.child_id,
            font_set: false,
            _class: std::marker::PhantomData,
        }
    }
}

/// A window handle with a known class type.
///
/// Without a type parameter (defaulting to `()`), the window handle is generic (class type
/// unknown).
pub struct Window<W: 'static = ()> {
    pub handle: HWND,
    pub child_id: i32,
    font_set: bool,
    _class: std::marker::PhantomData<&'static RefCell<W>>,
}

impl<W: CustomWindowClass> Window<W> {
    /// Get the window data of the window.
    #[allow(dead_code)]
    pub fn data(&self) -> &RefCell<W> {
        unsafe { W::get(self.handle).as_ref().unwrap() }
    }
}

impl<W> Window<W> {
    /// Get a generic window handle.
    pub fn generic(self) -> Window {
        Window {
            handle: self.handle,
            child_id: self.child_id,
            font_set: self.font_set,
            _class: std::marker::PhantomData,
        }
    }

    /// Set a window's font.
    pub fn set_font(&mut self, font: &Font) {
        unsafe { win::SendMessageW(self.handle, win::WM_SETFONT, **font as _, 1 as _) };
        self.font_set = true;
    }

    /// Set a window's font if not already set.
    pub fn set_default_font(&mut self, font: &Font) {
        if !self.font_set {
            self.set_font(font);
        }
    }
}
