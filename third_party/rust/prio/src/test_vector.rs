// SPDX-License-Identifier: MPL-2.0

//! Generates test vectors of serialized Prio inputs, enabling backward compatibility testing.

use crate::{
    client::{Client, ClientError},
    encrypt::{PrivateKey, PublicKey},
    field::{FieldElement, FieldPrio2},
    server::{Server, ServerError},
};
use rand::Rng;
use serde::{Deserialize, Serialize};
use std::fmt::Debug;

/// Errors propagated by functions in this module.
#[derive(Debug, thiserror::Error)]
pub enum TestVectorError {
    /// Error from Prio client
    #[error("Prio client error {0}")]
    Client(#[from] ClientError),
    /// Error from Prio server
    #[error("Prio server error {0}")]
    Server(#[from] ServerError),
    /// Error while converting primitive to FieldElement associated integer type
    #[error("Integer conversion error {0}")]
    IntegerConversion(String),
}

const SERVER_1_PRIVATE_KEY: &str =
    "BIl6j+J6dYttxALdjISDv6ZI4/VWVEhUzaS05LgrsfswmbLOgNt9HUC2E0w+9RqZx3XMkdEHBH\
     fNuCSMpOwofVSq3TfyKwn0NrftKisKKVSaTOt5seJ67P5QL4hxgPWvxw==";
const SERVER_2_PRIVATE_KEY: &str =
    "BNNOqoU54GPo+1gTPv+hCgA9U2ZCKd76yOMrWa1xTWgeb4LhFLMQIQoRwDVaW64g/WTdcxT4rD\
     ULoycUNFB60LER6hPEHg/ObBnRPV1rwS3nj9Bj0tbjVPPyL9p8QW8B+w==";

/// An ECDSA P-256 private key suitable for decrypting inputs, used to generate
/// test vectors and later to decrypt them.
fn server_1_private_key() -> PrivateKey {
    PrivateKey::from_base64(SERVER_1_PRIVATE_KEY).unwrap()
}

/// The public portion of [`server_1_private_key`].
fn server_1_public_key() -> PublicKey {
    PublicKey::from(&server_1_private_key())
}

/// An ECDSA P-256 private key suitable for decrypting inputs, used to generate
/// test vectors and later to decrypt them.
fn server_2_private_key() -> PrivateKey {
    PrivateKey::from_base64(SERVER_2_PRIVATE_KEY).unwrap()
}

/// The public portion of [`server_2_private_key`].
fn server_2_public_key() -> PublicKey {
    PublicKey::from(&server_2_private_key())
}

/// A test vector of Prio inputs, serialized and encrypted in the Priov2 format,
/// along with a reference sum. The field is always [`FieldPrio2`].
#[derive(Debug, Eq, PartialEq, Serialize, Deserialize)]
pub struct Priov2TestVector {
    /// Base64 encoded private key for the "first" a.k.a. "PHA" server, which
    /// may be used to decrypt `server_1_shares`.
    pub server_1_private_key: String,
    /// Base64 encoded private key for the non-"first" a.k.a. "facilitator"
    /// server, which may be used to decrypt `server_2_shares`.
    pub server_2_private_key: String,
    /// Dimension (number of buckets) of the inputs
    pub dimension: usize,
    /// Encrypted shares of Priov2 format inputs for the "first" a.k.a. "PHA"
    /// server. The inner `Vec`s are encrypted bytes.
    #[serde(
        serialize_with = "base64::serialize_bytes",
        deserialize_with = "base64::deserialize_bytes"
    )]
    pub server_1_shares: Vec<Vec<u8>>,
    /// Encrypted share of Priov2 format inputs for the non-"first" a.k.a.
    /// "facilitator" server.
    #[serde(
        serialize_with = "base64::serialize_bytes",
        deserialize_with = "base64::deserialize_bytes"
    )]
    pub server_2_shares: Vec<Vec<u8>>,
    /// The sum over the inputs.
    #[serde(
        serialize_with = "base64::serialize_field",
        deserialize_with = "base64::deserialize_field"
    )]
    pub reference_sum: Vec<FieldPrio2>,
    /// The version of the crate that generated this test vector
    pub prio_crate_version: String,
}

