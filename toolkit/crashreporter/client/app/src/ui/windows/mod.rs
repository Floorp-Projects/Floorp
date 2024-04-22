/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A UI using the windows API.
//!
//! This UI contains some edge cases that aren't implemented, for instance:
//! * there are a few cases where specific hierarchies are handled differently (e.g. a Button
//!   containing Label, Scroll behavior, etc).
//! * not all controls handle all Property variants (e.g. Checkbox doesn't handle ReadOnly, TextBox
//!   doesn't handle Binding, etc).
//!
//! The error handling is also a _little_ fast-and-loose, as many functions return an error value
//! that is acceptable to following logic (though it still would be a good idea to improve this).
//!
//! The rendering treats VBox, HBox, and Scroll as strictly layout-only: they do not create any
//! associated windows, and the layout logic handles their behavior.

// Our windows-targets doesn't link uxtheme correctly for GetThemeSysFont/GetThemeSysColor.
// This was working in windows-sys 0.48.
#[link(name = "uxtheme", kind = "static")]
extern "C" {}

use super::model::{self, Application, Element, ElementStyle, TypedElement};
use crate::data::Property;
use font::Font;
use once_cell::sync::Lazy;
use quit_token::QuitToken;
use std::cell::RefCell;
use std::collections::HashMap;
use std::pin::Pin;
use std::rc::Rc;
use widestring::WideString;
use window::{CustomWindowClass, Window, WindowBuilder};
use windows_sys::Win32::{
    Foundation::{HINSTANCE, HWND, LPARAM, LRESULT, RECT, WPARAM},
    Graphics::Gdi,
    System::{LibraryLoader::GetModuleHandleW, SystemServices, Threading::GetCurrentThreadId},
    UI::{Controls, Input::KeyboardAndMouse, Shell, WindowsAndMessaging as win},
};

macro_rules! success {
    ( nonzero $e:expr ) => {{
        let value = $e;
        assert_ne!(value, 0);
        value
    }};
    ( lasterror $e:expr ) => {{
        unsafe { windows_sys::Win32::Foundation::SetLastError(0) };
        let value = $e;
        assert!(value != 0 || windows_sys::Win32::Foundation::GetLastError() == 0);
        value
    }};
    ( hresult $e:expr ) => {
        assert_eq!($e, windows_sys::Win32::Foundation::S_OK);
    };
    ( pointer $e:expr ) => {{
        let ptr = $e;
        assert_ne!(ptr, 0);
        ptr
    }};
}

mod font;
mod gdi;
mod layout;
mod quit_token;
mod twoway;
mod widestring;
mod window;

/// A Windows API UI implementation.
pub struct UI {
    thread_id: u32,
}

/// Custom user messages.
#[repr(u32)]
enum UserMessage {
    Invoke = win::WM_USER,
}

fn get_invoke(msg: &win::MSG) -> Option<Box<model::InvokeFn>> {
    if msg.message == UserMessage::Invoke as u32 {
        Some(unsafe { Box::from_raw(msg.lParam as *mut model::InvokeFn) })
    } else {
        None
    }
}

