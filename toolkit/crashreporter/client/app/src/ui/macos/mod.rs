/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A UI using the macos cocoa API.
//!
//! This UI contains some edge cases that aren't implemented, for instance:
//! * there are a few cases where specific hierarchies are handled differently (e.g. a Button
//!   containing Label), etc.
//! * not all controls handle all Property variants (e.g. Checkbox doesn't handle ReadOnly, Text
//!   doesn't handle Binding, etc).
//!
//! The rendering ignores `ElementStyle::margin` entirely, because
//! * `NSView` doesn't support margins (so working them into constraints would be a bit annoying),
//!   and
//! * `NSView.layoutMarginsGuide` results in a layout almost identical to what the margins (at the
//!   time of this writing) in the UI layouts are achieving.
//!
//! In a few cases, init or creation functions are called which _could_ return nil and are wrapped
//! in their type wrapper (as those functions return `instancetype`/`id`). We consider this safe
//! enough because it won't cause unsoundness (they are only passed to objc functions which can
//! take nil arguments) and the failure case is very unlikely.

#![allow(non_upper_case_globals)]

use self::objc::*;
use super::model::{self, Alignment, Application, Element, TypedElement};
use crate::data::Property;
use cocoa::{
    INSApplication, INSBox, INSButton, INSColor, INSControl, INSFont, INSLayoutAnchor,
    INSLayoutConstraint, INSLayoutDimension, INSLayoutGuide, INSMenu, INSMenuItem,
    INSMutableParagraphStyle, INSObject, INSProcessInfo, INSProgressIndicator, INSRunLoop,
    INSScrollView, INSStackView, INSText, INSTextContainer, INSTextField, INSTextView, INSView,
    INSWindow, NSArray_NSArrayCreation, NSAttributedString_NSExtendedAttributedString,
    NSDictionary_NSDictionaryCreation, NSRunLoop_NSRunLoopConveniences,
    NSStackView_NSStackViewGravityAreas, NSString_NSStringExtensionMethods,
    NSTextField_NSTextFieldConvenience, NSView_NSConstraintBasedLayoutInstallingConstraints,
    NSView_NSConstraintBasedLayoutLayering, NSView_NSSafeAreas, PNSObject,
};

/// https://developer.apple.com/documentation/foundation/1497293-string_encodings/nsutf8stringencoding?language=objc
const NSUTF8StringEncoding: cocoa::NSStringEncoding = 4;

/// Constant from NSCell.h
const NSControlStateValueOn: cocoa::NSControlStateValue = 1;

/// Constant from NSLayoutConstraint.h
const NSLayoutPriorityDefaultHigh: cocoa::NSLayoutPriority = 750.0;

mod objc;

/// A MacOS Cocoa UI implementation.
#[derive(Default)]
pub struct UI;

impl UI {
    pub fn run_loop(&self, app: Application) {
        let nsapp = unsafe { cocoa::NSApplication::sharedApplication() };

        Objc::<AppDelegate>::register();
        Objc::<Button>::register();
        Objc::<Checkbox>::register();
        Objc::<Window>::register();

        rc::autoreleasepool(|| {
            let delegate = AppDelegate::new(app).into_object();
            // Set delegate
            unsafe { nsapp.setDelegate_(delegate.instance as *mut _) };

            // Set up the main menu
            unsafe {
                let appname = read_nsstring(cocoa::NSProcessInfo::processInfo().processName());
                let mainmenu = StrongRef::new(cocoa::NSMenu::alloc());
                mainmenu.init();

                {
                    // We don't need a title for the app menu item nor menu; it will always come from
                    // the process name regardless of what we set.
                    let appmenuitem = StrongRef::new(cocoa::NSMenuItem::alloc()).autorelease();
                    appmenuitem.init();
                    mainmenu.addItem_(appmenuitem);

                    let appmenu = StrongRef::new(cocoa::NSMenu::alloc());
                    appmenu.init();

                    {
                        let quit = StrongRef::new(cocoa::NSMenuItem::alloc());
                        quit.initWithTitle_action_keyEquivalent_(
                            nsstring(&format!("Quit {appname}")),
                            sel!(terminate:),
                            nsstring("q"),
                        );
                        appmenu.addItem_(quit.autorelease());
                    }
                    appmenuitem.setSubmenu_(appmenu.autorelease());
                }
                {
                    let editmenuitem = StrongRef::new(cocoa::NSMenuItem::alloc()).autorelease();
                    editmenuitem.initWithTitle_action_keyEquivalent_(
                        nsstring("Edit"),
                        runtime::Sel::from_ptr(std::ptr::null()),
                        nsstring(""),
                    );
                    mainmenu.addItem_(editmenuitem);

                    let editmenu = StrongRef::new(cocoa::NSMenu::alloc());
                    editmenu.initWithTitle_(nsstring("Edit"));

                    let add_item = |name, selector, shortcut| {
                        let item = StrongRef::new(cocoa::NSMenuItem::alloc());
                        item.initWithTitle_action_keyEquivalent_(
                            nsstring(name),
                            selector,
                            nsstring(shortcut),
                        );
                        editmenu.addItem_(item.autorelease());
                    };

                    add_item("Undo", sel!(undo:), "z");
                    add_item("Redo", sel!(redo:), "Z");
                    editmenu.addItem_(cocoa::NSMenuItem::separatorItem());
                    add_item("Cut", sel!(cut:), "x");
                    add_item("Copy", sel!(copy:), "c");
                    add_item("Paste", sel!(paste:), "v");
                    add_item("Delete", sel!(delete:), "");
                    add_item("Select All", sel!(selectAll:), "a");

                    editmenuitem.setSubmenu_(editmenu.autorelease());
                }

                nsapp.setMainMenu_(mainmenu.autorelease());
            }

            // Run the main application loop
            unsafe { nsapp.run() };
        });
    }

