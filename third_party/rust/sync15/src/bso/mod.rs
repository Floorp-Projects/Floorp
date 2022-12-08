/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This module defines our core "bso" abstractions.
/// In the terminology of this crate:
/// * "bso" is an acronym for "basic storage object" and used extensively in the sync server docs.
///    the record always has a well-defined "envelope" with metadata (eg, the ID of the record,
///    the server timestamp of the resource,  etc) and a field called `payload`.
///    A bso is serialized to and from JSON.
/// * There's a "cleartext" bso:
/// ** The payload is a String, which itself is JSON encoded (ie, this string `payload` is
///    always double JSON encoded in a server record)
/// ** This supplies helper methods for working with the "content" (some arbitrary <T>) in the
///    payload.
/// * There's an "encrypted" bso
/// ** The payload is an [crate::enc_payload::EncryptedPayload]
/// ** Only clients use this; as soon as practical we decrypt and as late as practical we encrypt
///    to and from encrypted bsos.
/// ** The encrypted bsos etc are all in the [crypto] module and require the `crypto` feature.
///
/// Let's look at some real-world examples:
/// # meta/global
/// A "bso" (ie, record with an "envelope" and a "payload" with a JSON string) - but the payload
/// is cleartext.
/// ```json
/// {
///   "id":"global",
///   "modified":1661564513.50,
///   "payload": "{\"syncID\":\"p1z5_oDdOfLF\",\"storageVersion\":5,\"engines\":{\"passwords\":{\"version\":1,\"syncID\":\"6Y6JJkB074cF\"} /* snip */},\"declined\":[]}"
/// }```
///
/// # encrypted bsos:
/// Encrypted BSOs are still a "bso" (ie, a record with a field names `payload` which is a string)
/// but the payload is in the form of an EncryptedPayload.
/// For example, crypto/keys:
/// ```json
/// {
///   "id":"keys",
///   "modified":1661564513.74,
///   "payload":"{\"IV\":\"snip-base-64==\",\"hmac\":\"snip-hex\",\"ciphertext\":\"snip-base64==\"}"
/// }```
/// (Note that as described above, most code working with bsos *do not* use that `payload`
/// directly, but instead a decrypted cleartext bso.
///
/// Note all collection responses are the same shape as `crypto/keys` - a `payload` field with a
/// JSON serialized EncryptedPayload, it's just that the final <T> content differs for each
/// collection (eg, tabs and bookmarks have quite different <T>s JSON-encoded in the
/// String payload.)
///
/// For completeness, some other "non-BSO" records - no "id", "modified" or "payload" fields in
/// the response, just plain-old clear-text JSON.
/// # Example
/// ## `info/collections`
/// ```json
/// {
///   "bookmarks":1661564648.65,
///   "meta":1661564513.50,
///   "addons":1661564649.09,
///   "clients":1661564643.57,
///   ...
/// }```
/// ## `info/configuration`
/// ```json
/// {
///   "max_post_bytes":2097152,
///   "max_post_records":100,
///   "max_record_payload_bytes":2097152,
///   ...
/// }```
///
/// Given our definitions above, these are not any kind of "bso", so are
/// not relevant to this module
use crate::{Guid, ServerTimestamp};
use serde::{Deserialize, Serialize};

#[cfg(feature = "crypto")]
mod crypto;
#[cfg(feature = "crypto")]
pub use crypto::{IncomingEncryptedBso, OutgoingEncryptedBso};

mod content;

// A feature for this would be ideal, but (a) the module is small and (b) it
// doesn't really fit the "features" model for sync15 to have a dev-dependency
// against itself but with a different feature set.
pub mod test_utils;

