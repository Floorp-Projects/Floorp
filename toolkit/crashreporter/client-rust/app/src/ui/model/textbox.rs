/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::data::Property;

/// A text box.
#[derive(Debug, Default)]
pub struct TextBox {
    pub placeholder: Option<String>,
    pub content: Property<String>,
    pub editable: bool,
}

impl super::ElementBuilder<TextBox> {
    pub fn placeholder(&mut self, text: impl Into<String>) {
        self.element_type.placeholder = Some(text.into());
    }

    pub fn content(&mut self, value: impl Into<Property<String>>) {
        self.element_type.content = value.into();
    }

    pub fn editable(&mut self, value: bool) {
        self.element_type.editable = value;
    }
}
