use super::{
    /*Signature,*/ COSEAlgorithm, COSEEC2Key, /*PlainText*/ COSEKey, COSEKeyType,
    /*CypherText,*/ ECDHSecret, ECDSACurve,
};
use openssl::bn::{BigNum, BigNumContext};
use openssl::derive::Deriver;
#[cfg(test)]
use openssl::ec::PointConversionForm;
use openssl::ec::{EcGroup, EcKey, EcPoint};
use openssl::error::ErrorStack;
use openssl::hash::{hash, MessageDigest};
use openssl::nid::Nid;
use openssl::pkey::{PKey, Private};
use openssl::sign::{Signer, Verifier};
use openssl::symm::{Cipher, Crypter, Mode};
use openssl::x509::X509;
use serde::{Serialize, Serializer};
use serde_bytes::ByteBuf;

fn openssl_string<S>(_: &ErrorStack, s: S) -> std::result::Result<S::Ok, S::Error>
where
    S: Serializer,
{
    s.serialize_str("OpenSSLError")
}

/// Errors that can be returned from COSE functions.
#[derive(Clone, Debug, Serialize)]
pub enum BackendError {
    #[serde(serialize_with = "openssl_string")]
    OpenSSL(ErrorStack),
    UnsupportedCurve(ECDSACurve),
    UnsupportedKeyType,
}

impl From<ErrorStack> for BackendError {
    fn from(e: ErrorStack) -> Self {
        BackendError::OpenSSL(e)
    }
}

impl From<&ErrorStack> for BackendError {
    fn from(e: &ErrorStack) -> Self {
        BackendError::OpenSSL(e.clone())
    }
}

pub type Result<T> = std::result::Result<T, BackendError>;

/* From CTAP2.1 spec:

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

fn to_openssl_name(curve: ECDSACurve) -> Result<Nid> {
    match curve {
        ECDSACurve::SECP256R1 => Ok(Nid::X9_62_PRIME256V1),
        ECDSACurve::SECP384R1 => Ok(Nid::SECP384R1),
        ECDSACurve::SECP521R1 => Ok(Nid::SECP521R1),
        x => Err(BackendError::UnsupportedCurve(x)),
    }
}

fn affine_coordinates(curve: ECDSACurve, bytes: &[u8]) -> Result<(ByteBuf, ByteBuf)> {
    let name = to_openssl_name(curve)?;
    let group = EcGroup::from_curve_name(name)?;

    let mut ctx = BigNumContext::new()?;
    let point = EcPoint::from_bytes(&group, bytes, &mut ctx)?;

    let mut x = BigNum::new()?;
    let mut y = BigNum::new()?;

    point.affine_coordinates_gfp(&group, &mut x, &mut y, &mut ctx)?;
    //point.affine_coordinates_gf2m(&group, &mut x, &mut y, &mut ctx)?;

    Ok((ByteBuf::from(x.to_vec()), ByteBuf::from(y.to_vec())))
}

// TODO(MS): Maybe remove ByteBuf and return Vec<u8>'s instead for a cleaner interface
pub(crate) fn serialize_key(curve: ECDSACurve, key: &[u8]) -> Result<(ByteBuf, ByteBuf)> {
    affine_coordinates(curve, key)
}

#[cfg(test)]
pub(crate) fn parse_key(curve: ECDSACurve, x: &[u8], y: &[u8]) -> Result<Vec<u8>> {
    let name = to_openssl_name(curve)?;
    let group = EcGroup::from_curve_name(name)?;

    let mut ctx = BigNumContext::new()?;
    let x = BigNum::from_slice(x)?;
    let y = BigNum::from_slice(y)?;

    let key = EcKey::from_public_key_affine_coordinates(&group, &x, &y)?;
    // TODO(baloo): what is uncompressed?!
    let pub_key = key.public_key();

    Ok(pub_key.to_bytes(&group, PointConversionForm::UNCOMPRESSED, &mut ctx)?)
}

/// This is run by the platform when starting a series of transactions with a specific authenticator.
//pub(crate) fn initialize() {
//
//}

/// Generates an encapsulation for the authenticator’s public key and returns the message
/// to transmit and the shared secret.
pub(crate) fn encapsulate(key: &COSEKey) -> Result<ECDHSecret> {
    if let COSEKeyType::EC2(ec2key) = &key.key {
        let curve_name = to_openssl_name(ec2key.curve)?;
        let group = EcGroup::from_curve_name(curve_name)?;
        let my_key = EcKey::generate(&group)?;

        encapsulate_helper(&ec2key, key.alg, group, my_key)
    } else {
        Err(BackendError::UnsupportedKeyType)
    }
}

pub(crate) fn encapsulate_helper(
    key: &COSEEC2Key,
    alg: COSEAlgorithm,
    group: EcGroup,
    my_key: EcKey<Private>,
) -> Result<ECDHSecret> {
    let mut ctx = BigNumContext::new()?;
    let mut x = BigNum::new()?;
    let mut y = BigNum::new()?;

    my_key
        .public_key()
        .affine_coordinates_gfp(&group, &mut x, &mut y, &mut ctx)?;

    let my_public_key = COSEKey {
        alg,
        key: COSEKeyType::EC2(COSEEC2Key {
            curve: key.curve.clone(),
            x: x.to_vec(),
            y: y.to_vec(),
        }),
    };

    // let point = EcPoint::from_bytes(&group, &key.key, &mut ctx)?;
    let peer_public_key = PKey::from_ec_key(EcKey::from_public_key_affine_coordinates(
        &group,
        BigNum::from_slice(&key.x).as_ref()?,
        BigNum::from_slice(&key.y).as_ref()?,
    )?)?;

    let my_ec_key = PKey::from_ec_key(my_key)?;
    let mut deriver = Deriver::new(my_ec_key.as_ref())?;
    deriver.set_peer(&peer_public_key)?;
    let shared_sec = deriver.derive_to_vec()?;

    // Hashing the key material
    let digest = hash(MessageDigest::sha256(), &shared_sec)?;

    Ok(ECDHSecret {
        remote: COSEKey {
            alg,
            key: COSEKeyType::EC2(key.clone()),
        },
        my: my_public_key,
        shared_secret: digest.as_ref().to_vec(),
    })
}

#[cfg(test)]
pub(crate) fn test_encapsulate(
    key: &COSEEC2Key,
    alg: COSEAlgorithm,
    my_pub_key: &[u8],
    my_priv_key: &[u8],
) -> Result<ECDHSecret> {
    let curve_name = to_openssl_name(key.curve)?;
    let group = EcGroup::from_curve_name(curve_name)?;

    let mut ctx = BigNumContext::new()?;
    let my_pub_point = EcPoint::from_bytes(&group, &my_pub_key, &mut ctx)?;
    let my_priv_bignum = BigNum::from_slice(my_priv_key)?;
    let my_key = EcKey::from_private_components(&group, &my_priv_bignum, &my_pub_point)?;

    encapsulate_helper(key, alg, group, my_key)
}

/// Encrypts a plaintext to produce a ciphertext, which may be longer than the plaintext.
/// The plaintext is restricted to being a multiple of the AES block size (16 bytes) in length.
pub(crate) fn encrypt(
    key: &[u8],
    plain_text: &[u8], /*PlainText*/
) -> Result<Vec<u8> /*CypherText*/> {
    let cipher = Cipher::aes_256_cbc();

    // TODO(baloo): This might trigger a panic if size is not big enough
    let mut cypher_text = vec![0; plain_text.len() * 2];
    cypher_text.resize(plain_text.len() * 2, 0);
    // Spec says explicitly IV=0
    let iv = [0u8; 16];
    let mut encrypter = Crypter::new(cipher, Mode::Encrypt, key, Some(&iv))?;
    encrypter.pad(false);
    let mut out_size = 0;
    out_size += encrypter.update(plain_text, cypher_text.as_mut_slice())?;
    out_size += encrypter.finalize(cypher_text.as_mut_slice())?;
    cypher_text.truncate(out_size);
    Ok(cypher_text)
}

