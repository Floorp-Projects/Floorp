use test_setup as test;
use {CoseError, SignatureAlgorithm};
use decoder::{COSE_HEADER_ALG, COSE_HEADER_KID, COSE_SIGN_TAG, COSE_TYPE_ES256, decode_signature};
use cbor::CborType;
use std::collections::BTreeMap;

#[test]
fn test_cose_decode() {
    let payload = b"This is the content.";
    let cose_signatures = decode_signature(&test::COSE_SIGNATURE_BYTES, payload).unwrap();
    assert_eq!(cose_signatures.len(), 1);
    assert_eq!(cose_signatures[0].signature_type, SignatureAlgorithm::ES256);
    assert_eq!(cose_signatures[0].signature, test::SIGNATURE_BYTES.to_vec());
    assert_eq!(cose_signatures[0].certs[0], test::P256_ROOT.to_vec());
    assert_eq!(cose_signatures[0].certs[1], test::P256_INT.to_vec());
}

fn test_cose_format_error(bytes: &[u8], expected_error: CoseError) {
    let payload = vec![0];
    let result = decode_signature(bytes, &payload);
    assert!(result.is_err());
    assert_eq!(result.err(), Some(expected_error));
}

// Helper function to take a `Vec<CborType>`, wrap it in a `CborType::Array`, tag it with the
// COSE_Sign tag (COSE_SIGN_TAG = 98), and serialize it to a `Vec<u8>`.
fn wrap_tag_and_encode_array(array: Vec<CborType>) -> Vec<u8> {
    CborType::Tag(COSE_SIGN_TAG, Box::new(CborType::Array(array))).serialize()
}

// Helper function to create an encoded protected header for a COSE_Sign or COSE_Signature
// structure.
fn encode_test_protected_header(keys: Vec<CborType>, values: Vec<CborType>) -> Vec<u8> {
    assert_eq!(keys.len(), values.len());
    let mut map: BTreeMap<CborType, CborType> = BTreeMap::new();
    for (key, value) in keys.iter().zip(values) {
        map.insert(key.clone(), value.clone());
    }
    CborType::Map(map).serialize()
}

// Helper function to create a test COSE_Signature structure with the given protected header.
fn build_test_cose_signature(protected_header: Vec<u8>) -> CborType {
    CborType::Array(vec![CborType::Bytes(protected_header),
         CborType::Map(BTreeMap::new()),
         CborType::Bytes(Vec::new())])
}

// Helper function to create the minimally-valid COSE_Sign (i.e. "body") protected header.
fn make_minimally_valid_cose_sign_protected_header() -> Vec<u8> {
    encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_KID)],
        vec![CborType::Array(Vec::new())],
    )
}

// Helper function to create a minimally-valid COSE_Signature (i.e. "body").
fn make_minimally_valid_cose_signature_protected_header() -> Vec<u8> {
    encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_ALG),
             CborType::Integer(COSE_HEADER_KID)],
        vec![CborType::SignedInteger(COSE_TYPE_ES256),
             CborType::Bytes(Vec::new())],
    )
}

// This tests the minimally-valid COSE_Sign structure according to this implementation.
// The structure must be a CBOR array of length 4 tagged with the integer 98.
// The COSE_Sign protected header must have the `kid` integer key and no others. The value for `kid`
// must be an array (although it may be empty). Each element of the array must be of type bytes.
// The COSE_Sign unprotected header must be an empty map.
// The COSE_Sign payload must be nil.
// The COSE_Sign signatures must be an array with at least one COSE_Signature.
// Each COSE_Signature must be an array of length 3.
// Each COSE_Signature protected header must have the `alg` and `kid` integer keys and no others.
// The value for `alg` must be a valid algorithm identifier. The value for `kid` must be bytes,
// although it may be empty.
// Each COSE_Signature unprotected header must be an empty map.
// Each COSE_Signature signature must be of type bytes (although it may be empty).
#[test]
fn test_cose_sign_minimally_valid() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    let payload = vec![0];
    let result = decode_signature(&bytes, &payload);
    assert!(result.is_ok());
    let decoded = result.unwrap();
    assert_eq!(decoded.len(), 1);
    assert_eq!(decoded[0].signer_cert.len(), 0);
    assert_eq!(decoded[0].certs.len(), 0);
}

