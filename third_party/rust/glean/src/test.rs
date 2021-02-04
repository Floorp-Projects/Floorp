// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::private::PingType;
use crate::private::{BooleanMetric, CounterMetric, EventMetric};
use std::path::PathBuf;

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
    };

    let _t = new_glean(Some(cfg), true);
    crate::block_on_dispatcher();

    // Define a new ping and submit it.
    const PING_NAME: &str = "test-ping";
    let custom_ping = private::PingType::new(PING_NAME, true, true, vec![]);
    custom_ping.submit(None);

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert_eq!(url.contains(PING_NAME), true);
}

#[test]
fn disabling_upload_disables_metrics_recording() {
    let _lock = lock_test();

    let _t = new_glean(None, true);
    crate::block_on_dispatcher();

    let metric = BooleanMetric::new(CommonMetricData {
        name: "bool_metric".into(),
        category: "test".into(),
        send_in_pings: vec!["store1".into()],
        lifetime: Lifetime::Application,
        disabled: false,
        dynamic_label: None,
    });

    crate::set_upload_enabled(false);

    assert!(metric.test_get_value("store1").is_none())
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
    let stored_data = test_get_experiment_data("experiment_api".to_string());
    assert_eq!("branch_b", stored_data.branch);
    assert_eq!("value", stored_data.extra.unwrap()["test_key"]);
}

#[test]
fn test_experiments_recording_before_glean_inits() {
    let _lock = lock_test();

    // Destroy the existing glean instance from glean-core so that we
    // can test the pre-init queueing of the experiment api commands.
    // This is doing the exact same thing that `reset_glean` is doing
    // but without calling `initialize`.
    if was_initialize_called() {
        // We need to check if the Glean object (from glean-core) is
        // initialized, otherwise this will crash on the first test
        // due to bug 1675215 (this check can be removed once that
        // bug is fixed).
        if global_glean().is_some() {
            with_glean_mut(|glean| {
                glean.test_clear_all_stores();
                glean.destroy_db();
            });
        }
        // Allow us to go through initialization again.
        INITIALIZE_CALLED.store(false, Ordering::SeqCst);
        // Reset the dispatcher.
        dispatcher::reset_dispatcher();
    }

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
    let tmpname = dir.path().display().to_string();

    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
        },
        ClientInfoMetrics::unknown(),
        false,
    );
    crate::block_on_dispatcher();

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
    };

    let _t = new_glean(Some(cfg), true);
    crate::block_on_dispatcher();

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

    // Create an instance of Glean, wait for init and then flip the dirty
    // bit to true.
    let data_dir = new_glean(None, true);

    crate::block_on_dispatcher();

    with_glean_mut(|glean| glean.set_dirty_flag(true));

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let tmpname = data_dir.path().display().to_string();

    // Now reset Glean: it should still send a baseline ping with reason
    // dirty_startup when starting, because of the dirty bit being set.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert_eq!(url.contains("baseline"), true);
}

#[test]
fn no_dirty_baseline_on_clean_shutdowns() {
    let _lock = lock_test();

    // Create an instance of Glean, wait for init and then flip the dirty
    // bit to true.
    let data_dir = new_glean(None, true);

    crate::block_on_dispatcher();

    with_glean_mut(|glean| glean.set_dirty_flag(true));

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let tmpname = data_dir.path().display().to_string();

    // Now reset Glean: it should not send a baseline ping, because
    // we cleared the dirty bit.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    crate::block_on_dispatcher();

    // We don't expect a startup ping.
    assert_eq!(r.try_recv(), Err(crossbeam_channel::TryRecvError::Empty));
}

#[test]
fn initialize_must_not_crash_if_data_dir_is_messed_up() {
    let _lock = lock_test();

    let dir = tempfile::tempdir().unwrap();
    let tmpdirname = dir.path().display().to_string();
    // Create a file in the temporary dir and use that as the
    // name of the Glean data dir.
    let file_path = PathBuf::from(tmpdirname).join("notadir");
    std::fs::write(file_path.clone(), "test").expect("The test Glean dir file must be created");

    let cfg = Configuration {
        data_path: file_path.to_string_lossy().to_string(),
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
    };

    test_reset_glean(cfg, ClientInfoMetrics::unknown(), false);
    // TODO(bug 1675215): ensure initialize runs through dispatcher.
    // Glean init is async and, for this test, it bails out early due to
    // an caused by not being able to create the data dir: we can do nothing
    // but wait. Tests in other bindings use the dispatcher's test mode, which
    // runs tasks sequentially on the main thread, so no sleep is required,
    // because we're guaranteed that, once we reach this point, the full
    // init potentially ran.
    std::thread::sleep(std::time::Duration::from_secs(3));
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
    let tmpname = dir.path().display().to_string();

    test_reset_glean(
        Configuration {
            data_path: tmpname.clone(),
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
        },
        ClientInfoMetrics::unknown(),
        true,
    );

    crate::block_on_dispatcher();

    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    // TODO(bug 1675215): ensure initialize runs through dispatcher.
    // Glean init is async and, for this test, it bails out early due to
    // being initialized: we can do nothing but wait. Tests in other bindings use
    // the dispatcher's test mode, which runs tasks sequentially on the main
    // thread, so no sleep is required. Bug 1675215 might fix this, as well.
    std::thread::sleep(std::time::Duration::from_secs(3));
}

