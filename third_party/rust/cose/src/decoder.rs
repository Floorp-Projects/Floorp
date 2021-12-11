//! Parse and decode COSE signatures.

use cbor::CborType;
use cbor::decoder::decode;
use {CoseError, SignatureAlgorithm};
use util::get_sig_struct_bytes;
use std::collections::BTreeMap;

pub const COSE_SIGN_TAG: u64 = 98;

/// The result of `decode_signature` holding a decoded COSE signature.
#[derive(Debug)]
pub struct CoseSignature {
    pub signature_type: SignatureAlgorithm,
    pub signature: Vec<u8>,
    pub signer_cert: Vec<u8>,
    pub certs: Vec<Vec<u8>>,
    pub to_verify: Vec<u8>,
}

pub const COSE_TYPE_ES256: i64 = -7;
pub const COSE_TYPE_ES384: i64 = -35;
pub const COSE_TYPE_ES512: i64 = -36;
pub const COSE_TYPE_PS256: i64 = -37;

pub const COSE_HEADER_ALG: u64 = 1;
pub const COSE_HEADER_KID: u64 = 4;

macro_rules! unpack {
   ($to:tt, $var:ident) => (
        match *$var {
            CborType::$to(ref cbor_object) => {
                cbor_object
            }
            _ => return Err(CoseError::UnexpectedType),
        };
    )
}

fn get_map_value(
    map: &BTreeMap<CborType, CborType>,
    key: &CborType,
) -> Result<CborType, CoseError> {
    match map.get(key) {
        Some(x) => Ok(x.clone()),
        _ => Err(CoseError::MissingHeader),
    }
}

/// Ensure that the referenced `CborType` is an empty map.
fn ensure_empty_map(map: &CborType) -> Result<(), CoseError> {
    let unpacked = unpack!(Map, map);
    if !unpacked.is_empty() {
        return Err(CoseError::MalformedInput);
    }
    Ok(())
}

// This syntax is a little unintuitive. Taken together, the two previous definitions essentially
// mean:
//
// COSE_Sign = [
//     protected : empty_or_serialized_map,
//     unprotected : header_map
//     payload : bstr / nil,
//     signatures : [+ COSE_Signature]
// ]
//
// (COSE_Sign is an array. The first element is an empty or serialized map (in our case, it is
// never expected to be empty). The second element is a map (it is expected to be empty. The third
// element is a bstr or nil (it is expected to be nil). The fourth element is an array of
// COSE_Signature.)
//
// COSE_Signature =  [
//     Headers,
//     signature : bstr
// ]
//
// but again, unpacking this:
//
// COSE_Signature =  [
//     protected : empty_or_serialized_map,
//     unprotected : header_map
//     signature : bstr
// ]
fn decode_signature_struct(
    cose_signature: &CborType,
    payload: &[u8],
    protected_body_head: &CborType,
) -> Result<CoseSignature, CoseError> {
    let cose_signature = unpack!(Array, cose_signature);
    if cose_signature.len() != 3 {
        return Err(CoseError::MalformedInput);
    }
    let protected_signature_header_serialized = &cose_signature[0];
    let protected_signature_header_bytes = unpack!(Bytes, protected_signature_header_serialized);

    // Parse the protected signature header.
    let protected_signature_header = &match decode(protected_signature_header_bytes) {
        Err(_) => return Err(CoseError::DecodingFailure),
        Ok(value) => value,
    };
    let protected_signature_header = unpack!(Map, protected_signature_header);
    if protected_signature_header.len() != 2 {
        return Err(CoseError::MalformedInput);
    }
    let signature_algorithm = get_map_value(
        protected_signature_header,
        &CborType::Integer(COSE_HEADER_ALG),
    )?;
    let signature_algorithm = match signature_algorithm {
        CborType::SignedInteger(val) => {
            match val {
                COSE_TYPE_ES256 => SignatureAlgorithm::ES256,
                COSE_TYPE_ES384 => SignatureAlgorithm::ES384,
                COSE_TYPE_ES512 => SignatureAlgorithm::ES512,
                COSE_TYPE_PS256 => SignatureAlgorithm::PS256,
                _ => return Err(CoseError::UnexpectedHeaderValue),
            }
        }
        _ => return Err(CoseError::UnexpectedType),
    };

    let ee_cert = &get_map_value(
        protected_signature_header,
        &CborType::Integer(COSE_HEADER_KID),
    )?;
    let ee_cert = unpack!(Bytes, ee_cert).clone();

    // The unprotected header section is expected to be an empty map.
    ensure_empty_map(&cose_signature[1])?;

    // Build signature structure to verify.
    let signature_bytes = &cose_signature[2];
    let signature_bytes = unpack!(Bytes, signature_bytes).clone();
    let sig_structure_bytes = get_sig_struct_bytes(
        protected_body_head.clone(),
        protected_signature_header_serialized.clone(),
        payload,
    );

    // Read intermediate certificates from protected_body_head.
    // Any tampering of the protected header during transport will be detected
    // because it is input to the signature verification.
    // Note that a protected header has to be present and hold a kid with an
    // empty list of intermediate certificates.
    let protected_body_head_bytes = unpack!(Bytes, protected_body_head);
    let protected_body_head_map = &match decode(protected_body_head_bytes) {
        Ok(value) => value,
        Err(_) => return Err(CoseError::DecodingFailure),
    };
    let protected_body_head_map = unpack!(Map, protected_body_head_map);
    if protected_body_head_map.len() != 1 {
        return Err(CoseError::MalformedInput);
    }
    let intermediate_certs_array =
        &get_map_value(protected_body_head_map, &CborType::Integer(COSE_HEADER_KID))?;
    let intermediate_certs = unpack!(Array, intermediate_certs_array);
    let mut certs: Vec<Vec<u8>> = Vec::new();
    for cert in intermediate_certs {
        let cert = unpack!(Bytes, cert);
        certs.push(cert.clone());
    }

    Ok(CoseSignature {
        signature_type: signature_algorithm,
        signature: signature_bytes,
        signer_cert: ee_cert,
        certs: certs,
        to_verify: sig_structure_bytes,
    })
}