    pub fn invoke(&self, f: model::InvokeFn) {
        // Blocks only take `Fn`, so we have to wrap the boxed function.
        let f = std::cell::RefCell::new(Some(f));
        enqueue(move || {
            if let Some(f) = f.borrow_mut().take() {
                f();
            }
        });
    }
}

fn enqueue<F: Fn() + 'static>(f: F) {
    let block = block::ConcreteBlock::new(f);
    // The block must be an RcBlock so addOperationWithBlock can retain it.
    // https://docs.rs/block/latest/block/#creating-blocks
    let block = block.copy();

    // We need to explicitly signal that the enqueued blocks can run in both the default mode (the
    // main loop) and modal mode, otherwise when modal windows are opened things get stuck.
    struct RunloopModes(cocoa::NSArray);

    impl RunloopModes {
        pub fn new() -> Self {
            unsafe {
                let objects: [cocoa::id; 2] = [
                    cocoa::NSDefaultRunLoopMode.0,
                    cocoa::NSModalPanelRunLoopMode.0,
                ];
                RunloopModes(
                    cocoa::NSArray(<cocoa::NSArray as NSArray_NSArrayCreation<
                        cocoa::NSRunLoopMode,
                    >>::arrayWithObjects_count_(
                        objects.as_slice().as_ptr() as *const *mut u64,
                        objects
                            .as_slice()
                            .len()
                            .try_into()
                            .expect("usize can't fit in u64"),
                    )),
                )
            }
        }
    }

    // # Safety
    // The array is static and cannot be changed.
    unsafe impl Sync for RunloopModes {}

    lazy_static::lazy_static! {
        static ref RUNLOOP_MODES: RunloopModes = RunloopModes::new();
    }

    unsafe {
        cocoa::NSRunLoop::mainRunLoop().performInModes_block_(RUNLOOP_MODES.0, &*block);
    }
}

#[repr(transparent)]
struct Rect(pub cocoa::NSRect);

unsafe impl Encode for Rect {
    fn encode() -> Encoding {
        unsafe { Encoding::from_str("{CGRect={CGPoint=dd}{CGSize=dd}}") }
    }
}

/// Create an NSString by copying a str.
fn nsstring(v: &str) -> cocoa::NSString {
    unsafe {
        StrongRef::new(cocoa::NSString(
            cocoa::NSString::alloc().initWithBytes_length_encoding_(
                v.as_ptr() as *const _,
                v.len().try_into().expect("usize can't fit in u64"),
                NSUTF8StringEncoding,
            ),
        ))
    }
    .autorelease()
}

/// Create a String by copying an NSString
fn read_nsstring(s: cocoa::NSString) -> String {
    let c_str = unsafe { std::ffi::CStr::from_ptr(s.UTF8String()) };
    c_str.to_str().expect("NSString isn't UTF8").to_owned()
}

