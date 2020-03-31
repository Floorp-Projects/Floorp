// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// NOTE: This is a test-only file that contains unit tests for
// the lib.rs file.

use super::*;
use crate::metrics::RecordedExperimentData;
use crate::metrics::StringMetric;

const GLOBAL_APPLICATION_ID: &str = "org.mozilla.glean.test.app";
pub fn new_glean(tempdir: Option<tempfile::TempDir>) -> (Glean, tempfile::TempDir) {
    let dir = match tempdir {
        Some(tempdir) => tempdir,
        None => tempfile::tempdir().unwrap(),
    };
    let tmpname = dir.path().display().to_string();
    let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();
    (glean, dir)
}

#[test]
fn path_is_constructed_from_data() {
    let (glean, _) = new_glean(None);

    assert_eq!(
        "/submit/org-mozilla-glean-test-app/baseline/1/this-is-a-docid",
        glean.make_path("baseline", "this-is-a-docid")
    );
}

// Experiment's API tests: the next two tests come from glean-ac's
// ExperimentsStorageEngineTest.kt.
#[test]
fn experiment_id_and_branch_get_truncated_if_too_long() {
    let t = tempfile::tempdir().unwrap();
    let name = t.path().display().to_string();
    let glean = Glean::with_options(&name, "org.mozilla.glean.tests", true).unwrap();

    // Generate long strings for the used ids.
    let very_long_id = "test-experiment-id".repeat(10);
    let very_long_branch_id = "test-branch-id".repeat(10);

    // Mark the experiment as active.
    glean.set_experiment_active(very_long_id.clone(), very_long_branch_id.clone(), None);

    // Generate the expected id and branch strings.
    let mut expected_id = very_long_id;
    expected_id.truncate(100);
    let mut expected_branch_id = very_long_branch_id;
    expected_branch_id.truncate(100);

    assert!(
        glean.test_is_experiment_active(expected_id.clone()),
        "An experiment with the truncated id should be available"
    );

    // Make sure the branch id was truncated as well.
    let experiment_data = glean.test_get_experiment_data_as_json(expected_id);
    assert!(
        !experiment_data.is_none(),
        "Experiment data must be available"
    );

    let parsed_json: RecordedExperimentData =
        ::serde_json::from_str(&experiment_data.unwrap()).unwrap();
    assert_eq!(expected_branch_id, parsed_json.branch);
}

#[test]
fn limits_on_experiments_extras_are_applied_correctly() {
    let t = tempfile::tempdir().unwrap();
    let name = t.path().display().to_string();
    let glean = Glean::with_options(&name, "org.mozilla.glean.tests", true).unwrap();

    let experiment_id = "test-experiment_id".to_string();
    let branch_id = "test-branch-id".to_string();
    let mut extras = HashMap::new();

    let too_long_key = "0123456789".repeat(11);
    let too_long_value = "0123456789".repeat(11);

    // Build and extras HashMap that's a little too long in every way
    for n in 0..21 {
        extras.insert(format!("{}-{}", n, too_long_key), too_long_value.clone());
    }

    // Mark the experiment as active.
    glean.set_experiment_active(experiment_id.clone(), branch_id, Some(extras));

    // Make sure it is active
    assert!(
        glean.test_is_experiment_active(experiment_id.clone()),
        "An experiment with the truncated id should be available"
    );

    // Get the data
    let experiment_data = glean.test_get_experiment_data_as_json(experiment_id);
    assert!(
        !experiment_data.is_none(),
        "Experiment data must be available"
    );

    // Parse the JSON and validate the lengths
    let parsed_json: RecordedExperimentData =
        ::serde_json::from_str(&experiment_data.unwrap()).unwrap();
    assert_eq!(
        20,
        parsed_json.clone().extra.unwrap().len(),
        "Experiments extra must be less than max length"
    );

    for (key, value) in parsed_json.extra.as_ref().unwrap().iter() {
        assert!(
            key.len() <= 100,
            "Experiments extra key must be less than max length"
        );
        assert!(
            value.len() <= 100,
            "Experiments extra value must be less than max length"
        );
    }
}

