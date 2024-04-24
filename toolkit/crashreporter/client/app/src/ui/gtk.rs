/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::model::{self, Alignment, Application, Element};
use crate::std::{
    cell::RefCell,
    ffi::{c_char, CString},
    rc::Rc,
    sync::atomic::{AtomicBool, Ordering::Relaxed},
};
use crate::{
    data::{Event, Property, Synchronized},
    std,
};

/// Create a `std::ffi::CStr` directly from a literal string.
///
/// The argument is an `expr` rather than `literal` so that other macros can be used (such as
/// `stringify!`).
macro_rules! cstr {
    ( $str:expr ) => {
        #[allow(unused_unsafe)]
        unsafe { std::ffi::CStr::from_bytes_with_nul_unchecked(concat!($str, "\0").as_bytes()) }
            .as_ptr()
    };
}

/// A GTK+ UI implementation.
#[derive(Default)]
pub struct UI {
    running: AtomicBool,
}

impl UI {
    pub fn run_loop(&self, app: Application) {
        unsafe {
            let stream = gtk::g_memory_input_stream_new_from_data(
                super::icon::PNG_DATA.as_ptr() as _,
                // unwrap() because the PNG_DATA length will be well within gssize limits (32-bit
                // at the smallest).
                super::icon::PNG_DATA.len().try_into().unwrap(),
                None,
            );
            let icon_pixbuf =
                gtk::gdk_pixbuf_new_from_stream(stream, std::ptr::null_mut(), std::ptr::null_mut());
            gtk::g_object_unref(stream as _);

            gtk::gtk_window_set_default_icon(icon_pixbuf);

            let app_ptr = gtk::gtk_application_new(
                std::ptr::null(),
                gtk::GApplicationFlags_G_APPLICATION_FLAGS_NONE,
            );
            gtk::g_signal_connect_data(
                app_ptr as *mut _,
                cstr!("activate"),
                Some(std::mem::transmute(
                    render_application
                        as for<'a> unsafe extern "C" fn(*mut gtk::GtkApplication, &'a Application),
                )),
                &app as *const Application as *mut Application as _,
                None,
                0,
            );
            self.running.store(true, Relaxed);
            gtk::g_application_run(app_ptr as *mut gtk::GApplication, 0, std::ptr::null_mut());
            self.running.store(false, Relaxed);
            gtk::g_object_unref(app_ptr as *mut _);
            gtk::g_object_unref(icon_pixbuf as _);
        }
    }

    pub fn invoke(&self, f: model::InvokeFn) {
        if !self.running.load(Relaxed) {
            log::debug!("ignoring `invoke` as main loop isn't running");
            return;
        }
        type BoxedData = Option<model::InvokeFn>;

        unsafe extern "C" fn call(ptr: gtk::gpointer) -> gtk::gboolean {
            let f: &mut BoxedData = ToPointer::from_ptr(ptr as _);
            f.take().unwrap()();
            false.into()
        }

        unsafe extern "C" fn drop(ptr: gtk::gpointer) {
            let _: Box<BoxedData> = ToPointer::from_ptr(ptr as _);
        }

        let data: Box<BoxedData> = Box::new(Some(f));

        unsafe {
            let main_context = gtk::g_main_context_default();
            gtk::g_main_context_invoke_full(
                main_context,
                0, // G_PRIORITY_DEFAULT
                Some(call as unsafe extern "C" fn(gtk::gpointer) -> gtk::gboolean),
                data.to_ptr() as _,
                Some(drop as unsafe extern "C" fn(gtk::gpointer)),
            );
        }
    }
}

/// Types that can be converted to and from a pointer.
///
/// These types must be sized to avoid fat pointers (i.e., the pointers must be FFI-compatible, the
/// same size as usize).
trait ToPointer: Sized {
    fn to_ptr(self) -> *mut ();
    /// # Safety
    /// The caller must ensure that the pointer was created as the result of `to_ptr` on the same
    /// or a compatible type, and that the data is still valid.
    unsafe fn from_ptr(ptr: *mut ()) -> Self;
}

