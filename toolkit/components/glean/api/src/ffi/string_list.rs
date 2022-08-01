// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::{nsACString, nsCString};
use thin_vec::ThinVec;

#[no_mangle]
pub extern "C" fn fog_string_list_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(STRING_LIST_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_string_list_test_get_value(
    id: u32,
    ping_name: &nsACString,
    value: &mut ThinVec<nsCString>,
) {
    let val = with_metric!(STRING_LIST_MAP, id, metric, test_get!(metric, ping_name));
    for v in val {
        value.push(v.into());
    }
}

#[no_mangle]
pub extern "C" fn fog_string_list_add(id: u32, value: &nsACString) {
    with_metric!(STRING_LIST_MAP, id, metric, metric.add(value.to_utf8()));
}

#[no_mangle]
pub extern "C" fn fog_string_list_set(id: u32, value: &ThinVec<nsCString>) {
    let value = value.iter().map(|s| s.to_utf8().into()).collect();
    with_metric!(STRING_LIST_MAP, id, metric, metric.set(value));
}

#[no_mangle]
pub extern "C" fn fog_string_list_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = with_metric!(STRING_LIST_MAP, id, metric, test_get_errors!(metric));
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
