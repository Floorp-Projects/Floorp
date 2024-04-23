/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{Element, ElementBuilder};

/// A box which lays out contents horizontally.
#[derive(Default, Debug)]
pub struct HBox {
    pub items: Vec<Element>,
    pub spacing: u32,
    pub affirmative_order: bool,
}

impl ElementBuilder<HBox> {
    pub fn spacing(&mut self, value: u32) {
        self.element_type.spacing = value;
    }

    /// Whether children are in affirmative order (and should be reordered based on platform
    /// conventions).
    ///
    /// The children passed to `add_child` should be in most-affirmative to least-affirmative order
    /// (e.g., "OK" then "Cancel" buttons).
    ///
    /// This is mainly useful for dialog buttons.
    pub fn affirmative_order(&mut self, value: bool) {
        self.element_type.affirmative_order = value;
    }

    pub fn add_child(&mut self, child: Element) {
        self.element_type.items.push(child);
    }
}