impl UI {
    pub fn run_loop(&self, app: Application) {
        // Initialize common controls.
        {
            let icc = Controls::INITCOMMONCONTROLSEX {
                dwSize: std::mem::size_of::<Controls::INITCOMMONCONTROLSEX>() as _,
                // Buttons, edit controls, and static controls are all included in 'standard'.
                dwICC: Controls::ICC_STANDARD_CLASSES | Controls::ICC_PROGRESS_CLASS,
            };
            success!(nonzero unsafe { Controls::InitCommonControlsEx(&icc) });
        }

        // Enable font smoothing (per
        // https://learn.microsoft.com/en-us/windows/win32/gdi/cleartype-antialiasing ).
        unsafe {
            // We don't check for failure on these, they are best-effort.
            win::SystemParametersInfoW(
                win::SPI_SETFONTSMOOTHING,
                1,
                std::ptr::null_mut(),
                win::SPIF_UPDATEINIFILE | win::SPIF_SENDCHANGE,
            );
            win::SystemParametersInfoW(
                win::SPI_SETFONTSMOOTHINGTYPE,
                0,
                win::FE_FONTSMOOTHINGCLEARTYPE as _,
                win::SPIF_UPDATEINIFILE | win::SPIF_SENDCHANGE,
            );
        }

        // Enable correct layout direction.
        if unsafe { win::SetProcessDefaultLayout(if app.rtl { Gdi::LAYOUT_RTL } else { 0 }) } == 0 {
            log::warn!("failed to set process layout direction");
        }

        let module: HINSTANCE = unsafe { GetModuleHandleW(std::ptr::null()) };

        // Register custom classes.
        AppWindow::register(module).expect("failed to register AppWindow window class");

        {
            // The quit token is cloned for each top-level window and dropped at the end of this
            // scope.
            let quit_token = QuitToken::new();

            for window in app.windows {
                let name = WideString::new(window.element_type.title.as_str());
                let w = top_level_window(
                    module,
                    AppWindow::new(
                        WindowRenderer::new(module, window.element_type, &window.style),
                        Some(quit_token.clone()),
                    ),
                    &name,
                    &window.style,
                );

                unsafe { win::ShowWindow(w.handle, win::SW_NORMAL) };
                unsafe { Gdi::UpdateWindow(w.handle) };
            }
        }

        // Run the event loop.
        let mut msg = unsafe { std::mem::zeroed::<win::MSG>() };
        while unsafe { win::GetMessageW(&mut msg, 0, 0, 0) } > 0 {
            if let Some(f) = get_invoke(&msg) {
                f();
                continue;
            }

            unsafe {
                // IsDialogMessageW is necessary to handle niceties like tab navigation
                if win::IsDialogMessageW(win::GetAncestor(msg.hwnd, win::GA_ROOT), &mut msg) == 0 {
                    win::TranslateMessage(&msg);
                    win::DispatchMessageW(&msg);
                }
            }
        }

        // Flush queue to properly drop late invokes (this is a very unlikely case)
        while unsafe { win::PeekMessageW(&mut msg, 0, 0, 0, win::PM_REMOVE) } > 0 {
            if let Some(f) = get_invoke(&msg) {
                drop(f);
            }
        }
    }

    pub fn invoke(&self, f: model::InvokeFn) {
        let ptr: *mut model::InvokeFn = Box::into_raw(Box::new(f));
        if unsafe {
            win::PostThreadMessageW(self.thread_id, UserMessage::Invoke as u32, 0, ptr as _)
        } == 0
        {
            let _ = unsafe { Box::from_raw(ptr) };
            log::warn!("failed to invoke function on thread message queue");
        }
    }
}

impl Default for UI {
    fn default() -> Self {
        UI {
            thread_id: unsafe { GetCurrentThreadId() },
        }
    }
}

/// A reference to an Element.
#[derive(PartialEq, Eq, Hash, Clone, Copy)]
struct ElementRef(*const Element);

impl ElementRef {
    pub fn new(element: &Element) -> Self {
        ElementRef(element as *const Element)
    }

    /// # Safety
    /// You must ensure the reference is still valid.
    pub unsafe fn get(&self) -> &Element {
        &*self.0
    }
}

// Equivalent of win32 HIWORD macro
fn hiword(v: u32) -> u16 {
    (v >> 16) as u16
}

// Equivalent of win32 LOWORD macro
fn loword(v: u32) -> u16 {
    v as u16
}

// Equivalent of win32 MAKELONG macro
fn makelong(low: u16, high: u16) -> u32 {
    (high as u32) << 16 | low as u32
}

fn top_level_window<W: window::WindowClass + window::WindowData>(
    module: HINSTANCE,
    class: W,
    title: &WideString,
    style: &ElementStyle,
) -> Window<W> {
    class
        .builder(module)
        .name(title)
        .style(win::WS_OVERLAPPEDWINDOW)
        .pos(win::CW_USEDEFAULT, win::CW_USEDEFAULT)
        .size(
            style
                .horizontal_size_request
                .and_then(|i| i.try_into().ok())
                .unwrap_or(win::CW_USEDEFAULT),
            style
                .vertical_size_request
                .and_then(|i| i.try_into().ok())
                .unwrap_or(win::CW_USEDEFAULT),
        )
        .create()
}

window::basic_window_classes! {
    /// Static control (text, image, etc) class.
    struct Static => "STATIC";

    /// Button control class.
    struct Button => "BUTTON";

    /// Edit control class.
    struct Edit => "EDIT";

    /// Progress control class.
    struct Progress => "msctls_progress32";
}

