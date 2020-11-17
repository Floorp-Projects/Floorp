// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::private::{BooleanMetric, CounterMetric};
use once_cell::sync::Lazy;
use std::path::PathBuf;
use std::sync::Mutex;

use super::*;

// Because glean_preview is a global-singleton, we need to run the tests one-by-one to avoid different tests stomping over each other.
// This is only an issue because we're resetting Glean, this cannot happen in normal use of the
// RLB.
//
// We use a global lock to force synchronization of all tests, even if run multi-threaded.
// This allows us to run without `--test-threads 1`.`
static GLOBAL_LOCK: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));
const GLOBAL_APPLICATION_ID: &str = "org.mozilla.rlb.test";

// Create a new instance of Glean with a temporary directory.
// We need to keep the `TempDir` alive, so that it's not deleted before we stop using it.
fn new_glean(configuration: Option<Configuration>, clear_stores: bool) -> tempfile::TempDir {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = match configuration {
        Some(c) => c,
        None => Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
        },
    };

    crate::reset_glean(cfg, ClientInfoMetrics::unknown(), clear_stores);
    dir
}

#[test]
fn send_a_ping() {
    let _lock = GLOBAL_LOCK.lock().unwrap();
    env_logger::try_init().ok();

    let (s, r) = crossbeam_channel::bounded::<String>(1);

    // Define a fake uploader that reports back the submission URL
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct FakeUploader {
        sender: crossbeam_channel::Sender<String>,
    };
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
    crate::dispatcher::block_on_queue();

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
    let _lock = GLOBAL_LOCK.lock().unwrap();
    env_logger::try_init().ok();

    let _t = new_glean(None, true);
    crate::dispatcher::block_on_queue();

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
    // setup glean for the test
    let _lock = GLOBAL_LOCK.lock().unwrap();
    env_logger::try_init().ok();

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
    let _lock = GLOBAL_LOCK.lock().unwrap();
    env_logger::try_init().ok();

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

    reset_glean(
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
    crate::dispatcher::block_on_queue();

    assert!(test_is_experiment_active(
        "experiment_set_preinit".to_string()
    ));
    assert!(!test_is_experiment_active(
        "experiment_preinit_disabled".to_string()
    ));
}

#[test]
#[ignore] // TODO: To be done in bug 1673645.
fn test_sending_of_foreground_background_pings() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1672958.
fn test_sending_of_startup_baseline_ping() {
    todo!()
}

#[test]
fn initialize_must_not_crash_if_data_dir_is_messed_up() {
    let _lock = GLOBAL_LOCK.lock().unwrap();
    env_logger::try_init().ok();

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

    reset_glean(cfg, ClientInfoMetrics::unknown(), false);
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
    let _lock = GLOBAL_LOCK.lock().unwrap();
    env_logger::try_init().ok();

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
    let _ = new_glean(None, false);

    // Verify that the callback was executed by testing for the correct value
    assert!(metric.test_get_value(None).is_some(), "Value must exist");
    assert_eq!(3, metric.test_get_value(None).unwrap(), "Value must match");
}

#[test]
fn initializing_twice_is_a_noop() {
    let _lock = GLOBAL_LOCK.lock().unwrap();
    env_logger::try_init().ok();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    reset_glean(
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

    dispatcher::block_on_queue();

    reset_glean(
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
#[ignore] // TODO: To be done in bug 1673672.
fn the_app_channel_must_be_correctly_set_if_requested() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn ping_collection_must_happen_after_concurrently_scheduled_metrics_recordings() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn basic_metrics_should_be_cleared_when_disabling_uploading() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn core_metrics_should_be_cleared_and_restored_when_disabling_and_enabling_uploading() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn overflowing_the_task_queue_records_telemetry() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn sending_deletion_ping_if_disabled_outside_of_run() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn no_sending_of_deletion_ping_if_unchanged_outside_of_run() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn test_sending_of_startup_baseline_ping_with_application_lifetime_metric() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn test_dirty_flag_is_reset_to_false() {
    todo!()
}

#[test]
#[ignore] // TODO: To be done in bug 1673672.
fn flipping_upload_enabled_respects_order_of_events() {
    todo!()
}
