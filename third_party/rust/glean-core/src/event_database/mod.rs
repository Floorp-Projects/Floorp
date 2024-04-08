// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::cmp::Ordering;
use std::collections::HashMap;
use std::convert::TryFrom;
use std::fs;
use std::fs::{create_dir_all, File, OpenOptions};
use std::io::BufRead;
use std::io::BufReader;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::RwLock;

use chrono::{DateTime, FixedOffset, Utc};

use serde::{Deserialize, Serialize};
use serde_json::{json, Value as JsonValue};

use crate::common_metric_data::CommonMetricDataInternal;
use crate::coverage::record_coverage;
use crate::error_recording::{record_error, ErrorType};
use crate::metrics::{DatetimeMetric, TimeUnit};
use crate::storage::INTERNAL_STORAGE;
use crate::util::get_iso_time_string;
use crate::Glean;
use crate::Result;
use crate::{CommonMetricData, CounterMetric, Lifetime};

/// Represents the recorded data for a single event.
#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
#[cfg_attr(test, derive(Default))]
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

/// Represents the stored data for a single event.
#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
struct StoredEvent {
    #[serde(flatten)]
    event: RecordedEvent,

    /// The monotonically-increasing execution counter.
    ///
    /// Included to allow sending of events across Glean restarts (bug 1716725).
    /// Is i32 because it is stored in a CounterMetric.
    #[serde(default)]
    #[serde(skip_serializing_if = "Option::is_none")]
    pub execution_counter: Option<i32>,
}

/// This struct handles the in-memory and on-disk storage logic for events.
///
/// So that the data survives shutting down of the application, events are stored
/// in an append-only file on disk, in addition to the store in memory. Each line
/// of this file records a single event in JSON, exactly as it will be sent in the
/// ping. There is one file per store.
///
/// When restarting the application, these on-disk files are checked, and if any are
/// found, they are loaded, and a `glean.restarted` event is added before any
/// further events are collected. This is because the timestamps for these events
/// may have come from a previous boot of the device, and therefore will not be
/// compatible with any newly-collected events.
///
/// Normalizing all these timestamps happens on serialization for submission (see
/// `serialize_as_json`) where the client time between restarts is calculated using
/// data stored in the `glean.startup.date` extra of the `glean.restarted` event, plus
/// the `execution_counter` stored in events on disk.
///
/// Neither `execution_counter` nor `glean.startup.date` is submitted in pings.
/// The `glean.restarted` event is, though.
/// (See [bug 1716725](https://bugzilla.mozilla.org/show_bug.cgi?id=1716725).)
#[derive(Debug)]
pub struct EventDatabase {
    /// Path to directory of on-disk event files
    pub path: PathBuf,
    /// The in-memory list of events
    event_stores: RwLock<HashMap<String, Vec<StoredEvent>>>,
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
    /// could potentially collect and send the "events" ping.
    ///
    /// If there are any events queued on disk, it loads them into memory so
    /// that the memory and disk representations are in sync.
    ///
    /// If event records for the "events" ping are present, they are assembled into
    /// an "events" ping which is submitted immediately with reason "startup".
    ///
    /// If event records for custom pings are present, we increment the custom pings'
    /// stores' `execution_counter` and record a `glean.restarted`
    /// event with the current client clock in its `glean.startup.date` extra.
    ///
    /// # Arguments
    ///
    /// * `glean` - The Glean instance.
    /// * `trim_data_to_registered_pings` - Whether we should trim the event storage of
    ///   any events not belonging to pings previously registered via `register_ping_type`.
    ///
    /// # Returns
    ///
    /// Whether the "events" ping was submitted.
    pub fn flush_pending_events_on_startup(
        &self,
        glean: &Glean,
        trim_data_to_registered_pings: bool,
    ) -> bool {
        match self.load_events_from_disk(glean, trim_data_to_registered_pings) {
            Ok(_) => {
                let stores_with_events: Vec<String> = {
                    self.event_stores
                        .read()
                        .unwrap()
                        .keys()
                        .map(|x| x.to_owned())
                        .collect() // safe unwrap, only error case is poisoning
                };
                // We do not want to be holding the event stores lock when
                // submitting a ping or recording new events.
                let has_events_events = stores_with_events.contains(&"events".to_owned());
                let glean_restarted_stores = if has_events_events {
                    stores_with_events
                        .into_iter()
                        .filter(|store| store != "events")
                        .collect()
                } else {
                    stores_with_events
                };
                if !glean_restarted_stores.is_empty() {
                    for store_name in glean_restarted_stores.iter() {
                        CounterMetric::new(CommonMetricData {
                            name: "execution_counter".into(),
                            category: store_name.into(),
                            send_in_pings: vec![INTERNAL_STORAGE.into()],
                            lifetime: Lifetime::Ping,
                            ..Default::default()
                        })
                        .add_sync(glean, 1);
                    }
                    let glean_restarted = CommonMetricData {
                        name: "restarted".into(),
                        category: "glean".into(),
                        send_in_pings: glean_restarted_stores,
                        lifetime: Lifetime::Ping,
                        ..Default::default()
                    };
                    let startup = get_iso_time_string(glean.start_time(), TimeUnit::Minute);
                    let mut extra: HashMap<String, String> =
                        [("glean.startup.date".into(), startup)].into();
                    if glean.with_timestamps() {
                        let now = Utc::now();
                        let precise_timestamp = now.timestamp_millis() as u64;
                        extra.insert("glean_timestamp".to_string(), precise_timestamp.to_string());
                    }
                    self.record(
                        glean,
                        &glean_restarted.into(),
                        crate::get_timestamp_ms(),
                        Some(extra),
                    );
                }
                has_events_events && glean.submit_ping_by_name("events", Some("startup"))
            }
            Err(err) => {
                log::warn!("Error loading events from disk: {}", err);
                false
            }
        }
    }