/// Types that can be attached to a GLib object to be dropped when the widget is dropped.
trait DropWithObject: Sized {
    fn drop_with_object(self, object: *mut gtk::GObject);
    fn drop_with_widget(self, widget: *mut gtk::GtkWidget) {
        self.drop_with_object(widget as *mut _);
    }

    fn set_data(self, object: *mut gtk::GObject, key: *const c_char);
}

impl<T: ToPointer> DropWithObject for T {
    fn drop_with_object(self, object: *mut gtk::GObject) {
        unsafe extern "C" fn free_ptr<T: ToPointer>(
            ptr: gtk::gpointer,
            _object: *mut gtk::GObject,
        ) {
            drop(T::from_ptr(ptr as *mut ()));
        }
        unsafe { gtk::g_object_weak_ref(object, Some(free_ptr::<T>), self.to_ptr() as *mut _) };
    }

    fn set_data(self, object: *mut gtk::GObject, key: *const c_char) {
        unsafe extern "C" fn free_ptr<T: ToPointer>(ptr: gtk::gpointer) {
            drop(T::from_ptr(ptr as *mut ()));
        }
        unsafe {
            gtk::g_object_set_data_full(object, key, self.to_ptr() as *mut _, Some(free_ptr::<T>))
        };
    }
}

impl ToPointer for CString {
    fn to_ptr(self) -> *mut () {
        self.into_raw() as _
    }

    unsafe fn from_ptr(ptr: *mut ()) -> Self {
        CString::from_raw(ptr as *mut c_char)
    }
}

impl<T> ToPointer for Rc<T> {
    fn to_ptr(self) -> *mut () {
        Rc::into_raw(self) as *mut T as *mut ()
    }

    unsafe fn from_ptr(ptr: *mut ()) -> Self {
        Rc::from_raw(ptr as *mut T as *const T)
    }
}

impl<T> ToPointer for Box<T> {
    fn to_ptr(self) -> *mut () {
        Box::into_raw(self) as _
    }

    unsafe fn from_ptr(ptr: *mut ()) -> Self {
        Box::from_raw(ptr as _)
    }
}

impl<T: Sized> ToPointer for &mut T {
    fn to_ptr(self) -> *mut () {
        self as *mut T as _
    }

    unsafe fn from_ptr(ptr: *mut ()) -> Self {
        &mut *(ptr as *mut T)
    }
}

/// Connect a GTK+ object signal to a function, providing an additional context value (by
/// reference).
macro_rules! connect_signal {
    ( object $object:expr ; with $with:expr ;
      signal $name:ident ($target:ident : &$type:ty $(, $argname:ident : $argtype:ty )* ) $( -> $ret:ty )? $body:block
    ) => {{
        unsafe extern "C" fn $name($($argname : $argtype ,)* $target: &$type) $( -> $ret )? $body
        #[allow(unused_unsafe)]
        unsafe {
            gtk::g_signal_connect_data(
                $object as *mut _,
                cstr!(stringify!($name)),
                Some(std::mem::transmute(
                    $name
                        as for<'a> unsafe extern "C" fn(
                            $($argtype,)*
                            &'a $type,
                        ) $( -> $ret )?,
                )),
                $with as *const $type as *mut $type as _,
                None,
                0,
            );
        }
    }};
}

/// Bind a read only (from the renderer perspective) property to a widget.
///
/// The `set` function is called initially and when the property value changes.
macro_rules! property_read_only {
    ( property $property:expr ;
      fn set( $name:ident : & $type:ty ) $setbody:block
    ) => {{
        let prop: &Property<$type> = $property;
        match prop {
            Property::Static($name) => $setbody,
            Property::Binding(v) => {
                {
                    let $name = v.borrow();
                    $setbody
                }
                v.on_change(move |$name| $setbody);
            }
            Property::ReadOnly(_) => (),
        }
    }};
}