/// Decode COSE signature bytes and return a vector of `CoseSignature`.
///
///```rust,ignore
/// COSE_Sign = [
///     Headers,
///     payload : bstr / nil,
///     signatures : [+ COSE_Signature]
/// ]
///
/// Headers = (
///     protected : empty_or_serialized_map,
///     unprotected : header_map
/// )
///```
pub fn decode_signature(bytes: &[u8], payload: &[u8]) -> Result<Vec<CoseSignature>, CoseError> {
    // This has to be a COSE_Sign object, which is a tagged array.
    let tagged_cose_sign = match decode(bytes) {
        Err(_) => return Err(CoseError::DecodingFailure),
        Ok(value) => value,
    };
    let cose_sign_array = match tagged_cose_sign {
        CborType::Tag(tag, cose_sign) => {
            if tag != COSE_SIGN_TAG {
                return Err(CoseError::UnexpectedTag);
            }
            match *cose_sign {
                CborType::Array(values) => values,
                _ => return Err(CoseError::UnexpectedType),
            }
        }
        _ => return Err(CoseError::UnexpectedType),
    };
    if cose_sign_array.len() != 4 {
        return Err(CoseError::MalformedInput);
    }

    // The unprotected header section is expected to be an empty map.
    ensure_empty_map(&cose_sign_array[1])?;

    // The payload is expected to be Null (i.e. this is a detached signature).
    match cose_sign_array[2] {
        CborType::Null => {}
        _ => return Err(CoseError::UnexpectedType),
    };

    let signatures = &cose_sign_array[3];
    let signatures = unpack!(Array, signatures);

    // Decode COSE_Signatures.
    // There has to be at least one signature to make this a valid COSE signature.
    if signatures.len() < 1 {
        return Err(CoseError::MalformedInput);
    }
    let mut result = Vec::new();
    for cose_signature in signatures {
        // cose_sign_array[0] holds the protected body header.
        let signature = decode_signature_struct(cose_signature, payload, &cose_sign_array[0])?;
        result.push(signature);
    }

    Ok(result)
}