#[test]
fn test_cose_sign_not_tagged() {
    let bytes = CborType::Array(vec![CborType::Integer(0)]).serialize();
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_sign_wrong_tag() {
    // The expected COSE_Sign tag is 98.
    let bytes = CborType::Tag(99, Box::new(CborType::Integer(0))).serialize();
    test_cose_format_error(&bytes, CoseError::UnexpectedTag);
}

#[test]
fn test_cose_sign_right_tag_wrong_contents() {
    // The COSE_Sign tag is 98, but the contents should be an array.
    let bytes = CborType::Tag(98, Box::new(CborType::Integer(0))).serialize();
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_sign_too_small() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_sign_too_large() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(Vec::new()),
                      CborType::Array(Vec::new())];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_sign_protected_header_empty() {
    let body_protected_header = encode_test_protected_header(Vec::new(), Vec::new());
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_sign_protected_header_missing_kid() {
    let body_protected_header =
        encode_test_protected_header(vec![CborType::Integer(2)], vec![CborType::Integer(2)]);
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MissingHeader);
}

#[test]
fn test_cose_sign_protected_header_kid_wrong_type() {
    let body_protected_header = encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_KID)],
        vec![CborType::Integer(2)],
    );
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_sign_protected_header_extra_header_key() {
    let body_protected_header = encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_KID),
             CborType::Integer(2)],
        vec![CborType::Bytes(Vec::new()),
             CborType::Integer(2)],
    );
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_sign_unprotected_header_wrong_type() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Integer(1),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_sign_unprotected_header_not_empty() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let mut unprotected_header_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    unprotected_header_map.insert(CborType::Integer(0), CborType::SignedInteger(-1));
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(unprotected_header_map),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_sign_payload_not_null() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Integer(0),
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_signatures_not_array() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Integer(0)];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_signatures_empty() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(Vec::new())];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_signature_protected_header_wrong_type() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature = CborType::Array(vec![CborType::Null,
         CborType::Map(BTreeMap::new()),
         CborType::SignedInteger(-1)]);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_signature_protected_header_empty() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(Vec::new(), Vec::new());
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_signature_protected_header_too_large() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = CborType::Array(vec![CborType::Bytes(signature_protected_header),
         CborType::Map(BTreeMap::new()),
         CborType::Bytes(Vec::new()),
         CborType::Null]);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_signature_protected_header_bad_encoding() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    // The bytes here are a truncated integer encoding.
    let signature = CborType::Array(vec![CborType::Bytes(vec![0x1a, 0x00, 0x00]),
         CborType::Map(BTreeMap::new()),
         CborType::Bytes(Vec::new())]);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::DecodingFailure);
}

#[test]
fn test_cose_signature_protected_header_missing_alg() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(
        vec![CborType::Integer(2),
             CborType::Integer(COSE_HEADER_KID)],
        vec![CborType::SignedInteger(COSE_TYPE_ES256),
             CborType::Bytes(Vec::new())],
    );
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MissingHeader);
}

#[test]
fn test_cose_signature_protected_header_missing_kid() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_ALG),
             CborType::Integer(3)],
        vec![CborType::SignedInteger(COSE_TYPE_ES256),
             CborType::Bytes(Vec::new())],
    );
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MissingHeader);
}

#[test]
fn test_cose_signature_protected_header_wrong_key_types() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(
        vec![CborType::SignedInteger(-1),
             CborType::Bytes(vec![0])],
        vec![CborType::SignedInteger(COSE_TYPE_ES256),
             CborType::Bytes(Vec::new())],
    );
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MissingHeader);
}

#[test]
fn test_cose_signature_protected_header_unexpected_alg_type() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_ALG),
             CborType::Integer(COSE_HEADER_KID)],
        vec![CborType::Integer(10),
             CborType::Integer(4)],
    );
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_signature_protected_header_unsupported_alg() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_ALG),
             CborType::Integer(COSE_HEADER_KID)],
        vec![CborType::SignedInteger(-10),
             CborType::Bytes(Vec::new())],
    );
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedHeaderValue);
}

#[test]
fn test_cose_signature_protected_header_unexpected_kid_type() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_ALG),
             CborType::Integer(COSE_HEADER_KID)],
        vec![CborType::SignedInteger(COSE_TYPE_ES256),
             CborType::Integer(0)],
    );
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_signature_protected_header_extra_key() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = encode_test_protected_header(
        vec![CborType::Integer(COSE_HEADER_ALG),
             CborType::Integer(COSE_HEADER_KID),
             CborType::Integer(5)],
        vec![CborType::SignedInteger(COSE_TYPE_ES256),
             CborType::Bytes(Vec::new()),
             CborType::Integer(5)],
    );
    let signature = build_test_cose_signature(signature_protected_header);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_signature_unprotected_header_wrong_type() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = CborType::Array(vec![CborType::Bytes(signature_protected_header),
         CborType::Integer(1),
         CborType::Bytes(Vec::new())]);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}

#[test]
fn test_cose_signature_unprotected_header_not_empty() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let mut unprotected_header_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    unprotected_header_map.insert(CborType::Integer(0), CborType::SignedInteger(-1));
    let signature = CborType::Array(vec![CborType::Bytes(signature_protected_header),
         CborType::Map(unprotected_header_map),
         CborType::Bytes(Vec::new())]);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::MalformedInput);
}

#[test]
fn test_cose_signature_signature_wrong_type() {
    let body_protected_header = make_minimally_valid_cose_sign_protected_header();
    let signature_protected_header = make_minimally_valid_cose_signature_protected_header();
    let signature = CborType::Array(vec![CborType::Bytes(signature_protected_header),
         CborType::Map(BTreeMap::new()),
         CborType::SignedInteger(-1)]);
    let values = vec![CborType::Bytes(body_protected_header),
                      CborType::Map(BTreeMap::new()),
                      CborType::Null,
                      CborType::Array(vec![signature])];
    let bytes = wrap_tag_and_encode_array(values);
    test_cose_format_error(&bytes, CoseError::UnexpectedType);
}