/// Bind a read/write property to a widget signal.
///
/// This currently only allows signals which are of the form
/// `void(*)(SomeGtkObject*, gpointer user_data)`.
macro_rules! property_with_signal {
    ( object $object:expr ; property $property:expr ; signal $signame:ident ;
      fn set( $name:ident : & $type:ty ) $setbody:block
      fn get( $getobj:ident : $gettype:ty ) -> $result:ty $getbody:block
    ) => {{
        let prop: &Property<$type> = $property;
        match prop {
            Property::Static($name) => $setbody,
            Property::Binding(v) => {
                {
                    let $name = v.borrow();
                    $setbody
                }
                let changing = Rc::new(RefCell::new(false));
                struct SignalData {
                    changing: Rc<RefCell<bool>>,
                    value: Synchronized<$result>,
                }
                let signal_data = Box::new(SignalData {
                    changing: changing.clone(),
                    value: v.clone(),
                });
                v.on_change(move |$name| {
                    if !*changing.borrow() {
                        *changing.borrow_mut() = true;
                        $setbody;
                        *changing.borrow_mut() = false;
                    }
                });
                connect_signal! {
                    object $object;
                    with signal_data.as_ref();
                    signal $signame(signal_data: &SignalData, $getobj: $gettype) {
                        let new_value = (|| $getbody)();
                        if !*signal_data.changing.borrow() {
                            *signal_data.changing.borrow_mut() = true;
                            *signal_data.value.borrow_mut() = new_value;
                            *signal_data.changing.borrow_mut() = false;
                        }
                    }
                }
                signal_data.drop_with_object($object as _);
            }
            Property::ReadOnly(v) => {
                v.register(move |target: &mut $result| {
                    let $getobj: $gettype = $object as _;
                    *target = (|| $getbody)();
                });
            }
        }
    }};
}

unsafe extern "C" fn render_application(app_ptr: *mut gtk::GtkApplication, app: &Application) {
    unsafe {
        gtk::gtk_widget_set_default_direction(if app.rtl {
            gtk::GtkTextDirection_GTK_TEXT_DIR_RTL
        } else {
            gtk::GtkTextDirection_GTK_TEXT_DIR_LTR
        });
    }
    for window in &app.windows {
        let window_ptr = render_window(&window.element_type);
        let style = &window.style;

        // Set size before applying style (since the style will set the visibility and show the
        // window). Note that we take the size request as an initial size here.
        //
        // `gtk_window_set_default_size` doesn't work; it resizes to the size request of the inner
        // labels (instead of wrapping them) since it doesn't know how small they should be (that's
        // dictated by the window size!).
        unsafe {
            gtk::gtk_window_resize(
                window_ptr as _,
                style
                    .horizontal_size_request
                    .map(|v| v as i32)
                    .unwrap_or(-1),
                style.vertical_size_request.map(|v| v as i32).unwrap_or(-1),
            );
        }

        apply_style(window_ptr, style);
        unsafe {
            gtk::gtk_application_add_window(app_ptr, window_ptr as *mut _);
        }
    }
}

fn render(element: &Element) -> Option<*mut gtk::GtkWidget> {
    let widget = render_element_type(&element.element_type)?;
    apply_style(widget, &element.style);
    Some(widget)
}

fn apply_style(widget: *mut gtk::GtkWidget, style: &model::ElementStyle) {
    unsafe {
        gtk::gtk_widget_set_halign(widget, alignment(&style.horizontal_alignment));
        if style.horizontal_alignment == Alignment::Fill {
            gtk::gtk_widget_set_hexpand(widget, true.into());
        }
        gtk::gtk_widget_set_valign(widget, alignment(&style.vertical_alignment));
        if style.vertical_alignment == Alignment::Fill {
            gtk::gtk_widget_set_vexpand(widget, true.into());
        }
        gtk::gtk_widget_set_size_request(
            widget,
            style
                .horizontal_size_request
                .map(|v| v as i32)
                .unwrap_or(-1),
            style.vertical_size_request.map(|v| v as i32).unwrap_or(-1),
        );

        gtk::gtk_widget_set_margin_start(widget, style.margin.start as i32);
        gtk::gtk_widget_set_margin_end(widget, style.margin.end as i32);
        gtk::gtk_widget_set_margin_top(widget, style.margin.top as i32);
        gtk::gtk_widget_set_margin_bottom(widget, style.margin.bottom as i32);
    }
    property_read_only! {
        property &style.visible;
        fn set(new_value: &bool) {
            unsafe {
                gtk::gtk_widget_set_visible(widget, new_value.clone().into());
            };
        }
    }
    property_read_only! {
        property &style.enabled;
        fn set(new_value: &bool) {
            unsafe {
                gtk::gtk_widget_set_sensitive(widget, new_value.clone().into());
            }
        }
    }
}