fn nsrect<X: Into<f64>, Y: Into<f64>, W: Into<f64>, H: Into<f64>>(
    x: X,
    y: Y,
    w: W,
    h: H,
) -> cocoa::NSRect {
    cocoa::NSRect {
        origin: cocoa::NSPoint {
            x: x.into(),
            y: y.into(),
        },
        size: cocoa::NSSize {
            width: w.into(),
            height: h.into(),
        },
    }
}

struct AppDelegate {
    app: Option<Application>,
    windows: Vec<StrongRef<cocoa::NSWindow>>,
}

impl AppDelegate {
    pub fn new(app: Application) -> Self {
        AppDelegate {
            app: Some(app),
            windows: Default::default(),
        }
    }
}

objc_class! {
    impl AppDelegate: NSObject /*<NSApplicationDelegate>*/ {
        #[sel(applicationDidFinishLaunching:)]
        fn application_did_finish_launching(&mut self, _notification: Ptr<cocoa::NSNotification>) {
            // Activate the application (bringing windows to the active foreground later)
            unsafe { cocoa::NSApplication::sharedApplication().activateIgnoringOtherApps_(runtime::YES) };

            let mut first = true;
            let mut windows = WindowRenderer::default();
            let app = self.app.take().unwrap();
            windows.rtl = app.rtl;
            for window in app.windows {
                let w = windows.render(window);
                unsafe {
                    if first {
                        w.makeKeyAndOrderFront_(self.instance);
                        w.makeMainWindow();
                        first = false;
                    }
                }
            }
            self.windows = windows.unwrap();

        }

        #[sel(applicationShouldTerminateAfterLastWindowClosed:)]
        fn application_should_terminate_after_window_closed(&mut self, _app: Ptr<cocoa::NSApplication>) -> runtime::BOOL {
            runtime::YES
        }
    }
}

struct Window {
    modal: bool,
    title: String,
    style: model::ElementStyle,
}

objc_class! {
    impl Window: NSWindow /*<NSWindowDelegate>*/ {
        #[sel(init)]
        fn init(&mut self) -> cocoa::id {
            let style = &self.style;
            let title = &self.title;
            let w = cocoa::NSWindow(self.instance);

            unsafe {
                if w.initWithContentRect_styleMask_backing_defer_(
                    nsrect(
                        0,
                        0,
                        style.horizontal_size_request.unwrap_or(800),
                        style.vertical_size_request.unwrap_or(500),
                    ),
                    cocoa::NSWindowStyleMaskTitled
                        | cocoa::NSWindowStyleMaskClosable
                        | cocoa::NSWindowStyleMaskResizable
                        | cocoa::NSWindowStyleMaskMiniaturizable,
                    cocoa::NSBackingStoreBuffered,
                    runtime::NO,
                ).is_null() {
                    return std::ptr::null_mut();
                }

                w.setDelegate_(self.instance as _);

                if !title.is_empty() {
                    w.setTitle_(nsstring(title.as_str()));
                }
            }

            self.instance
        }

        #[sel(windowWillClose:)]
        fn window_will_close(&mut self, _notification: Ptr<cocoa::NSNotification>) {
            if self.modal {
                unsafe {
                    let nsapp = cocoa::NSApplication::sharedApplication();
                    nsapp.stopModal();
                }
            }
        }
    }
}

impl From<Objc<Window>> for cocoa::NSWindow {
    fn from(ptr: Objc<Window>) -> Self {
        cocoa::NSWindow(ptr.instance)
    }
}

struct Button {
    element: model::Button,
}

impl Button {
    pub fn with_title(self, title: &str) -> cocoa::NSButton {
        let obj = self.into_object();
        unsafe {
            let () = msg_send![obj.instance, setTitle: nsstring(title)];
        }
        // # Safety
        // NSButton is the superclass of Objc<Button>.
        unsafe { std::mem::transmute(obj.autorelease()) }
    }
}

objc_class! {
    impl Button: NSButton {
        #[sel(initWithFrame:)]
        fn init_with_frame(&mut self, frame_rect: Rect) -> cocoa::id {
            unsafe {
                let object: cocoa::id = msg_send![super(self.instance, class!(NSButton)), initWithFrame: frame_rect.0];
                if object.is_null() {
                    return object;
                }
                let () = msg_send![object, setBezelStyle: cocoa::NSBezelStyleRounded];
                let () = msg_send![object, setAction: sel!(didClick)];
                let () = msg_send![object, setTarget: object];
                object
            }
        }

        #[sel(didClick)]
        fn did_click(&mut self) {
            self.element.click.fire(&());
        }
    }
}

