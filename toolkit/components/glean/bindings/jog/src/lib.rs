/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use firefox_on_glean::factory;
use firefox_on_glean::private::traits::HistogramType;
use firefox_on_glean::private::{CommonMetricData, Lifetime, MemoryUnit, TimeUnit};
#[cfg(feature = "with_gecko")]
use nsstring::{nsACString, nsAString, nsCString};
use serde::Deserialize;
use std::borrow::Cow;
use std::collections::BTreeMap;
use std::fs::File;
use std::io::BufReader;
use thin_vec::ThinVec;

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
    ordered_labels: Option<Vec<Cow<'static, str>>>,
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
    create_and_register_metric(
        metric_type,
        category,
        name,
        send_in_pings,
        lifetime,
        disabled,
        extra_args,
    )
    .expect("Creation/Registration of metric failed") // ok to panic in test-only method
    .0
}

fn create_and_register_metric(
    metric_type: &str,
    category: String,
    name: String,
    send_in_pings: Vec<String>,
    lifetime: Lifetime,
    disabled: bool,
    extra_args: ExtraMetricArgs,
) -> Result<(u32, u32), Box<dyn std::error::Error>> {
    let ns_name = nsCString::from(&name);
    let ns_category = nsCString::from(&category);
    let metric = factory::create_and_register_metric(
        metric_type,
        category,
        name,
        send_in_pings,
        lifetime,
        disabled,
        extra_args.time_unit,
        extra_args.memory_unit,
        extra_args.allowed_extra_keys.or_else(|| Some(Vec::new())),
        extra_args.range_min,
        extra_args.range_max,
        extra_args.bucket_count,
        extra_args.histogram_type,
        extra_args.numerators,
        extra_args.ordered_labels,
    );
    extern "C" {
        fn JOG_RegisterMetric(
            category: &nsACString,
            name: &nsACString,
            metric: u32,
            metric_id: u32,
        );
    }
    if let Ok((metric, metric_id)) = metric {
        unsafe {
            // Safety: We're loaning to C++ data we don't later use.
            JOG_RegisterMetric(&ns_category, &ns_name, metric, metric_id);
        }
    } else {
        log::warn!(
            "Could not register metric {}.{} due to {:?}",
            ns_category,
            ns_name,
            metric
        );
    }
    metric
}

/// Test-only method.
///
/// Registers a ping. Doesn't check to see if it's been registered before.
/// Doesn't check that it would pass schema validation if it were a real ping.
#[no_mangle]
pub extern "C" fn jog_test_register_ping(
    name: &nsACString,
    include_client_id: bool,
    send_if_empty: bool,
    precise_timestamps: bool,
    reason_codes: &ThinVec<nsCString>,
) -> u32 {
    let ping_name = name.to_string();
    let reason_codes = reason_codes
        .iter()
        .map(|reason| reason.to_string())
        .collect();
    create_and_register_ping(ping_name, include_client_id, send_if_empty, precise_timestamps, reason_codes)
        .expect("Creation or registration of ping failed.") // permitted to panic in test-only method.
}

fn create_and_register_ping(
    ping_name: String,
    include_client_id: bool,
    send_if_empty: bool,
    precise_timestamps: bool,
    reason_codes: Vec<String>,
) -> Result<u32, Box<dyn std::error::Error>> {
    let ns_name = nsCString::from(&ping_name);
    let ping_id = factory::create_and_register_ping(
        ping_name,
        include_client_id,
        send_if_empty,
        precise_timestamps,
        reason_codes,
    );
    extern "C" {
        fn JOG_RegisterPing(name: &nsACString, ping_id: u32);
    }
    if let Ok(ping_id) = ping_id {
        unsafe {
            // Safety: We're loaning to C++ data we don't later use.
            JOG_RegisterPing(&ns_name, ping_id);
        }
    } else {
        log::warn!("Could not register ping {} due to {:?}", ns_name, ping_id);
    }
    ping_id
}

/// Test-only method.
///
/// Clears all runtime registration storage of registered metrics and pings.
#[no_mangle]
pub extern "C" fn jog_test_clear_registered_metrics_and_pings() {}

#[derive(Default, Deserialize)]
struct Jogfile {
    // Using BTreeMap to ensure stable iteration ordering.
    metrics: BTreeMap<String, Vec<MetricDefinitionData>>,
    pings: Vec<PingDefinitionData>,
}

#[derive(Default, Deserialize)]
struct MetricDefinitionData {
    metric_type: String,
    name: String,
    send_in_pings: Vec<String>,
    lifetime: Lifetime,
    disabled: bool,
    #[serde(default)]
    extra_args: Option<ExtraMetricArgs>,
}

#[derive(Default, Deserialize)]
struct PingDefinitionData {
    name: String,
    include_client_id: bool,
    send_if_empty: bool,
    precise_timestamps: bool,
    reason_codes: Option<Vec<String>>,
}

/// Read the file at the provided location, interpret it as a jogfile,
/// and register those pings and metrics.
/// Returns true if we successfully parsed the jogfile. Does not mean
/// all or any metrics and pings successfully registered,
/// just that serde managed to deserialize it into metrics and pings and we tried to register them all.
#[no_mangle]
pub extern "C" fn jog_load_jogfile(jogfile_path: &nsAString) -> bool {
    let f = match File::open(jogfile_path.to_string()) {
        Ok(f) => f,
        _ => {
            log::error!("Boo, couldn't open jogfile at {}", jogfile_path.to_string());
            return false;
        }
    };
    let reader = BufReader::new(f);

    let j: Jogfile = match serde_json::from_reader(reader) {
        Ok(j) => j,
        Err(e) => {
            log::error!("Boo, couldn't read jogfile because of: {:?}", e);
            return false;
        }
    };
    log::trace!("Loaded jogfile. Registering metrics+pings.");
    for (category, metrics) in j.metrics.into_iter() {
        for metric in metrics.into_iter() {
            let _ = create_and_register_metric(
                &metric.metric_type,
                category.to_string(),
                metric.name,
                metric.send_in_pings,
                metric.lifetime,
                metric.disabled,
                metric.extra_args.unwrap_or_else(Default::default),
            );
        }
    }
    for ping in j.pings.into_iter() {
        let _ = create_and_register_ping(
            ping.name,
            ping.include_client_id,
            ping.send_if_empty,
            ping.precise_timestamps,
            ping.reason_codes.unwrap_or_else(Vec::new),
        );
    }
    true
}
