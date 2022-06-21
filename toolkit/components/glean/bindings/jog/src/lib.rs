/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use fog::factory;
use fog::private::Lifetime;

/// Activate the JOG runtime registrar.
/// This is where the Artefact Build support happens.
///
/// returns whether it successfully found and processed metrics files.
#[no_mangle]
pub extern "C" fn jog_runtime_registrar() -> bool {
    let _ = factory::create_and_register_metric(
        "counter",
        "category".into(),
        "name".into(),
        vec!["store1".into()],
        Lifetime::Ping,
        true,
        None,
        None,
        None,
        None,
        None,
        None,
        None,
        None,
        None,
    );
    false
}

/// Test-only method.
///
/// Registers a metric.
/// Doesn't check to see if it's been registered before.
/// Doesn't check that it would pass schema validation if it were a real metric.
#[no_mangle]
pub extern "C" fn jog_test_register_metric() {
    // What information do we need for registration?
    // Pretty much everything in CommonMetricData except `disabled` which we presume is `false`, so
    // name, category, send_in_pings, lifetime

    // Then we need everything else we might need (cf glean_parser.util.extra_metric_args:
    // time_unit, memory_unit, allowed_extra_keys, reason_codes, range_min, range_max, bucket_count, histogram_type, numerators
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
