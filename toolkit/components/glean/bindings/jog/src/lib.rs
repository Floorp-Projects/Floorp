/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use fog::factory;
use fog::private::traits::HistogramType;
use fog::private::{CommonMetricData, MemoryUnit, TimeUnit};
#[cfg(feature = "with_gecko")]
use nsstring::{nsACString, nsCString};
use serde::Deserialize;
use thin_vec::ThinVec;

/// Activate the JOG runtime registrar.
/// This is where the Artefact Build support happens.
///
/// returns whether it successfully found and processed metrics files.
#[no_mangle]
pub extern "C" fn jog_runtime_registrar() -> bool {
    false
}

#[derive(Default, Deserialize)]
struct ExtraMetricArgs {
    time_unit: Option<TimeUnit>,
    memory_unit: Option<MemoryUnit>,
    allowed_extra_keys: Option<Vec<String>>,
    range_min: Option<u64>,
    range_max: Option<u64>,
    bucket_count: Option<u64>,
    histogram_type: Option<HistogramType>,
    numerators: Option<Vec<CommonMetricData>>,
    labels: Option<Vec<String>>,
}

/// Test-only method.
///
/// Registers a metric.
/// Doesn't check to see if it's been registered before.
/// Doesn't check that it would pass schema validation if it were a real metric.
///
/// `extra_args` is a JSON-encoded string in a form that serde can read into an ExtraMetricArgs.
///
/// No effort has been made to make this pleasant to use, since it's for
/// internal testing only (ie, the testing of JOG itself).
#[cfg(feature = "with_gecko")]
#[no_mangle]
pub extern "C" fn jog_test_register_metric(
    metric_type: &nsACString,
    category: &nsACString,
    name: &nsACString,
    send_in_pings: &ThinVec<nsCString>,
    lifetime: &nsACString,
    disabled: bool,
    extra_args: &nsACString,
) -> u32 {
    log::warn!("Type: {:?}, Category: {:?}, Name: {:?}, SendInPings: {:?}, Lifetime: {:?}, Disabled: {}, ExtraArgs: {}",
      metric_type, category, name, send_in_pings, lifetime, disabled, extra_args);
    let ns_category = category;
    let ns_name = name;
    let metric_type = &metric_type.to_utf8();
    let category = category.to_string();
    let name = name.to_string();
    let send_in_pings = send_in_pings.iter().map(|ping| ping.to_string()).collect();
    let lifetime = serde_json::from_str(&lifetime.to_utf8())
        .expect("Lifetime didn't deserialize happily. Is it valid JSON?");

    let extra_args: ExtraMetricArgs = if extra_args.is_empty() {
        Default::default()
    } else {
        serde_json::from_str(&extra_args.to_utf8())
            .expect("Extras didn't deserialize happily. Are they valid JSON?")
    };
    let metric = factory::create_and_register_metric(
        metric_type,
        category,
        name,
        send_in_pings,
        lifetime,
        disabled,
        extra_args.time_unit,
        extra_args.memory_unit,
        extra_args.allowed_extra_keys,
        extra_args.range_min,
        extra_args.range_max,
        extra_args.bucket_count,
        extra_args.histogram_type,
        extra_args.numerators,
        extra_args.labels,
    );
    extern "C" {
        fn JOG_RegisterMetric(category: &nsACString, name: &nsACString, metric: u32);
    }
    let metric = metric.unwrap();
    // Safety: We're loaning to C++ the same nsCStrings they leant us.
    unsafe {
        JOG_RegisterMetric(ns_category, ns_name, metric);
    }
    metric
}

/// Test-only method.
///
/// Registers a ping. Doesn't check to see if it's been registered before.
/// Doesn't check that it would pass schema validation if it were a real ping.
#[no_mangle]
pub extern "C" fn jog_test_register_ping() {
    // What information do we need for registration? See glean_parser.util.ping_args:
    // include_client_id, send_if_empty, name, reason_codes
}

/// Test-only method.
///
/// Clears all runtime registration storage of registered metrics and pings.
#[no_mangle]
pub extern "C" fn jog_test_clear_registered_metrics_and_pings() {}

#[cfg(test)]
mod tests {
    #[test]
    fn runtime_registrar_does_nothing() {
        assert!(!jog_runtime_registrar());
    }
}