struct Checkbox {
    element: model::Checkbox,
}

objc_class! {
    impl Checkbox: NSButton {
        #[sel(initWithFrame:)]
        fn init_with_frame(&mut self, frame_rect: Rect) -> cocoa::id {
            unsafe {
                let object: cocoa::id = msg_send![super(self.instance, class!(NSButton)), initWithFrame: frame_rect.0];
                if object.is_null() {
                    return object;
                }
                let () = msg_send![object, setButtonType: cocoa::NSButtonTypeSwitch];
                if let Some(label) = &self.element.label {
                    let () = msg_send![object, setTitle: nsstring(label.as_str())];
                }
                let () = msg_send![object, setAction: sel!(didClick:)];
                let () = msg_send![object, setTarget: object];

                match &self.element.checked {
                    Property::Binding(s) => {
                        if *s.borrow() {
                            let () = msg_send![object, setState: NSControlStateValueOn];
                        }
                    }
                    Property::ReadOnly(_) => (),
                    Property::Static(_) => (),
                }

                object
            }
        }

        #[sel(didClick:)]
        fn did_click(&mut self, button: Objc<Checkbox>) {
            match &self.element.checked {
                Property::Binding(s) => {
                    let state = unsafe { std::mem::transmute::<_, cocoa::NSButton>(button).state() };
                    *s.borrow_mut() = state == NSControlStateValueOn;
                }
                Property::ReadOnly(_) => (),
                Property::Static(_) => (),
            }
        }
    }
}

impl Checkbox {
    pub fn into_button(self) -> cocoa::NSButton {
        let obj = self.into_object();
        // # Safety
        // NSButton is the superclass of Objc<Checkbox>.
        unsafe { std::mem::transmute(obj.autorelease()) }
    }
}

// For some reason the bindgen code for the nslayoutanchor subclasses doesn't have
// `Into<NSLayoutAnchor>`, so we add our own.
trait IntoNSLayoutAnchor {
    fn into_layout_anchor(self) -> cocoa::NSLayoutAnchor;
}

impl IntoNSLayoutAnchor for cocoa::NSLayoutXAxisAnchor {
    fn into_layout_anchor(self) -> cocoa::NSLayoutAnchor {
        // # Safety
        // NSLayoutXAxisAnchor is a subclass of NSLayoutAnchor
        cocoa::NSLayoutAnchor(self.0)
    }
}

impl IntoNSLayoutAnchor for cocoa::NSLayoutYAxisAnchor {
    fn into_layout_anchor(self) -> cocoa::NSLayoutAnchor {
        // # Safety
        // NSLayoutYAxisAnchor is a subclass of NSLayoutAnchor
        cocoa::NSLayoutAnchor(self.0)
    }
}

unsafe fn constraint_equal<T, O>(anchor: T, to: O)
where
    T: INSLayoutAnchor<()> + std::ops::Deref,
    T::Target: Message + Sized,
    O: IntoNSLayoutAnchor,
{
    anchor
        .constraintEqualToAnchor_(to.into_layout_anchor())
        .setActive_(runtime::YES);
}

#[derive(Default)]
struct WindowRenderer {
    windows_to_retain: Vec<StrongRef<cocoa::NSWindow>>,
    rtl: bool,
}

impl WindowRenderer {
    pub fn unwrap(self) -> Vec<StrongRef<cocoa::NSWindow>> {
        self.windows_to_retain
    }

