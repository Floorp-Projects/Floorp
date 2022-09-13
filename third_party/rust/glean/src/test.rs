// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{Arc, Barrier, Mutex};
use std::thread::{self, ThreadId};

use crate::private::PingType;
use crate::private::{BooleanMetric, CounterMetric, EventMetric, StringMetric};

use super::*;
use crate::common_test::{lock_test, new_glean, GLOBAL_APPLICATION_ID};

#[test]
fn send_a_ping() {
    let _lock = lock_test();

    let (s, r) = crossbeam_channel::bounded::<String>(1);

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Define a new ping and submit it.
    const PING_NAME: &str = "test-ping";
    let custom_ping = private::PingType::new(PING_NAME, true, true, vec![]);
    custom_ping.submit(None);

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert!(url.contains(PING_NAME));
}

#[test]
fn disabling_upload_disables_metrics_recording() {
    let _lock = lock_test();

    let _t = new_glean(None, true);

    let metric = BooleanMetric::new(CommonMetricData {
        name: "bool_metric".into(),
        category: "test".into(),
        send_in_pings: vec!["store1".into()],
        lifetime: Lifetime::Application,
        disabled: false,
        dynamic_label: None,
    });

    crate::set_upload_enabled(false);

    assert!(metric.test_get_value(Some("store1".into())).is_none())
}

#[test]
fn test_experiments_recording() {
    let _lock = lock_test();

    let _t = new_glean(None, true);

    set_experiment_active("experiment_test".to_string(), "branch_a".to_string(), None);
    let mut extra = HashMap::new();
    extra.insert("test_key".to_string(), "value".to_string());
    set_experiment_active(
        "experiment_api".to_string(),
        "branch_b".to_string(),
        Some(extra),
    );
    assert!(test_is_experiment_active("experiment_test".to_string()));
    assert!(test_is_experiment_active("experiment_api".to_string()));
    set_experiment_inactive("experiment_test".to_string());
    assert!(!test_is_experiment_active("experiment_test".to_string()));
    assert!(test_is_experiment_active("experiment_api".to_string()));
    let stored_data = test_get_experiment_data("experiment_api".to_string()).unwrap();
    assert_eq!("branch_b", stored_data.branch);
    assert_eq!("value", stored_data.extra.unwrap()["test_key"]);
}

#[test]
fn test_experiments_recording_before_glean_inits() {
    let _lock = lock_test();

    destroy_glean(true);

    set_experiment_active(
        "experiment_set_preinit".to_string(),
        "branch_a".to_string(),
        None,
    );
    set_experiment_active(
        "experiment_preinit_disabled".to_string(),
        "branch_a".to_string(),
        None,
    );
    set_experiment_inactive("experiment_preinit_disabled".to_string());

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    assert!(test_is_experiment_active(
        "experiment_set_preinit".to_string()
    ));
    assert!(!test_is_experiment_active(
        "experiment_preinit_disabled".to_string()
    ));
}

#[test]
fn sending_of_foreground_background_pings() {
    let _lock = lock_test();

    let click: EventMetric<traits::NoExtraKeys> = private::EventMetric::new(CommonMetricData {
        name: "click".into(),
        category: "ui".into(),
        send_in_pings: vec!["events".into()],
        lifetime: Lifetime::Ping,
        disabled: false,
        ..Default::default()
    });

    // Define a fake uploader that reports back the submission headers
    // using a crossbeam channel.
    let (s, r) = crossbeam_channel::bounded::<String>(3);

    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Simulate becoming active.
    handle_client_active();

    // We expect a baseline ping to be generated here (reason: 'active').
    let url = r.recv().unwrap();
    assert!(url.contains("baseline"));

    // Recording an event so that an "events" ping will contain data.
    click.record(None);

    // Simulate becoming inactive
    handle_client_inactive();

    // Wait for the pings to arrive.
    let mut expected_pings = vec!["baseline", "events"];
    for _ in 0..2 {
        let url = r.recv().unwrap();
        // If the url contains the expected reason, remove it from the list.
        expected_pings.retain(|&name| !url.contains(name));
    }
    // We received all the expected pings.
    assert_eq!(0, expected_pings.len());

    // Simulate becoming active again.
    handle_client_active();

    // We expect a baseline ping to be generated here (reason: 'active').
    let url = r.recv().unwrap();
    assert!(url.contains("baseline"));
}