fn alignment(align: &Alignment) -> gtk::GtkAlign {
    use Alignment::*;
    match align {
        Start => gtk::GtkAlign_GTK_ALIGN_START,
        Center => gtk::GtkAlign_GTK_ALIGN_CENTER,
        End => gtk::GtkAlign_GTK_ALIGN_END,
        Fill => gtk::GtkAlign_GTK_ALIGN_FILL,
    }
}

struct PangoAttrList {
    list: *mut gtk::PangoAttrList,
}

impl PangoAttrList {
    pub fn new() -> Self {
        PangoAttrList {
            list: unsafe { gtk::pango_attr_list_new() },
        }
    }

    pub fn bold(&mut self) -> &mut Self {
        unsafe {
            gtk::pango_attr_list_insert(
                self.list,
                gtk::pango_attr_weight_new(gtk::PangoWeight_PANGO_WEIGHT_BOLD),
            )
        };
        self
    }
}

impl std::ops::Deref for PangoAttrList {
    type Target = *mut gtk::PangoAttrList;

    fn deref(&self) -> &Self::Target {
        &self.list
    }
}

impl std::ops::DerefMut for PangoAttrList {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.list
    }
}

impl Drop for PangoAttrList {
    fn drop(&mut self) {
        unsafe { gtk::pango_attr_list_unref(self.list) };
    }
}

