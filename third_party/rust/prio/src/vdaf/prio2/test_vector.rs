// SPDX-License-Identifier: MPL-2.0

//! Test vectors of serialized Prio inputs, enabling backward compatibility testing.

use crate::{field::FieldPrio2, vdaf::prio2::client::ClientError};
use serde::{Deserialize, Serialize};
use std::fmt::Debug;

/// Errors propagated by functions in this module.
#[derive(Debug, thiserror::Error)]
pub(crate) enum TestVectorError {
    /// Error from Prio client
    #[error("Prio client error {0}")]
    Client(#[from] ClientError),
}

/// A test vector of serialized Priov2 inputs, along with a reference sum. The field is always
/// [`FieldPrio2`].
#[derive(Debug, Eq, PartialEq, Serialize, Deserialize)]
pub(crate) struct Priov2TestVector {
    /// Dimension (number of buckets) of the inputs
    pub dimension: usize,
    /// Decrypted shares of Priov2 format inputs for the "first" a.k.a. "PHA"
    /// server. The inner `Vec`s are encrypted bytes.
    #[serde(
        serialize_with = "base64::serialize_bytes",
        deserialize_with = "base64::deserialize_bytes"
    )]
    pub server_1_decrypted_shares: Vec<Vec<u8>>,
    /// Decrypted share of Priov2 format inputs for the non-"first" a.k.a.
    /// "facilitator" server.
    #[serde(
        serialize_with = "base64::serialize_bytes",
        deserialize_with = "base64::deserialize_bytes"
    )]
    pub server_2_decrypted_shares: Vec<Vec<u8>>,
    /// The sum over the inputs.
    #[serde(
        serialize_with = "base64::serialize_field",
        deserialize_with = "base64::deserialize_field"
    )]
    pub reference_sum: Vec<FieldPrio2>,
}

mod base64 {
    //! Custom serialization module used for some members of struct
    //! `Priov2TestVector` so that byte slices are serialized as base64 strings
    //! instead of an array of an array of integers when serializing to JSON.
    //
    // Thank you, Alice! https://users.rust-lang.org/t/serialize-a-vec-u8-to-json-as-base64/57781/2
    use crate::field::{FieldElement, FieldPrio2};
    use base64::{engine::Engine, prelude::BASE64_STANDARD};
    use serde::{de::Error, Deserialize, Deserializer, Serialize, Serializer};

    pub fn serialize_bytes<S: Serializer>(v: &[Vec<u8>], s: S) -> Result<S::Ok, S::Error> {
        let base64_vec = v
            .iter()
            .map(|bytes| BASE64_STANDARD.encode(bytes))
            .collect();
        <Vec<String>>::serialize(&base64_vec, s)
    }

    pub fn deserialize_bytes<'de, D: Deserializer<'de>>(d: D) -> Result<Vec<Vec<u8>>, D::Error> {
        <Vec<String>>::deserialize(d)?
            .iter()
            .map(|s| BASE64_STANDARD.decode(s.as_bytes()).map_err(Error::custom))
            .collect()
    }

    pub fn serialize_field<S: Serializer>(v: &[FieldPrio2], s: S) -> Result<S::Ok, S::Error> {
        String::serialize(
            &BASE64_STANDARD.encode(FieldPrio2::slice_into_byte_vec(v)),
            s,
        )
    }

    pub fn deserialize_field<'de, D: Deserializer<'de>>(d: D) -> Result<Vec<FieldPrio2>, D::Error> {
        let bytes = BASE64_STANDARD
            .decode(String::deserialize(d)?.as_bytes())
            .map_err(Error::custom)?;
        FieldPrio2::byte_slice_into_vec(&bytes).map_err(Error::custom)
    }
}