#[test]
fn experiments_status_is_correctly_toggled() {
    let t = tempfile::tempdir().unwrap();
    let name = t.path().display().to_string();
    let glean = Glean::with_options(&name, "org.mozilla.glean.tests", true).unwrap();

    // Define the experiment's data.
    let experiment_id: String = "test-toggle-experiment".into();
    let branch_id: String = "test-branch-toggle".into();
    let extra: HashMap<String, String> = [("test-key".into(), "test-value".into())]
        .iter()
        .cloned()
        .collect();

    // Activate an experiment.
    glean.set_experiment_active(experiment_id.clone(), branch_id, Some(extra.clone()));

    // Check that the experiment is marekd as active.
    assert!(
        glean.test_is_experiment_active(experiment_id.clone()),
        "The experiment must be marked as active."
    );

    // Check that the extra data was stored.
    let experiment_data = glean.test_get_experiment_data_as_json(experiment_id.clone());
    assert!(
        experiment_data.is_some(),
        "Experiment data must be available"
    );

    let parsed_data: RecordedExperimentData =
        ::serde_json::from_str(&experiment_data.unwrap()).unwrap();
    assert_eq!(parsed_data.extra.unwrap(), extra);

    // Disable the experiment and check that is no longer available.
    glean.set_experiment_inactive(experiment_id.clone());
    assert!(
        !glean.test_is_experiment_active(experiment_id),
        "The experiment must not be available any more."
    );
}

#[test]
fn client_id_and_first_run_date_must_be_regenerated() {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();
    {
        let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();

        glean.data_store.as_ref().unwrap().clear_all();

        assert!(glean
            .core_metrics
            .client_id
            .test_get_value(&glean, "glean_client_info")
            .is_none());
        assert!(glean
            .core_metrics
            .first_run_date
            .test_get_value_as_string(&glean, "glean_client_info")
            .is_none());
    }

    {
        let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();
        assert!(glean
            .core_metrics
            .client_id
            .test_get_value(&glean, "glean_client_info")
            .is_some());
        assert!(glean
            .core_metrics
            .first_run_date
            .test_get_value_as_string(&glean, "glean_client_info")
            .is_some());
    }
}

#[test]
fn basic_metrics_should_be_cleared_when_uploading_is_disabled() {
    let (mut glean, _t) = new_glean(None);
    let metric = StringMetric::new(CommonMetricData::new(
        "category",
        "string_metric",
        "baseline",
    ));

    metric.set(&glean, "TEST VALUE");
    assert!(metric.test_get_value(&glean, "baseline").is_some());

    glean.set_upload_enabled(false);
    assert!(metric.test_get_value(&glean, "baseline").is_none());

    metric.set(&glean, "TEST VALUE");
    assert!(metric.test_get_value(&glean, "baseline").is_none());

    glean.set_upload_enabled(true);
    assert!(metric.test_get_value(&glean, "baseline").is_none());

    metric.set(&glean, "TEST VALUE");
    assert!(metric.test_get_value(&glean, "baseline").is_some());
}

#[test]
fn first_run_date_is_managed_correctly_when_toggling_uploading() {
    let (mut glean, _) = new_glean(None);

    let original_first_run_date = glean
        .core_metrics
        .first_run_date
        .get_value(&glean, "glean_client_info");

    glean.set_upload_enabled(false);
    assert_eq!(
        original_first_run_date,
        glean
            .core_metrics
            .first_run_date
            .get_value(&glean, "glean_client_info")
    );

    glean.set_upload_enabled(true);
    assert_eq!(
        original_first_run_date,
        glean
            .core_metrics
            .first_run_date
            .get_value(&glean, "glean_client_info")
    );
}

#[test]
fn client_id_is_managed_correctly_when_toggling_uploading() {
    let (mut glean, _) = new_glean(None);

    let original_client_id = glean
        .core_metrics
        .client_id
        .get_value(&glean, "glean_client_info");
    assert!(original_client_id.is_some());
    assert_ne!(*KNOWN_CLIENT_ID, original_client_id.unwrap());

    glean.set_upload_enabled(false);
    assert_eq!(
        *KNOWN_CLIENT_ID,
        glean
            .core_metrics
            .client_id
            .get_value(&glean, "glean_client_info")
            .unwrap()
    );

    glean.set_upload_enabled(true);
    let current_client_id = glean
        .core_metrics
        .client_id
        .get_value(&glean, "glean_client_info");
    assert!(current_client_id.is_some());
    assert_ne!(*KNOWN_CLIENT_ID, current_client_id.unwrap());
    assert_ne!(original_client_id, current_client_id);
}

#[test]
fn client_id_is_set_to_known_value_when_uploading_disabled_at_start() {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();
    let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, false).unwrap();

    assert_eq!(
        *KNOWN_CLIENT_ID,
        glean
            .core_metrics
            .client_id
            .get_value(&glean, "glean_client_info")
            .unwrap()
    );
}

#[test]
fn client_id_is_set_to_random_value_when_uploading_enabled_at_start() {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();
    let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();

    let current_client_id = glean
        .core_metrics
        .client_id
        .get_value(&glean, "glean_client_info");
    assert!(current_client_id.is_some());
    assert_ne!(*KNOWN_CLIENT_ID, current_client_id.unwrap());
}

