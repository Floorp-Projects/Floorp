// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Ping collection, assembly & submission.

use std::fs::{create_dir_all, File};
use std::io::Write;
use std::path::{Path, PathBuf};

use log::info;
use serde_json::{json, Value as JsonValue};

use crate::common_metric_data::{CommonMetricData, Lifetime};
use crate::metrics::{CounterMetric, DatetimeMetric, Metric, MetricType, PingType, TimeUnit};
use crate::storage::StorageManager;
use crate::upload::HeaderMap;
use crate::util::{get_iso_time_string, local_now_with_offset_and_record};
use crate::{
    Glean, Result, DELETION_REQUEST_PINGS_DIRECTORY, INTERNAL_STORAGE, PENDING_PINGS_DIRECTORY,
};

/// Holds everything you need to store or send a ping.
pub struct Ping<'a> {
    /// The unique document id.
    pub doc_id: &'a str,
    /// The ping's name.
    pub name: &'a str,
    /// The path on the server to use when uplaoding this ping.
    pub url_path: &'a str,
    /// The payload, including `*_info` fields.
    pub content: JsonValue,
    /// The headers to upload with the payload.
    pub headers: HeaderMap,
}

/// Collect a ping's data, assemble it into its full payload and store it on disk.
pub struct PingMaker;

fn merge(a: &mut JsonValue, b: &JsonValue) {
    match (a, b) {
        (&mut JsonValue::Object(ref mut a), &JsonValue::Object(ref b)) => {
            for (k, v) in b {
                merge(a.entry(k.clone()).or_insert(JsonValue::Null), v);
            }
        }
        (a, b) => {
            *a = b.clone();
        }
    }
}

impl Default for PingMaker {
    fn default() -> Self {
        Self::new()
    }
}

impl PingMaker {
    /// Creates a new [`PingMaker`].
    pub fn new() -> Self {
        Self
    }

    /// Gets, and then increments, the sequence number for a given ping.
    ///
    /// This is crate-internal exclusively for enabling the migration tests.
    pub(super) fn get_ping_seq(&self, glean: &Glean, storage_name: &str) -> usize {
        // Sequence numbers are stored as a counter under a name that includes the storage name
        let seq = CounterMetric::new(CommonMetricData {
            name: format!("{}#sequence", storage_name),
            // We don't need a category, the name is already unique
            category: "".into(),
            send_in_pings: vec![INTERNAL_STORAGE.into()],
            lifetime: Lifetime::User,
            ..Default::default()
        });

        let current_seq = match StorageManager.snapshot_metric(
            glean.storage(),
            INTERNAL_STORAGE,
            &seq.meta().identifier(glean),
            seq.meta().lifetime,
        ) {
            Some(Metric::Counter(i)) => i,
            _ => 0,
        };

        // Increase to next sequence id
        seq.add(glean, 1);

        current_seq as usize
    }

    /// Gets the formatted start and end times for this ping and update for the next ping.
    fn get_start_end_times(&self, glean: &Glean, storage_name: &str) -> (String, String) {
        let time_unit = TimeUnit::Minute;

        let start_time = DatetimeMetric::new(
            CommonMetricData {
                name: format!("{}#start", storage_name),
                category: "".into(),
                send_in_pings: vec![INTERNAL_STORAGE.into()],
                lifetime: Lifetime::User,
                ..Default::default()
            },
            time_unit,
        );

        // "start_time" is the time the ping was generated the last time.
        // If not available, we use the date the Glean object was initialized.
        let start_time_data = start_time
            .get_value(glean, INTERNAL_STORAGE)
            .unwrap_or_else(|| glean.start_time());
        let end_time_data = local_now_with_offset_and_record(glean);

        // Update the start time with the current time.
        start_time.set(glean, Some(end_time_data));

        // Format the times.
        let start_time_data = get_iso_time_string(start_time_data, time_unit);
        let end_time_data = get_iso_time_string(end_time_data, time_unit);
        (start_time_data, end_time_data)
    }

