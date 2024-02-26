/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{Element, ElementBuilder};

/// A box which lays out contents vertically.
#[derive(Debug, Default)]
pub struct VBox {
    pub items: Vec<Element>,
    pub spacing: u32,
}

impl ElementBuilder<VBox> {
    pub fn spacing(&mut self, value: u32) {
        self.element_type.spacing = value;
    }

    pub fn add_child(&mut self, child: Element) {
        self.element_type.items.push(child);
    }
}
