// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use serde_json::json;

use glean_core::metrics::*;
use glean_core::storage::StorageManager;
use glean_core::{CommonMetricData, Lifetime};

const HEADER: &str = "eyJhbGciOiJSU0EtT0FFUCIsImVuYyI6IkEyNTZHQ00ifQ";
const KEY: &str = "OKOawDo13gRp2ojaHV7LFpZcgV7T6DVZKTyKOMTYUmKoTCVJRgckCL9kiMT03JGeipsEdY3mx_etLbbWSrFr05kLzcSr4qKAq7YN7e9jwQRb23nfa6c9d-StnImGyFDbSv04uVuxIp5Zms1gNxKKK2Da14B8S4rzVRltdYwam_lDp5XnZAYpQdb76FdIKLaVmqgfwX7XWRxv2322i-vDxRfqNzo_tETKzpVLzfiwQyeyPGLBIO56YJ7eObdv0je81860ppamavo35UgoRdbYaBcoh9QcfylQr66oc6vFWXRcZ_ZT2LawVCWTIy3brGPi6UklfCpIMfIjf7iGdXKHzg";
const INIT_VECTOR: &str = "48V1_ALb6US04U3b";
const CIPHER_TEXT: &str =
    "5eym8TW_c8SuK0ltJ3rpYIzOeDQz7TALvtu6UG9oMo4vpzs9tX_EFShS8iB7j6jiSdiwkIr3ajwQzaBtQD_A";
const AUTH_TAG: &str = "XFBoMYUZodetZdvTiFvSkQ";
const JWE: &str = "eyJhbGciOiJSU0EtT0FFUCIsImVuYyI6IkEyNTZHQ00ifQ.OKOawDo13gRp2ojaHV7LFpZcgV7T6DVZKTyKOMTYUmKoTCVJRgckCL9kiMT03JGeipsEdY3mx_etLbbWSrFr05kLzcSr4qKAq7YN7e9jwQRb23nfa6c9d-StnImGyFDbSv04uVuxIp5Zms1gNxKKK2Da14B8S4rzVRltdYwam_lDp5XnZAYpQdb76FdIKLaVmqgfwX7XWRxv2322i-vDxRfqNzo_tETKzpVLzfiwQyeyPGLBIO56YJ7eObdv0je81860ppamavo35UgoRdbYaBcoh9QcfylQr66oc6vFWXRcZ_ZT2LawVCWTIy3brGPi6UklfCpIMfIjf7iGdXKHzg.48V1_ALb6US04U3b.5eym8TW_c8SuK0ltJ3rpYIzOeDQz7TALvtu6UG9oMo4vpzs9tX_EFShS8iB7j6jiSdiwkIr3ajwQzaBtQD_A.XFBoMYUZodetZdvTiFvSkQ";

#[test]
fn jwe_metric_is_generated_and_stored() {
    let (glean, _t) = new_glean(None);

    let metric = JweMetric::new(CommonMetricData {
        name: "jwe_metric".into(),
        category: "local".into(),
        send_in_pings: vec!["core".into()],
        ..Default::default()
    });

    metric.set_with_compact_representation(&glean, JWE);
    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "core", false)
        .unwrap();

    assert_eq!(
        json!({"jwe": {"local.jwe_metric": metric.test_get_value(&glean, "core") }}),
        snapshot
    );
}

#[test]
fn set_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let metric = JweMetric::new(CommonMetricData {
        name: "jwe_metric".into(),
        category: "local".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set_with_compact_representation(&glean, JWE);

    // Check that the data was correctly set in each store.
    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, false)
            .unwrap();

        assert_eq!(
            json!({"jwe": {"local.jwe_metric": metric.test_get_value(&glean, &store_name) }}),
            snapshot
        );
    }
}

#[test]
fn get_test_value_returns_the_period_delimited_string() {
    let (glean, _t) = new_glean(None);

    let metric = JweMetric::new(CommonMetricData {
        name: "jwe_metric".into(),
        category: "local".into(),
        send_in_pings: vec!["core".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set_with_compact_representation(&glean, JWE);

    assert_eq!(metric.test_get_value(&glean, "core").unwrap(), JWE);
}

#[test]
fn get_test_value_as_json_string_returns_the_expected_repr() {
    let (glean, _t) = new_glean(None);

    let metric = JweMetric::new(CommonMetricData {
        name: "jwe_metric".into(),
        category: "local".into(),
        send_in_pings: vec!["core".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set_with_compact_representation(&glean, JWE);

    let expected_json = format!("{{\"header\":\"{}\",\"key\":\"{}\",\"init_vector\":\"{}\",\"cipher_text\":\"{}\",\"auth_tag\":\"{}\"}}", HEADER, KEY, INIT_VECTOR, CIPHER_TEXT, AUTH_TAG);
    assert_eq!(
        metric
            .test_get_value_as_json_string(&glean, "core")
            .unwrap(),
        expected_json
    );
}
