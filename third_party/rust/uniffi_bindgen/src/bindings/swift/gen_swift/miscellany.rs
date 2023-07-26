/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::CodeType;

#[derive(Debug)]
pub struct TimestampCodeType;

impl CodeType for TimestampCodeType {
    fn type_label(&self) -> String {
        "Date".into()
    }

    fn canonical_name(&self) -> String {
        "Timestamp".into()
    }
}

#[derive(Debug)]
pub struct DurationCodeType;

impl CodeType for DurationCodeType {
    fn type_label(&self) -> String {
        "TimeInterval".into()
    }

    fn canonical_name(&self) -> String {
        "Duration".into()
    }
}
