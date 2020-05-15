/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::*;
use std::any::Any;

pub(crate) mod holder;
#[cfg(feature = "backend-openssl")]
mod openssl;

#[cfg(not(feature = "backend-openssl"))]
pub use holder::{set_boxed_cryptographer, set_cryptographer};

pub trait RemotePublicKey: Send + Sync + 'static {
    /// Export the key component in the
    /// binary uncompressed point representation.
    fn as_raw(&self) -> Result<Vec<u8>>;
    /// For downcasting purposes.
    fn as_any(&self) -> &dyn Any;
}

pub trait LocalKeyPair: Send + Sync + 'static {
    /// Export the public key component in the
    /// binary uncompressed point representation.
    fn pub_as_raw(&self) -> Result<Vec<u8>>;
    /// Export the raw components of the keypair.
    fn raw_components(&self) -> Result<EcKeyComponents>;
    /// For downcasting purposes.
    fn as_any(&self) -> &dyn Any;
}

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(
    feature = "serializable-keys",
    derive(serde::Serialize, serde::Deserialize)
)]
pub enum EcCurve {
    P256,
}

impl Default for EcCurve {
    fn default() -> Self {
        EcCurve::P256
    }
}

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(
    feature = "serializable-keys",
    derive(serde::Serialize, serde::Deserialize)
)]
pub struct EcKeyComponents {
    // The curve is only kept in case the ECE standard changes in the future.
    curve: EcCurve,
    // The `d` value of the EC Key.
    private_key: Vec<u8>,
    // The uncompressed x,y-representation of the public component of the EC Key.
    public_key: Vec<u8>,
}

impl EcKeyComponents {
    pub fn new<T: Into<Vec<u8>>>(private_key: T, public_key: T) -> Self {
        EcKeyComponents {
            private_key: private_key.into(),
            public_key: public_key.into(),
            curve: Default::default(),
        }
    }
    pub fn curve(&self) -> &EcCurve {
        &self.curve
    }
    /// The `d` value of the EC Key.
    pub fn private_key(&self) -> &[u8] {
        &self.private_key
    }
    /// The uncompressed x,y-representation of the public component of the EC Key.
    pub fn public_key(&self) -> &[u8] {
        &self.public_key
    }
}

pub trait Cryptographer: Send + Sync + 'static {
    /// Generate a random ephemeral local key pair.
    fn generate_ephemeral_keypair(&self) -> Result<Box<dyn LocalKeyPair>>;
    /// Import a local keypair from its raw components.
    fn import_key_pair(&self, components: &EcKeyComponents) -> Result<Box<dyn LocalKeyPair>>;
    /// Import the public key component in the binary uncompressed point representation.
    fn import_public_key(&self, raw: &[u8]) -> Result<Box<dyn RemotePublicKey>>;
    fn compute_ecdh_secret(
        &self,
        remote: &dyn RemotePublicKey,
        local: &dyn LocalKeyPair,
    ) -> Result<Vec<u8>>;
    fn hkdf_sha256(&self, salt: &[u8], secret: &[u8], info: &[u8], len: usize) -> Result<Vec<u8>>;
    /// Should return [ciphertext, auth_tag].
    fn aes_gcm_128_encrypt(&self, key: &[u8], iv: &[u8], data: &[u8]) -> Result<Vec<u8>>;
    fn aes_gcm_128_decrypt(
        &self,
        key: &[u8],
        iv: &[u8],
        ciphertext_and_tag: &[u8],
    ) -> Result<Vec<u8>>;
    fn random_bytes(&self, dest: &mut [u8]) -> Result<()>;
}
