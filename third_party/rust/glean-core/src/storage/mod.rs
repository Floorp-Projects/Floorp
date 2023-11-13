// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![allow(non_upper_case_globals)]

//! Storage snapshotting.

use std::collections::HashMap;

use serde_json::{json, Value as JsonValue};

use crate::coverage::record_coverage;
use crate::database::Database;
use crate::metrics::Metric;
use crate::Lifetime;

// An internal ping name, not to be touched by anything else
pub(crate) const INTERNAL_STORAGE: &str = "glean_internal_info";

/// Snapshot metrics from the underlying database.
pub struct StorageManager;

/// Labeled metrics are stored as `<metric id>/<label>`.
/// They need to go into a nested object in the final snapshot.
///
/// We therefore extract the metric id and the label from the key and construct the new object or
/// add to it.
fn snapshot_labeled_metrics(
    snapshot: &mut HashMap<String, HashMap<String, JsonValue>>,
    metric_id: &str,
    metric: &Metric,
) {
    let ping_section = format!("labeled_{}", metric.ping_section());
    let map = snapshot.entry(ping_section).or_default();

    // Safe unwrap, the function is only called when the id does contain a '/'
    let (metric_id, label) = metric_id.split_once('/').unwrap();

    let obj = map.entry(metric_id.into()).or_insert_with(|| json!({}));
    let obj = obj.as_object_mut().unwrap(); // safe unwrap, we constructed the object above
    obj.insert(label.into(), metric.as_json());
}

impl StorageManager {
    /// Snapshots the given store and optionally clear it.
    ///
    /// # Arguments
    ///
    /// * `storage` - the database to read from.
    /// * `store_name` - the store to snapshot.
    /// * `clear_store` - whether to clear the data after snapshotting.
    ///
    /// # Returns
    ///
    /// The stored data in a string encoded as JSON.
    /// If no data for the store exists, `None` is returned.
    pub fn snapshot(
        &self,
        storage: &Database,
        store_name: &str,
        clear_store: bool,
    ) -> Option<String> {
        self.snapshot_as_json(storage, store_name, clear_store)
            .map(|data| ::serde_json::to_string_pretty(&data).unwrap())
    }

    /// Snapshots the given store and optionally clear it.
    ///
    /// # Arguments
    ///
    /// * `storage` - the database to read from.
    /// * `store_name` - the store to snapshot.
    /// * `clear_store` - whether to clear the data after snapshotting.
    ///
    /// # Returns
    ///
    /// A JSON representation of the stored data.
    /// If no data for the store exists, `None` is returned.
    pub fn snapshot_as_json(
        &self,
        storage: &Database,
        store_name: &str,
        clear_store: bool,
    ) -> Option<JsonValue> {
        let mut snapshot: HashMap<String, HashMap<String, JsonValue>> = HashMap::new();

        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            if metric_id.contains('/') {
                snapshot_labeled_metrics(&mut snapshot, &metric_id, metric);
            } else {
                let map = snapshot.entry(metric.ping_section().into()).or_default();
                map.insert(metric_id, metric.as_json());
            }
        };

        storage.iter_store_from(Lifetime::Ping, store_name, None, &mut snapshotter);
        storage.iter_store_from(Lifetime::Application, store_name, None, &mut snapshotter);
        storage.iter_store_from(Lifetime::User, store_name, None, &mut snapshotter);

        // Add send in all pings client.annotations
        if store_name != "glean_client_info" {
            storage.iter_store_from(Lifetime::Application, "all-pings", None, snapshotter);
        }

        if clear_store {
            if let Err(e) = storage.clear_ping_lifetime_storage(store_name) {
                log::warn!("Failed to clear lifetime storage: {:?}", e);
            }
        }