/// A top-level application window.
///
/// This is used for the main window and modal windows.
struct AppWindow {
    renderer: WindowRenderer,
    _quit_token: Option<QuitToken>,
}

impl AppWindow {
    pub fn new(renderer: WindowRenderer, quit_token: Option<QuitToken>) -> Self {
        AppWindow {
            renderer,
            _quit_token: quit_token,
        }
    }
}

impl window::WindowClass for AppWindow {
    fn class_name() -> WideString {
        WideString::new("App Window")
    }
}

impl CustomWindowClass for AppWindow {
    fn icon() -> win::HICON {
        static ICON: Lazy<win::HICON> = Lazy::new(|| unsafe {
            // If CreateIconFromResource fails it returns NULL, which is fine (a default icon will be
            // used).
            win::CreateIconFromResource(
                // We take advantage of the fact that since Windows Vista, an RT_ICON resource entry
                // can simply be a PNG image.
                super::icon::PNG_DATA.as_ptr(),
                super::icon::PNG_DATA.len() as u32,
                true.into(),
                // The 0x00030000 constant isn't available anywhere; the docs basically say to just
                // pass it...
                0x00030000,
            )
        });

        *ICON
    }

    fn message(
        data: &RefCell<Self>,
        hwnd: HWND,
        umsg: u32,
        wparam: WPARAM,
        lparam: LPARAM,
    ) -> Option<LRESULT> {
        let me = data.borrow();
        let model = me.renderer.model();
        match umsg {
            win::WM_CREATE => {
                if let Some(close) = &model.close {
                    close.subscribe(move |&()| unsafe {
                        win::SendMessageW(hwnd, win::WM_CLOSE, 0, 0);
                    });
                }

                let mut renderer = me.renderer.child_renderer(hwnd);
                if let Some(child) = &model.content {
                    renderer.render_child(child);
                }

                drop(model);
                let children = std::mem::take(&mut me.renderer.model_mut().children);
                for child in children {
                    renderer.render_window(child);
                }
            }
            win::WM_CLOSE => {
                if model.modal {
                    // Modal windows should hide themselves rather than closing/destroying.
                    unsafe { win::ShowWindow(hwnd, win::SW_HIDE) };
                    return Some(0);
                }
            }
            win::WM_SHOWWINDOW => {
                if model.modal {
                    // Modal windows should disable/enable their parent as they are shown/hid,
                    // respectively.
                    let shown = wparam != 0;
                    unsafe {
                        KeyboardAndMouse::EnableWindow(
                            win::GetWindow(hwnd, win::GW_OWNER),
                            (!shown).into(),
                        )
                    };
                    return Some(0);
                }
            }
            win::WM_GETMINMAXINFO => {
                let minmaxinfo = unsafe { (lparam as *mut win::MINMAXINFO).as_mut().unwrap() };
                minmaxinfo.ptMinTrackSize.x = me.renderer.min_size.0.try_into().unwrap();
                minmaxinfo.ptMinTrackSize.y = me.renderer.min_size.1.try_into().unwrap();
                return Some(0);
            }
            win::WM_SIZE => {
                // When resized, recompute the layout.
                let width = loword(lparam as _) as u32;
                let height = hiword(lparam as _) as u32;

                if let Some(child) = &model.content {
                    me.renderer.layout(child, width, height);
                    unsafe { Gdi::UpdateWindow(hwnd) };
                }
                return Some(0);
            }
            win::WM_GETFONT => return Some(**me.renderer.font() as _),
            win::WM_COMMAND => {
                let child = lparam as HWND;
                let windows = me.renderer.windows.borrow();
                if let Some(&element) = windows.reverse().get(&child) {
                    // # Safety
                    // The ElementRefs all pertain to the model stored in the renderer.
                    let element = unsafe { element.get() };
                    // Handle button presses.
                    use model::ElementType::*;
                    match &element.element_type {
                        Button(model::Button { click, .. }) => {
                            let code = hiword(wparam as _) as u32;
                            if code == win::BN_CLICKED {
                                click.fire(&());
                                return Some(0);
                            }
                        }
                        Checkbox(model::Checkbox { checked, .. }) => {
                            let code = hiword(wparam as _) as u32;
                            if code == win::BN_CLICKED {
                                let check_state =
                                    unsafe { win::SendMessageW(child, win::BM_GETCHECK, 0, 0) };
                                if let Property::Binding(s) = checked {
                                    *s.borrow_mut() = check_state == Controls::BST_CHECKED as isize;
                                }
                                return Some(0);
                            }
                        }
                        _ => (),
                    }
                }
            }
            _ => (),
        }
        None
    }
}

