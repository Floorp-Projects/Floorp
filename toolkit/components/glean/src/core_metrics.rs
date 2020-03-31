// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Internal metrics used to fill in the [client info section][client_info] included in every ping.
//!
//! [client_info]: https://mozilla.github.io/glean/book/user/pings/index.html#the-client_info-section

use glean_core::{metrics::StringMetric, CommonMetricData, Lifetime};

#[derive(Debug)]
pub struct InternalMetrics {
    pub app_build: StringMetric,
    pub app_display_version: StringMetric,
    pub app_channel: StringMetric,
    pub os: StringMetric,
    pub os_version: StringMetric,
    pub architecture: StringMetric,
    pub device_manufacturer: StringMetric,
    pub device_model: StringMetric,
}

impl InternalMetrics {
    pub fn new() -> Self {
        Self {
            app_build: StringMetric::new(CommonMetricData {
                name: "app_build".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
            app_display_version: StringMetric::new(CommonMetricData {
                name: "app_display_version".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
            app_channel: StringMetric::new(CommonMetricData {
                name: "app_channel".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
            os: StringMetric::new(CommonMetricData {
                name: "os".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
            os_version: StringMetric::new(CommonMetricData {
                name: "os_version".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
            architecture: StringMetric::new(CommonMetricData {
                name: "architecture".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
            device_manufacturer: StringMetric::new(CommonMetricData {
                name: "device_manufacturer".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
            device_model: StringMetric::new(CommonMetricData {
                name: "device_model".into(),
                category: "".into(),
                send_in_pings: vec!["glean_client_info".into()],
                lifetime: Lifetime::Application,
                disabled: false,
                dynamic_label: None,
            }),
        }
    }
}
