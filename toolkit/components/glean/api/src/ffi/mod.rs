// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#[macro_use]
mod macros;

use ffi_support::FfiStr;
#[cfg(feature = "with_gecko")]
use nsstring::nsACString;
use uuid::Uuid;

define_metric_ffi!(COUNTER_MAP {
    test_has -> fog_counter_test_has_value,
    test_get -> fog_counter_test_get_value: i32,
    add -> fog_counter_add(amount: i32),
});

define_metric_ffi!(TIMESPAN_MAP {
    test_has -> fog_timespan_test_has_value,
    test_get -> fog_timespan_test_get_value: u64,
    start -> fog_timespan_start(),
    stop -> fog_timespan_stop(),
});

// The Uuid functions are custom because test_get needs to use an outparam.
// If we can make test_get optional, we can go back to using the macro to
// generate the rest of the functions, or something.

#[no_mangle]
pub extern "C" fn fog_uuid_test_has_value(id: u32, storage_name: FfiStr) -> u8 {
    match crate::metrics::__glean_metric_maps::UUID_MAP.get(&id.into()) {
        Some(metric) => metric.test_get_value(storage_name.as_str()).is_some() as u8,
        None => panic!("No metric for id {}", id),
    }
}

#[cfg(feature = "with_gecko")]
#[no_mangle]
pub extern "C" fn fog_uuid_test_get_value(id: u32, storage_name: FfiStr, value: &mut nsACString) {
    match crate::metrics::__glean_metric_maps::UUID_MAP.get(&id.into()) {
        Some(uuid) => {
            value.assign(
                &uuid
                    .test_get_value(storage_name.as_str())
                    .unwrap()
                    .to_string(),
            );
        }
        None => panic!("No metric for id {}", id),
    }
}

#[cfg(feature = "with_gecko")]
#[no_mangle]
pub extern "C" fn fog_uuid_set(id: u32, value: &nsACString) {
    if let Ok(uuid) = Uuid::parse_str(&value.to_utf8()) {
        match crate::metrics::__glean_metric_maps::UUID_MAP.get(&id.into()) {
            Some(metric) => metric.set(uuid),
            None => panic!("No metric for id {}", id),
        }
    }
}

#[no_mangle]
pub extern "C" fn fog_uuid_generate_and_set(id: u32) {
    match crate::metrics::__glean_metric_maps::UUID_MAP.get(&id.into()) {
        Some(metric) => metric.generate_and_set(),
        None => panic!("No metric for id {}", id),
    }
}
