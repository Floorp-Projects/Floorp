// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use super::{metrics::*, CommonMetricData, Lifetime};

#[derive(Debug)]
pub struct CoreMetrics {
    pub client_id: UuidMetric,
    pub first_run_date: DatetimeMetric,
    pub first_run_hour: DatetimeMetric,
    pub os: StringMetric,
}

#[derive(Debug)]
pub struct AdditionalMetrics {
    /// The number of times we encountered an IO error
    /// when writing a pending ping to disk.
    pub io_errors: CounterMetric,

    /// A count of the pings submitted, by ping type.
    pub pings_submitted: LabeledMetric<CounterMetric>,

    /// The number of times we encountered an invalid timezone offset
    /// (outside of [-24, +24] hours).
    ///
    /// **Note**: This metric has an expiration date set.
    /// However because it's statically defined here we can't specify that.
    /// Needs to be removed after 2021-06-30.
    pub invalid_timezone_offset: CounterMetric,
}

impl CoreMetrics {
    pub fn new() -> CoreMetrics {
        CoreMetrics {
            client_id: UuidMetric::new(CommonMetricData {
                name: "client_id".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::User,
                disabled: false,
                dynamic_label: None,
            }),

            first_run_date: DatetimeMetric::new(
                CommonMetricData {
                    name: "first_run_date".into(),
                    category: "".into(),
                    send_in_pings: vec!["glean_client_info".into()],
                    lifetime: Lifetime::User,
                    disabled: false,
                    dynamic_label: None,
                },
                TimeUnit::Day,
            ),

            first_run_hour: DatetimeMetric::new(
                CommonMetricData {
                    name: "first_run_hour".into(),
                    category: "glean.validation".into(),
                    send_in_pings: vec!["metrics".into(), "baseline".into()],
                    lifetime: Lifetime::User,
                    disabled: false,
                    dynamic_label: None,
                },
                TimeUnit::Hour,
            ),

            os: StringMetric::new(CommonMetricData {
                name: "os".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
        }
    }
}

impl AdditionalMetrics {
    pub fn new() -> AdditionalMetrics {
        AdditionalMetrics {
            io_errors: CounterMetric::new(CommonMetricData {
                name: "io".into(),
                category: "glean.error".into(),
                send_in_pings: vec!["metrics".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                dynamic_label: None,
            }),

            pings_submitted: LabeledMetric::new(
                CounterMetric::new(CommonMetricData {
                    name: "pings_submitted".into(),
                    category: "glean.validation".into(),
                    send_in_pings: vec!["metrics".into(), "baseline".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                }),
                None,
            ),

            invalid_timezone_offset: CounterMetric::new(CommonMetricData {
                name: "invalid_timezone_offset".into(),
                category: "glean.time".into(),
                send_in_pings: vec!["metrics".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                dynamic_label: None,
            }),
        }
    }
}

#[derive(Debug)]
pub struct UploadMetrics {
    pub ping_upload_failure: LabeledMetric<CounterMetric>,
    pub discarded_exceeding_pings_size: MemoryDistributionMetric,
    pub pending_pings_directory_size: MemoryDistributionMetric,
    pub deleted_pings_after_quota_hit: CounterMetric,
    pub pending_pings: CounterMetric,
}

impl UploadMetrics {
    pub fn new() -> UploadMetrics {
        UploadMetrics {
            ping_upload_failure: LabeledMetric::new(
                CounterMetric::new(CommonMetricData {
                    name: "ping_upload_failure".into(),
                    category: "glean.upload".into(),
                    send_in_pings: vec!["metrics".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                }),
                Some(vec![
                    "status_code_4xx".into(),
                    "status_code_5xx".into(),
                    "status_code_unknown".into(),
                    "unrecoverable".into(),
                    "recoverable".into(),
                ]),
            ),

            discarded_exceeding_pings_size: MemoryDistributionMetric::new(
                CommonMetricData {
                    name: "discarded_exceeding_ping_size".into(),
                    category: "glean.upload".into(),
                    send_in_pings: vec!["metrics".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                },
                MemoryUnit::Kilobyte,
            ),

            pending_pings_directory_size: MemoryDistributionMetric::new(
                CommonMetricData {
                    name: "pending_pings_directory_size".into(),
                    category: "glean.upload".into(),
                    send_in_pings: vec!["metrics".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                },
                MemoryUnit::Kilobyte,
            ),

            deleted_pings_after_quota_hit: CounterMetric::new(CommonMetricData {
                name: "deleted_pings_after_quota_hit".into(),
                category: "glean.upload".into(),
                send_in_pings: vec!["metrics".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                dynamic_label: None,
            }),

            pending_pings: CounterMetric::new(CommonMetricData {
                name: "pending_pings".into(),
                category: "glean.upload".into(),
                send_in_pings: vec!["metrics".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                dynamic_label: None,
            }),
        }
    }
}

#[derive(Debug)]
pub struct DatabaseMetrics {
    pub size: MemoryDistributionMetric,
}

impl DatabaseMetrics {
    pub fn new() -> DatabaseMetrics {
        DatabaseMetrics {
            size: MemoryDistributionMetric::new(
                CommonMetricData {
                    name: "size".into(),
                    category: "glean.database".into(),
                    send_in_pings: vec!["metrics".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                },
                MemoryUnit::Byte,
            ),
        }
    }
}