impl Priov2TestVector {
    /// Construct a test vector of `number_of_clients` inputs, each of which is a
    /// `dimension`-dimension vector of random Boolean values encoded as
    /// [`FieldPrio2`].
    pub fn new(dimension: usize, number_of_clients: usize) -> Result<Self, TestVectorError> {
        let mut client: Client<FieldPrio2> =
            Client::new(dimension, server_1_public_key(), server_2_public_key())?;

        let mut reference_sum = vec![FieldPrio2::zero(); dimension];
        let mut server_1_shares = Vec::with_capacity(number_of_clients);
        let mut server_2_shares = Vec::with_capacity(number_of_clients);

        let mut rng = rand::thread_rng();

        for _ in 0..number_of_clients {
            // Generate a random vector of booleans
            let data: Vec<FieldPrio2> = (0..dimension)
                .map(|_| FieldPrio2::from(rng.gen_range(0..2)))
                .collect();

            // Update reference sum
            for (r, d) in reference_sum.iter_mut().zip(&data) {
                *r += *d;
            }

            let (server_1_share, server_2_share) = client.encode_simple(&data)?;

            server_1_shares.push(server_1_share);
            server_2_shares.push(server_2_share);
        }

        Ok(Self {
            server_1_private_key: SERVER_1_PRIVATE_KEY.to_owned(),
            server_2_private_key: SERVER_2_PRIVATE_KEY.to_owned(),
            dimension,
            server_1_shares,
            server_2_shares,
            reference_sum,
            prio_crate_version: env!("CARGO_PKG_VERSION").to_owned(),
        })
    }

    /// Construct a [`Client`] that can encrypt input shares to this test
    /// vector's servers.
    pub fn client(&self) -> Result<Client<FieldPrio2>, TestVectorError> {
        Ok(Client::new(
            self.dimension,
            PublicKey::from(&PrivateKey::from_base64(&self.server_1_private_key).unwrap()),
            PublicKey::from(&PrivateKey::from_base64(&self.server_2_private_key).unwrap()),
        )?)
    }

    /// Construct a [`Server`] that can decrypt `server_1_shares`.
    pub fn server_1(&self) -> Result<Server<FieldPrio2>, TestVectorError> {
        Ok(Server::new(
            self.dimension,
            true,
            PrivateKey::from_base64(&self.server_1_private_key).unwrap(),
        )?)
    }

    /// Construct a [`Server`] that can decrypt `server_2_shares`.
    pub fn server_2(&self) -> Result<Server<FieldPrio2>, TestVectorError> {
        Ok(Server::new(
            self.dimension,
            false,
            PrivateKey::from_base64(&self.server_2_private_key).unwrap(),
        )?)
    }
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::util::reconstruct_shares;

    #[test]
    fn roundtrip_test_vector_serialization() {
        let test_vector = Priov2TestVector::new(123, 100).unwrap();
        let serialized = serde_json::to_vec(&test_vector).unwrap();
        let test_vector_again: Priov2TestVector = serde_json::from_slice(&serialized).unwrap();

        assert_eq!(test_vector, test_vector_again);
    }

    #[test]
    fn accumulation_field_priov2() {
        let dimension = 123;
        let test_vector = Priov2TestVector::new(dimension, 100).unwrap();

        let mut server1 = test_vector.server_1().unwrap();
        let mut server2 = test_vector.server_2().unwrap();

        for (server_1_share, server_2_share) in test_vector
            .server_1_shares
            .iter()
            .zip(&test_vector.server_2_shares)
        {
            let eval_at = server1.choose_eval_at();

            let v1 = server1
                .generate_verification_message(eval_at, server_1_share)
                .unwrap();
            let v2 = server2
                .generate_verification_message(eval_at, server_2_share)
                .unwrap();

            assert!(server1.aggregate(server_1_share, &v1, &v2).unwrap());
            assert!(server2.aggregate(server_2_share, &v1, &v2).unwrap());
        }

        let total1 = server1.total_shares();
        let total2 = server2.total_shares();

        let reconstructed = reconstruct_shares(total1, total2).unwrap();
        assert_eq!(reconstructed, test_vector.reference_sum);
    }
}