/// Decrypts a ciphertext and returns the plaintext.
pub(crate) fn decrypt(
    key: &[u8],
    cypher_text: &[u8], /*CypherText*/
) -> Result<Vec<u8> /*PlainText*/> {
    let cipher = Cipher::aes_256_cbc();

    // TODO(baloo): This might trigger a panic if size is not big enough
    let mut plain_text = vec![0; cypher_text.len() * 2];
    plain_text.resize(cypher_text.len() * 2, 0);
    // Spec says explicitly IV=0
    let iv = [0u8; 16];
    let mut encrypter = Crypter::new(cipher, Mode::Decrypt, key, Some(&iv))?;
    encrypter.pad(false);
    let mut out_size = 0;
    out_size += encrypter.update(cypher_text, plain_text.as_mut_slice())?;
    out_size += encrypter.finalize(plain_text.as_mut_slice())?;
    plain_text.truncate(out_size);

    Ok(plain_text)
}

/// Computes a MAC of the given message.
pub(crate) fn authenticate(token: &[u8], input: &[u8]) -> Result<Vec<u8>> {
    // Create a PKey
    let key = PKey::hmac(token)?;

    // Compute the HMAC
    let mut signer = Signer::new(MessageDigest::sha256(), &key)?;
    signer.update(input)?;
    let hmac = signer.sign_to_vec()?;
    Ok(hmac)
}

// Currently unsued, because rc_crypto does not expose PKCS12 of NSS, so we can't parse the cert there
// To use it in statemachine.rs for example, do:
// if let Ok(cdhash) = client_data.hash() {
//     let verification_data: Vec<u8> = attestation
//         .auth_data
//         .to_vec()
//         .iter()
//         .chain(cdhash.as_ref().iter())
//         .copied()
//         .collect();
//     let res = attestation.att_statement.verify(&verification_data);
//     ...
// }
#[allow(dead_code)]
pub(crate) fn verify(
    sig_alg: ECDSACurve,
    pub_key: &[u8],
    signature: &[u8],
    data: &[u8],
) -> Result<bool> {
    let _alg = to_openssl_name(sig_alg)?; // TODO(MS): Actually use this to determine the right MessageDigest below
    let pkey = X509::from_der(&pub_key)?;
    let pubkey = pkey.public_key()?;
    let mut verifier = Verifier::new(MessageDigest::sha256(), &pubkey)?;
    verifier.update(data)?;
    let res = verifier.verify(signature)?;
    Ok(res)
}