    fn get_ping_info(&self, glean: &Glean, storage_name: &str, reason: Option<&str>) -> JsonValue {
        let (start_time, end_time) = self.get_start_end_times(glean, storage_name);
        let mut map = json!({
            "seq": self.get_ping_seq(glean, storage_name),
            "start_time": start_time,
            "end_time": end_time,
        });

        if let Some(reason) = reason {
            map.as_object_mut()
                .unwrap() // safe unwrap, we created the object above
                .insert("reason".to_string(), JsonValue::String(reason.to_string()));
        };

        // Get the experiment data, if available.
        if let Some(experiment_data) =
            StorageManager.snapshot_experiments_as_json(glean.storage(), INTERNAL_STORAGE)
        {
            map.as_object_mut()
                .unwrap() // safe unwrap, we created the object above
                .insert("experiments".to_string(), experiment_data);
        };

        map
    }

    fn get_client_info(&self, glean: &Glean, include_client_id: bool) -> JsonValue {
        // Add the "telemetry_sdk_build", which is the glean-core version.
        let mut map = json!({
            "telemetry_sdk_build": crate::GLEAN_VERSION,
        });

        // Flatten the whole thing.
        if let Some(client_info) =
            StorageManager.snapshot_as_json(glean.storage(), "glean_client_info", true)
        {
            let client_info_obj = client_info.as_object().unwrap(); // safe unwrap, snapshot always returns an object.
            for (_key, value) in client_info_obj {
                merge(&mut map, value);
            }
        } else {
            log::warn!("Empty client info data.");
        }

        if !include_client_id {
            // safe unwrap, we created the object above
            map.as_object_mut().unwrap().remove("client_id");
        }

        json!(map)
    }

    /// Build the headers to be persisted and sent with a ping.
    ///
    /// Currently the only headers we persist are `X-Debug-ID` and `X-Source-Tags`.
    ///
    /// # Arguments
    ///
    /// * `glean` - the [`Glean`] instance to collect headers from.
    ///
    /// # Returns
    ///
    /// A map of header names to header values.
    /// Might be empty if there are no extra headers to send.
    /// ```
    fn get_headers(&self, glean: &Glean) -> HeaderMap {
        let mut headers_map = HeaderMap::new();

        if let Some(debug_view_tag) = glean.debug_view_tag() {
            headers_map.insert("X-Debug-ID".to_string(), debug_view_tag.to_string());
        }

        if let Some(source_tags) = glean.source_tags() {
            headers_map.insert("X-Source-Tags".to_string(), source_tags.join(","));
        }

        headers_map
    }