/// State used while creating and updating windows.
struct WindowRenderer {
    // We wrap with an Rc to get weak references in property callbacks (like that of
    // `ElementStyle::visible`).
    inner: Rc<WindowRendererInner>,
}

impl std::ops::Deref for WindowRenderer {
    type Target = WindowRendererInner;

    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

struct WindowRendererInner {
    pub module: HINSTANCE,
    /// The model is pinned and boxed to ensure that references in `windows` remain valid.
    ///
    /// We need to keep the model around so we can correctly perform layout as the window size
    /// changes. Unfortunately the win32 API doesn't have any nice ways to automatically perform
    /// layout.
    pub model: RefCell<Pin<Box<model::Window>>>,
    pub min_size: (u32, u32),
    /// Mapping between model elements and windows.
    ///
    /// Element references pertain to elements in `model`.
    pub windows: RefCell<twoway::TwoWay<ElementRef, HWND>>,
    pub font: Font,
    pub bold_font: Font,
}

impl WindowRenderer {
    pub fn new(module: HINSTANCE, model: model::Window, style: &model::ElementStyle) -> Self {
        WindowRenderer {
            inner: Rc::new(WindowRendererInner {
                module,
                model: RefCell::new(Box::pin(model)),
                min_size: (
                    style.horizontal_size_request.unwrap_or(0),
                    style.vertical_size_request.unwrap_or(0),
                ),
                windows: Default::default(),
                font: Font::caption(),
                bold_font: Font::caption_bold().unwrap_or_else(Font::caption),
            }),
        }
    }

    pub fn child_renderer(&self, window: HWND) -> WindowChildRenderer {
        WindowChildRenderer {
            renderer: &self.inner,
            window,
            child_id: 0,
            scroll: false,
        }
    }

    pub fn layout(&self, element: &Element, max_width: u32, max_height: u32) {
        layout::Layout::new(self.inner.windows.borrow().forward())
            .layout(element, max_width, max_height);
    }

    pub fn model(&self) -> std::cell::Ref<'_, model::Window> {
        std::cell::Ref::map(self.inner.model.borrow(), |b| &**b)
    }

    pub fn model_mut(&self) -> std::cell::RefMut<'_, model::Window> {
        std::cell::RefMut::map(self.inner.model.borrow_mut(), |b| &mut **b)
    }

    pub fn font(&self) -> &Font {
        &self.inner.font
    }
}

struct WindowChildRenderer<'a> {
    renderer: &'a Rc<WindowRendererInner>,
    window: HWND,
    child_id: i32,
    scroll: bool,
}

impl<'a> WindowChildRenderer<'a> {
    fn add_child<W: window::WindowClass>(&mut self, class: W) -> WindowBuilder<W> {
        let builder = class
            .builder(self.renderer.module)
            .style(win::WS_CHILD | win::WS_VISIBLE)
            .parent(self.window)
            .child_id(self.child_id);
        self.child_id += 1;
        builder
    }

    fn add_window<W: window::WindowClass>(&mut self, class: W) -> WindowBuilder<W> {
        class
            .builder(self.renderer.module)
            .style(win::WS_OVERLAPPEDWINDOW)
            .pos(win::CW_USEDEFAULT, win::CW_USEDEFAULT)
            .parent(self.window)
    }

    fn render_window(&mut self, model: TypedElement<model::Window>) -> Window {
        let name = WideString::new(model.element_type.title.as_str());
        let style = model.style;
        let w = self
            .add_window(AppWindow::new(
                WindowRenderer::new(self.renderer.module, model.element_type, &style),
                None,
            ))
            .size(
                style
                    .horizontal_size_request
                    .and_then(|i| i.try_into().ok())
                    .unwrap_or(win::CW_USEDEFAULT),
                style
                    .vertical_size_request
                    .and_then(|i| i.try_into().ok())
                    .unwrap_or(win::CW_USEDEFAULT),
            )
            .name(&name)
            .create();

        enabled_property(&style.enabled, w.handle);

        let hwnd = w.handle;
        let set_visible = move |visible| unsafe {
            win::ShowWindow(hwnd, if visible { win::SW_SHOW } else { win::SW_HIDE });
        };

        match &style.visible {
            Property::Static(false) => set_visible(false),
            Property::Binding(s) => {
                s.on_change(move |v| set_visible(*v));
                if !*s.borrow() {
                    set_visible(false);
                }
            }
            _ => (),
        }

        w.generic()
    }

