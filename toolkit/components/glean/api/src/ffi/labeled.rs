// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;
use std::sync::atomic::Ordering;

#[no_mangle]
pub extern "C" fn fog_labeled_boolean_get(id: u32, label: &nsACString) -> u32 {
    labeled_submetric_get!(
        id,
        label,
        LABELED_BOOLEAN_MAP,
        BOOLEAN_MAP,
        LabeledBooleanMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_labeled_counter_get(id: u32, label: &nsACString) -> u32 {
    labeled_submetric_get!(
        id,
        label,
        LABELED_COUNTER_MAP,
        COUNTER_MAP,
        LabeledCounterMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_labeled_string_get(id: u32, label: &nsACString) -> u32 {
    labeled_submetric_get!(
        id,
        label,
        LABELED_STRING_MAP,
        STRING_MAP,
        LabeledStringMetric
    )
}