fn render_element_type(element_type: &model::ElementType) -> Option<*mut gtk::GtkWidget> {
    use model::ElementType::*;
    Some(match element_type {
        Label(model::Label { text, bold }) => {
            let label_ptr = unsafe { gtk::gtk_label_new(std::ptr::null()) };
            match text {
                Property::Static(text) => {
                    let text = CString::new(text.clone()).ok()?;
                    unsafe { gtk::gtk_label_set_text(label_ptr as _, text.as_ptr()) };
                    text.drop_with_widget(label_ptr);
                }
                Property::Binding(b) => {
                    let label_text = Rc::new(RefCell::new(CString::new(b.borrow().clone()).ok()?));
                    unsafe {
                        gtk::gtk_label_set_text(label_ptr as _, label_text.borrow().as_ptr())
                    };
                    let lt = label_text.clone();
                    label_text.drop_with_widget(label_ptr);
                    b.on_change(move |t| {
                        let Some(cstr) = CString::new(t.clone()).ok() else {
                            return;
                        };
                        unsafe { gtk::gtk_label_set_text(label_ptr as _, cstr.as_ptr()) };
                        *lt.borrow_mut() = cstr;
                    });
                }
                Property::ReadOnly(_) => unimplemented!("ReadOnly not supported for Label::text"),
            }
            unsafe { gtk::gtk_label_set_line_wrap(label_ptr as _, true.into()) };
            // This is gtk_label_set_{xalign,yalign} in gtk 3.16+
            unsafe { gtk::gtk_misc_set_alignment(label_ptr as _, 0.0, 0.5) };
            if *bold {
                unsafe {
                    gtk::gtk_label_set_attributes(label_ptr as _, **PangoAttrList::new().bold())
                };
            }
            label_ptr
        }
        HBox(model::HBox {
            items,
            spacing,
            affirmative_order,
        }) => {
            let box_ptr =
                unsafe { gtk::gtk_box_new(gtk::GtkOrientation_GTK_ORIENTATION_HORIZONTAL, 0) };
            unsafe { gtk::gtk_box_set_spacing(box_ptr as *mut _, *spacing as i32) };
            let items_iter: Box<dyn Iterator<Item = &Element>> = if *affirmative_order {
                Box::new(items.iter().rev())
            } else {
                Box::new(items.iter())
            };
            for item in items_iter {
                if let Some(widget) = render(item) {
                    unsafe {
                        gtk::gtk_container_add(box_ptr as *mut gtk::GtkContainer, widget);
                    }
                    // Special case horizontal alignment to pack into the end if appropriate
                    if item.style.horizontal_alignment == Alignment::End {
                        unsafe {
                            gtk::gtk_box_set_child_packing(
                                box_ptr as _,
                                widget,
                                false.into(),
                                false.into(),
                                0,
                                gtk::GtkPackType_GTK_PACK_END,
                            );
                        }
                    }
                }
            }
            box_ptr
        }
        VBox(model::VBox { items, spacing }) => {
            let box_ptr =
                unsafe { gtk::gtk_box_new(gtk::GtkOrientation_GTK_ORIENTATION_VERTICAL, 0) };
            unsafe { gtk::gtk_box_set_spacing(box_ptr as *mut _, *spacing as i32) };
            for item in items {
                if let Some(widget) = render(item) {
                    unsafe {
                        gtk::gtk_container_add(box_ptr as *mut gtk::GtkContainer, widget);
                    }
                    // Special case vertical alignment to pack into the end if appropriate
                    if item.style.vertical_alignment == Alignment::End {
                        unsafe {
                            gtk::gtk_box_set_child_packing(
                                box_ptr as _,
                                widget,
                                false.into(),
                                false.into(),
                                0,
                                gtk::GtkPackType_GTK_PACK_END,
                            );
                        }
                    }
                }
            }
            box_ptr
        }
        Button(model::Button { content, click }) => {
            let button_ptr = unsafe { gtk::gtk_button_new() };
            if let Some(widget) = content.as_deref().and_then(render) {
                unsafe {
                    // Always center widgets in buttons.
                    gtk::gtk_widget_set_valign(widget, alignment(&Alignment::Center));
                    gtk::gtk_widget_set_halign(widget, alignment(&Alignment::Center));
                    gtk::gtk_container_add(button_ptr as *mut gtk::GtkContainer, widget);
                }
            }
            connect_signal! {
                object button_ptr;
                with click;
                signal clicked(event: &Event<()>, _button: *mut gtk::GtkButton) {
                    event.fire(&());
                }
            }
            button_ptr
        }
        Checkbox(model::Checkbox { checked, label }) => {
            let cb_ptr = match label {
                None => unsafe { gtk::gtk_check_button_new() },
                Some(s) => {
                    let label = CString::new(s.clone()).ok()?;
                    let cb = unsafe { gtk::gtk_check_button_new_with_label(label.as_ptr()) };
                    label.drop_with_widget(cb);
                    cb
                }
            };
            property_with_signal! {
                object cb_ptr;
                property checked;
                signal toggled;
                fn set(new_value: &bool) {
                    unsafe {
                        gtk::gtk_toggle_button_set_active(cb_ptr as *mut _, new_value.clone().into())
                    };
                }
                fn get(button: *mut gtk::GtkToggleButton) -> bool {
                    unsafe {
                        gtk::gtk_toggle_button_get_active(button) == 1
                    }
                }
            }
            cb_ptr
        }
        TextBox(model::TextBox {
            placeholder,
            content,
            editable,
        }) => {
            let text_ptr = unsafe { gtk::gtk_text_view_new() };
            unsafe {
                const GTK_WRAP_WORD_CHAR: u32 = 3;
                gtk::gtk_text_view_set_wrap_mode(text_ptr as *mut _, GTK_WRAP_WORD_CHAR);
                gtk::gtk_text_view_set_editable(text_ptr as *mut _, editable.clone().into());
                gtk::gtk_text_view_set_accepts_tab(text_ptr as *mut _, false.into());
            }
            let buffer = unsafe { gtk::gtk_text_view_get_buffer(text_ptr as *mut _) };

            struct State {
                placeholder: Option<Placeholder>,
            }

            struct Placeholder {
                string: CString,
                visible: RefCell<bool>,
            }

            impl Placeholder {
                fn focus(&self, widget: *mut gtk::GtkWidget) {
                    if *self.visible.borrow() {
                        unsafe {
                            let buffer = gtk::gtk_text_view_get_buffer(widget as *mut _);
                            gtk::gtk_text_buffer_set_text(buffer, self.string.as_ptr(), 0);
                            gtk::gtk_widget_override_color(
                                widget,
                                gtk::GtkStateFlags_GTK_STATE_FLAG_NORMAL,
                                std::ptr::null(),
                            );
                        }
                        *self.visible.borrow_mut() = false;
                    }
                }

                fn unfocus(&self, widget: *mut gtk::GtkWidget) {
                    unsafe {
                        let buffer = gtk::gtk_text_view_get_buffer(widget as *mut _);

                        let mut end_iter = gtk::GtkTextIter::default();
                        gtk::gtk_text_buffer_get_end_iter(buffer, &mut end_iter);
                        let is_empty = gtk::gtk_text_iter_get_offset(&end_iter) == 0;

                        if is_empty && !*self.visible.borrow() {
                            gtk::gtk_text_buffer_set_text(buffer, self.string.as_ptr(), -1);
                            let context = gtk::gtk_widget_get_style_context(widget);
                            let mut color = gtk::GdkRGBA::default();
                            gtk::gtk_style_context_get_color(
                                context,
                                gtk::GtkStateFlags_GTK_STATE_FLAG_INSENSITIVE,
                                &mut color,
                            );
                            gtk::gtk_widget_override_color(
                                widget,
                                gtk::GtkStateFlags_GTK_STATE_FLAG_NORMAL,
                                &color,
                            );
                            *self.visible.borrow_mut() = true;
                        }
                    }
                }
            }

            let mut state = Box::new(State { placeholder: None });

            if let Some(placeholder) = placeholder {
                state.placeholder = Some(Placeholder {
                    string: CString::new(placeholder.clone()).ok()?,
                    visible: RefCell::new(false),
                });

                let placeholder = state.placeholder.as_ref().unwrap();

                placeholder.unfocus(text_ptr);

                connect_signal! {
                    object text_ptr;
                    with placeholder;
                    signal focus_in_event(placeholder: &Placeholder, widget: *mut gtk::GtkWidget,
                                          _event: *mut gtk::GdkEventFocus) -> gtk::gboolean {
                        placeholder.focus(widget);
                        false.into()
                    }
                }
                connect_signal! {
                    object text_ptr;
                    with placeholder;
                    signal focus_out_event(placeholder: &Placeholder, widget: *mut gtk::GtkWidget,
                                          _event: *mut gtk::GdkEventFocus) -> gtk::gboolean {
                        placeholder.unfocus(widget);
                        false.into()
                    }
                }
            }

            // Attach the state so that we can access it in the changed signal.
            // This is kind of ugly; in the future it might be a nicer developer interface to simply
            // always use a closure as the user data (which itself can capture arbitrary things). This
            // would move the data management from GTK to rust.
            state.set_data(buffer as *mut _, cstr!("textview-state"));

            property_with_signal! {
                object buffer;
                property content;
                signal changed;
                fn set(new_value: &String) {
                    unsafe {
                        gtk::gtk_text_buffer_set_text(buffer, new_value.as_ptr() as *const c_char, new_value.len().try_into().unwrap());
                    }
                }
                fn get(buffer: *mut gtk::GtkTextBuffer) -> String {
                    let state = unsafe {
                        gtk::g_object_get_data(buffer as *mut _, cstr!("textview-state"))
                    };
                    if !state.is_null() {
                        let s: &State = unsafe { &*(state as *mut State as *const State) };
                        if let Some(placeholder) = &s.placeholder {
                            if *placeholder.visible.borrow() {
                                return "".to_owned();
                            }
                        }
                    }
                    let mut start_iter = gtk::GtkTextIter::default();
                    unsafe {
                        gtk::gtk_text_buffer_get_start_iter(buffer, &mut start_iter);
                    }

                    let mut s = String::new();
                    loop {
                        let c = unsafe { gtk::gtk_text_iter_get_char(&start_iter) };
                        if c == 0 {
                            break;
                        }
                        // Safety:
                        // gunichar is guaranteed to be a valid unicode codepoint (if nonzero).
                        s.push(unsafe { char::from_u32_unchecked(c) });
                        unsafe {
                            gtk::gtk_text_iter_forward_char(&mut start_iter);
                        }
                    }
                    s
                }
            }
            text_ptr
        }
        Scroll(model::Scroll { content }) => {
            let scroll_ptr =
                unsafe { gtk::gtk_scrolled_window_new(std::ptr::null_mut(), std::ptr::null_mut()) };
            unsafe {
                gtk::gtk_scrolled_window_set_policy(
                    scroll_ptr as *mut _,
                    gtk::GtkPolicyType_GTK_POLICY_NEVER,
                    gtk::GtkPolicyType_GTK_POLICY_ALWAYS,
                );
                gtk::gtk_scrolled_window_set_shadow_type(
                    scroll_ptr as *mut _,
                    gtk::GtkShadowType_GTK_SHADOW_IN,
                );
            };
            if let Some(widget) = content.as_deref().and_then(render) {
                unsafe {
                    gtk::gtk_container_add(scroll_ptr as *mut gtk::GtkContainer, widget);
                }
            }
            scroll_ptr
        }
        Progress(model::Progress { amount }) => {
            let progress_ptr = unsafe { gtk::gtk_progress_bar_new() };
            property_read_only! {
                property amount;
                fn set(value: &Option<f32>) {
                    match &*value {
                        Some(v) => unsafe {
                            gtk::gtk_progress_bar_set_fraction(
                                progress_ptr as *mut _,
                                v.clamp(0f32,1f32) as f64,
                            );
                        }
                        None => unsafe {
                            gtk::gtk_progress_bar_pulse(progress_ptr as *mut _);

                            fn auto_pulse_progress_bar(progress: *mut gtk::GtkProgressBar) {
                                unsafe extern fn pulse(progress: *mut std::ffi::c_void) -> gtk::gboolean {
                                    if gtk::gtk_widget_is_visible(progress as _) == 0 {
                                        false.into()
                                    } else {
                                        gtk::gtk_progress_bar_pulse(progress as _);
                                        true.into()
                                    }
                                }
                                unsafe {
                                    gtk::g_timeout_add(100, Some(pulse as unsafe extern fn(*mut std::ffi::c_void) -> gtk::gboolean), progress as _);
                                }

                            }

                            connect_signal! {
                                object progress_ptr;
                                with std::ptr::null_mut();
                                signal show(_user_data: &(), progress: *mut gtk::GtkWidget) {
                                    auto_pulse_progress_bar(progress as *mut _);
                                }
                            }
                            auto_pulse_progress_bar(progress_ptr as *mut _);
                        }
                    }
                }
            }
            progress_ptr
        }
    })
}

