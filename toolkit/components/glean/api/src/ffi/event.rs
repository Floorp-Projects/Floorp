// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use std::collections::HashMap;

use nsstring::{nsACString, nsCString, nsCStringRepr};
use thin_vec::ThinVec;

use crate::metrics::__glean_metric_maps as metric_maps;
use crate::private::EventRecordingError;

#[no_mangle]
pub extern "C" fn fog_event_record(
    id: u32,
    extra_keys: &ThinVec<u32>,
    extra_values: &ThinVec<nsCString>,
) {
    // If no extra keys are passed, we can shortcut here.
    if extra_keys.is_empty() {
        if metric_maps::record_event_by_id(id, Default::default()).is_err() {
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
        .map(|(&k, v)| (k as i32, v.to_string()))
        .collect();
    match metric_maps::record_event_by_id(id, extra) {
        Ok(()) => {}
        Err(EventRecordingError::InvalidId) => panic!("No event for id {}", id),
        Err(EventRecordingError::InvalidExtraKey) => {
            // TODO: Record an error. bug 1704504.
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
        if metric_maps::record_event_by_id_with_strings(id, Default::default()).is_err() {
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
    match metric_maps::record_event_by_id_with_strings(id, extra) {
        Ok(()) => {}
        Err(EventRecordingError::InvalidId) => panic!("No event for id {}", id),
        Err(EventRecordingError::InvalidExtraKey) => {
            // TODO: Record an error. bug 1704504.
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn fog_event_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    let storage = if ping_name.is_empty() {
        None
    } else {
        Some(ping_name.to_utf8().into_owned())
    };
    metric_maps::event_test_get_value_wrapper(id, storage).is_some()
}

#[no_mangle]
pub extern "C" fn fog_event_test_get_error(
    id: u32,
    ping_name: &nsACString,
    error_str: &mut nsACString,
) -> bool {
    let storage = if ping_name.is_empty() {
        None
    } else {
        Some(ping_name.to_utf8().into_owned())
    };
    let err = metric_maps::event_test_get_error(id, storage);
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}

/// FFI-compatible representation of recorded event data.
///
/// This wraps Gecko strings, but ensures they are not dropped on the Rust side.
/// That happens on the C++ side, where they are represented as `nsCString`.
#[repr(C)]
pub struct FfiRecordedEvent {
    timestamp: u64,
    category: nsCStringRepr,
    name: nsCStringRepr,

    /// Number of extra item pairs.
    /// The `extra` array has length `2 * extra_len`.
    extra_len: u32,

    /// Flat array of extra data, keys and values are interleaved.
    ///
    /// This needs to be passed to `fog_event_free_event_extra` for deallocation.
    extra: *mut nsCStringRepr,
}

#[no_mangle]
pub unsafe extern "C" fn fog_event_test_get_value(
    id: u32,
    ping_name: &nsACString,
    out_events: &mut ThinVec<FfiRecordedEvent>,
) {
    let storage = if ping_name.is_empty() {
        None
    } else {
        Some(ping_name.to_utf8().into_owned())
    };

    let events = match metric_maps::event_test_get_value_wrapper(id, storage) {
        Some(events) => events,
        None => return,
    };

    for event in events {
        let extra = event.extra.unwrap_or_else(HashMap::new);
        let extra_len = extra.len();
        let mut extras = Vec::with_capacity(extra_len * 2);
        for (k, v) in extra.into_iter() {
            extras.push(nsCString::from(k).into_repr());
            extras.push(nsCString::from(v).into_repr());
        }

        // Ensure that `len == cap` to make deallocation later easier.
        extras.shrink_to_fit();
        let extra_ptr = extras.as_mut_ptr();

        // Need to forget it, so it's not deallocated.
        // It needs to be deallocated later by passing it to `fog_event_free_event_extra`.
        std::mem::forget(extras);

        let event = FfiRecordedEvent {
            timestamp: event.timestamp,
            category: nsCString::from(event.category).into_repr(),
            name: nsCString::from(event.name).into_repr(),
            extra_len: extra_len as u32,
            extra: extra_ptr,
        };

        out_events.push(event);
    }
}

/// Free the event extra array from a `FfiRecordedEvent` as returned by `fog_event_test_get_value`.
///
/// SAFETY:
/// * The pointer needs to be from a `FfiRecordedEvent` returned from `fog_event_test_get_value`
/// * The array is assumed to be fitted, that is `len == capacity`
/// * `nsCStringRepr` does not drop its iternal buffer on the Rust side,
///   this needs to happen on the C++ side.
#[no_mangle]
pub unsafe extern "C" fn fog_event_free_event_extra(extra: *mut nsCStringRepr, len: u32) {
    assert!(!extra.is_null());
    let v = Vec::from_raw_parts(extra, len as usize, len as usize);
    drop(v);
}