    fn render_child(&mut self, element: &Element) {
        if let Some(mut window) = self.render_element_type(&element.element_type) {
            window.set_default_font(&self.renderer.font);

            // Store the element to handle mapping.
            self.renderer
                .windows
                .borrow_mut()
                .insert(ElementRef::new(element), window.handle);

            enabled_property(&element.style.enabled, window.handle);
        }

        // Handle visibility properties.
        match &element.style.visible {
            Property::Static(false) => {
                set_visibility(element, false, self.renderer.windows.borrow().forward())
            }
            Property::Binding(s) => {
                let weak_renderer = Rc::downgrade(self.renderer);
                let element_ref = ElementRef::new(element);
                let parent = self.window;
                s.on_change(move |visible| {
                    let Some(renderer) = weak_renderer.upgrade() else {
                        return;
                    };
                    // # Safety
                    // ElementRefs are valid as long as the renderer is (and we have a strong
                    // reference to it).
                    let element = unsafe { element_ref.get() };
                    set_visibility(element, *visible, renderer.windows.borrow().forward());
                    // Send WM_SIZE so that the parent recomputes the layout.
                    unsafe {
                        let mut rect = std::mem::zeroed::<RECT>();
                        win::GetClientRect(parent, &mut rect);
                        win::SendMessageW(
                            parent,
                            win::WM_SIZE,
                            0,
                            makelong(
                                (rect.right - rect.left) as u16,
                                (rect.bottom - rect.top) as u16,
                            ) as isize,
                        );
                    }
                });
                if !*s.borrow() {
                    set_visibility(element, false, self.renderer.windows.borrow().forward());
                }
            }
            _ => (),
        }
    }

