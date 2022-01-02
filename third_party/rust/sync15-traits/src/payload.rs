/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::Guid;
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value as JsonValue};

/// Represents the decrypted payload in a Bso. Provides a minimal layer of type
/// safety to avoid double-encrypting.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Payload {
    pub id: Guid,

    #[serde(default)]
    #[serde(skip_serializing_if = "crate::skip_if_default")]
    pub deleted: bool,

    #[serde(flatten)]
    pub data: Map<String, JsonValue>,
}

impl Payload {
    pub fn new_tombstone(id: impl Into<Guid>) -> Payload {
        Payload {
            id: id.into(),
            deleted: true,
            data: Map::new(),
        }
    }

    pub fn new_tombstone_with_ttl(id: impl Into<Guid>, ttl: u32) -> Payload {
        let mut result = Payload::new_tombstone(id);
        result.data.insert("ttl".into(), ttl.into());
        result
    }

    #[inline]
    pub fn with_sortindex(mut self, index: i32) -> Payload {
        self.data.insert("sortindex".into(), index.into());
        self
    }

    /// "Auto" fields are fields like 'sortindex' and 'ttl', which are:
    ///
    /// - Added to the payload automatically when deserializing if present on
    ///   the incoming BSO or envelope.
    /// - Removed from the payload automatically and attached to the BSO or
    ///   envelope if present on the outgoing payload.
    pub fn with_auto_field<T: Into<JsonValue>>(mut self, name: &str, v: Option<T>) -> Payload {
        let old_value: Option<JsonValue> = if let Some(value) = v {
            self.data.insert(name.into(), value.into())
        } else {
            self.data.remove(name)
        };

        // This is a little dubious, but it seems like if we have a e.g. `sortindex` field on the payload
        // it's going to be a bug if we use it instead of the "real" sort index.
        if let Some(old_value) = old_value {
            log::warn!(
                "Payload for record {} already contains 'automatic' field \"{}\"? \
                 Overwriting auto value: {} with 'real' value",
                self.id,
                name,
                old_value,
            );
        }
        self
    }

    pub fn take_auto_field<V>(&mut self, name: &str) -> Option<V>
    where
        for<'a> V: Deserialize<'a>,
    {
        let v = self.data.remove(name)?;
        match serde_json::from_value(v) {
            Ok(v) => Some(v),
            Err(e) => {
                log::error!(
                    "Automatic field {} exists on payload, but cannot be deserialized: {}",
                    name,
                    e
                );
                None
            }
        }
    }

    #[inline]
    pub fn id(&self) -> &str {
        &self.id[..]
    }

    #[inline]
    pub fn is_tombstone(&self) -> bool {
        self.deleted
    }

    pub fn from_json(value: JsonValue) -> Result<Payload, serde_json::Error> {
        serde_json::from_value(value)
    }

    /// Deserializes the BSO payload into a specific record type `T`.
    ///
    /// BSO payloads are unstructured JSON objects, with string keys and
    /// dynamically-typed values. `into_record` makes it more convenient to
    /// work with payloads by converting them into data type-specific structs.
    /// Your record type only needs to derive or implement `serde::Deserialize`;
    /// Serde will take care of the rest.
    ///
    /// # Errors
    ///
    /// `into_record` returns errors for type mismatches. As an example, trying
    /// to deserialize a string value from the payload into an integer field in
    /// `T` will fail.
    ///
    /// If there's a chance that a field contains invalid or mistyped data,
    /// you'll want to extract it from `payload.data` manually, instead of using
    /// `into_record`. This has been seen in the wild: for example, `dateAdded`
    /// for bookmarks can be either an integer or a string.
    pub fn into_record<T>(self) -> Result<T, serde_json::Error>
    where
        for<'a> T: Deserialize<'a>,
    {
        serde_json::from_value(JsonValue::from(self))
    }

    pub fn from_record<T: Serialize>(v: T) -> Result<Payload, serde_json::Error> {
        // TODO(issue #2588): This is kind of dumb, we do to_value and then
        // from_value. In general a more strongly typed API would help us avoid
        // this sort of thing... But also concretely this could probably be
        // avoided? At least in some cases.
        Ok(Payload::from_json(serde_json::to_value(v)?)?)
    }

    pub fn into_json_string(self) -> String {
        serde_json::to_string(&JsonValue::from(self))
            .expect("JSON.stringify failed, which shouldn't be possible")
    }
}

impl From<Payload> for JsonValue {
    fn from(cleartext: Payload) -> Self {
        let Payload {
            mut data,
            id,
            deleted,
        } = cleartext;
        data.insert("id".to_string(), JsonValue::String(id.into_string()));
        if deleted {
            data.insert("deleted".to_string(), JsonValue::Bool(true));
        }
        JsonValue::Object(data)
    }
}