/// An envelope for an incoming item. Envelopes carry all the metadata for
/// a Sync BSO record (`id`, `modified`, `sortindex`), *but not* the payload
/// itself.
#[derive(Debug, Clone, Deserialize)]
pub struct IncomingEnvelope {
    /// The ID of the record.
    pub id: Guid,
    // If we don't give it a default, a small handful of tests fail.
    // XXX - we should probably fix the tests and kill this?
    #[serde(default = "ServerTimestamp::default")]
    pub modified: ServerTimestamp,
    pub sortindex: Option<i32>,
    pub ttl: Option<u32>,
}

/// An envelope for an outgoing item. This is conceptually identical to
/// [IncomingEnvelope], but omits fields that are only set by the server,
/// like `modified`.
#[derive(Debug, Default, Clone, Serialize)]
pub struct OutgoingEnvelope {
    /// The ID of the record.
    pub id: Guid,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub sortindex: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ttl: Option<u32>,
}

/// Allow an outgoing envelope to be constructed with just a guid when default
/// values for the other fields are OK.
impl From<Guid> for OutgoingEnvelope {
    fn from(id: Guid) -> Self {
        OutgoingEnvelope {
            id,
            ..Default::default()
        }
    }
}

/// IncomingBso's can come from:
/// * Directly from the server (ie, some records aren't encrypted, such as meta/global)
/// * From environments where the encryption is done externally (eg, Rust syncing in Desktop
///   Firefox has the encryption/decryption done by Firefox and the cleartext BSOs are passed in.
/// * Read from the server as an EncryptedBso; see EncryptedBso description above.
#[derive(Deserialize, Debug)]
pub struct IncomingBso {
    #[serde(flatten)]
    pub envelope: IncomingEnvelope,
    // payload is public for some edge-cases in some components, but in general,
    // you should use into_content<> to get a record out of it.
    pub payload: String,
}

impl IncomingBso {
    pub fn new(envelope: IncomingEnvelope, payload: String) -> Self {
        Self { envelope, payload }
    }
}

#[derive(Serialize, Debug)]
pub struct OutgoingBso {
    #[serde(flatten)]
    pub envelope: OutgoingEnvelope,
    // payload is public for some edge-cases in some components, but in general,
    // you should use into_content<> to get a record out of it.
    pub payload: String,
}

impl OutgoingBso {
    /// Most consumers will use `self.from_content` and `self.from_content_with_id`
    /// but this exists for the few consumers for whom that doesn't make sense.
    pub fn new<T: Serialize>(
        envelope: OutgoingEnvelope,
        val: &T,
    ) -> Result<Self, serde_json::Error> {
        Ok(Self {
            envelope,
            payload: serde_json::to_string(&val)?,
        })
    }
}

/// We also have the concept of "content", which helps work with a `T` which
/// is represented inside the payload. Real-world examples of a `T` include
/// Bookmarks or Tabs.
/// See the content module for the implementations.
///
/// So this all flows together in the following way:
/// * Incoming encrypted data:
///   EncryptedIncomingBso -> IncomingBso -> [specific engine] -> IncomingContent<T>
/// * Incoming cleartext data:
///   IncomingBso -> IncomingContent<T>
///   (Note that incoming cleartext only happens for a few collections managed by
///   the sync client and never by specific engines - engine BSOs are always encryted)
/// * Outgoing encrypted data:
///   OutgoingBso (created in the engine) -> [this crate] -> EncryptedOutgoingBso
///  * Outgoing cleartext data: just an OutgoingBso with no conversions needed.

/// [IncomingContent] is the result of converting an [IncomingBso] into
/// some <T> - it consumes the Bso, so you get the envelope, and the [IncomingKind]
/// which reflects the state of parsing the json.
#[derive(Debug)]
pub struct IncomingContent<T> {
    pub envelope: IncomingEnvelope,
    pub kind: IncomingKind<T>,
}

/// The "kind" of incoming content after deserializing it.
pub enum IncomingKind<T> {
    /// A good, live T.
    Content(T),
    /// A record that used to be a T but has been replaced with a tombstone.
    Tombstone,
    /// Either not JSON, or can't be made into a T.
    Malformed,
}