    fn render_element_type(&mut self, element_type: &model::ElementType) -> Option<Window> {
        use model::ElementType as ET;
        match element_type {
            ET::Label(model::Label { text, bold }) => {
                let mut window = match text {
                    Property::Static(text) => {
                        let text = WideString::new(text.as_str());
                        self.add_child(Static)
                            .name(&text)
                            .add_style(SystemServices::SS_LEFT | SystemServices::SS_NOPREFIX)
                            .create()
                    }
                    Property::Binding(b) => {
                        let text = WideString::new(b.borrow().as_str());
                        let window = self
                            .add_child(Static)
                            .name(&text)
                            .add_style(SystemServices::SS_LEFT | SystemServices::SS_NOPREFIX)
                            .create();
                        let handle = window.handle;
                        b.on_change(move |text| {
                            let text = WideString::new(text.as_str());
                            unsafe { win::SetWindowTextW(handle, text.pcwstr()) };
                        });
                        window
                    }
                    Property::ReadOnly(_) => {
                        unimplemented!("ReadOnly property not supported for Label::text")
                    }
                };
                if *bold {
                    window.set_font(&self.renderer.bold_font);
                }
                Some(window.generic())
            }
            ET::TextBox(model::TextBox {
                placeholder,
                content,
                editable,
            }) => {
                let scroll = self.scroll;
                let window = self
                    .add_child(Edit)
                    .add_style(
                        (win::ES_LEFT
                            | win::ES_MULTILINE
                            | win::ES_WANTRETURN
                            | if *editable { 0 } else { win::ES_READONLY })
                            as u32
                            | win::WS_BORDER
                            | win::WS_TABSTOP
                            | if scroll { win::WS_VSCROLL } else { 0 },
                    )
                    .create();

                fn to_control_text(s: &str) -> String {
                    s.replace("\n", "\r\n")
                }

                fn from_control_text(s: &str) -> String {
                    s.replace("\r\n", "\n")
                }

                struct SubClassData {
                    placeholder: Option<WideString>,
                }

                // EM_SETCUEBANNER doesn't work with multiline edit controls (for no particular
                // reason?), so we have to draw it ourselves.
                unsafe extern "system" fn subclass_proc(
                    hwnd: HWND,
                    msg: u32,
                    wparam: WPARAM,
                    lparam: LPARAM,
                    _uidsubclass: usize,
                    dw_ref_data: usize,
                ) -> LRESULT {
                    let ret = Shell::DefSubclassProc(hwnd, msg, wparam, lparam);
                    if msg == win::WM_PAINT
                        && KeyboardAndMouse::GetFocus() != hwnd
                        && win::GetWindowTextLengthW(hwnd) == 0
                    {
                        let data = (dw_ref_data as *const SubClassData).as_ref().unwrap();
                        if let Some(placeholder) = &data.placeholder {
                            let mut rect = std::mem::zeroed::<RECT>();
                            win::GetClientRect(hwnd, &mut rect);
                            Gdi::InflateRect(&mut rect, -2, -2);

                            let dc = gdi::DC::new(hwnd).expect("failed to create GDI DC");
                            dc.with_object_selected(
                                win::SendMessageW(hwnd, win::WM_GETFONT, 0, 0) as _,
                                |hdc| {
                                    Gdi::SetTextColor(
                                        hdc,
                                        Controls::GetThemeSysColor(0, Gdi::COLOR_GRAYTEXT),
                                    );
                                    Gdi::SetBkMode(hdc, Gdi::TRANSPARENT as i32);
                                    success!(nonzero Gdi::DrawTextW(
                                        hdc,
                                        placeholder.pcwstr(),
                                        -1,
                                        &mut rect,
                                        Gdi::DT_LEFT | Gdi::DT_TOP | Gdi::DT_WORDBREAK,
                                    ));
                                },
                            )
                            .expect("failed to select font gdi object");
                        }
                    }

                    // Multiline edit controls capture the tab key. We want it to work as usual in
                    // the dialog (focusing the next input control).
                    if msg == win::WM_GETDLGCODE && wparam == KeyboardAndMouse::VK_TAB as usize {
                        return 0;
                    }

                    if msg == win::WM_DESTROY {
                        drop(unsafe { Box::from_raw(dw_ref_data as *mut SubClassData) });
                    }
                    return ret;
                }

                let subclassdata = Box::into_raw(Box::new(SubClassData {
                    placeholder: placeholder
                        .as_ref()
                        .map(|s| WideString::new(to_control_text(s))),
                }));

                unsafe {
                    Shell::SetWindowSubclass(
                        window.handle,
                        Some(subclass_proc),
                        0,
                        subclassdata as _,
                    );
                }

                // Set up content property.
                match content {
                    Property::ReadOnly(od) => {
                        let handle = window.handle;
                        od.register(move |target| {
                            // GetWindowText requires the buffer be large enough for the terminating
                            // null character (otherwise it truncates the string), but
                            // GetWindowTextLength returns the length without the null character, so we
                            // add 1.
                            let length = unsafe { win::GetWindowTextLengthW(handle) } + 1;
                            let mut buf = vec![0u16; length as usize];
                            unsafe { win::GetWindowTextW(handle, buf.as_mut_ptr(), length) };
                            buf.pop(); // null character; `String` doesn't want that
                            *target = from_control_text(&String::from_utf16_lossy(&buf));
                        });
                    }
                    Property::Static(s) => {
                        let text = WideString::new(to_control_text(s));
                        unsafe { win::SetWindowTextW(window.handle, text.pcwstr()) };
                    }
                    Property::Binding(b) => {
                        let handle = window.handle;
                        b.on_change(move |text| {
                            let text = WideString::new(to_control_text(text.as_str()));
                            unsafe { win::SetWindowTextW(handle, text.pcwstr()) };
                        });
                        let text = WideString::new(to_control_text(b.borrow().as_str()));
                        unsafe { win::SetWindowTextW(window.handle, text.pcwstr()) };
                    }
                }
                Some(window.generic())
            }
            ET::Scroll(model::Scroll { content }) => {
                if let Some(content) = content {
                    // Scrolling is implemented in a cooperative, non-universal way right now.
                    self.scroll = true;
                    self.render_child(content);
                    self.scroll = false;
                }
                None
            }
            ET::Button(model::Button { content, .. }) => {
                if let Some(ET::Label(model::Label {
                    text: Property::Static(text),
                    ..
                })) = content.as_ref().map(|e| &e.element_type)
                {
                    let text = WideString::new(text);

                    let window = self
                        .add_child(Button)
                        .add_style(win::BS_PUSHBUTTON as u32 | win::WS_TABSTOP)
                        .name(&text)
                        .create();
                    Some(window.generic())
                } else {
                    None
                }
            }
            ET::Checkbox(model::Checkbox { checked, label }) => {
                let label = label.as_ref().map(WideString::new);
                let mut builder = self
                    .add_child(Button)
                    .add_style((win::BS_AUTOCHECKBOX | win::BS_MULTILINE) as u32 | win::WS_TABSTOP);
                if let Some(label) = &label {
                    builder = builder.name(label);
                }
                let window = builder.create();

                fn set_check(handle: HWND, value: bool) {
                    unsafe {
                        win::SendMessageW(
                            handle,
                            win::BM_SETCHECK,
                            if value {
                                Controls::BST_CHECKED
                            } else {
                                Controls::BST_UNCHECKED
                            } as usize,
                            0,
                        );
                    }
                }

                match checked {
                    Property::Static(checked) => set_check(window.handle, *checked),
                    Property::Binding(s) => {
                        let handle = window.handle;
                        s.on_change(move |v| {
                            set_check(handle, *v);
                        });
                        set_check(window.handle, *s.borrow());
                    }
                    _ => unimplemented!("ReadOnly properties not supported for Checkbox"),
                }

                Some(window.generic())
            }
            ET::Progress(model::Progress { amount }) => {
                let window = self
                    .add_child(Progress)
                    .add_style(Controls::PBS_MARQUEE)
                    .create();

                fn set_amount(handle: HWND, value: Option<f32>) {
                    match value {
                        None => unsafe {
                            win::SendMessageW(handle, Controls::PBM_SETMARQUEE, 1, 0);
                        },
                        Some(v) => unsafe {
                            win::SendMessageW(handle, Controls::PBM_SETMARQUEE, 0, 0);
                            win::SendMessageW(
                                handle,
                                Controls::PBM_SETPOS,
                                (v.clamp(0f32, 1f32) * 100f32) as usize,
                                0,
                            );
                        },
                    }
                }

                match amount {
                    Property::Static(v) => set_amount(window.handle, *v),
                    Property::Binding(s) => {
                        let handle = window.handle;
                        s.on_change(move |v| set_amount(handle, *v));
                        set_amount(window.handle, *s.borrow());
                    }
                    _ => unimplemented!("ReadOnly properties not supported for Progress"),
                }

                Some(window.generic())
            }
            // VBox/HBox are virtual, their behaviors are implemented entirely in the renderer layout.
            // No need for additional windows.
            ET::VBox(model::VBox { items, .. }) => {
                for item in items {
                    self.render_child(item);
                }
                None
            }
            ET::HBox(model::HBox { items, .. }) => {
                for item in items {
                    self.render_child(item);
                }
                None
            }
        }
    }
}

