/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::data::Property;

/// A checkbox (with optional label).
#[derive(Default, Debug)]
pub struct Checkbox {
    pub checked: Property<bool>,
    pub label: Option<String>,
}

impl super::ElementBuilder<Checkbox> {
    pub fn checked(&mut self, value: impl Into<Property<bool>>) {
        self.element_type.checked = value.into();
    }

    pub fn label<S: Into<String>>(&mut self, label: S) {
        self.element_type.label = Some(label.into());
    }
}