    pub fn render(&mut self, window: TypedElement<model::Window>) -> StrongRef<cocoa::NSWindow> {
        let style = window.style;
        let model::Window {
            close,
            children,
            content,
            modal,
            title,
        } = window.element_type;

        let w = Window {
            modal,
            title,
            style,
        }
        .into_object();

        let nswindow: StrongRef<cocoa::NSWindow> = w.clone().cast();

        unsafe {
            // Don't release windows when closed: we retain windows at the top-level.
            nswindow.setReleasedWhenClosed_(runtime::NO);

            if let Some(close) = close {
                let nswindow = nswindow.weak();
                close.subscribe(move |&()| {
                    if let Some(nswindow) = nswindow.lock() {
                        nswindow.close();
                    }
                });
            }

            if let Some(e) = content {
                // Use an NSBox as a container view so that the window's content can easily have
                // constraints set up relative to the parent (they can't be set relative to the
                // window).
                let content_parent: StrongRef<cocoa::NSBox> = msg_send![class!(NSBox), new];
                content_parent.setTitlePosition_(cocoa::NSNoTitle);
                content_parent.setTransparent_(runtime::YES);
                content_parent.setContentViewMargins_(cocoa::NSSize {
                    width: 5.0,
                    height: 5.0,
                });
                if ViewRenderer::new_with_selector(self.rtl, *content_parent, sel!(setContentView:))
                    .render(*e)
                {
                    nswindow.setContentView_((*content_parent).into());
                }
            }

            for child in children {
                let modal = child.element_type.modal;
                let visible = child.style.visible.clone();
                let child_window = self.render(child);

                #[derive(Clone, Copy)]
                struct ShowChild {
                    modal: bool,
                }

                impl ShowChild {
                    pub fn show(&self, parent: cocoa::NSWindow, child: cocoa::NSWindow) {
                        unsafe {
                            parent.addChildWindow_ordered_(child, cocoa::NSWindowAbove);
                            child.makeKeyAndOrderFront_(parent.0);
                            if self.modal {
                                // Run the modal from the main nsapp.run() loop to prevent binding
                                // updates from being nested (as this will block until the modal is
                                // stopped).
                                enqueue(move || {
                                    let nsapp = cocoa::NSApplication::sharedApplication();
                                    nsapp.runModalForWindow_(child);
                                });
                            }
                        }
                    }
                }

                let show_child = ShowChild { modal };

                match visible {
                    Property::Static(visible) => {
                        if visible {
                            show_child.show(*nswindow, *child_window);
                        }
                    }
                    Property::Binding(b) => {
                        let child = child_window.weak();
                        let parent = nswindow.weak();
                        b.on_change(move |visible| {
                            let Some((w, child_window)) = parent.lock().zip(child.lock()) else {
                                return;
                            };
                            if *visible {
                                show_child.show(*w, *child_window);
                            } else {
                                child_window.close();
                            }
                        });
                        if *b.borrow() {
                            show_child.show(*nswindow, *child_window);
                        }
                    }
                    Property::ReadOnly(_) => panic!("window visibility cannot be ReadOnly"),
                }
            }
        }
        self.windows_to_retain.push(nswindow.clone());
        nswindow
    }
}

struct ViewRenderer {
    parent: cocoa::NSView,
    add_subview: Box<dyn Fn(cocoa::NSView, &model::ElementStyle, cocoa::NSView)>,
    ignore_vertical: bool,
    ignore_horizontal: bool,
    rtl: bool,
}

impl ViewRenderer {
    /// add_subview should add the rendered child view.
    pub fn new<F>(rtl: bool, parent: impl Into<cocoa::NSView>, add_subview: F) -> Self
    where
        F: Fn(cocoa::NSView, &model::ElementStyle, cocoa::NSView) + 'static,
    {
        ViewRenderer {
            parent: parent.into(),
            add_subview: Box::new(add_subview),
            ignore_vertical: false,
            ignore_horizontal: false,
            rtl,
        }
    }

    /// add_subview should be the selector to call on the parent view to add the rendered child view.
    pub fn new_with_selector(
        rtl: bool,
        parent: impl Into<cocoa::NSView>,
        add_subview: runtime::Sel,
    ) -> Self {
        Self::new(rtl, parent, move |parent, _style, child| {
            let () = unsafe { (*parent.0).send_message(add_subview, (child,)) }.unwrap();
        })
    }

    /// Ignore vertical layout settings when rendering views.
    pub fn ignore_vertical(mut self, setting: bool) -> Self {
        self.ignore_vertical = setting;
        self
    }

    /// Ignore horizontal layout settings when rendering views.
    pub fn ignore_horizontal(mut self, setting: bool) -> Self {
        self.ignore_horizontal = setting;
        self
    }

