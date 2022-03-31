// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;
use thin_vec::ThinVec;

#[no_mangle]
pub extern "C" fn fog_timing_distribution_start(id: u32) -> u64 {
    with_metric!(TIMING_DISTRIBUTION_MAP, id, metric, metric.start())
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_stop_and_accumulate(id: u32, timing_id: u64) {
    with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        metric.stop_and_accumulate(timing_id)
    );
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_cancel(id: u32, timing_id: u64) {
    with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        metric.cancel(timing_id)
    );
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        test_has!(metric, ping_name)
    )
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_test_get_value(
    id: u32,
    ping_name: &nsACString,
    sum: &mut u64,
    buckets: &mut ThinVec<u64>,
    counts: &mut ThinVec<u64>,
) {
    let val = with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        test_get!(metric, ping_name)
    );
    *sum = val.sum;
    for (&bucket, &count) in val.values.iter() {
        buckets.push(bucket);
        counts.push(count);
    }
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_test_get_error(
    id: u32,
    ping_name: &nsACString,
    error_str: &mut nsACString,
) -> bool {
    let err = with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        test_get_errors!(metric, ping_name)
    );
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