    /// Collects a snapshot for the given ping from storage and attach required meta information.
    ///
    /// # Arguments
    ///
    /// * `glean` - the [`Glean`] instance to collect data from.
    /// * `ping` - the ping to collect for.
    /// * `reason` - an optional reason code to include in the ping.
    /// * `doc_id` - the ping's unique document identifier.
    /// * `url_path` - the path on the server to upload this ping to.
    ///
    /// # Returns
    ///
    /// A fully assembled representation of the ping payload and associated metadata.
    /// If there is no data stored for the ping, `None` is returned.
    pub fn collect<'a>(
        &self,
        glean: &Glean,
        ping: &'a PingType,
        reason: Option<&str>,
        doc_id: &'a str,
        url_path: &'a str,
    ) -> Option<Ping<'a>> {
        info!("Collecting {}", ping.name);

        let metrics_data = StorageManager.snapshot_as_json(glean.storage(), &ping.name, true);
        let events_data = glean.event_storage().snapshot_as_json(&ping.name, true);

        let is_empty = metrics_data.is_none() && events_data.is_none();
        if !ping.send_if_empty && is_empty {
            info!("Storage for {} empty. Bailing out.", ping.name);
            return None;
        } else if is_empty {
            info!("Storage for {} empty. Ping will still be sent.", ping.name);
        }

        let ping_info = self.get_ping_info(glean, &ping.name, reason);
        let client_info = self.get_client_info(glean, ping.include_client_id);

        let mut json = json!({
            "ping_info": ping_info,
            "client_info": client_info
        });
        let json_obj = json.as_object_mut()?;
        if let Some(metrics_data) = metrics_data {
            json_obj.insert("metrics".to_string(), metrics_data);
        }
        if let Some(events_data) = events_data {
            json_obj.insert("events".to_string(), events_data);
        }

        Some(Ping {
            content: json,
            name: &ping.name,
            doc_id,
            url_path,
            headers: self.get_headers(glean),
        })
    }

    /// Collects a snapshot for the given ping from storage and attach required meta information.
    ///
    /// # Arguments
    ///
    /// * `glean` - the [`Glean`] instance to collect data from.
    /// * `ping` - the ping to collect for.
    /// * `reason` - an optional reason code to include in the ping.
    ///
    /// # Returns
    ///
    /// A fully assembled ping payload in a string encoded as JSON.
    /// If there is no data stored for the ping, `None` is returned.
    pub fn collect_string(
        &self,
        glean: &Glean,
        ping: &PingType,
        reason: Option<&str>,
    ) -> Option<String> {
        self.collect(glean, ping, reason, "", "")
            .map(|ping| ::serde_json::to_string_pretty(&ping.content).unwrap())
    }

    /// Gets the path to a directory for ping storage.
    ///
    /// The directory will be created inside the `data_path`.
    /// The `pings` directory (and its parents) is created if it does not exist.
    fn get_pings_dir(&self, data_path: &Path, ping_type: Option<&str>) -> std::io::Result<PathBuf> {
        // Use a special directory for deletion-request pings
        let pings_dir = match ping_type {
            Some(ping_type) if ping_type == "deletion-request" => {
                data_path.join(DELETION_REQUEST_PINGS_DIRECTORY)
            }
            _ => data_path.join(PENDING_PINGS_DIRECTORY),
        };

        create_dir_all(&pings_dir)?;
        Ok(pings_dir)
    }

    /// Gets path to a directory for temporary storage.
    ///
    /// The directory will be created inside the `data_path`.
    /// The `tmp` directory (and its parents) is created if it does not exist.
    fn get_tmp_dir(&self, data_path: &Path) -> std::io::Result<PathBuf> {
        let pings_dir = data_path.join("tmp");
        create_dir_all(&pings_dir)?;
        Ok(pings_dir)
    }

    /// Stores a ping to disk in the pings directory.
    pub fn store_ping(&self, data_path: &Path, ping: &Ping) -> std::io::Result<()> {
        let pings_dir = self.get_pings_dir(data_path, Some(ping.name))?;
        let temp_dir = self.get_tmp_dir(data_path)?;

        // Write to a temporary location and then move when done,
        // for transactional writes.
        let temp_ping_path = temp_dir.join(ping.doc_id);
        let ping_path = pings_dir.join(ping.doc_id);

        log::debug!(
            "Storing ping '{}' at '{}'",
            ping.doc_id,
            ping_path.display()
        );

        {
            let mut file = File::create(&temp_ping_path)?;
            file.write_all(ping.url_path.as_bytes())?;
            file.write_all(b"\n")?;
            file.write_all(::serde_json::to_string(&ping.content)?.as_bytes())?;
            if !ping.headers.is_empty() {
                file.write_all(b"\n{\"headers\":")?;
                file.write_all(::serde_json::to_string(&ping.headers)?.as_bytes())?;
                file.write_all(b"}")?;
            }
        }

        if let Err(e) = std::fs::rename(&temp_ping_path, &ping_path) {
            log::warn!(
                "Unable to move '{}' to '{}",
                temp_ping_path.display(),
                ping_path.display()
            );
            return Err(e);
        }

        Ok(())
    }

    /// Clears any pending pings in the queue.
    pub fn clear_pending_pings(&self, data_path: &Path) -> Result<()> {
        let pings_dir = self.get_pings_dir(data_path, None)?;

        std::fs::remove_dir_all(&pings_dir)?;
        create_dir_all(&pings_dir)?;

        log::debug!("All pending pings deleted");

        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::tests::new_glean;

    #[test]
    fn sequence_numbers_should_be_reset_when_toggling_uploading() {
        let (mut glean, _) = new_glean(None);
        let ping_maker = PingMaker::new();

        assert_eq!(0, ping_maker.get_ping_seq(&glean, "custom"));
        assert_eq!(1, ping_maker.get_ping_seq(&glean, "custom"));

        glean.set_upload_enabled(false);
        assert_eq!(0, ping_maker.get_ping_seq(&glean, "custom"));
        assert_eq!(0, ping_maker.get_ping_seq(&glean, "custom"));

        glean.set_upload_enabled(true);
        assert_eq!(0, ping_maker.get_ping_seq(&glean, "custom"));
        assert_eq!(1, ping_maker.get_ping_seq(&glean, "custom"));
    }
}
