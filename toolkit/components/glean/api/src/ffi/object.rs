// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;

use crate::metrics::__glean_metric_maps as metric_maps;

#[no_mangle]
pub extern "C" fn fog_object_set_string(id: u32, value: &nsACString) {
    if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        panic!("No dynamic metric for objects");
    }

    let value = value.to_utf8().to_string();
    if metric_maps::set_object_by_id(id, value).is_err() {
        panic!("No object for id {}", id);
    }
}

#[no_mangle]
pub unsafe extern "C" fn fog_object_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    let storage = if ping_name.is_empty() {
        None
    } else {
        Some(ping_name.to_utf8().into_owned())
    };
    if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        panic!("No dynamic metric for objects");
    } else {
        metric_maps::object_test_get_value(id, storage).is_some()
    }
}

#[no_mangle]
pub extern "C" fn fog_object_test_get_value(
    id: u32,
    ping_name: &nsACString,
    value: &mut nsACString,
) {
    let storage = if ping_name.is_empty() {
        None
    } else {
        Some(ping_name.to_utf8().into_owned())
    };

    let object = if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        panic!("No dynamic metric for objects");
    } else {
        match metric_maps::object_test_get_value(id, storage) {
            Some(object) => object,
            None => return,
        }
    };
    value.assign(&object);
}

#[no_mangle]
pub extern "C" fn fog_object_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = if id & (1 << crate::factory::DYNAMIC_METRIC_BIT) > 0 {
        panic!("No dynamic metric for objects");
    } else {
        metric_maps::object_test_get_error(id)
    };
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