fn render_window(
    model::Window {
        title,
        content,
        children,
        modal,
        close,
    }: &model::Window,
) -> *mut gtk::GtkWidget {
    unsafe {
        let window_ptr = gtk::gtk_window_new(gtk::GtkWindowType_GTK_WINDOW_TOPLEVEL);
        if !title.is_empty() {
            if let Some(title) = CString::new(title.clone()).ok() {
                gtk::gtk_window_set_title(window_ptr as *mut _, title.as_ptr());
                title.drop_with_widget(window_ptr);
            }
        }
        if let Some(content) = content {
            if let Some(widget) = render(content) {
                gtk::gtk_container_add(window_ptr as *mut gtk::GtkContainer, widget);
            }
        }
        for child in children {
            let widget = render_window(&child.element_type);
            apply_style(widget, &child.style);
            gtk::gtk_window_set_transient_for(widget as *mut _, window_ptr as *mut _);
            // Delete should hide the window.
            gtk::g_signal_connect_data(
                widget as *mut _,
                cstr!("delete-event"),
                Some(std::mem::transmute(
                    gtk::gtk_widget_hide_on_delete
                        as unsafe extern "C" fn(*mut gtk::GtkWidget) -> i32,
                )),
                std::ptr::null_mut(),
                None,
                0,
            );
        }
        if *modal {
            gtk::gtk_window_set_modal(window_ptr as *mut _, true.into());
        }
        if let Some(close) = close {
            close.subscribe(move |&()| gtk::gtk_window_close(window_ptr as *mut _));
        }

        window_ptr
    }
}