    fn load_events_from_disk(
        &self,
        glean: &Glean,
        trim_data_to_registered_pings: bool,
    ) -> Result<()> {
        // NOTE: The order of locks here is important.
        // In other code parts we might acquire the `file_lock` when we already have acquired
        // a lock on `event_stores`.
        // This is a potential lock-order-inversion.
        let mut db = self.event_stores.write().unwrap(); // safe unwrap, only error case is poisoning
        let _lock = self.file_lock.write().unwrap(); // safe unwrap, only error case is poisoning

        for entry in fs::read_dir(&self.path)? {
            let entry = entry?;
            if entry.file_type()?.is_file() {
                let store_name = entry.file_name().into_string()?;
                log::info!("Loading events for {}", store_name);
                if trim_data_to_registered_pings && glean.get_ping_by_name(&store_name).is_none() {
                    log::warn!("Trimming {}'s events", store_name);
                    if let Err(err) = fs::remove_file(entry.path()) {
                        match err.kind() {
                            std::io::ErrorKind::NotFound => {
                                // silently drop this error, the file was already non-existing
                            }
                            _ => log::warn!("Error trimming events file '{}': {}", store_name, err),
                        }
                    }
                    continue;
                }
                let file = BufReader::new(File::open(entry.path())?);
                db.insert(
                    store_name,
                    file.lines()
                        .map_while(Result::ok)
                        .filter_map(|line| serde_json::from_str::<StoredEvent>(&line).ok())
                        .collect(),
                );
            }
        }
        Ok(())
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
    ///
    /// ## Returns
    ///
    /// `true` if a ping was submitted and should be uploaded.
    /// `false` otherwise.
    pub fn record(
        &self,
        glean: &Glean,
        meta: &CommonMetricDataInternal,
        timestamp: u64,
        extra: Option<HashMap<String, String>>,
    ) -> bool {
        // If upload is disabled we don't want to record.
        if !glean.is_upload_enabled() {
            return false;
        }

        let mut submit_max_capacity_event_ping = false;
        {
            let mut db = self.event_stores.write().unwrap(); // safe unwrap, only error case is poisoning
            for store_name in meta.inner.send_in_pings.iter() {
                let store = db.entry(store_name.to_string()).or_default();
                let execution_counter = CounterMetric::new(CommonMetricData {
                    name: "execution_counter".into(),
                    category: store_name.into(),
                    send_in_pings: vec![INTERNAL_STORAGE.into()],
                    lifetime: Lifetime::Ping,
                    ..Default::default()
                })
                .get_value(glean, INTERNAL_STORAGE);
                // Create StoredEvent object, and its JSON form for serialization on disk.
                let event = StoredEvent {
                    event: RecordedEvent {
                        timestamp,
                        category: meta.inner.category.to_string(),
                        name: meta.inner.name.to_string(),
                        extra: extra.clone(),
                    },
                    execution_counter,
                };
                let event_json = serde_json::to_string(&event).unwrap(); // safe unwrap, event can always be serialized
                store.push(event);
                self.write_event_to_disk(store_name, &event_json);
                if store_name == "events" && store.len() == glean.get_max_events() {
                    submit_max_capacity_event_ping = true;
                }
            }
        }
        if submit_max_capacity_event_ping {
            glean.submit_ping_by_name("events", Some("max_capacity"));
            true
        } else {
            false
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

    /// Normalizes the store in-place.
    ///
    /// A store may be in any order and contain any number of `glean.restarted` events,
    /// whose values must be taken into account, along with `execution_counter` values,
    /// to come up with the correct events with correct `timestamp` values,
    /// on which we then sort.
    ///
    /// 1. Sort by `execution_counter` and `timestamp`,
    ///    breaking ties so that `glean.restarted` comes first.
    /// 2. Remove all initial and final `glean.restarted` events
    /// 3. For each group of events that share a `execution_counter`,
    ///    i. calculate the initial `glean.restarted` event's `timestamp`s to be
    ///       clamp(glean.startup.date - ping_info.start_time, biggest_timestamp_of_previous_group + 1)
    ///    ii. normalize each non-`glean-restarted` event's `timestamp`
    ///        relative to the `glean.restarted` event's uncalculated `timestamp`
    /// 4. Remove `execution_counter` and `glean.startup.date` extra keys
    /// 5. Sort by `timestamp`
    ///
    /// In the event that something goes awry, this will record an invalid_state on
    /// glean.restarted if it is due to internal inconsistencies, or invalid_value
    /// on client clock weirdness.
    ///
    /// # Arguments
    ///
    /// * `glean` - Used to report errors
    /// * `store_name` - The name of the store we're normalizing.
    /// * `store` - The store we're to normalize.
    /// * `glean_start_time` - Used if the glean.startup.date or ping_info.start_time aren't available. Passed as a parameter to ease unit-testing.
    fn normalize_store(
        &self,
        glean: &Glean,
        store_name: &str,
        store: &mut Vec<StoredEvent>,
        glean_start_time: DateTime<FixedOffset>,
    ) {
        let is_glean_restarted =
            |event: &RecordedEvent| event.category == "glean" && event.name == "restarted";
        let glean_restarted_meta = |store_name: &str| CommonMetricData {
            name: "restarted".into(),
            category: "glean".into(),
            send_in_pings: vec![store_name.into()],
            lifetime: Lifetime::Ping,
            ..Default::default()
        };
        // Step 1
        store.sort_by(|a, b| {
            a.execution_counter
                .cmp(&b.execution_counter)
                .then_with(|| a.event.timestamp.cmp(&b.event.timestamp))
                .then_with(|| {
                    if is_glean_restarted(&a.event) {
                        Ordering::Less
                    } else {
                        Ordering::Greater
                    }
                })
        });
        // Step 2
        // Find the index of the first and final non-`glean.restarted` events.
        // Remove events before the first and after the final.
        let final_event = match store
            .iter()
            .rposition(|event| !is_glean_restarted(&event.event))
        {
            Some(idx) => idx + 1,
            _ => 0,
        };
        store.drain(final_event..);
        let first_event = store
            .iter()
            .position(|event| !is_glean_restarted(&event.event))
            .unwrap_or(store.len());
        store.drain(..first_event);
        if store.is_empty() {
            // There was nothing but `glean.restarted` events. Job's done!
            return;
        }
        // Step 3
        // It is allowed that there might not be any `glean.restarted` event, nor
        // `execution_counter` extra values. (This should always be the case for the
        // "events" ping, for instance).
        // Other inconsistencies are evidence of errors, and so are logged.
        let mut cur_ec = 0;
        // The offset within a group of events with the same `execution_counter`.
        let mut intra_group_offset = store[0].event.timestamp;
        // The offset between this group and ping_info.start_date.
        let mut inter_group_offset = 0;
        let mut highest_ts = 0;
        for event in store.iter_mut() {
            let execution_counter = event.execution_counter.take().unwrap_or(0);
            if is_glean_restarted(&event.event) {
                // We've entered the next "event group".
                // We need a new epoch based on glean.startup.date - ping_info.start_date
                cur_ec = execution_counter;
                let glean_startup_date = event
                    .event
                    .extra
                    .as_mut()
                    .and_then(|extra| {
                        extra.remove("glean.startup.date").and_then(|date_str| {
                            DateTime::parse_from_str(&date_str, TimeUnit::Minute.format_pattern())
                                .map_err(|_| {
                                    record_error(
                                        glean,
                                        &glean_restarted_meta(store_name).into(),
                                        ErrorType::InvalidState,
                                        format!("Unparseable glean.startup.date '{}'", date_str),
                                        None,
                                    );
                                })
                                .ok()
                        })
                    })
                    .unwrap_or(glean_start_time);
                if event
                    .event
                    .extra
                    .as_ref()
                    .map_or(false, |extra| extra.is_empty())
                {
                    // Small optimization to save us sending empty dicts.
                    event.event.extra = None;
                }
                let ping_start = DatetimeMetric::new(
                    CommonMetricData {
                        name: format!("{}#start", store_name),
                        category: "".into(),
                        send_in_pings: vec![INTERNAL_STORAGE.into()],
                        lifetime: Lifetime::User,
                        ..Default::default()
                    },
                    TimeUnit::Minute,
                );
                let ping_start = ping_start
                    .get_value(glean, INTERNAL_STORAGE)
                    .unwrap_or(glean_start_time);
                let time_from_ping_start_to_glean_restarted =
                    (glean_startup_date - ping_start).num_milliseconds();
                intra_group_offset = event.event.timestamp;
                inter_group_offset =
                    u64::try_from(time_from_ping_start_to_glean_restarted).unwrap_or(0);
                if inter_group_offset < highest_ts {
                    record_error(
                        glean,
                        &glean_restarted_meta(store_name).into(),
                        ErrorType::InvalidValue,
                        format!("Time between restart and ping start {} indicates client clock weirdness.", time_from_ping_start_to_glean_restarted),
                        None,
                    );
                    // The client's clock went backwards enough that this event group's
                    // glean.restarted looks like it happened _before_ the final event of the previous group.
                    // Or, it went ahead enough to overflow u64.
                    // Adjust things so this group starts 1ms after the previous one.
                    inter_group_offset = highest_ts + 1;
                }
            } else if cur_ec == 0 {
                // bug 1811872 - cur_ec might need initialization.
                cur_ec = execution_counter;
            }
            event.event.timestamp = event.event.timestamp - intra_group_offset + inter_group_offset;
            if execution_counter != cur_ec {
                record_error(
                    glean,
                    &glean_restarted_meta(store_name).into(),
                    ErrorType::InvalidState,
                    format!(
                        "Inconsistent execution counter {} (expected {})",
                        execution_counter, cur_ec
                    ),
                    None,
                );
                // Let's fix cur_ec up and hope this isn't a sign something big is broken.
                cur_ec = execution_counter;
            }
            if highest_ts > event.event.timestamp {
                // Even though we sorted everything, something in the
                // execution_counter or glean.startup.date math went awry.
                record_error(
                    glean,
                    &glean_restarted_meta(store_name).into(),
                    ErrorType::InvalidState,
                    format!(
                        "Inconsistent previous highest timestamp {} (expected <= {})",
                        highest_ts, event.event.timestamp
                    ),
                    None,
                );
                // Let the highest_ts regress to event.timestamp to hope this minimizes weirdness.
            }
            highest_ts = event.event.timestamp
        }
    }

    /// Gets a snapshot of the stored event data as a JsonValue.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance.
    /// * `store_name` - The name of the desired store.
    /// * `clear_store` - Whether to clear the store after snapshotting.
    ///
    /// # Returns
    ///
    /// A array of events, JSON encoded, if any. Otherwise `None`.
    pub fn snapshot_as_json(
        &self,
        glean: &Glean,
        store_name: &str,
        clear_store: bool,
    ) -> Option<JsonValue> {
        let result = {
            let mut db = self.event_stores.write().unwrap(); // safe unwrap, only error case is poisoning
            db.get_mut(&store_name.to_string()).and_then(|store| {
                if !store.is_empty() {
                    // Normalization happens in-place, so if we're not clearing,
                    // operate on a clone.
                    let mut clone;
                    let store = if clear_store {
                        store
                    } else {
                        clone = store.clone();
                        &mut clone
                    };
                    // We may need to normalize event timestamps across multiple restarts.
                    self.normalize_store(glean, store_name, store, glean.start_time());
                    Some(json!(store))
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
        meta: &'a CommonMetricDataInternal,
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
            .map(|stored_event| stored_event.event.clone())
            .filter(|event| event.name == meta.inner.name && event.category == meta.inner.category)
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
    use crate::{test_get_num_recorded_errors, CommonMetricData};
    use chrono::{TimeZone, Timelike};

    #[test]
    fn handle_truncated_events_on_disk() {
        let (glean, t) = new_glean(None);

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
            db.load_events_from_disk(&glean, false).unwrap();
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
            StoredEvent {
                event: event_empty,
                execution_counter: None
            },
            serde_json::from_str(&event_empty_json).unwrap()
        );
        assert_eq!(
            StoredEvent {
                event: event_data,
                execution_counter: None
            },
            serde_json::from_str(&event_data_json).unwrap()
        );
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

        assert_eq!(
            StoredEvent {
                event: event_empty,
                execution_counter: None
            },
            serde_json::from_str(event_empty_json).unwrap()
        );
        assert_eq!(
            StoredEvent {
                event: event_data,
                execution_counter: None
            },
            serde_json::from_str(event_data_json).unwrap()
        );
    }

    #[test]
    fn doesnt_record_when_upload_is_disabled() {
        let (mut glean, dir) = new_glean(None);
        let db = EventDatabase::new(dir.path()).unwrap();

        let test_storage = "test-storage";
        let test_category = "category";
        let test_name = "name";
        let test_timestamp = 2;
        let test_meta = CommonMetricDataInternal::new(test_category, test_name, test_storage);
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
            assert_eq!(
                &StoredEvent {
                    event: event_data,
                    execution_counter: None
                },
                &event_stores.get(test_storage).unwrap()[0]
            );
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

    #[test]
    fn normalize_store_of_glean_restarted() {
        // Make sure stores empty of anything but glean.restarted events normalize without issue.
        let (glean, _dir) = new_glean(None);

        let store_name = "store-name";
        let glean_restarted = StoredEvent {
            event: RecordedEvent {
                timestamp: 2,
                category: "glean".into(),
                name: "restarted".into(),
                extra: None,
            },
            execution_counter: None,
        };
        let mut store = vec![glean_restarted.clone()];
        let glean_start_time = glean.start_time();

        glean
            .event_storage()
            .normalize_store(&glean, store_name, &mut store, glean_start_time);
        assert!(store.is_empty());

        let mut store = vec![glean_restarted.clone(), glean_restarted.clone()];
        glean
            .event_storage()
            .normalize_store(&glean, store_name, &mut store, glean_start_time);
        assert!(store.is_empty());

        let mut store = vec![
            glean_restarted.clone(),
            glean_restarted.clone(),
            glean_restarted,
        ];
        glean
            .event_storage()
            .normalize_store(&glean, store_name, &mut store, glean_start_time);
        assert!(store.is_empty());
    }

    #[test]
    fn normalize_store_of_glean_restarted_on_both_ends() {
        // Make sure stores with non-glean.restarted events don't get drained too far.
        let (glean, _dir) = new_glean(None);

        let store_name = "store-name";
        let glean_restarted = StoredEvent {
            event: RecordedEvent {
                timestamp: 2,
                category: "glean".into(),
                name: "restarted".into(),
                extra: None,
            },
            execution_counter: None,
        };
        let not_glean_restarted = StoredEvent {
            event: RecordedEvent {
                timestamp: 20,
                category: "category".into(),
                name: "name".into(),
                extra: None,
            },
            execution_counter: None,
        };
        let mut store = vec![
            glean_restarted.clone(),
            not_glean_restarted.clone(),
            glean_restarted,
        ];
        let glean_start_time = glean.start_time();

        glean
            .event_storage()
            .normalize_store(&glean, store_name, &mut store, glean_start_time);
        assert_eq!(1, store.len());
        assert_eq!(
            StoredEvent {
                event: RecordedEvent {
                    timestamp: 0,
                    ..not_glean_restarted.event
                },
                execution_counter: None
            },
            store[0]
        );
    }

    #[test]
    fn normalize_store_single_run_timestamp_math() {
        // With a single run of events (no non-initial or non-terminal `glean.restarted`),
        // ensure the timestamp math works.
        // (( works = Initial event gets to be 0, subsequent events get normalized to that 0 ))
        let (glean, _dir) = new_glean(None);

        let store_name = "store-name";
        let glean_restarted = StoredEvent {
            event: RecordedEvent {
                timestamp: 2,
                category: "glean".into(),
                name: "restarted".into(),
                extra: None,
            },
            execution_counter: None,
        };
        let timestamps = [20, 40, 200];
        let not_glean_restarted = StoredEvent {
            event: RecordedEvent {
                timestamp: timestamps[0],
                category: "category".into(),
                name: "name".into(),
                extra: None,
            },
            execution_counter: None,
        };
        let mut store = vec![
            glean_restarted.clone(),
            not_glean_restarted.clone(),
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[1],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: None,
            },
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[2],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: None,
            },
            glean_restarted,
        ];

        glean
            .event_storage()
            .normalize_store(&glean, store_name, &mut store, glean.start_time());
        assert_eq!(3, store.len());
        for (timestamp, event) in timestamps.iter().zip(store.iter()) {
            assert_eq!(
                &StoredEvent {
                    event: RecordedEvent {
                        timestamp: timestamp - timestamps[0],
                        ..not_glean_restarted.clone().event
                    },
                    execution_counter: None
                },
                event
            );
        }
    }

    #[test]
    fn normalize_store_multi_run_timestamp_math() {
        // With multiple runs of events (separated by `glean.restarted`),
        // ensure the timestamp math works.
        // (( works = Initial event gets to be 0, subsequent events get normalized to that 0.
        //            Subsequent runs figure it out via glean.restarted.date and ping_info.start_time ))
        let (glean, _dir) = new_glean(None);

        let store_name = "store-name";
        let glean_restarted = StoredEvent {
            event: RecordedEvent {
                category: "glean".into(),
                name: "restarted".into(),
                ..Default::default()
            },
            execution_counter: None,
        };
        let not_glean_restarted = StoredEvent {
            event: RecordedEvent {
                category: "category".into(),
                name: "name".into(),
                ..Default::default()
            },
            execution_counter: None,
        };

        // This scenario represents a run of three events followed by an hour between runs,
        // followed by one final event.
        let timestamps = [20, 40, 200, 12];
        let ecs = [0, 1];
        let some_hour = 16;
        let startup_date = FixedOffset::east(0)
            .ymd(2022, 11, 24)
            .and_hms(some_hour, 29, 0); // TimeUnit::Minute -- don't put seconds
        let glean_start_time = startup_date.with_hour(some_hour - 1);
        let restarted_ts = 2;
        let mut store = vec![
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[0],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[0]),
            },
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[1],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[0]),
            },
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[2],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[0]),
            },
            StoredEvent {
                event: RecordedEvent {
                    extra: Some(
                        [(
                            "glean.startup.date".into(),
                            get_iso_time_string(startup_date, TimeUnit::Minute),
                        )]
                        .into(),
                    ),
                    timestamp: restarted_ts,
                    ..glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[1]),
            },
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[3],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[1]),
            },
        ];

        glean.event_storage().normalize_store(
            &glean,
            store_name,
            &mut store,
            glean_start_time.unwrap(),
        );
        assert_eq!(5, store.len()); // 4 "real" events plus 1 `glean.restarted`

        // Let's check the first three.
        for (timestamp, event) in timestamps[..timestamps.len() - 1].iter().zip(store.clone()) {
            assert_eq!(
                StoredEvent {
                    event: RecordedEvent {
                        timestamp: timestamp - timestamps[0],
                        ..not_glean_restarted.event.clone()
                    },
                    execution_counter: None,
                },
                event
            );
        }
        // The fourth should be a glean.restarted and have a realtime-based timestamp.
        let hour_in_millis = 3600000;
        assert_eq!(
            store[3],
            StoredEvent {
                event: RecordedEvent {
                    timestamp: hour_in_millis,
                    ..glean_restarted.event
                },
                execution_counter: None,
            }
        );
        // The fifth should have a timestamp based on the new origin.
        assert_eq!(
            store[4],
            StoredEvent {
                event: RecordedEvent {
                    timestamp: hour_in_millis + timestamps[3] - restarted_ts,
                    ..not_glean_restarted.event
                },
                execution_counter: None,
            }
        );
    }

    #[test]
    fn normalize_store_multi_run_client_clocks() {
        // With multiple runs of events (separated by `glean.restarted`),
        // ensure the timestamp math works. Even when the client clock goes backwards.
        let (glean, _dir) = new_glean(None);

        let store_name = "store-name";
        let glean_restarted = StoredEvent {
            event: RecordedEvent {
                category: "glean".into(),
                name: "restarted".into(),
                ..Default::default()
            },
            execution_counter: None,
        };
        let not_glean_restarted = StoredEvent {
            event: RecordedEvent {
                category: "category".into(),
                name: "name".into(),
                ..Default::default()
            },
            execution_counter: None,
        };

        // This scenario represents a run of two events followed by negative one hours between runs,
        // followed by two more events.
        let timestamps = [20, 40, 12, 200];
        let ecs = [0, 1];
        let some_hour = 10;
        let startup_date = FixedOffset::east(0)
            .ymd(2022, 11, 25)
            .and_hms(some_hour, 37, 0); // TimeUnit::Minute -- don't put seconds
        let glean_start_time = startup_date.with_hour(some_hour + 1);
        let restarted_ts = 2;
        let mut store = vec![
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[0],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[0]),
            },
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[1],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[0]),
            },
            StoredEvent {
                event: RecordedEvent {
                    extra: Some(
                        [(
                            "glean.startup.date".into(),
                            get_iso_time_string(startup_date, TimeUnit::Minute),
                        )]
                        .into(),
                    ),
                    timestamp: restarted_ts,
                    ..glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[1]),
            },
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[2],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[1]),
            },
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[3],
                    ..not_glean_restarted.event.clone()
                },
                execution_counter: Some(ecs[1]),
            },
        ];

        glean.event_storage().normalize_store(
            &glean,
            store_name,
            &mut store,
            glean_start_time.unwrap(),
        );
        assert_eq!(5, store.len()); // 4 "real" events plus 1 `glean.restarted`

        // Let's check the first two.
        for (timestamp, event) in timestamps[..timestamps.len() - 2].iter().zip(store.clone()) {
            assert_eq!(
                StoredEvent {
                    event: RecordedEvent {
                        timestamp: timestamp - timestamps[0],
                        ..not_glean_restarted.event.clone()
                    },
                    execution_counter: None,
                },
                event
            );
        }
        // The third should be a glean.restarted. Its timestamp should be
        // one larger than the largest timestamp seen so far (because that's
        // how we ensure monotonic timestamps when client clocks go backwards).
        assert_eq!(
            store[2],
            StoredEvent {
                event: RecordedEvent {
                    timestamp: store[1].event.timestamp + 1,
                    ..glean_restarted.event
                },
                execution_counter: None,
            }
        );
        // The fifth should have a timestamp based on the new origin.
        assert_eq!(
            store[3],
            StoredEvent {
                event: RecordedEvent {
                    timestamp: timestamps[2] - restarted_ts + store[2].event.timestamp,
                    ..not_glean_restarted.event
                },
                execution_counter: None,
            }
        );
        // And we should have an InvalidValue on glean.restarted to show for it.
        assert_eq!(
            Ok(1),
            test_get_num_recorded_errors(
                &glean,
                &CommonMetricData {
                    name: "restarted".into(),
                    category: "glean".into(),
                    send_in_pings: vec![store_name.into()],
                    lifetime: Lifetime::Ping,
                    ..Default::default()
                }
                .into(),
                ErrorType::InvalidValue
            )
        );
    }

    #[test]
    fn normalize_store_non_zero_ec() {
        // After the first run, execution_counter will likely be non-zero.
        // Ensure normalizing a store that begins with non-zero ec works.
        let (glean, _dir) = new_glean(None);

        let store_name = "store-name";
        let glean_restarted = StoredEvent {
            event: RecordedEvent {
                timestamp: 2,
                category: "glean".into(),
                name: "restarted".into(),
                extra: None,
            },
            execution_counter: Some(2),
        };
        let not_glean_restarted = StoredEvent {
            event: RecordedEvent {
                timestamp: 20,
                category: "category".into(),
                name: "name".into(),
                extra: None,
            },
            execution_counter: Some(2),
        };
        let glean_restarted_2 = StoredEvent {
            event: RecordedEvent {
                timestamp: 2,
                category: "glean".into(),
                name: "restarted".into(),
                extra: None,
            },
            execution_counter: Some(3),
        };
        let mut store = vec![
            glean_restarted,
            not_glean_restarted.clone(),
            glean_restarted_2,
        ];
        let glean_start_time = glean.start_time();

        glean
            .event_storage()
            .normalize_store(&glean, store_name, &mut store, glean_start_time);

        assert_eq!(1, store.len());
        assert_eq!(
            StoredEvent {
                event: RecordedEvent {
                    timestamp: 0,
                    ..not_glean_restarted.event
                },
                execution_counter: None
            },
            store[0]
        );
        // And we should have no InvalidState errors on glean.restarted.
        assert!(test_get_num_recorded_errors(
            &glean,
            &CommonMetricData {
                name: "restarted".into(),
                category: "glean".into(),
                send_in_pings: vec![store_name.into()],
                lifetime: Lifetime::Ping,
                ..Default::default()
            }
            .into(),
            ErrorType::InvalidState
        )
        .is_err());
        // (and, just because we're here, double-check there are no InvalidValue either).
        assert!(test_get_num_recorded_errors(
            &glean,
            &CommonMetricData {
                name: "restarted".into(),
                category: "glean".into(),
                send_in_pings: vec![store_name.into()],
                lifetime: Lifetime::Ping,
                ..Default::default()
            }
            .into(),
            ErrorType::InvalidValue
        )
        .is_err());
    }
}
