use super::{
    /*Signature,*/ COSEAlgorithm, COSEEC2Key, /*PlainText*/ COSEKey, COSEKeyType,
    /*CypherText,*/ ECDHSecret, ECDSACurve,
};
use ring::agreement::{
    agree_ephemeral, Algorithm, EphemeralPrivateKey, UnparsedPublicKey, ECDH_P256, ECDH_P384,
};
use ring::digest;
use ring::hmac;
use ring::rand::SystemRandom;
use ring::signature::KeyPair;
use serde::Serialize;
use serde_bytes::ByteBuf;
/*
initialize()

    This is run by the platform when starting a series of transactions with a specific authenticator.
encapsulate(peerCoseKey) → (coseKey, sharedSecret) | error

    Generates an encapsulation for the authenticator’s public key and returns the message to transmit and the shared secret.
encrypt(key, demPlaintext) → ciphertext

    Encrypts a plaintext to produce a ciphertext, which may be longer than the plaintext. The plaintext is restricted to being a multiple of the AES block size (16 bytes) in length.
decrypt(key, ciphertext) → plaintext | error

    Decrypts a ciphertext and returns the plaintext.
authenticate(key, message) → signature

    Computes a MAC of the given message.
*/

pub type Result<T> = std::result::Result<T, BackendError>;

#[derive(Clone, Debug, PartialEq, Serialize)]
pub enum BackendError {
    AgreementError,
    UnspecifiedRingError,
    KeyRejected,
    UnsupportedKeyType,
    UnsupportedCurve(ECDSACurve),
}

fn to_ring_curve(curve: ECDSACurve) -> Result<&'static Algorithm> {
    match curve {
        ECDSACurve::SECP256R1 => Ok(&ECDH_P256),
        ECDSACurve::SECP384R1 => Ok(&ECDH_P384),
        x => Err(BackendError::UnsupportedCurve(x)),
    }
}

impl From<ring::error::Unspecified> for BackendError {
    fn from(e: ring::error::Unspecified) -> Self {
        BackendError::UnspecifiedRingError
    }
}

impl From<ring::error::KeyRejected> for BackendError {
    fn from(e: ring::error::KeyRejected) -> Self {
        BackendError::KeyRejected
    }
}

pub(crate) fn parse_key(curve: ECDSACurve, x: &[u8], y: &[u8]) -> Result<Vec<u8>> {
    compile_error!(
        "Ring-backend is not yet implemented. Compile with `--features crypto_openssl` for now."
    )
}

// TODO(MS): Maybe remove ByteBuf and return Vec<u8>'s instead for a cleaner interface
pub(crate) fn serialize_key(curve: ECDSACurve, key: &[u8]) -> Result<(ByteBuf, ByteBuf)> {
    compile_error!(
        "Ring-backend is not yet implemented. Compile with `--features crypto_openssl` for now."
    )
}

/// This is run by the platform when starting a series of transactions with a specific authenticator.
pub(crate) fn initialize() {
    compile_error!(
        "Ring-backend is not yet implemented. Compile with `--features crypto_openssl` for now."
    )
}

/// Generates an encapsulation for the authenticator’s public key and returns the message
/// to transmit and the shared secret.
pub(crate) fn encapsulate(key: &COSEKey) -> Result<ECDHSecret> {
    if let COSEKeyType::EC2(ec2key) = &key.key {
        // let curve_name = to_openssl_name(ec2key.curve)?;
        // let group = EcGroup::from_curve_name(curve_name)?;
        // let my_key = EcKey::generate(&group)?;

        // encapsulate_helper(&ec2key, key.alg, group, my_key)
        let rng = SystemRandom::new();
        let peer_public_key_alg = to_ring_curve(ec2key.curve)?;
        let private_key = EphemeralPrivateKey::generate(peer_public_key_alg, &rng)?;
        let my_public_key = private_key.compute_public_key()?;
        let (x, y) = serialize_key(ec2key.curve, my_public_key.as_ref())?;
        let my_public_key = COSEKey {
            alg: key.alg,
            key: COSEKeyType::EC2(COSEEC2Key {
                curve: ec2key.curve,
                x: x.to_vec(),
                y: y.to_vec(),
            }),
        };

        let key_bytes = parse_key(ec2key.curve, &ec2key.x, &ec2key.y)?;
        let peer_public_key = UnparsedPublicKey::new(peer_public_key_alg, &key_bytes);

        let shared_secret = agree_ephemeral(private_key, &peer_public_key, (), |key_material| {
            let digest = digest::digest(&digest::SHA256, key_material);
            Ok(Vec::from(digest.as_ref()))
        })
        .map_err(|_| BackendError::AgreementError)?;

        Ok(ECDHSecret {
            remote: COSEKey {
                alg: key.alg,
                key: COSEKeyType::EC2(ec2key.clone()),
            },
            my: my_public_key,
            shared_secret,
        })
    } else {
        Err(BackendError::UnsupportedKeyType)
    }
}

#[cfg(test)]
pub(crate) fn test_encapsulate(
    key: &COSEEC2Key,
    alg: COSEAlgorithm,
    my_pub_key: &[u8],
    my_priv_key: &[u8],
) -> Result<ECDHSecret> {
    compile_error!(
        "Ring-backend is not yet implemented. Compile with `--features crypto_openssl` for now."
    )
}

/// Encrypts a plaintext to produce a ciphertext, which may be longer than the plaintext.
/// The plaintext is restricted to being a multiple of the AES block size (16 bytes) in length.
pub(crate) fn encrypt(
    key: &[u8],
    plain_text: &[u8], /*PlainText*/
) -> Result<Vec<u8> /*CypherText*/> {
    // Ring doesn't support AES-CBC yet
    compile_error!(
        "Ring-backend is not yet implemented. Compile with `--features crypto_openssl` for now."
    )
}

/// Decrypts a ciphertext and returns the plaintext.
pub(crate) fn decrypt(
    key: &[u8],
    cypher_text: &[u8], /*CypherText*/
) -> Result<Vec<u8> /*PlainText*/> {
    // Ring doesn't support AES-CBC yet
    compile_error!(
        "Ring-backend is not yet implemented. Compile with `--features crypto_openssl` for now."
    )
}

/// Computes a MAC of the given message.
pub(crate) fn authenticate(token: &[u8], input: &[u8]) -> Result<Vec<u8>> {
    let s_key = hmac::Key::new(hmac::HMAC_SHA256, token);
    let tag = hmac::sign(&s_key, input);
    Ok(tag.as_ref().to_vec())
}