#[test]
fn enabling_when_already_enabled_is_a_noop() {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();
    let mut glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();

    assert!(!glean.set_upload_enabled(true));
}

#[test]
fn disabling_when_already_disabled_is_a_noop() {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();
    let mut glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, false).unwrap();

    assert!(!glean.set_upload_enabled(false));
}

// Test that the enum variants keep a stable discriminant when serialized.
// Discriminant values are taken from a stable ordering from v20.0.0.
// New metrics after that should be added in order.
#[test]
#[rustfmt::skip] // Let's not add newlines unnecessary
fn correct_order() {
    use histogram::Histogram;
    use metrics::{Metric::*, TimeUnit};
    use std::time::Duration;
    use util::local_now_with_offset;

    // Extract the discriminant of the serialized value,
    // that is: the first 4 bytes.
    fn discriminant(metric: &metrics::Metric) -> u32 {
        let ser = bincode::serialize(metric).unwrap();
        (ser[0] as u32)
        | (ser[1] as u32) << 8
        | (ser[2] as u32) << 16
        | (ser[3] as u32) << 24
    }

    // One of every metric type. The values are arbitrary and don't matter.
    let all_metrics = vec![
        Boolean(false),
        Counter(0),
        CustomDistributionExponential(Histogram::exponential(1, 500, 10)),
        CustomDistributionLinear(Histogram::linear(1, 500, 10)),
        Datetime(local_now_with_offset(), TimeUnit::Second),
        Experiment(RecordedExperimentData { branch: "branch".into(), extra: None, }),
        Quantity(0),
        String("glean".into()),
        StringList(vec!["glean".into()]),
        Uuid("082c3e52-0a18-11ea-946f-0fe0c98c361c".into()),
        Timespan(Duration::new(5, 0), TimeUnit::Second),
        TimingDistribution(Histogram::functional(2.0, 8.0)),
        MemoryDistribution(Histogram::functional(2.0, 8.0)),
    ];

    for metric in all_metrics {
        let disc = discriminant(&metric);

        // DO NOT TOUCH THE EXPECTED VALUE.
        // If this test fails because of non-equal discriminants, that is a bug in the code, not
        // the test.

        // We're matching here, thus fail the build if new variants are added.
        match metric {
            Boolean(..)                       => assert_eq!( 0, disc),
            Counter(..)                       => assert_eq!( 1, disc),
            CustomDistributionExponential(..) => assert_eq!( 2, disc),
            CustomDistributionLinear(..)      => assert_eq!( 3, disc),
            Datetime(..)                      => assert_eq!( 4, disc),
            Experiment(..)                    => assert_eq!( 5, disc),
            Quantity(..)                      => assert_eq!( 6, disc),
            String(..)                        => assert_eq!( 7, disc),
            StringList(..)                    => assert_eq!( 8, disc),
            Uuid(..)                          => assert_eq!( 9, disc),
            Timespan(..)                      => assert_eq!(10, disc),
            TimingDistribution(..)            => assert_eq!(11, disc),
            MemoryDistribution(..)            => assert_eq!(12, disc),
        }
    }
}

