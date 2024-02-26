/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{Element, ElementBuilder, TypedElement};
use crate::data::Event;

/// A window.
#[derive(Debug, Default)]
pub struct Window {
    pub title: String,
    /// The window content is the first element.
    pub content: Option<Box<Element>>,
    /// Logical child windows.
    pub children: Vec<TypedElement<Self>>,
    pub modal: bool,
    pub close: Option<Event<()>>,
}

impl ElementBuilder<Window> {
    /// Set the window title.
    pub fn title(&mut self, s: impl Into<String>) {
        self.element_type.title = s.into();
    }

    /// Set whether the window is modal (blocking interaction with other windows when displayed).
    pub fn modal(&mut self, value: bool) {
        self.element_type.modal = value;
    }

    /// Register an event to close the window.
    pub fn close_when(&mut self, event: &Event<()>) {
        self.element_type.close = Some(event.clone());
    }

    /// Add a window as a logical child of this one.
    ///
    /// Logical children are always displayed above their parents.
    pub fn child_window(&mut self, window: TypedElement<Window>) {
        self.element_type.children.push(window);
    }

    pub fn add_child(&mut self, child: Element) {
        Self::single_child(&mut self.element_type.content, child);
    }
}
