// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::fs;
use std::fs::{create_dir_all, File, OpenOptions};
use std::io::BufRead;
use std::io::BufReader;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::RwLock;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value as JsonValue};

use crate::coverage::record_coverage;
use crate::CommonMetricData;
use crate::Glean;
use crate::Result;

/// Represents the recorded data for a single event.
#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
pub struct RecordedEvent {
    /// The timestamp of when the event was recorded.
    ///
    /// This allows to order events from a single process run.
    pub timestamp: u64,

    /// The event's category.
    ///
    /// This is defined by users in the metrics file.
    pub category: String,

    /// The event's name.
    ///
    /// This is defined by users in the metrics file.
    pub name: String,

    /// A map of all extra data values.
    ///
    /// The set of allowed extra keys is defined by users in the metrics file.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extra: Option<HashMap<String, String>>,
}

impl RecordedEvent {
    /// Serialize an event to JSON, adjusting its timestamp relative to a base timestamp
    fn serialize_relative(&self, timestamp_offset: u64) -> JsonValue {
        json!(&RecordedEvent {
            timestamp: self.timestamp - timestamp_offset,
            category: self.category.clone(),
            name: self.name.clone(),
            extra: self.extra.clone(),
        })
    }
}

/// This struct handles the in-memory and on-disk storage logic for events.
///
/// So that the data survives shutting down of the application, events are stored
/// in an append-only file on disk, in addition to the store in memory. Each line
/// of this file records a single event in JSON, exactly as it will be sent in the
/// ping. There is one file per store.
///
/// When restarting the application, these on-disk files are checked, and if any are
/// found, they are loaded, queued for sending and flushed immediately before any
/// further events are collected. This is because the timestamps for these events
/// may have come from a previous boot of the device, and therefore will not be
/// compatible with any newly-collected events.
#[derive(Debug)]
pub struct EventDatabase {
    /// Path to directory of on-disk event files
    pub path: PathBuf,
    /// The in-memory list of events
    event_stores: RwLock<HashMap<String, Vec<RecordedEvent>>>,
    /// A lock to be held when doing operations on the filesystem
    file_lock: RwLock<()>,
}

impl EventDatabase {
    /// Creates a new event database.
    ///
    /// # Arguments
    ///
    /// * `data_path` - The directory to store events in. A new directory
    /// * `events` - will be created inside of this directory.
    pub fn new(data_path: &Path) -> Result<Self> {
        let path = data_path.join("events");
        create_dir_all(&path)?;

        Ok(Self {
            path,
            event_stores: RwLock::new(HashMap::new()),
            file_lock: RwLock::new(()),
        })
    }

    /// Initializes events storage after Glean is fully initialized and ready to send pings.
    ///
    /// This must be called once on application startup, e.g. from
    /// [Glean.initialize], but after we are ready to send pings, since this
    /// could potentially collect and send pings.
    ///
    /// If there are any events queued on disk, it loads them into memory so
    /// that the memory and disk representations are in sync.
    ///
    /// Secondly, if this is the first time the application has been run since
    /// rebooting, any pings containing events are assembled into pings and cleared
    /// immediately, since their timestamps won't be compatible with the timestamps
    /// we would create during this boot of the device.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance.
    ///
    /// # Returns
    ///
    /// Whether at least one ping was generated.
    pub fn flush_pending_events_on_startup(&self, glean: &Glean) -> bool {
        match self.load_events_from_disk() {
            Ok(_) => self.send_all_events(glean),
            Err(err) => {
                log::warn!("Error loading events from disk: {}", err);
                false
            }
        }
    }

    fn load_events_from_disk(&self) -> Result<()> {
        // NOTE: The order of locks here is important.
        // In other code parts we might acquire the `file_lock` when we already have acquired
        // a lock on `event_stores`.
        // This is a potential lock-order-inversion.
        let mut db = self.event_stores.write().unwrap(); // safe unwrap, only error case is poisoning
        let _lock = self.file_lock.read().unwrap(); // safe unwrap, only error case is poisoning

        for entry in fs::read_dir(&self.path)? {
            let entry = entry?;
            if entry.file_type()?.is_file() {
                let store_name = entry.file_name().into_string()?;
                let file = BufReader::new(File::open(entry.path())?);
                db.insert(
                    store_name,
                    file.lines()
                        .filter_map(|line| line.ok())
                        .filter_map(|line| serde_json::from_str::<RecordedEvent>(&line).ok())
                        .collect(),
                );
            }
        }
        Ok(())
    }