#[test]
fn sending_of_startup_baseline_ping() {
    let _lock = lock_test();

    // Create an instance of Glean and then flip the dirty
    // bit to true.
    let data_dir = new_glean(None, true);

    glean_core::glean_set_dirty_flag(true);

    // Restart glean and wait for a baseline ping to be generated.
    let (s, r) = crossbeam_channel::bounded::<String>(1);

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let tmpname = data_dir.path().to_path_buf();

    // Now reset Glean: it should still send a baseline ping with reason
    // dirty_startup when starting, because of the dirty bit being set.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert!(url.contains("baseline"));
}

#[test]
fn no_dirty_baseline_on_clean_shutdowns() {
    let _lock = lock_test();

    // Create an instance of Glean, wait for init and then flip the dirty
    // bit to true.
    let data_dir = new_glean(None, true);

    glean_core::glean_set_dirty_flag(true);

    crate::shutdown();

    // Restart glean and wait for a baseline ping to be generated.
    let (s, r) = crossbeam_channel::bounded::<String>(1);

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let tmpname = data_dir.path().to_path_buf();

    // Now reset Glean: it should not send a baseline ping, because
    // we cleared the dirty bit.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    // We don't expect a startup ping.
    assert_eq!(r.try_recv(), Err(crossbeam_channel::TryRecvError::Empty));
}

#[test]
fn initialize_must_not_crash_if_data_dir_is_messed_up() {
    let _lock = lock_test();

    let dir = tempfile::tempdir().unwrap();
    let tmpdirname = dir.path();
    // Create a file in the temporary dir and use that as the
    // name of the Glean data dir.
    let file_path = tmpdirname.to_path_buf().join("notadir");
    std::fs::write(file_path.clone(), "test").expect("The test Glean dir file must be created");

    let cfg = Configuration {
        data_path: file_path,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
        use_core_mps: false,
    };

    test_reset_glean(cfg, ClientInfoMetrics::unknown(), false);

    // We don't need to sleep here.
    // The `test_reset_glean` already waited on the initialize task.
}

#[test]
fn queued_recorded_metrics_correctly_record_during_init() {
    let _lock = lock_test();

    destroy_glean(true);

    let metric = CounterMetric::new(CommonMetricData {
        name: "counter_metric".into(),
        category: "test".into(),
        send_in_pings: vec!["store1".into()],
        lifetime: Lifetime::Application,
        disabled: false,
        dynamic_label: None,
    });

    // This will queue 3 tasks that will add to the metric value once Glean is initialized
    for _ in 0..3 {
        metric.add(1);
    }

    // TODO: To be fixed in bug 1677150.
    // Ensure that no value has been stored yet since the tasks have only been queued
    // and not executed yet

    // Calling `new_glean` here will cause Glean to be initialized and should cause the queued
    // tasks recording metrics to execute
    let _t = new_glean(None, false);

    // Verify that the callback was executed by testing for the correct value
    assert!(metric.test_get_value(None).is_some(), "Value must exist");
    assert_eq!(3, metric.test_get_value(None).unwrap(), "Value must match");
}

