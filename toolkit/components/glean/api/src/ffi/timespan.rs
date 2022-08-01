// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;

#[no_mangle]
pub extern "C" fn fog_timespan_start(id: u32) {
    with_metric!(TIMESPAN_MAP, id, metric, metric.start());
}

#[no_mangle]
pub extern "C" fn fog_timespan_stop(id: u32) {
    with_metric!(TIMESPAN_MAP, id, metric, metric.stop());
}

#[no_mangle]
pub extern "C" fn fog_timespan_cancel(id: u32) {
    with_metric!(TIMESPAN_MAP, id, metric, metric.cancel());
}

#[no_mangle]
pub extern "C" fn fog_timespan_set_raw(id: u32, duration: u32) {
    with_metric!(
        TIMESPAN_MAP,
        id,
        metric,
        metric.set_raw_unitless(duration.into())
    );
}

#[no_mangle]
pub extern "C" fn fog_timespan_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(TIMESPAN_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_timespan_test_get_value(id: u32, ping_name: &nsACString) -> u64 {
    with_metric!(TIMESPAN_MAP, id, metric, test_get!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_timespan_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = with_metric!(TIMESPAN_MAP, id, metric, test_get_errors!(metric));
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