    fn send_all_events(&self, glean: &Glean) -> bool {
        let store_names = {
            let db = self.event_stores.read().unwrap(); // safe unwrap, only error case is poisoning
            db.keys().cloned().collect::<Vec<String>>()
        };

        let mut ping_sent = false;
        for store_name in store_names {
            ping_sent |= glean.submit_ping_by_name(&store_name, Some("startup"));
        }

        ping_sent
    }

    /// Records an event in the desired stores.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance.
    /// * `meta` - The metadata about the event metric. Used to get the category,
    ///   name and stores for the metric.
    /// * `timestamp` - The timestamp of the event, in milliseconds. Must use a
    ///   monotonically increasing timer (this value is obtained on the
    ///   platform-specific side).
    /// * `extra` - Extra data values, mapping strings to strings.
    pub fn record(
        &self,
        glean: &Glean,
        meta: &CommonMetricData,
        timestamp: u64,
        extra: Option<HashMap<String, String>>,
    ) {
        // If upload is disabled we don't want to record.
        if !glean.is_upload_enabled() {
            return;
        }

        // Create RecordedEvent object, and its JSON form for serialization
        // on disk.
        let event = RecordedEvent {
            timestamp,
            category: meta.category.to_string(),
            name: meta.name.to_string(),
            extra,
        };
        let event_json = serde_json::to_string(&event).unwrap(); // safe unwrap, event can always be serialized

        // Store the event in memory and on disk to each of the stores.
        let mut stores_to_submit: Vec<&str> = Vec::new();
        {
            let mut db = self.event_stores.write().unwrap(); // safe unwrap, only error case is poisoning
            for store_name in meta.send_in_pings.iter() {
                let store = db.entry(store_name.to_string()).or_insert_with(Vec::new);
                store.push(event.clone());
                self.write_event_to_disk(store_name, &event_json);
                if store.len() == glean.get_max_events() {
                    stores_to_submit.push(store_name);
                }
            }
        }

        // If any of the event stores reached maximum size, submit the pings
        // containing those events immediately.
        for store_name in stores_to_submit {
            glean.submit_ping_by_name(store_name, Some("max_capacity"));
        }
    }

    /// Writes an event to a single store on disk.
    ///
    /// # Arguments
    ///
    /// * `store_name` - The name of the store.
    /// * `event_json` - The event content, as a single-line JSON-encoded string.
    fn write_event_to_disk(&self, store_name: &str, event_json: &str) {
        let _lock = self.file_lock.write().unwrap(); // safe unwrap, only error case is poisoning
        if let Err(err) = OpenOptions::new()
            .create(true)
            .append(true)
            .open(self.path.join(store_name))
            .and_then(|mut file| writeln!(file, "{}", event_json))
        {
            log::warn!("IO error writing event to store '{}': {}", store_name, err);
        }
    }

    /// Gets a snapshot of the stored event data as a JsonValue.
    ///
    /// # Arguments
    ///
    /// * `store_name` - The name of the desired store.
    /// * `clear_store` - Whether to clear the store after snapshotting.
    ///
    /// # Returns
    ///
    /// A array of events, JSON encoded, if any. Otherwise `None`.
    pub fn snapshot_as_json(&self, store_name: &str, clear_store: bool) -> Option<JsonValue> {
        let result = {
            let mut db = self.event_stores.write().unwrap(); // safe unwrap, only error case is poisoning
            db.get_mut(&store_name.to_string()).and_then(|store| {
                if !store.is_empty() {
                    // Timestamps may have been recorded out-of-order, so sort the events
                    // by the timestamp.
                    // We can't insert events in order as-we-go, because we also append
                    // events to a file on disk, where this would be expensive. Best to
                    // handle this in every case (whether events came from disk or memory)
                    // in a single location.
                    store.sort_by(|a, b| a.timestamp.cmp(&b.timestamp));
                    let first_timestamp = store[0].timestamp;
                    let snapshot = store
                        .iter()
                        .map(|e| e.serialize_relative(first_timestamp))
                        .collect();
                    Some(snapshot)
                } else {
                    log::warn!("Unexpectly got empty event store for '{}'", store_name);
                    None
                }
            })
        };

        if clear_store {
            self.event_stores
                .write()
                .unwrap() // safe unwrap, only error case is poisoning
                .remove(&store_name.to_string());

            let _lock = self.file_lock.write().unwrap(); // safe unwrap, only error case is poisoning
            if let Err(err) = fs::remove_file(self.path.join(store_name)) {
                match err.kind() {
                    std::io::ErrorKind::NotFound => {
                        // silently drop this error, the file was already non-existing
                    }
                    _ => log::warn!("Error removing events queue file '{}': {}", store_name, err),
                }
            }
        }

        result
    }