#[test]
#[ignore] // TODO: To be done in bug 1673668.
fn dont_handle_events_when_uninitialized() {
    todo!()
}

#[test]
fn the_app_channel_must_be_correctly_set_if_requested() {
    let _lock = lock_test();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    // No appChannel must be set if nothing was provided through the config
    // options.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: None,
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
        },
        ClientInfoMetrics::unknown(),
        true,
    );
    assert!(core_metrics::internal_metrics::app_channel
        .test_get_value(None)
        .is_none());

    // The appChannel must be correctly reported if a channel value
    // was provided.
    let _t = new_glean(None, true);
    assert_eq!(
        "testing",
        core_metrics::internal_metrics::app_channel
            .test_get_value(None)
            .unwrap()
    );
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

    metric.set("TEST VALUE");
    assert!(metric.test_get_value(None).is_some());

    set_upload_enabled(false);
    assert!(metric.test_get_value(None).is_none());
    metric.set("TEST VALUE");
    assert!(metric.test_get_value(None).is_none());

    set_upload_enabled(true);
    assert!(metric.test_get_value(None).is_none());
    metric.set("TEST VALUE");
    assert_eq!("TEST VALUE", metric.test_get_value(None).unwrap());
}

#[test]
fn core_metrics_should_be_cleared_and_restored_when_disabling_and_enabling_uploading() {
    let _lock = lock_test();

    let _t = new_glean(None, false);

    assert!(core_metrics::internal_metrics::os_version
        .test_get_value(None)
        .is_some());

    set_upload_enabled(false);
    assert!(core_metrics::internal_metrics::os_version
        .test_get_value(None)
        .is_none());

    set_upload_enabled(true);
    assert!(core_metrics::internal_metrics::os_version
        .test_get_value(None)
        .is_some());
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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname.clone(),
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
    };

    let _t = new_glean(Some(cfg), true);

    crate::block_on_dispatcher();

    // Now reset Glean and disable upload: it should still send a deletion request
    // ping even though we're just starting.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: false,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert_eq!(url.contains("deletion-request"), true);
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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname.clone(),
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
    };

    let _t = new_glean(Some(cfg), true);

    crate::block_on_dispatcher();

    // Now reset Glean and keep upload enabled: no deletion-request
    // should be sent.
    test_reset_glean(
        Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: Some(Box::new(FakeUploader { sender: s })),
        },
        ClientInfoMetrics::unknown(),
        false,
    );

    crate::block_on_dispatcher();

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
    assert!(!was_initialize_called());

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Attempt to set a debug view tag before Glean is initialized.
    set_debug_view_tag("valid-tag");

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
    };

    let _t = new_glean(Some(cfg), true);
    crate::block_on_dispatcher();

    // Submit a baseline ping.
    submit_ping_by_name("baseline", Some("background"));

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
    assert!(!was_initialize_called());

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Attempt to set source tags before Glean is initialized.
    set_source_tags(vec!["valid-tag1".to_string(), "valid-tag2".to_string()]);

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
    };

    let _t = new_glean(Some(cfg), true);
    crate::block_on_dispatcher();

    // Submit a baseline ping.
    submit_ping_by_name("baseline", Some("background"));

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
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
    metric.set("some-test-value");
    sample_ping.submit(None);

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert_eq!(url.contains("deletion-request"), true);
}

#[test]
fn registering_pings_before_init_must_work() {
    let _lock = lock_test();

    destroy_glean(true);
    assert!(!was_initialize_called());

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
            net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom ping and attempt its registration.
    let sample_ping = PingType::new("pre-register", true, true, vec![]);

    // Create a custom configuration to use a fake uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(FakeUploader { sender: s })),
    };

    let _t = new_glean(Some(cfg), true);
    crate::block_on_dispatcher();

    // Submit a baseline ping.
    sample_ping.submit(None);

    // Wait for the ping to arrive.
    let url = r.recv().unwrap();
    assert!(url.contains("pre-register"));
}