        if snapshot.is_empty() {
            None
        } else {
            Some(json!(snapshot))
        }
    }

    /// Gets the current value of a single metric identified by name.
    ///
    /// # Arguments
    ///
    /// * `storage` - The database to get data from.
    /// * `store_name` - The store name to look into.
    /// * `metric_id` - The full metric identifier.
    ///
    /// # Returns
    ///
    /// The decoded metric or `None` if no data is found.
    pub fn snapshot_metric(
        &self,
        storage: &Database,
        store_name: &str,
        metric_id: &str,
        metric_lifetime: Lifetime,
    ) -> Option<Metric> {
        let mut snapshot: Option<Metric> = None;

        let mut snapshotter = |id: &[u8], metric: &Metric| {
            let id = String::from_utf8_lossy(id).into_owned();
            if id == metric_id {
                snapshot = Some(metric.clone())
            }
        };

        storage.iter_store_from(metric_lifetime, store_name, None, &mut snapshotter);

        snapshot
    }

    /// Gets the current value of a single metric identified by name.
    ///
    /// Use this API, rather than `snapshot_metric` within the testing API, so
    /// that the usage will be reported in coverage, if enabled.
    ///
    /// # Arguments
    ///
    /// * `storage` - The database to get data from.
    /// * `store_name` - The store name to look into.
    /// * `metric_id` - The full metric identifier.
    ///
    /// # Returns
    ///
    /// The decoded metric or `None` if no data is found.
    pub fn snapshot_metric_for_test(
        &self,
        storage: &Database,
        store_name: &str,
        metric_id: &str,
        metric_lifetime: Lifetime,
    ) -> Option<Metric> {
        record_coverage(metric_id);
        self.snapshot_metric(storage, store_name, metric_id, metric_lifetime)
    }

    ///  Snapshots the experiments.
    ///
    /// # Arguments
    ///
    /// * `storage` - The database to get data from.
    /// * `store_name` - The store name to look into.
    ///
    /// # Returns
    ///
    /// A JSON representation of the experiment data, in the following format:
    ///
    /// ```json
    /// {
    ///  "experiment-id": {
    ///    "branch": "branch-id",
    ///    "extra": {
    ///      "additional": "property",
    ///      // ...
    ///    }
    ///  }
    /// }
    /// ```
    ///
    /// If no data for the store exists, `None` is returned.
    pub fn snapshot_experiments_as_json(
        &self,
        storage: &Database,
        store_name: &str,
    ) -> Option<JsonValue> {
        let mut snapshot: HashMap<String, JsonValue> = HashMap::new();

        let mut snapshotter = |metric_id: &[u8], metric: &Metric| {
            let metric_id = String::from_utf8_lossy(metric_id).into_owned();
            if metric_id.ends_with("#experiment") {
                let (name, _) = metric_id.split_once('#').unwrap(); // safe unwrap, we ensured there's a `#` in the string
                snapshot.insert(name.to_string(), metric.as_json());
            }
        };

        storage.iter_store_from(Lifetime::Application, store_name, None, &mut snapshotter);

        if snapshot.is_empty() {
            None
        } else {
            Some(json!(snapshot))
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::metrics::ExperimentMetric;
    use crate::Glean;

    // Experiment's API tests: the next test comes from glean-ac's
    // ExperimentsStorageEngineTest.kt.
    #[test]
    fn test_experiments_json_serialization() {
        let t = tempfile::tempdir().unwrap();
        let name = t.path().display().to_string();
        let glean = Glean::with_options(&name, "org.mozilla.glean", true);

        let extra: HashMap<String, String> = [("test-key".into(), "test-value".into())]
            .iter()
            .cloned()
            .collect();

        let metric = ExperimentMetric::new(&glean, "some-experiment".to_string());

        metric.set_active_sync(&glean, "test-branch".to_string(), extra);
        let snapshot = StorageManager
            .snapshot_experiments_as_json(glean.storage(), "glean_internal_info")
            .unwrap();
        assert_eq!(
            json!({"some-experiment": {"branch": "test-branch", "extra": {"test-key": "test-value"}}}),
            snapshot
        );

        metric.set_inactive_sync(&glean);

        let empty_snapshot =
            StorageManager.snapshot_experiments_as_json(glean.storage(), "glean_internal_info");
        assert!(empty_snapshot.is_none());
    }

    #[test]
    fn test_experiments_json_serialization_empty() {
        let t = tempfile::tempdir().unwrap();
        let name = t.path().display().to_string();
        let glean = Glean::with_options(&name, "org.mozilla.glean", true);

        let metric = ExperimentMetric::new(&glean, "some-experiment".to_string());

        metric.set_active_sync(&glean, "test-branch".to_string(), HashMap::new());
        let snapshot = StorageManager
            .snapshot_experiments_as_json(glean.storage(), "glean_internal_info")
            .unwrap();
        assert_eq!(
            json!({"some-experiment": {"branch": "test-branch"}}),
            snapshot
        );

        metric.set_inactive_sync(&glean);

        let empty_snapshot =
            StorageManager.snapshot_experiments_as_json(glean.storage(), "glean_internal_info");
        assert!(empty_snapshot.is_none());
    }
}
