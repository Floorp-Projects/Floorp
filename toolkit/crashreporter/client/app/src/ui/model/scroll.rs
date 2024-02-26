/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{Element, ElementBuilder};

/// A scrollable region.
#[derive(Debug, Default)]
pub struct Scroll {
    pub content: Option<Box<Element>>,
}

impl ElementBuilder<Scroll> {
    pub fn add_child(&mut self, child: Element) {
        Self::single_child(&mut self.element_type.content, child);
    }
}
