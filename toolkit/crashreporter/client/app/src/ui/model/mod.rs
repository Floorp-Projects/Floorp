/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The UI model.
//!
//! Model elements should generally be declared as types with all fields `pub` (to be accessed by
//! UI implementations), though accessor methods are acceptable if needed. An
//! `ElementBuilder<TYPE>` impl should be provided to create methods that will be used in the
//! [`ui!`] macro. The model types are accessible when being _consumed_ by a UI implementation,
//! whereas the `ElementBuilder` types are accessible when the model is being _created_.
//!
//! All elements should be listed in the `element_types!` macro in this file (note that [`Window`],
//! while an element, isn't listed here as it cannot be a child element). This populates the
//! `ElementType` enum and generates `From<Element>` for `ElementType`, and `TryFrom<ElementType>`
//! for the element (as well as reference `TryFrom`).
//!
//! The model is written to accommodate layout and text direction differences (e.g. for RTL
//! languages), and UI implementations are expected to account for this correctly.

use crate::data::Property;
pub use button::Button;
pub use checkbox::Checkbox;
pub use hbox::HBox;
pub use label::Label;
pub use progress::Progress;
pub use scroll::Scroll;
pub use textbox::TextBox;
pub use vbox::VBox;
pub use window::Window;

mod button;
mod checkbox;
mod hbox;
mod label;
mod progress;
mod scroll;
mod textbox;
mod vbox;
mod window;

/// A GUI element, including general style attributes and a more specific type.
///
/// `From<ElementBuilder<...>>` is implemented for all elements listed in `element_types!`.
#[derive(Debug)]
pub struct Element {
    pub style: ElementStyle,
    pub element_type: ElementType,
}

// This macro creates the `ElementType` enum and corresponding `From<ElementBuilder>` impls for
// Element. The `ElementType` discriminants match the element type names.
macro_rules! element_types {
    ( $($name:ident),* ) => {
        /// A type of GUI element.
        #[derive(Debug)]
        pub enum ElementType {
            $($name($name)),*
        }

        $(
        impl From<$name> for ElementType {
            fn from(e: $name) -> ElementType {
                ElementType::$name(e)
            }
        }

        impl TryFrom<ElementType> for $name {
            type Error = &'static str;

            fn try_from(et: ElementType) -> Result<Self, Self::Error> {
                if let ElementType::$name(v) = et {
                    Ok(v)
                } else {
                    Err(concat!("ElementType was not ", stringify!($name)))
                }
            }
        }

        impl<'a> TryFrom<&'a ElementType> for &'a $name {
            type Error = &'static str;

            fn try_from(et: &'a ElementType) -> Result<Self, Self::Error> {
                if let ElementType::$name(v) = et {
                    Ok(v)
                } else {
                    Err(concat!("ElementType was not ", stringify!($name)))
                }
            }
        }

        impl From<ElementBuilder<$name>> for Element {
             fn from(b: ElementBuilder<$name>) -> Self {
                 Element {
                     style: b.style,
                     element_type: b.element_type.into(),
                 }
             }
        }
        )*
    }
}
element_types! {
    Button, Checkbox, HBox, Label, Progress, Scroll, TextBox, VBox
}

/// Common element style values.
#[derive(Debug)]
pub struct ElementStyle {
    pub horizontal_alignment: Alignment,
    pub vertical_alignment: Alignment,
    pub horizontal_size_request: Option<u32>,
    pub vertical_size_request: Option<u32>,
    pub margin: Margin,
    pub visible: Property<bool>,
    pub enabled: Property<bool>,
    #[cfg(test)]
    pub id: Option<String>,
}

impl Default for ElementStyle {
    fn default() -> Self {
        ElementStyle {
            horizontal_alignment: Default::default(),
            vertical_alignment: Default::default(),
            horizontal_size_request: Default::default(),
            vertical_size_request: Default::default(),
            margin: Default::default(),
            visible: true.into(),
            enabled: true.into(),
            #[cfg(test)]
            id: Default::default(),
        }
    }
}

/// A builder for `Element`s.
///
/// Each element should add an `impl ElementBuilder<TYPE>` to add methods to their builder.
#[derive(Debug, Default)]
pub struct ElementBuilder<T> {
    pub style: ElementStyle,
    pub element_type: T,
}

impl<T> ElementBuilder<T> {
    /// Set horizontal alignment.
    pub fn halign(&mut self, alignment: Alignment) {
        self.style.horizontal_alignment = alignment;
    }

    /// Set vertical alignment.
    pub fn valign(&mut self, alignment: Alignment) {
        self.style.vertical_alignment = alignment;
    }

    /// Set the horizontal size request.
    pub fn hsize(&mut self, value: u32) {
        assert!(value <= i32::MAX as u32);
        self.style.horizontal_size_request = Some(value);
    }

    /// Set the vertical size request.
    pub fn vsize(&mut self, value: u32) {
        assert!(value <= i32::MAX as u32);
        self.style.vertical_size_request = Some(value);
    }

    /// Set start margin.
    pub fn margin_start(&mut self, amount: u32) {
        self.style.margin.start = amount;
    }