#[test]
fn initializing_twice_is_a_noop() {
    let _lock = lock_test();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    test_reset_glean(
        Configuration {
            data_path: tmpname.clone(),
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
        true,
    );

    // Glean was initialized and it waited for a full initialization to finish.
    // We now just want to try to initialize again.
    // This will bail out early.

    crate::initialize(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
    );

    // We don't need to sleep here.
    // The `test_reset_glean` already waited on the initialize task,
    // and the 2nd initialize will bail out early.
    //
    // All we tested is that this didn't crash.
}

#[test]
#[ignore] // TODO: To be done in bug 1673668.
fn dont_handle_events_when_uninitialized() {
    todo!()
}

// TODO: Should probably move into glean-core.
#[test]
fn the_app_channel_must_be_correctly_set_if_requested() {
    let _lock = lock_test();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    // Internal metric, replicated here for testing.
    let app_channel = StringMetric::new(CommonMetricData {
        name: "app_channel".into(),
        category: "".into(),
        send_in_pings: vec!["glean_client_info".into()],
        lifetime: Lifetime::Application,
        disabled: false,
        ..Default::default()
    });

    // No app_channel reported.
    let client_info = ClientInfoMetrics {
        channel: None,
        ..ClientInfoMetrics::unknown()
    };
    test_reset_glean(
        Configuration {
            data_path: tmpname.clone(),
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
            use_core_mps: false,
        },
        client_info,
        true,
    );
    assert!(app_channel.test_get_value(None).is_none());

    // Custom app_channel reported.
    let client_info = ClientInfoMetrics {
        channel: Some("testing".into()),
        ..ClientInfoMetrics::unknown()
    };
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
            use_core_mps: false,
        },
        client_info,
        true,
    );
    assert_eq!("testing", app_channel.test_get_value(None).unwrap());
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn ping_collection_must_happen_after_concurrently_scheduled_metrics_recordings() {
    todo!()
}

#[test]
fn basic_metrics_should_be_cleared_when_disabling_uploading() {
    let _lock = lock_test();

    let _t = new_glean(None, false);

    let metric = private::StringMetric::new(CommonMetricData {
        name: "string_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["default".into()],
        lifetime: Lifetime::Ping,
        disabled: false,
        ..Default::default()
    });

    assert!(metric.test_get_value(None).is_none());

    metric.set("TEST VALUE".into());
    assert!(metric.test_get_value(None).is_some());

    set_upload_enabled(false);
    assert!(metric.test_get_value(None).is_none());
    metric.set("TEST VALUE".into());
    assert!(metric.test_get_value(None).is_none());

    set_upload_enabled(true);
    assert!(metric.test_get_value(None).is_none());
    metric.set("TEST VALUE".into());
    assert_eq!("TEST VALUE", metric.test_get_value(None).unwrap());
}

// TODO: Should probably move into glean-core.
#[test]
fn core_metrics_should_be_cleared_and_restored_when_disabling_and_enabling_uploading() {
    let _lock = lock_test();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    // No app_channel reported.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
        true,
    );

    // Internal metric, replicated here for testing.
    let os_version = StringMetric::new(CommonMetricData {
        name: "os_version".into(),
        category: "".into(),
        send_in_pings: vec!["glean_client_info".into()],
        lifetime: Lifetime::Application,
        disabled: false,
        ..Default::default()
    });

    assert!(os_version.test_get_value(None).is_some());

    set_upload_enabled(false);
    assert!(os_version.test_get_value(None).is_none());

    set_upload_enabled(true);
    assert!(os_version.test_get_value(None).is_some());
}

#[test]
fn sending_deletion_ping_if_disabled_outside_of_run() {
    let _lock = lock_test();

    let (s, r) = crossbeam_channel::bounded::<String>(1);

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname.clone(),
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Now reset Glean and disable upload: it should still send a deletion request
    // ping even though we're just starting.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: false,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert!(url.contains("deletion-request"));
}

#[test]
fn no_sending_of_deletion_ping_if_unchanged_outside_of_run() {
    let _lock = lock_test();

    let (s, r) = crossbeam_channel::bounded::<String>(1);

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname.clone(),
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Now reset Glean and keep upload enabled: no deletion-request
    // should be sent.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
            use_core_mps: false,
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    assert_eq!(0, r.len());
}

#[test]
#[ignore] // TODO: To be done in bug 1672956.
fn test_sending_of_startup_baseline_ping_with_application_lifetime_metric() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1672956.
fn test_dirty_flag_is_reset_to_false() {
    todo!()
}

