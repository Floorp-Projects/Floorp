// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fmt::Display;

use serde::{Deserialize, Serialize};
use serde_json::Value as JsonValue;

/// This type represents all possible errors that can occur when serializing or deserializing an object from/to JSON.
#[derive(Debug)]
pub struct ObjectError(serde_json::Error);

impl Display for ObjectError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Display::fmt(&self.0, f)
    }
}

impl std::error::Error for ObjectError {}

/// An object that can be serialized into JSON.
///
/// Objects are defined by their structure in the metrics definition.
///
/// This is essentially a wrapper around serde's `Serialize`/`Deserialize`,
/// but in a way we can name it for our JSON (de)serialization.
pub trait ObjectSerialize {
    /// Deserialize the object from its JSON representation.
    ///
    /// Returns an error if deserialization fails.
    /// This should not happen for glean_parser-generated and later serialized objects.
    fn from_str(obj: &str) -> Result<Self, ObjectError>
    where
        Self: Sized;

    /// Serialize this object into a JSON string.
    fn into_serialized_object(self) -> Result<JsonValue, ObjectError>;
}

impl<V> ObjectSerialize for V
where
    V: Serialize,
    V: for<'de> Deserialize<'de>,
{
    fn from_str(obj: &str) -> Result<Self, ObjectError> {
        serde_json::from_str(obj).map_err(ObjectError)
    }

    fn into_serialized_object(self) -> Result<JsonValue, ObjectError> {
        serde_json::to_value(self).map_err(ObjectError)
    }
}
