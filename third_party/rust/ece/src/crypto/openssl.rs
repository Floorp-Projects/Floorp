/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    crypto::{Cryptographer, EcKeyComponents, LocalKeyPair, RemotePublicKey},
    error::*,
};
use hkdf::Hkdf;
use lazy_static::lazy_static;
use openssl::{
    bn::{BigNum, BigNumContext},
    derive::Deriver,
    ec::{EcGroup, EcKey, EcPoint, PointConversionForm},
    nid::Nid,
    pkey::{PKey, Private, Public},
    rand::rand_bytes,
    symm::{Cipher, Crypter, Mode},
};
use sha2::Sha256;
use std::{any::Any, fmt};

lazy_static! {
    static ref GROUP_P256: EcGroup = EcGroup::from_curve_name(Nid::X9_62_PRIME256V1).unwrap();
}
const AES_GCM_TAG_LENGTH: usize = 16;

#[derive(Clone, Debug)]
pub struct OpenSSLRemotePublicKey {
    raw_pub_key: Vec<u8>,
}

impl OpenSSLRemotePublicKey {
    fn from_raw(raw: &[u8]) -> Result<Self> {
        Ok(OpenSSLRemotePublicKey {
            raw_pub_key: raw.to_vec(),
        })
    }

    fn to_pkey(&self) -> Result<PKey<Public>> {
        let mut bn_ctx = BigNumContext::new()?;
        let point = EcPoint::from_bytes(&GROUP_P256, &self.raw_pub_key, &mut bn_ctx)?;
        let ec = EcKey::from_public_key(&GROUP_P256, &point)?;
        PKey::from_ec_key(ec).map_err(std::convert::Into::into)
    }
}

impl RemotePublicKey for OpenSSLRemotePublicKey {
    fn as_raw(&self) -> Result<Vec<u8>> {
        Ok(self.raw_pub_key.to_vec())
    }
    fn as_any(&self) -> &dyn Any {
        self
    }
}

#[derive(Clone)]
pub struct OpenSSLLocalKeyPair {
    ec_key: EcKey<Private>,
}

impl fmt::Debug for OpenSSLLocalKeyPair {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{:?}",
            base64::encode_config(&self.ec_key.private_key().to_vec(), base64::URL_SAFE)
        )
    }
}

impl OpenSSLLocalKeyPair {
    /// Generate a random local key pair using OpenSSL `ECKey::generate`.
    fn generate_random() -> Result<Self> {
        let ec_key = EcKey::generate(&GROUP_P256)?;
        Ok(OpenSSLLocalKeyPair { ec_key })
    }

    fn to_pkey(&self) -> Result<PKey<Private>> {
        PKey::from_ec_key(self.ec_key.clone()).map_err(std::convert::Into::into)
    }

    fn from_raw_components(components: &EcKeyComponents) -> Result<Self> {
        let d = BigNum::from_slice(&components.private_key())?;
        let mut bn_ctx = BigNumContext::new()?;
        let ec_point = EcPoint::from_bytes(&GROUP_P256, &components.public_key(), &mut bn_ctx)?;
        let mut x = BigNum::new()?;
        let mut y = BigNum::new()?;
        ec_point.affine_coordinates_gfp(&GROUP_P256, &mut x, &mut y, &mut bn_ctx)?;
        let public_key = EcKey::from_public_key_affine_coordinates(&GROUP_P256, &x, &y)?;
        let private_key = EcKey::from_private_components(&GROUP_P256, &d, public_key.public_key())?;
        Ok(Self {
            ec_key: private_key,
        })
    }
}

impl LocalKeyPair for OpenSSLLocalKeyPair {
    /// Export the public key component in the binary uncompressed point representation
    /// using OpenSSL `PointConversionForm::UNCOMPRESSED`.
    fn pub_as_raw(&self) -> Result<Vec<u8>> {
        let pub_key_point = self.ec_key.public_key();
        let mut bn_ctx = BigNumContext::new()?;
        let uncompressed =
            pub_key_point.to_bytes(&GROUP_P256, PointConversionForm::UNCOMPRESSED, &mut bn_ctx)?;
        Ok(uncompressed)
    }

