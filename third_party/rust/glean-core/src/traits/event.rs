// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::convert::TryFrom;
use std::hash::Hash;

use crate::event_database::RecordedEvent;
use crate::ErrorType;

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
/// that an [`EventMetric`](crate::metrics::EventMetric) can accept.
///
/// *Note*: There exist no values for this enum, it can never exist.
/// It its equivalent to the [`never / !` type](https://doc.rust-lang.org/std/primitive.never.html).
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub enum NoExtraKeys {}

impl ExtraKeys for NoExtraKeys {
    const ALLOWED_KEYS: &'static [&'static str] = &[];

    fn index(self) -> i32 {
        // This index will never be used.
        -1
    }
}

/// The possible errors when parsing to an extra key.
pub enum EventRecordingError {
    /// The id doesn't correspond to a valid extra key
    InvalidId,
    /// The value doesn't correspond to a valid extra key
    InvalidExtraKey,
}

impl TryFrom<i32> for NoExtraKeys {
    type Error = EventRecordingError;

    fn try_from(_value: i32) -> Result<Self, Self::Error> {
        Err(EventRecordingError::InvalidExtraKey)
    }
}

impl TryFrom<&str> for NoExtraKeys {
    type Error = EventRecordingError;

    fn try_from(_value: &str) -> Result<Self, Self::Error> {
        Err(EventRecordingError::InvalidExtraKey)
    }
}

/// A description for the [`EventMetric`](crate::metrics::EventMetric) type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Event {
    /// The type of the allowed extra keys for this event.
    type Extra: ExtraKeys;

    /// Records an event.
    ///
    /// # Arguments
    ///
    /// * `extra` - A [`HashMap`] of (key, value) pairs. The key is one of the allowed extra keys as
    ///             set in the metric defintion.
    ///             If a wrong key is used or a value is larger than allowed, an error is reported
    ///             and no event is recorded.
    fn record<M: Into<Option<HashMap<Self::Extra, String>>>>(&self, extra: M);

    /// **Exported for test purposes.**
    ///
    /// Get the vector of currently stored events for this event metric.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<Vec<RecordedEvent>>;

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    fn test_get_num_recorded_errors<'a, S: Into<Option<&'a str>>>(
        &self,
        error: ErrorType,
        ping_name: S,
    ) -> i32;
}