    /// Clears all stored events, both in memory and on-disk.
    pub fn clear_all(&self) -> Result<()> {
        // safe unwrap, only error case is poisoning
        self.event_stores.write().unwrap().clear();

        // safe unwrap, only error case is poisoning
        let _lock = self.file_lock.write().unwrap();
        std::fs::remove_dir_all(&self.path)?;
        create_dir_all(&self.path)?;

        Ok(())
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the vector of currently stored events for the given event metric in
    /// the given store.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value<'a>(
        &'a self,
        meta: &'a CommonMetricData,
        store_name: &str,
    ) -> Option<Vec<RecordedEvent>> {
        record_coverage(&meta.base_identifier());

        let value: Vec<RecordedEvent> = self
            .event_stores
            .read()
            .unwrap() // safe unwrap, only error case is poisoning
            .get(&store_name.to_string())
            .into_iter()
            .flatten()
            .filter(|event| event.name == meta.name && event.category == meta.category)
            .cloned()
            .collect();
        if !value.is_empty() {
            Some(value)
        } else {
            None
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::tests::new_glean;
    use crate::CommonMetricData;

    #[test]
    fn handle_truncated_events_on_disk() {
        let t = tempfile::tempdir().unwrap();

        {
            let db = EventDatabase::new(t.path()).unwrap();
            db.write_event_to_disk("events", "{\"timestamp\": 500");
            db.write_event_to_disk("events", "{\"timestamp\"");
            db.write_event_to_disk(
                "events",
                "{\"timestamp\": 501, \"category\": \"ui\", \"name\": \"click\"}",
            );
        }

        {
            let db = EventDatabase::new(t.path()).unwrap();
            db.load_events_from_disk().unwrap();
            let events = &db.event_stores.read().unwrap()["events"];
            assert_eq!(1, events.len());
        }
    }

    #[test]
    fn stable_serialization() {
        let event_empty = RecordedEvent {
            timestamp: 2,
            category: "cat".to_string(),
            name: "name".to_string(),
            extra: None,
        };

        let mut data = HashMap::new();
        data.insert("a key".to_string(), "a value".to_string());
        let event_data = RecordedEvent {
            timestamp: 2,
            category: "cat".to_string(),
            name: "name".to_string(),
            extra: Some(data),
        };

        let event_empty_json = ::serde_json::to_string_pretty(&event_empty).unwrap();
        let event_data_json = ::serde_json::to_string_pretty(&event_data).unwrap();

        assert_eq!(
            event_empty,
            serde_json::from_str(&event_empty_json).unwrap()
        );
        assert_eq!(event_data, serde_json::from_str(&event_data_json).unwrap());
    }

    #[test]
    fn deserialize_existing_data() {
        let event_empty_json = r#"
{
  "timestamp": 2,
  "category": "cat",
  "name": "name"
}
            "#;

        let event_data_json = r#"
{
  "timestamp": 2,
  "category": "cat",
  "name": "name",
  "extra": {
    "a key": "a value"
  }
}
        "#;

        let event_empty = RecordedEvent {
            timestamp: 2,
            category: "cat".to_string(),
            name: "name".to_string(),
            extra: None,
        };

        let mut data = HashMap::new();
        data.insert("a key".to_string(), "a value".to_string());
        let event_data = RecordedEvent {
            timestamp: 2,
            category: "cat".to_string(),
            name: "name".to_string(),
            extra: Some(data),
        };

        assert_eq!(event_empty, serde_json::from_str(event_empty_json).unwrap());
        assert_eq!(event_data, serde_json::from_str(event_data_json).unwrap());
    }

    #[test]
    fn doesnt_record_when_upload_is_disabled() {
        let (mut glean, dir) = new_glean(None);
        let db = EventDatabase::new(dir.path()).unwrap();

        let test_storage = "test-storage";
        let test_category = "category";
        let test_name = "name";
        let test_timestamp = 2;
        let test_meta = CommonMetricData::new(test_category, test_name, test_storage);
        let event_data = RecordedEvent {
            timestamp: test_timestamp,
            category: test_category.to_string(),
            name: test_name.to_string(),
            extra: None,
        };

        // Upload is not yet disabled,
        // so let's check that everything is getting recorded as expected.
        db.record(&glean, &test_meta, 2, None);
        {
            let event_stores = db.event_stores.read().unwrap();
            assert_eq!(&event_data, &event_stores.get(test_storage).unwrap()[0]);
            assert_eq!(event_stores.get(test_storage).unwrap().len(), 1);
        }

        glean.set_upload_enabled(false);

        // Now that upload is disabled, let's check nothing is recorded.
        db.record(&glean, &test_meta, 2, None);
        {
            let event_stores = db.event_stores.read().unwrap();
            assert_eq!(event_stores.get(test_storage).unwrap().len(), 1);
        }
    }
}
