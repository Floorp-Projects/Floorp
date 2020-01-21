use super::*;

const GLOBAL_APPLICATION_ID: &str = "org.mozilla.fogotype.test";

// Create a new instance of Glean with a temporary directory.
// We need to keep the `TempDir` alive, so that it's not deleted before we stop using it.
fn new_glean() -> tempfile::TempDir {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
    };

    initialize(cfg, ClientInfoMetrics::unknown()).unwrap();
    dir
}

#[test]
fn it_initializes() {
    env_logger::try_init().ok();
    let _ = new_glean();
}

#[test]
fn it_toggles_upload() {
    env_logger::try_init().ok();

    let _t = new_glean();

    assert!(crate::is_upload_enabled());
    crate::set_upload_enabled(false);
    assert!(!crate::is_upload_enabled());
}

#[test]
fn client_info_reset_after_toggle() {
    env_logger::try_init().ok();

    let _t = new_glean();

    assert!(crate::is_upload_enabled());

    // Metrics are identified by category.name, so it's safe to recreate the objects here.
    let core_metrics = core_metrics::InternalMetrics::new();

    // At start we should have a value.
    with_glean(|glean| {
        assert!(core_metrics
            .os
            .test_get_value(glean, "glean_client_info")
            .is_some());
    });

    // Disabling upload clears everything.
    crate::set_upload_enabled(false);
    with_glean(|glean| {
        assert!(!core_metrics
            .os
            .test_get_value(glean, "glean_client_info")
            .is_some());
    });

    // Re-enabling upload should reset the values.
    crate::set_upload_enabled(true);
    with_glean(|glean| {
        assert!(core_metrics
            .os
            .test_get_value(glean, "glean_client_info")
            .is_some());
    });
}