    fn raw_components(&self) -> Result<EcKeyComponents> {
        let private_key = self.ec_key.private_key();
        Ok(EcKeyComponents::new(
            private_key.to_vec(),
            self.pub_as_raw()?,
        ))
    }

    fn as_any(&self) -> &dyn Any {
        self
    }
}

impl From<EcKey<Private>> for OpenSSLLocalKeyPair {
    fn from(key: EcKey<Private>) -> OpenSSLLocalKeyPair {
        OpenSSLLocalKeyPair { ec_key: key }
    }
}

pub struct OpensslCryptographer;
impl Cryptographer for OpensslCryptographer {
    fn generate_ephemeral_keypair(&self) -> Result<Box<dyn LocalKeyPair>> {
        Ok(Box::new(OpenSSLLocalKeyPair::generate_random()?))
    }

    fn import_key_pair(&self, components: &EcKeyComponents) -> Result<Box<dyn LocalKeyPair>> {
        Ok(Box::new(OpenSSLLocalKeyPair::from_raw_components(
            components,
        )?))
    }

    fn import_public_key(&self, raw: &[u8]) -> Result<Box<dyn RemotePublicKey>> {
        Ok(Box::new(OpenSSLRemotePublicKey::from_raw(raw)?))
    }

    fn compute_ecdh_secret(
        &self,
        remote: &dyn RemotePublicKey,
        local: &dyn LocalKeyPair,
    ) -> Result<Vec<u8>> {
        let local_any = local.as_any();
        let local = local_any.downcast_ref::<OpenSSLLocalKeyPair>().unwrap();
        let private = local.to_pkey()?;
        let remote_any = remote.as_any();
        let remote = remote_any.downcast_ref::<OpenSSLRemotePublicKey>().unwrap();
        let public = remote.to_pkey()?;
        let mut deriver = Deriver::new(&private)?;
        deriver.set_peer(&public)?;
        let shared_key = deriver.derive_to_vec()?;
        Ok(shared_key)
    }

    fn hkdf_sha256(&self, salt: &[u8], secret: &[u8], info: &[u8], len: usize) -> Result<Vec<u8>> {
        let (_, hk) = Hkdf::<Sha256>::extract(Some(&salt[..]), &secret);
        let mut okm = vec![0u8; len];
        hk.expand(&info, &mut okm).unwrap();
        Ok(okm)
    }

    fn aes_gcm_128_encrypt(&self, key: &[u8], iv: &[u8], data: &[u8]) -> Result<Vec<u8>> {
        let cipher = Cipher::aes_128_gcm();
        let mut c = Crypter::new(cipher, Mode::Encrypt, key, Some(iv))?;
        let mut out = vec![0u8; data.len() + cipher.block_size()];
        let count = c.update(data, &mut out)?;
        let rest = c.finalize(&mut out[count..])?;
        let mut tag = vec![0u8; AES_GCM_TAG_LENGTH];
        c.get_tag(&mut tag)?;
        out.truncate(count + rest);
        out.append(&mut tag);
        Ok(out)
    }

    fn aes_gcm_128_decrypt(
        &self,
        key: &[u8],
        iv: &[u8],
        ciphertext_and_tag: &[u8],
    ) -> Result<Vec<u8>> {
        let block_len = ciphertext_and_tag.len() - AES_GCM_TAG_LENGTH;
        let ciphertext = &ciphertext_and_tag[0..block_len];
        let tag = &ciphertext_and_tag[block_len..];
        let cipher = Cipher::aes_128_gcm();
        let mut c = Crypter::new(cipher, Mode::Decrypt, key, Some(iv))?;
        let mut out = vec![0u8; ciphertext.len() + cipher.block_size()];
        let count = c.update(ciphertext, &mut out)?;
        c.set_tag(tag)?;
        let rest = c.finalize(&mut out[count..])?;
        out.truncate(count + rest);
        Ok(out)
    }

    fn random_bytes(&self, dest: &mut [u8]) -> Result<()> {
        Ok(rand_bytes(dest)?)
    }
}