/// Handle the enabled property.
///
/// This function assumes the default state of the window is enabled.
fn enabled_property(enabled: &Property<bool>, window: HWND) {
    match enabled {
        Property::Static(false) => unsafe {
            KeyboardAndMouse::EnableWindow(window, false.into());
        },
        Property::Binding(s) => {
            let handle = window;
            s.on_change(move |enabled| {
                unsafe { KeyboardAndMouse::EnableWindow(handle, (*enabled).into()) };
            });
            if !*s.borrow() {
                unsafe { KeyboardAndMouse::EnableWindow(window, false.into()) };
            }
        }
        _ => (),
    }
}

/// Set the visibility of the given element. This recurses down the element tree and hides children
/// as necessary.
fn set_visibility(element: &Element, visible: bool, windows: &HashMap<ElementRef, HWND>) {
    if let Some(&hwnd) = windows.get(&ElementRef::new(element)) {
        unsafe {
            win::ShowWindow(hwnd, if visible { win::SW_SHOW } else { win::SW_HIDE });
        }
    } else {
        match &element.element_type {
            model::ElementType::VBox(model::VBox { items, .. }) => {
                for item in items {
                    set_visibility(item, visible, windows);
                }
            }
            model::ElementType::HBox(model::HBox { items, .. }) => {
                for item in items {
                    set_visibility(item, visible, windows);
                }
            }
            model::ElementType::Scroll(model::Scroll {
                content: Some(content),
            }) => {
                set_visibility(&*content, visible, windows);
            }
            _ => (),
        }
    }
}