#[test]
fn setting_debug_view_tag_before_initialization_should_not_crash() {
    let _lock = lock_test();

    destroy_glean(true);

    // Define a fake uploader that reports back the submission headers
    // using a crossbeam channel.
    let (s, r) = crossbeam_channel::bounded::<Vec<(String, String)>>(1);

    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<Vec<(String, String)>>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            _url: String,
            _body: Vec<u8>,
            headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(headers).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Attempt to set a debug view tag before Glean is initialized.
    set_debug_view_tag("valid-tag");

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Submit a baseline ping.
    submit_ping_by_name("baseline", Some("inactive"));

    // Wait for the ping to arrive.
    let headers = r.recv().unwrap();
    assert_eq!(
        "valid-tag",
        headers.iter().find(|&kv| kv.0 == "X-Debug-ID").unwrap().1
    );
}

#[test]
fn setting_source_tags_before_initialization_should_not_crash() {
    let _lock = lock_test();

    destroy_glean(true);
    //assert!(!was_initialize_called());

    // Define a fake uploader that reports back the submission headers
    // using a crossbeam channel.
    let (s, r) = crossbeam_channel::bounded::<Vec<(String, String)>>(1);

    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<Vec<(String, String)>>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            _url: String,
            _body: Vec<u8>,
            headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(headers).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Attempt to set source tags before Glean is initialized.
    set_source_tags(vec!["valid-tag1".to_string(), "valid-tag2".to_string()]);

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Submit a baseline ping.
    submit_ping_by_name("baseline", Some("inactive"));

    // Wait for the ping to arrive.
    let headers = r.recv().unwrap();
    assert_eq!(
        "valid-tag1,valid-tag2",
        headers
            .iter()
            .find(|&kv| kv.0 == "X-Source-Tags")
            .unwrap()
            .1
    );
}

#[test]
fn setting_source_tags_after_initialization_should_not_crash() {
    let _lock = lock_test();

    destroy_glean(true);
    //assert!(!was_initialize_called());

    // Define a fake uploader that reports back the submission headers
    // using a crossbeam channel.
    let (s, r) = crossbeam_channel::bounded::<Vec<(String, String)>>(1);

    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<Vec<(String, String)>>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            _url: String,
            _body: Vec<u8>,
            headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(headers).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Attempt to set source tags after `Glean.initialize` is called,
    // but before Glean is fully initialized.
    //assert!(was_initialize_called());
    set_source_tags(vec!["valid-tag1".to_string(), "valid-tag2".to_string()]);

    // Submit a baseline ping.
    submit_ping_by_name("baseline", Some("inactive"));

    // Wait for the ping to arrive.
    let headers = r.recv().unwrap();
    assert_eq!(
        "valid-tag1,valid-tag2",
        headers
            .iter()
            .find(|&kv| kv.0 == "X-Source-Tags")
            .unwrap()
            .1
    );
}

#[test]
fn flipping_upload_enabled_respects_order_of_events() {
    // NOTES(janerik):
    // I'm reasonably sure this test is excercising the right code paths
    // and from the log output it does the right thing:
    //
    // * It fully initializes with the assumption uploadEnabled=true
    // * It then disables upload
    // * Then it submits the custom ping, which rightfully is ignored because uploadEnabled=false.
    //
    // The test passes.
    let _lock = lock_test();

    let (s, r) = crossbeam_channel::bounded::<String>(1);

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    // We create a ping and a metric before we initialize Glean
    let sample_ping = PingType::new("sample-ping-1", true, false, vec![]);
    let metric = private::StringMetric::new(CommonMetricData {
        name: "string_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["sample-ping-1".into()],
        lifetime: Lifetime::Ping,
        disabled: false,
        ..Default::default()
    });

    let _t = new_glean(Some(cfg), true);

    // Glean might still be initializing. Disable upload.
    set_upload_enabled(false);

    // Set data and try to submit a custom ping.
    metric.set("some-test-value".into());
    sample_ping.submit(None);

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert!(url.contains("deletion-request"));
}

