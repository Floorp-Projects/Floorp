/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{Element, ElementBuilder};
use crate::data::Event;

/// A clickable button.
#[derive(Default, Debug)]
pub struct Button {
    pub content: Option<Box<Element>>,
    pub click: Event<()>,
}

impl ElementBuilder<Button> {
    pub fn on_click<F>(&mut self, f: F)
    where
        F: Fn() + 'static,
    {
        self.element_type.click.subscribe(move |_| f());
    }

    pub fn add_child(&mut self, child: Element) {
        Self::single_child(&mut self.element_type.content, child);
    }
}