    /// Render the given element.
    ///
    /// Returns whether the element was rendered.
    pub fn render(
        &self,
        Element {
            style,
            element_type,
        }: Element,
    ) -> bool {
        let Some(view) = render_element(element_type, &style, self.rtl) else {
            return false;
        };

        (self.add_subview)(self.parent, &style, view);

        // Setting the content hugging priority to a high value causes stackviews to not stretch
        // subviews during autolayout.
        unsafe {
            view.setContentHuggingPriority_forOrientation_(
                NSLayoutPriorityDefaultHigh,
                cocoa::NSLayoutConstraintOrientationHorizontal,
            );
            view.setContentHuggingPriority_forOrientation_(
                NSLayoutPriorityDefaultHigh,
                cocoa::NSLayoutConstraintOrientationVertical,
            );
        }

        // Set layout and writing direction based on RTL.
        unsafe {
            view.setUserInterfaceLayoutDirection_(if self.rtl {
                cocoa::NSUserInterfaceLayoutDirectionRightToLeft
            } else {
                cocoa::NSUserInterfaceLayoutDirectionLeftToRight
            });
            if let Ok(control) = cocoa::NSControl::try_from(view) {
                control.setBaseWritingDirection_(if self.rtl {
                    cocoa::NSWritingDirectionRightToLeft
                } else {
                    cocoa::NSWritingDirectionLeftToRight
                });
            }
        }

        let lmg = unsafe { self.parent.layoutMarginsGuide() };

        if !matches!(style.horizontal_alignment, Alignment::Fill) {
            if let Some(size) = style.horizontal_size_request {
                unsafe {
                    view.widthAnchor()
                        .constraintGreaterThanOrEqualToConstant_(size as _)
                        .setActive_(runtime::YES);
                }
            }
        }

        if !self.ignore_horizontal {
            unsafe {
                let la = view.leadingAnchor();
                let ta = view.trailingAnchor();
                let pla = lmg.leadingAnchor();
                let pta = lmg.trailingAnchor();
                match style.horizontal_alignment {
                    Alignment::Fill => {
                        constraint_equal(la, pla);
                        constraint_equal(ta, pta);
                        // Without the autoresizing mask set, Text within Scroll doesn't display
                        // properly (it shrinks to 0-width, likely due to some specific interaction
                        // of NSScrollView with autolayout).
                        view.setAutoresizingMask_(cocoa::NSViewWidthSizable);
                    }
                    Alignment::Start => {
                        constraint_equal(la, pla);
                    }
                    Alignment::Center => {
                        let ca = view.centerXAnchor();
                        let pca = lmg.centerXAnchor();
                        constraint_equal(ca, pca);
                    }
                    Alignment::End => {
                        constraint_equal(ta, pta);
                    }
                }
            }
        }

        if !matches!(style.vertical_alignment, Alignment::Fill) {
            if let Some(size) = style.vertical_size_request {
                unsafe {
                    view.heightAnchor()
                        .constraintGreaterThanOrEqualToConstant_(size as _)
                        .setActive_(runtime::YES);
                }
            }
        }

        if !self.ignore_vertical {
            unsafe {
                let ta = view.topAnchor();
                let ba = view.bottomAnchor();
                let pta = lmg.topAnchor();
                let pba = lmg.bottomAnchor();
                match style.vertical_alignment {
                    Alignment::Fill => {
                        constraint_equal(ta, pta);
                        constraint_equal(ba, pba);
                        // Set the autoresizing mask to be consistent with the horizontal settings
                        // (see the comment there as to why it's necessary).
                        view.setAutoresizingMask_(cocoa::NSViewHeightSizable);
                    }
                    Alignment::Start => {
                        constraint_equal(ta, pta);
                    }
                    Alignment::Center => {
                        let ca = view.centerYAnchor();
                        let pca = lmg.centerYAnchor();
                        constraint_equal(ca, pca);
                    }
                    Alignment::End => {
                        constraint_equal(ba, pba);
                    }
                }
            }
        }

        match &style.visible {
            Property::Static(ref v) => {
                unsafe { view.setHidden_((!v).into()) };
            }
            Property::Binding(b) => {
                b.on_change(move |&visible| unsafe {
                    view.setHidden_((!visible).into());
                });
                unsafe { view.setHidden_((!*b.borrow()).into()) };
            }
            Property::ReadOnly(_) => {
                unimplemented!("ElementStyle::visible doesn't support ReadOnly")
            }
        }

        if let Ok(control) = cocoa::NSControl::try_from(view) {
            match &style.enabled {
                Property::Static(e) => {
                    unsafe { control.setEnabled_((*e).into()) };
                }
                Property::Binding(b) => {
                    b.on_change(move |&enabled| unsafe {
                        control.setEnabled_(enabled.into());
                    });
                    unsafe { control.setEnabled_((*b.borrow()).into()) };
                }
                Property::ReadOnly(_) => {
                    unimplemented!("ElementStyle::enabled doesn't support ReadOnly")
                }
            }
        }

        unsafe { view.setNeedsDisplay_(runtime::YES) };

        true
    }
}

