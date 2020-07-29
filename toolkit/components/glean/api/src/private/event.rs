// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::hash::Hash;
use std::marker::PhantomData;

use super::{CommonMetricData, Instant};

/// Extra keys for events.
///
/// Extra keys need to be pre-defined and map to a string representation.
///
/// For user-defined `EventMetric`s these will be defined as `enums`.
/// Each variant will correspond to an entry in the `ALLOWED_KEYS` list.
/// The Glean SDK requires the keys as strings for submission in pings,
/// whereas in code we want to provide users a type to work with
/// (e.g. to avoid typos or misuse of the API).
pub trait ExtraKeys: Hash + Eq + PartialEq + Copy {
    /// List of allowed extra keys as strings.
    const ALLOWED_KEYS: &'static [&'static str];

    /// The index of the extra key.
    ///
    /// It corresponds to its position in the associated `ALLOWED_KEYS` list.
    ///
    /// *Note*: An index of `-1` indicates an invalid / non-existing extra key.
    /// Invalid / non-existing extra keys will be recorded as an error.
    /// This cannot happen for generated code.
    fn index(self) -> i32;
}

/// Default of no extra keys for events.
///
/// An enum with no values for convenient use as the default set of extra keys
/// that an `EventMetric` can accept.
///
/// *Note*: There exist no values for this enum, it can never exist.
/// It its equivalent to the [`never / !` type](https://doc.rust-lang.org/std/primitive.never.html).
#[derive(Clone, Copy, Hash, Eq, PartialEq)]
pub enum NoExtraKeys {}

impl ExtraKeys for NoExtraKeys {
    const ALLOWED_KEYS: &'static [&'static str] = &[];

    fn index(self) -> i32 {
        // This index will never be used.
        -1
    }
}

/// An event metric.
///
/// Events allow recording of e.g. individual occurences of user actions, say
/// every time a view was open and from where. Each time you record an event, it
/// records a timestamp, the event's name and a set of custom values.
#[derive(Clone, Debug)]
pub struct EventMetric<K> {
    inner: glean_core::metrics::EventMetric,
    extra_keys: PhantomData<K>,
}

impl<K: ExtraKeys> EventMetric<K> {
    /// Create a new event metric.
    pub fn new(meta: CommonMetricData) -> Self {
        let allowed_extra_keys = K::ALLOWED_KEYS.iter().map(|s| s.to_string()).collect();
        let inner = glean_core::metrics::EventMetric::new(meta, allowed_extra_keys);

        Self {
            inner,
            extra_keys: PhantomData,
        }
    }

    /// Record an event.
    ///
    /// Records an event under this metric's name and attaches a timestamp.
    /// If given a map of extra values is added to the event log.
    ///
    /// ## Arguments
    ///
    /// * `extra` - An (optional) map of (key, value) pairs.
    pub fn record<M: Into<Option<HashMap<K, String>>>>(&self, extra: M) {
        let now = Instant::now();

        // Translate from [ExtraKey -> String] to a [Int -> String] map
        let extra = extra
            .into()
            .map(|h| h.into_iter().map(|(k, v)| (k.index(), v)).collect());

        crate::with_glean(|glean| self.inner.record(glean, now.as_millis(), extra))
    }

    /// **Test-only API.**
    ///
    /// Get the currently stored events for this event metric as a JSON-encoded string.
    /// This doesn't clear the stored value.
    ///
    /// ## Note
    ///
    /// This currently returns the value as a JSON encoded string.
    /// `glean_core` doesn't expose the underlying recorded event type.
    /// This will eventually change to a proper `RecordedEventData` type.
    /// See [Bug 1635074](https://bugzilla.mozilla.org/show_bug.cgi?id=1635074).
    ///
    /// ## Arguments
    ///
    /// * `storage_name` - the storage name to look into.
    ///
    /// ## Return value
    ///
    /// Returns the stored value or `None` if nothing stored.
    pub fn test_get_value(&self, storage_name: &str) -> Option<String> {
        crate::with_glean(|glean| {
            // Unfortunately we need to do this branch ourselves right now,
            // as `None` is encoded into the JSON `null`.

            let inner = &self.inner;
            if inner.test_has_value(glean, storage_name) {
                Some(inner.test_get_value_as_json_string(glean, storage_name))
            } else {
                None
            }
        })
    }
}
