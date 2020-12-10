// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::{nsACString, nsCString};
use thin_vec::ThinVec;

use crate::metrics::__glean_metric_maps as metric_maps;
use crate::private::EventRecordingError;

#[no_mangle]
pub extern "C" fn fog_event_record(
    id: u32,
    extra_keys: &ThinVec<i32>,
    extra_values: &ThinVec<nsCString>,
) {
    // If no extra keys are passed, we can shortcut here.
    if extra_keys.is_empty() {
        if metric_maps::event_record_wrapper(id, Default::default()).is_err() {
            panic!("No event for id {}", id);
        }

        return;
    }

    assert_eq!(
        extra_keys.len(),
        extra_values.len(),
        "Extra keys and values differ in length. ID: {}",
        id
    );

    // Otherwise we need to decode them and pass them along.
    let extra = extra_keys
        .iter()
        .zip(extra_values.iter())
        .map(|(&k, v)| (k, v.to_string()))
        .collect();
    match metric_maps::event_record_wrapper(id, extra) {
        Ok(()) => {}
        Err(EventRecordingError::InvalidId) => panic!("No event for id {}", id),
        Err(EventRecordingError::InvalidExtraKey) => {
            panic!("Invalid extra keys in map for id {}", id)
        }
    }
}

#[no_mangle]
pub extern "C" fn fog_event_record_str(
    id: u32,
    extra_keys: &ThinVec<nsCString>,
    extra_values: &ThinVec<nsCString>,
) {
    // If no extra keys are passed, we can shortcut here.
    if extra_keys.is_empty() {
        if metric_maps::event_record_wrapper_str(id, Default::default()).is_err() {
            panic!("No event for id {}", id);
        }

        return;
    }

    assert_eq!(
        extra_keys.len(),
        extra_values.len(),
        "Extra keys and values differ in length. ID: {}",
        id
    );

    // Otherwise we need to decode them and pass them along.
    let extra = extra_keys
        .iter()
        .zip(extra_values.iter())
        .map(|(k, v)| (k.to_string(), v.to_string()))
        .collect();
    match metric_maps::event_record_wrapper_str(id, extra) {
        Ok(()) => {}
        Err(EventRecordingError::InvalidId) => panic!("No event for id {}", id),
        Err(EventRecordingError::InvalidExtraKey) => {
            panic!("Invalid extra keys in map for id {}", id)
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn fog_event_test_has_value(id: u32, storage_name: &nsACString) -> bool {
    metric_maps::event_test_get_value_wrapper(id, &storage_name.to_utf8()).is_some()
}
