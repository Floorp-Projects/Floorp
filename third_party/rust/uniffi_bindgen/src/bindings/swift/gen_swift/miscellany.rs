/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeOracle, CodeType};

pub struct TimestampCodeType;

impl CodeType for TimestampCodeType {
    fn type_label(&self, _oracle: &dyn CodeOracle) -> String {
        "Date".into()
    }

    fn canonical_name(&self, _oracle: &dyn CodeOracle) -> String {
        "Timestamp".into()
    }
}

pub struct DurationCodeType;

impl CodeType for DurationCodeType {
    fn type_label(&self, _oracle: &dyn CodeOracle) -> String {
        "TimeInterval".into()
    }

    fn canonical_name(&self, _oracle: &dyn CodeOracle) -> String {
        "Duration".into()
    }
}
