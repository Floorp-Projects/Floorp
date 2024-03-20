/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::data::Property;

/// A progress indicator.
#[derive(Debug, Default)]
pub struct Progress {
    /// Progress between 0 and 1, or None if indeterminate.
    pub amount: Property<Option<f32>>,
}

impl super::ElementBuilder<Progress> {
    #[allow(dead_code)]
    pub fn amount(&mut self, value: impl Into<Property<Option<f32>>>) {
        self.element_type.amount = value.into();
    }
}
