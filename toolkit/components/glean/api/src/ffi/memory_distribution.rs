// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;
use thin_vec::ThinVec;

#[no_mangle]
pub extern "C" fn fog_memory_distribution_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(
        MEMORY_DISTRIBUTION_MAP,
        id,
        metric,
        test_has!(metric, ping_name)
    )
}

#[no_mangle]
pub extern "C" fn fog_memory_distribution_test_get_value(
    id: u32,
    ping_name: &nsACString,
    sum: &mut u64,
    buckets: &mut ThinVec<u64>,
    counts: &mut ThinVec<u64>,
) {
    let val = with_metric!(
        MEMORY_DISTRIBUTION_MAP,
        id,
        metric,
        test_get!(metric, ping_name)
    );
    *sum = val.sum as _;
    for (&bucket, &count) in val.values.iter() {
        buckets.push(bucket as _);
        counts.push(count as _);
    }
}

#[no_mangle]
pub extern "C" fn fog_memory_distribution_accumulate(id: u32, sample: u64) {
    with_metric!(
        MEMORY_DISTRIBUTION_MAP,
        id,
        metric,
        metric.accumulate(sample)
    );
}

#[no_mangle]
pub extern "C" fn fog_memory_distribution_test_get_error(
    id: u32,

    error_str: &mut nsACString,
) -> bool {
    let err = with_metric!(
        MEMORY_DISTRIBUTION_MAP,
        id,
        metric,
        test_get_errors!(metric)
    );
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