#[test]
#[rustfmt::skip] // Let's not merge lines
fn backwards_compatible_deserialization() {
    use std::env;
    use std::time::Duration;
    use chrono::prelude::*;
    use histogram::Histogram;
    use metrics::{Metric::*, TimeUnit};

    // Prepare some data to fill in
    let dt = FixedOffset::east(9*3600).ymd(2014, 11, 28).and_hms_nano(21, 45, 59, 12);

    let mut custom_dist_exp = Histogram::exponential(1, 500, 10);
    custom_dist_exp.accumulate(10);

    let mut custom_dist_linear = Histogram::linear(1, 500, 10);
    custom_dist_linear.accumulate(10);

    let mut time_dist = Histogram::functional(2.0, 8.0);
    time_dist.accumulate(10);

    let mut mem_dist = Histogram::functional(2.0, 16.0);
    mem_dist.accumulate(10);

    // One of every metric type. The values are arbitrary, but stable.
    let all_metrics = vec![
        (
            "boolean",
            vec![0, 0, 0, 0, 1],
            Boolean(true)
        ),
        (
            "counter",
            vec![1, 0, 0, 0, 20, 0, 0, 0],
            Counter(20)
        ),
        (
            "custom exponential distribution",
                vec![2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 1,
                     0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0,
                     0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 244, 1, 0, 0, 0, 0, 0, 0, 10, 0,
                     0, 0, 0, 0, 0, 0],
            CustomDistributionExponential(custom_dist_exp)
        ),
        (
            "custom linear distribution",
            vec![3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
                 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 1, 0,
                 0, 0, 0, 0, 0, 0, 244, 1, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0],
            CustomDistributionLinear(custom_dist_linear)
        ),
        (
            "datetime",
            vec![4, 0, 0, 0, 35, 0, 0, 0, 0, 0, 0, 0, 50, 48, 49, 52, 45, 49, 49, 45,
                 50, 56, 84, 50, 49, 58, 52, 53, 58, 53, 57, 46, 48, 48, 48, 48, 48,
                 48, 48, 49, 50, 43, 48, 57, 58, 48, 48, 3, 0, 0, 0],
            Datetime(dt, TimeUnit::Second),
        ),
        (
            "experiment",
            vec![5, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 98, 114, 97, 110, 99, 104, 0],
            Experiment(RecordedExperimentData { branch: "branch".into(), extra: None, }),
        ),
        (
            "quantity",
            vec![6, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0],
            Quantity(17)
        ),
        (
            "string",
            vec![7, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 103, 108, 101, 97, 110],
            String("glean".into())
        ),
        (
            "string list",
            vec![8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
                 103, 108, 101, 97, 110],
            StringList(vec!["glean".into()])
        ),
        (
            "uuid",
            vec![9, 0, 0, 0, 36, 0, 0, 0, 0, 0, 0, 0, 48, 56, 50, 99, 51, 101, 53, 50,
                 45, 48, 97, 49, 56, 45, 49, 49, 101, 97, 45, 57, 52, 54, 102, 45, 48,
                 102, 101, 48, 99, 57, 56, 99, 51, 54, 49, 99],
            Uuid("082c3e52-0a18-11ea-946f-0fe0c98c361c".into()),
        ),
        (
            "timespan",
            vec![10, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0],
            Timespan(Duration::new(5, 0), TimeUnit::Second),
        ),
        (
            "timing distribution",
            vec![11, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 123, 81, 125,
                 60, 184, 114, 241, 63],
            TimingDistribution(time_dist),
        ),
        (
            "memory distribution",
            vec![12, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 15, 137, 249,
                 108, 88, 181, 240, 63],
             MemoryDistribution(mem_dist),
        ),
    ];

    for (name, data, metric) in all_metrics {
        // Helper to print serialization data if instructed by environment variable
        // Run with:
        //
        // ```text
        // PRINT_DATA=1 cargo test -p glean-core --lib -- --nocapture backwards
        // ```
        //
        // This should not be necessary to re-run and change here, unless a bincode upgrade
        // requires us to also migrate existing data.
        if env::var("PRINT_DATA").is_ok() {
            let bindata = bincode::serialize(&metric).unwrap();
            println!("(\n    {:?},\n    vec!{:?},", name, bindata);
        } else {
            // Otherwise run the test
            let deserialized = bincode::deserialize(&data).unwrap();
            if let CustomDistributionExponential(hist) = &deserialized {
                hist.snapshot_values(); // Force initialization of the ranges
            }
            if let CustomDistributionLinear(hist) = &deserialized {
                hist.snapshot_values(); // Force initialization of the ranges
            }

            assert_eq!(
                metric, deserialized,
                "Expected properly deserialized {}",
                name
            );
        }
    }
}

#[test]
fn test_first_run() {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();
    {
        let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();
        // Check that this is indeed the first run.
        assert!(glean.is_first_run());
    }

    {
        // Other runs must be not marked as "first run".
        let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();
        assert!(!glean.is_first_run());
    }
}

#[test]
fn test_dirty_bit() {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();
    {
        let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();
        // The dirty flag must not be set the first time Glean runs.
        assert!(!glean.is_dirty_flag_set());

        // Set the dirty flag and check that it gets correctly set.
        glean.set_dirty_flag(true);
        assert!(glean.is_dirty_flag_set());
    }

    {
        // Check that next time Glean runs, it correctly picks up the "dirty flag".
        // It is expected to be 'true'.
        let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();
        assert!(glean.is_dirty_flag_set());

        // Set the dirty flag to false.
        glean.set_dirty_flag(false);
        assert!(!glean.is_dirty_flag_set());
    }

    {
        // Check that next time Glean runs, it correctly picks up the "dirty flag".
        // It is expected to be 'false'.
        let glean = Glean::with_options(&tmpname, GLOBAL_APPLICATION_ID, true).unwrap();
        assert!(!glean.is_dirty_flag_set());
    }
}
