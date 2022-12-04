/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Support for "encrypted bso"s, as received by the storage servers.
//! This module decrypts them into IncomingBso's suitable for use by the
//! engines.
use super::{IncomingBso, IncomingEnvelope, OutgoingBso, OutgoingEnvelope};
use crate::error;
use crate::key_bundle::KeyBundle;
use crate::EncryptedPayload;
use serde::{de::DeserializeOwned, Deserialize, Serialize};

// The BSO implementation we use for encrypted payloads.
// Note that this is almost identical to the IncomingBso implementations, except
// instead of a String payload we use an EncryptedPayload. Obviously we *could*
// just use a String payload and transform it into an EncryptedPayload - any maybe we
// should - but this is marginally optimal in terms of deserialization.
#[derive(Deserialize, Debug)]
pub struct IncomingEncryptedBso {
    #[serde(flatten)]
    pub envelope: IncomingEnvelope,
    #[serde(
        with = "as_json",
        bound(deserialize = "EncryptedPayload: DeserializeOwned")
    )]
    pub(crate) payload: EncryptedPayload,
}

impl IncomingEncryptedBso {
    pub fn new(envelope: IncomingEnvelope, payload: EncryptedPayload) -> Self {
        Self { envelope, payload }
    }
    /// Decrypt a BSO, consuming it into a clear-text version.
    pub fn into_decrypted(self, key: &KeyBundle) -> error::Result<IncomingBso> {
        Ok(IncomingBso::new(self.envelope, self.payload.decrypt(key)?))
    }
}

#[derive(Serialize, Debug)]
pub struct OutgoingEncryptedBso {
    #[serde(flatten)]
    pub envelope: OutgoingEnvelope,
    #[serde(with = "as_json", bound(serialize = "EncryptedPayload: Serialize"))]
    payload: EncryptedPayload,
}

impl OutgoingEncryptedBso {
    pub fn new(envelope: OutgoingEnvelope, payload: EncryptedPayload) -> Self {
        Self { envelope, payload }
    }

    #[inline]
    pub fn serialized_payload_len(&self) -> usize {
        self.payload.serialized_len()
    }
}

impl OutgoingBso {
    pub fn into_encrypted(self, key: &KeyBundle) -> error::Result<OutgoingEncryptedBso> {
        Ok(OutgoingEncryptedBso {
            envelope: self.envelope,
            payload: EncryptedPayload::from_cleartext(key, self.payload)?,
        })
    }
}

// The BSOs we write to the servers expect a "payload" attribute which is a JSON serialized
// string, rather than the JSON representation of the object.
// ie, the serialized object is expected to look like:
// `{"id": "some-guid", "payload": "{\"IV\": ... }"}` <-- payload is a string.
// However, if we just serialize it directly, we end up with:
// `{"id": "some-guid", "payload": {"IV":  ... }}` <-- payload is an object.
// The magic here means we can serialize and deserialize directly into/from the object, correctly
// working with the payload as a string, instead of needing to explicitly stringify/parse the
// payload as an extra step.
//
// This would work for any <T>, but we only use it for EncryptedPayload - the way our cleartext
// BSOs work mean it's not necessary there as they define the payload as a String - ie, they do
// explicitly end up doing 2 JSON operations as an ergonomic design choice.
mod as_json {
    use serde::de::{self, Deserialize, DeserializeOwned, Deserializer};
    use serde::ser::{self, Serialize, Serializer};

    pub fn serialize<T, S>(t: &T, serializer: S) -> Result<S::Ok, S::Error>
    where
        T: Serialize,
        S: Serializer,
    {
        let j = serde_json::to_string(t).map_err(ser::Error::custom)?;
        serializer.serialize_str(&j)
    }

    pub fn deserialize<'de, T, D>(deserializer: D) -> Result<T, D::Error>
    where
        T: DeserializeOwned,
        D: Deserializer<'de>,
    {
        let j = String::deserialize(deserializer)?;
        serde_json::from_str(&j).map_err(de::Error::custom)
    }
}

// Lots of stuff for testing the sizes of encrypted records, because the servers have
// certain limits in terms of max-POST sizes, forcing us to chunk uploads, but
// we need to calculate based on encrypted record size rather than the raw <T> size.
//
// This is a little cludgey but I couldn't think of another way to have easy deserialization
// without a bunch of wrapper types, while still only serializing a single time in the
// postqueue.
#[cfg(test)]
impl OutgoingEncryptedBso {
    /// Return the length of the serialized payload.
    pub fn payload_serialized_len(&self) -> usize {
        self.payload.serialized_len()
    }

    // self.payload is private, but tests want to create funky things.
    // XXX - test only, but test in another crate :(
    //#[cfg(test)]
    pub fn make_test_bso(ciphertext: String) -> Self {
        Self {
            envelope: OutgoingEnvelope {
                id: "".into(),
                sortindex: None,
                ttl: None,
            },
            payload: EncryptedPayload {
                iv: "".into(),
                hmac: "".into(),
                ciphertext,
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bso::OutgoingEnvelope;

    #[test]
    fn test_deserialize_enc() {
        let serialized = r#"{
            "id": "1234",
            "collection": "passwords",
            "modified": 12344321.0,
            "payload": "{\"IV\": \"aaaaa\", \"hmac\": \"bbbbb\", \"ciphertext\": \"ccccc\"}"
        }"#;
        let record: IncomingEncryptedBso = serde_json::from_str(serialized).unwrap();
        assert_eq!(&record.envelope.id, "1234");
        assert_eq!((record.envelope.modified.0 - 12_344_321_000).abs(), 0);
        assert_eq!(record.envelope.sortindex, None);
        assert_eq!(&record.payload.iv, "aaaaa");
        assert_eq!(&record.payload.hmac, "bbbbb");
        assert_eq!(&record.payload.ciphertext, "ccccc");
    }

    #[test]
    fn test_deserialize_autofields() {
        let serialized = r#"{
            "id": "1234",
            "collection": "passwords",
            "modified": 12344321.0,
            "sortindex": 100,
            "ttl": 99,
            "payload": "{\"IV\": \"aaaaa\", \"hmac\": \"bbbbb\", \"ciphertext\": \"ccccc\"}"
        }"#;
        let record: IncomingEncryptedBso = serde_json::from_str(serialized).unwrap();
        assert_eq!(record.envelope.sortindex, Some(100));
        assert_eq!(record.envelope.ttl, Some(99));
    }

    #[test]
    fn test_serialize_enc() {
        let goal = r#"{"id":"1234","payload":"{\"IV\":\"aaaaa\",\"hmac\":\"bbbbb\",\"ciphertext\":\"ccccc\"}"}"#;
        let record = OutgoingEncryptedBso {
            envelope: OutgoingEnvelope {
                id: "1234".into(),
                ..Default::default()
            },
            payload: EncryptedPayload {
                iv: "aaaaa".into(),
                hmac: "bbbbb".into(),
                ciphertext: "ccccc".into(),
            },
        };
        let actual = serde_json::to_string(&record).unwrap();
        assert_eq!(actual, goal);

        let val_str_payload: serde_json::Value = serde_json::from_str(goal).unwrap();
        assert_eq!(
            val_str_payload["payload"].as_str().unwrap().len(),
            record.payload.serialized_len()
        )
    }
}