#[test]
fn registering_pings_before_init_must_work() {
    let _lock = lock_test();

    destroy_glean(true);
    //assert!(!was_initialize_called());

    // Define a fake uploader that reports back the submission headers
    // using a crossbeam channel.
    let (s, r) = crossbeam_channel::bounded::<String>(1);

    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom ping and attempt its registration.
    let sample_ping = PingType::new("pre-register", true, true, vec![]);

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Submit a baseline ping.
    sample_ping.submit(None);

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert!(url.contains("pre-register"));
}

#[test]
fn test_a_ping_before_submission() {
    let _lock = lock_test();

    // Define a fake uploader that reports back the submission headers
    // using a crossbeam channel.
    let (s, r) = crossbeam_channel::bounded::<String>(1);

    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            self.sender.send(url).unwrap();
            net::UploadResult::http_status(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Create a custom ping and register it.
    let sample_ping = PingType::new("custom1", true, true, vec![]);

    let metric = CounterMetric::new(CommonMetricData {
        name: "counter_metric".into(),
        category: "test".into(),
        send_in_pings: vec!["custom1".into()],
        lifetime: Lifetime::Application,
        disabled: false,
        dynamic_label: None,
    });

    metric.add(1);

    sample_ping.test_before_next_submit(move |reason| {
        assert_eq!(None, reason);
        assert_eq!(1, metric.test_get_value(None).unwrap());
    });

    // Submit a baseline ping.
    sample_ping.submit(None);

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert!(url.contains("custom1"));
}

#[test]
fn test_boolean_get_num_errors() {
    let _lock = lock_test();

    let _t = new_glean(None, false);

    let metric = BooleanMetric::new(CommonMetricData {
        name: "counter_metric".into(),
        category: "test".into(),
        send_in_pings: vec!["custom1".into()],
        lifetime: Lifetime::Application,
        disabled: false,
        dynamic_label: Some(str::to_string("asdf")),
    });

    // Check specifically for an invalid label
    let result = metric.test_get_num_recorded_errors(ErrorType::InvalidLabel);

    assert_eq!(result, 0);
}

#[test]
fn signaling_done() {
    let _lock = lock_test();

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        barrier: Arc<Barrier>,
        counter: Arc<Mutex<HashMap<ThreadId, u32>>>,
    }
    impl net::PingUploader for FakeUploader {
        fn upload(
            &self,
            _url: String,
            _body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> net::UploadResult {
            let mut map = self.counter.lock().unwrap();
            *map.entry(thread::current().id()).or_insert(0) += 1;

            // Wait for the sync.
            self.barrier.wait();

            // Signal that this uploader thread is done.
            net::UploadResult::done()
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    // We use a barrier to sync this test thread with the uploader thread.
    let barrier = Arc::new(Barrier::new(2));
    // We count how many times `upload` was invoked per thread.
    let call_count = Arc::new(Mutex::default());

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader {
            barrier: Arc::clone(&barrier),
            counter: Arc::clone(&call_count),
        })),
        use_core_mps: false,
    };

    let _t = new_glean(Some(cfg), true);

    // Define a new ping and submit it.
    const PING_NAME: &str = "test-ping";
    let custom_ping = private::PingType::new(PING_NAME, true, true, vec![]);
    custom_ping.submit(None);
    custom_ping.submit(None);

    // Sync up with the upload thread.
    barrier.wait();

    // Submit another ping and wait for it to do work.
    custom_ping.submit(None);

    // Sync up with the upload thread again.
    // This will not be the same thread as the one before (hopefully).
    barrier.wait();

    // No one's ever gonna wait for the uploader thread (the RLB doesn't store the handle to it),
    // so all we can do is hope it finishes within time.
    std::thread::sleep(std::time::Duration::from_millis(100));

    let map = call_count.lock().unwrap();
    assert_eq!(2, map.len(), "should have launched 2 uploader threads");
    for &count in map.values() {
        assert_eq!(1, count, "each thread should call upload only once");
    }
}
