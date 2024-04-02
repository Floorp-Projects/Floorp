/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::data::Property;

/// A text label.
#[derive(Debug, Default)]
pub struct Label {
    pub text: Property<String>,
    pub bold: bool,
}

impl super::ElementBuilder<Label> {
    pub fn text(&mut self, s: impl Into<Property<String>>) {
        self.element_type.text = s.into();
    }

    pub fn bold(&mut self, value: bool) {
        self.element_type.bold = value;
    }
}