    /// Set end margin.
    pub fn margin_end(&mut self, amount: u32) {
        self.style.margin.end = amount;
    }

    /// Set start and end margins.
    pub fn margin_horizontal(&mut self, amount: u32) {
        self.margin_start(amount);
        self.margin_end(amount)
    }

    /// Set top margin.
    pub fn margin_top(&mut self, amount: u32) {
        self.style.margin.top = amount;
    }

    /// Set bottom margin.
    pub fn margin_bottom(&mut self, amount: u32) {
        self.style.margin.bottom = amount;
    }

    /// Set top and bottom margins.
    pub fn margin_vertical(&mut self, amount: u32) {
        self.margin_top(amount);
        self.margin_bottom(amount)
    }

    /// Set all margins.
    pub fn margin(&mut self, amount: u32) {
        self.margin_horizontal(amount);
        self.margin_vertical(amount)
    }

    /// Set visibility.
    pub fn visible(&mut self, value: impl Into<Property<bool>>) {
        self.style.visible = value.into();
    }

    /// Set whether an element is enabled.
    ///
    /// This generally should enable/disable interaction with an element.
    pub fn enabled(&mut self, value: impl Into<Property<bool>>) {
        self.style.enabled = value.into();
    }

    /// Set the element identifier.
    #[cfg(test)]
    pub fn id(&mut self, value: impl Into<String>) {
        self.style.id = Some(value.into());
    }

    /// Set the element identifier (stub).
    #[cfg(not(test))]
    pub fn id(&mut self, _value: impl Into<String>) {}

    fn single_child(slot: &mut Option<Box<Element>>, child: Element) {
        if slot.replace(Box::new(child)).is_some() {
            panic!("{} can only have one child", std::any::type_name::<T>());
        }
    }
}

/// A typed [`Element`].
///
/// This is useful for the [`ui!`] macro when a method should accept a specific element type, since
/// the macro always creates [`ElementBuilder<T>`](ElementBuilder) and ends with a `.into()` (and this implements
/// `From<ElementBuilder<T>>`).
#[derive(Debug, Default)]
pub struct TypedElement<T> {
    pub style: ElementStyle,
    pub element_type: T,
}

impl<T> From<ElementBuilder<T>> for TypedElement<T> {
    fn from(b: ElementBuilder<T>) -> Self {
        TypedElement {
            style: b.style,
            element_type: b.element_type,
        }
    }
}

/// The alignment of an element in one direction.
///
/// Note that rather than `Left`/`Right`, this class has `Start`/`End` as it is meant to be
/// layout-direction-aware.
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum Alignment {
    /// Align to the start of the direction.
    #[default]
    Start,
    /// Align to the center of the direction.
    Center,
    /// Align to the end of the direction.
    End,
    /// Fill all available space.
    Fill,
}

/// The margins of an element.
///
/// These are RTL-aware: for instance, `start` is the left margin in left-to-right languages and
/// the right margin in right-to-left languages.
#[derive(Default, Debug)]
pub struct Margin {
    pub start: u32,
    pub end: u32,
    pub top: u32,
    pub bottom: u32,
}

/// A macro to allow a convenient syntax for creating elements.
///
/// The macro expects the following syntax:
/// ```
/// ElementTypeName some_method(arg1, arg2) other_method() {
///     Child ...,
///     Child2 ...
/// }
/// ```
///
/// The type is wrapped in an `ElementBuilder`, and methods are called on this builder with a
/// mutable reference. This means that element types must implement Default and must implement
/// builder methods on `ElementBuilder<ElementTypeName>`. The children block is optional, and calls
/// `add_child(child: Element)` for each provided child (so implement this method if desired).
///
/// For testing, a string identifier can be set on any element with a `["my_identifier"]` following
/// the element type.
macro_rules! ui {
    ( $el:ident
        $([ $id:literal ])?
        $( $method:ident $methodargs:tt )*
        $({ $($contents:tt)* })?
    ) => {
        {
            #[allow(unused_imports)]
            use $crate::ui::model::*;
            let mut el: ElementBuilder<$el> = Default::default();
            $( el.id($id); )?
            $( el.$method $methodargs ; )*
            $( ui! { @children (el) $($contents)* } )?
            el.into()
        }
    };
    ( @children ($parent:expr) ) => {};
    ( @children ($parent:expr)
      $el:ident
        $([ $id:literal ])?
        $( $method:ident $methodargs:tt )*
        $({ $($contents:tt)* })?
      $(, $($rest:tt)* )?
    ) => {
        $parent.add_child(ui!( $el $([$id])? $( $method $methodargs )* $({ $($contents)* })? ));
        $(ui!( @children ($parent) $($rest)* ))?
    };
}

pub(crate) use ui;

/// An application, defined as a set of windows.
///
/// When all windows are closed, the application is considered complete (and loops should exit).
pub struct Application {
    pub windows: Vec<TypedElement<Window>>,
    /// Whether the text direction should be right-to-left.
    #[cfg_attr(test, allow(dead_code))]
    pub rtl: bool,
}

/// A function to be invoked in the UI loop.
pub type InvokeFn = Box<dyn FnOnce() + Send + 'static>;