fn render_element(
    element_type: model::ElementType,
    style: &model::ElementStyle,
    rtl: bool,
) -> Option<cocoa::NSView> {
    use model::ElementType::*;
    Some(match element_type {
        VBox(model::VBox { items, spacing }) => {
            let sv = unsafe { StrongRef::new(cocoa::NSStackView::alloc()) }.autorelease();
            unsafe {
                sv.init();
                sv.setOrientation_(cocoa::NSUserInterfaceLayoutOrientationVertical);
                sv.setAlignment_(cocoa::NSLayoutAttributeLeading);
                sv.setSpacing_(spacing as _);
                if style.vertical_alignment != Alignment::Fill {
                    // Make sure the vbox stays as small as its content.
                    sv.setHuggingPriority_forOrientation_(
                        NSLayoutPriorityDefaultHigh,
                        cocoa::NSLayoutConstraintOrientationVertical,
                    );
                }
            }
            let renderer = ViewRenderer::new(rtl, sv, |parent, style, child| {
                let gravity: cocoa::NSInteger = match style.vertical_alignment {
                    Alignment::Start | Alignment::Fill => 1,
                    Alignment::Center => 2,
                    Alignment::End => 3,
                };
                let parent: cocoa::NSStackView = parent.try_into().unwrap();
                unsafe { parent.addView_inGravity_(child, gravity) };
            })
            .ignore_vertical(true);
            for item in items {
                renderer.render(item);
            }
            sv.into()
        }
        HBox(model::HBox { items, spacing }) => {
            let sv = unsafe { StrongRef::new(cocoa::NSStackView::alloc()) }.autorelease();
            unsafe {
                sv.init();
                sv.setOrientation_(cocoa::NSUserInterfaceLayoutOrientationHorizontal);
                sv.setAlignment_(cocoa::NSLayoutAttributeTop);
                sv.setSpacing_(spacing as _);
                if style.horizontal_alignment != Alignment::Fill {
                    // Make sure the hbox stays as small as its content.
                    sv.setHuggingPriority_forOrientation_(
                        NSLayoutPriorityDefaultHigh,
                        cocoa::NSLayoutConstraintOrientationHorizontal,
                    );
                }
            }
            let renderer = ViewRenderer::new(rtl, sv, |parent, style, child| {
                let gravity: cocoa::NSInteger = match style.horizontal_alignment {
                    Alignment::Start | Alignment::Fill => 1,
                    Alignment::Center => 2,
                    Alignment::End => 3,
                };
                let parent: cocoa::NSStackView = parent.try_into().unwrap();
                unsafe { parent.addView_inGravity_(child, gravity) };
            })
            .ignore_horizontal(true);
            for item in items {
                renderer.render(item);
            }
            sv.into()
        }
        Button(mut b) => {
            if let Some(Label(model::Label {
                text: Property::Static(text),
                ..
            })) = b.content.take().map(|e| e.element_type)
            {
                let button = self::Button { element: b }.with_title(text.as_str());
                button.into()
            } else {
                return None;
            }
        }
        Checkbox(cb) => {
            let button = self::Checkbox { element: cb }.into_button();
            button.into()
        }
        Label(model::Label { text, bold }) => {
            let tf = cocoa::NSTextField(unsafe {
                cocoa::NSTextField::wrappingLabelWithString_(nsstring(""))
            });
            unsafe { tf.setSelectable_(runtime::NO) };
            if bold {
                unsafe { tf.setFont_(cocoa::NSFont::boldSystemFontOfSize_(0.0)) };
            }
            match text {
                Property::Static(text) => {
                    unsafe { tf.setStringValue_(nsstring(text.as_str())) };
                }
                Property::Binding(b) => {
                    unsafe { tf.setStringValue_(nsstring(b.borrow().as_str())) };
                    b.on_change(move |s| unsafe { tf.setStringValue_(nsstring(s)) });
                }
                Property::ReadOnly(_) => unimplemented!("ReadOnly not supported for Label::text"),
            }
            tf.into()
        }
        Progress(model::Progress { amount }) => {
            fn update(progress: cocoa::NSProgressIndicator, value: Option<f32>) {
                unsafe {
                    match value {
                        None => {
                            progress.setIndeterminate_(runtime::YES);
                            progress.startAnimation_(progress.0);
                        }
                        Some(v) => {
                            progress.setDoubleValue_(v as f64);
                            progress.setIndeterminate_(runtime::NO);
                        }
                    }
                }
            }

            let progress = unsafe { StrongRef::new(cocoa::NSProgressIndicator::alloc()) };
            unsafe {
                progress.init();
                progress.setMinValue_(0.0);
                progress.setMaxValue_(1.0);
            }
            match amount {
                Property::Static(v) => update(*progress, v),
                Property::Binding(s) => {
                    update(*progress, *s.borrow());
                    let weak = progress.weak();
                    s.on_change(move |v| {
                        if let Some(r) = weak.lock() {
                            update(*r, *v);
                        }
                    });
                }
                Property::ReadOnly(_) => (),
            }
            progress.autorelease().into()
        }
        Scroll(model::Scroll { content }) => {
            let sv = unsafe { StrongRef::new(cocoa::NSScrollView::alloc()) }.autorelease();
            unsafe {
                sv.init();
                sv.setHasVerticalScroller_(runtime::YES);
            }
            if let Some(content) = content {
                ViewRenderer::new_with_selector(rtl, sv, sel!(setDocumentView:))
                    .ignore_vertical(true)
                    .render(*content);
            }
            sv.into()
        }
        TextBox(model::TextBox {
            placeholder,
            content,
            editable,
        }) => {
            let tv = unsafe { StrongRef::new(cocoa::NSTextView::alloc()) };
            unsafe {
                tv.init();
                tv.setEditable_(editable.into());
                cocoa::NSTextView_NSSharing::setAllowsUndo_(&*tv, runtime::YES);
                tv.setVerticallyResizable_(runtime::YES);
                if rtl {
                    let ps = StrongRef::new(cocoa::NSMutableParagraphStyle::alloc());
                    ps.init();
                    ps.setAlignment_(cocoa::NSTextAlignmentRight);
                    // We don't `use cocoa::NSTextView_NSSharing` because it has some methods which
                    // conflict with others that make it inconvenient.
                    cocoa::NSTextView_NSSharing::setDefaultParagraphStyle_(&*tv, (*ps).into());
                }
                {
                    let container = tv.textContainer();
                    container.setSize_(cocoa::NSSize {
                        width: f64::MAX,
                        height: f64::MAX,
                    });
                    container.setWidthTracksTextView_(runtime::YES);
                }
                if let Some(placeholder) = placeholder {
                    // It's unclear why dictionaryWithObject_forKey_ takes `u64` rather than `id`
                    // arguments.
                    let attrs = cocoa::NSDictionary(
                        <cocoa::NSDictionary as NSDictionary_NSDictionaryCreation<
                            cocoa::NSAttributedStringKey,
                            cocoa::id,
                        >>::dictionaryWithObject_forKey_(
                            cocoa::NSColor::systemGrayColor().0 as u64,
                            cocoa::NSForegroundColorAttributeName.0 as u64,
                        ),
                    );
                    let string = StrongRef::new(cocoa::NSAttributedString(
                        cocoa::NSAttributedString::alloc()
                            .initWithString_attributes_(nsstring(placeholder.as_str()), attrs),
                    ));
                    // XXX: `setPlaceholderAttributedString` is undocumented (discovered at
                    // https://stackoverflow.com/a/43028577 and works identically to NSTextField),
                    // though hopefully it will be exposed in a public API some day.
                    tv.performSelector_withObject_(sel!(setPlaceholderAttributedString:), string.0);
                }
            }
            match content {
                Property::Static(s) => unsafe { tv.setString_(nsstring(s.as_str())) },
                Property::ReadOnly(od) => {
                    let weak = tv.weak();
                    od.register(move |s| {
                        if let Some(tv) = weak.lock() {
                            *s = read_nsstring(unsafe { tv.string() });
                        }
                    });
                }
                Property::Binding(b) => {
                    let weak = tv.weak();
                    b.on_change(move |s| {
                        if let Some(tv) = weak.lock() {
                            unsafe { tv.setString_(nsstring(s.as_str())) };
                        }
                    });
                    unsafe { tv.setString_(nsstring(b.borrow().as_str())) };
                }
            }
            tv.autorelease().into()
        }
    })
}
