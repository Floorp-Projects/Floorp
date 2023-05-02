// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use crate::metrics::__glean_metric_maps as metric_maps;
use nsstring::nsACString;
use std::sync::atomic::Ordering;

#[no_mangle]
pub extern "C" fn fog_labeled_enum_to_str(id: u32, label: u16, value: &mut nsACString) {
    let val = metric_maps::labeled_enum_to_str(id, label);
    value.assign(&val);
}

#[no_mangle]
pub extern "C" fn fog_labeled_boolean_get(id: u32, label: &nsACString) -> u32 {
    labeled_submetric_get!(
        id,
        label,
        LABELED_BOOLEAN_MAP,
        labeled_boolean_get,
        BOOLEAN_MAP,
        LabeledBooleanMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_labeled_boolean_enum_get(id: u32, label: u16) -> u32 {
    labeled_submetric_enum_get!(
        id,
        label,
        labeled_boolean_enum_get,
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
        labeled_counter_get,
        COUNTER_MAP,
        LabeledCounterMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_labeled_counter_enum_get(id: u32, label: u16) -> u32 {
    labeled_submetric_enum_get!(
        id,
        label,
        labeled_counter_enum_get,
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
        labeled_string_get,
        STRING_MAP,
        LabeledStringMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_labeled_string_enum_get(id: u32, label: u16) -> u32 {
    labeled_submetric_enum_get!(
        id,
        label,
        labeled_string_enum_get,
        STRING_MAP,
        LabeledStringMetric
    )
}
