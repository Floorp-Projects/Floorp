extern crate moz_cbor as cbor;
extern crate cose;

#[macro_use(defer)]
extern crate scopeguard;

mod nss;
mod test_nss;
mod test_setup;
mod util_test;

use util_test::{sign, verify_signature};
use test_setup as test;
use std::str::FromStr;
use cose::{CoseError, SignatureAlgorithm};

// All keys here are from pykey.py/pycert.py from mozilla-central.
// Certificates can be generated with tools/certs/certs.sh and mozilla-central.

#[derive(Debug)]
pub struct SignatureParameters<'a> {
    certificate: &'a [u8],
    algorithm: SignatureAlgorithm,
    pkcs8: &'a [u8],
}

#[derive(Debug)]
pub struct Signature<'a> {
    parameter: &'a SignatureParameters<'a>,
    signature_bytes: Vec<u8>,
}

const P256_PARAMS: SignatureParameters = SignatureParameters {
    certificate: &test::P256_EE,
    algorithm: SignatureAlgorithm::ES256,
    pkcs8: &test::PKCS8_P256_EE,
};
const P384_PARAMS: SignatureParameters = SignatureParameters {
    certificate: &test::P384_EE,
    algorithm: SignatureAlgorithm::ES384,
    pkcs8: &test::PKCS8_P384_EE,
};
const P521_PARAMS: SignatureParameters = SignatureParameters {
    certificate: &test::P521_EE,
    algorithm: SignatureAlgorithm::ES512,
    pkcs8: &test::PKCS8_P521_EE,
};

fn test_verify(payload: &[u8], cert_chain: &[&[u8]], params_vec: Vec<SignatureParameters>) {
    test::setup();
    let cose_signature = sign(payload, cert_chain, &params_vec);
    assert!(cose_signature.is_ok());
    let cose_signature = cose_signature.unwrap();

    // Verify signature.
    assert!(verify_signature(payload, cose_signature).is_ok());
}

fn test_verify_modified_payload(
    payload: &mut [u8],
    cert_chain: &[&[u8]],
    params_vec: Vec<SignatureParameters>,
) {
    test::setup();
    let cose_signature = sign(payload, cert_chain, &params_vec);
    assert!(cose_signature.is_ok());
    let cose_signature = cose_signature.unwrap();

    // Verify signature.
    payload[0] = !payload[0];
    let verify_result = verify_signature(payload, cose_signature);
    assert!(verify_result.is_err());
    assert_eq!(verify_result, Err(CoseError::VerificationFailed));
}

fn test_verify_modified_signature(
    payload: &[u8],
    cert_chain: &[&[u8]],
    params_vec: Vec<SignatureParameters>,
) {
    test::setup();
    let cose_signature = sign(payload, cert_chain, &params_vec);
    assert!(cose_signature.is_ok());
    let mut cose_signature = cose_signature.unwrap();

    // Tamper with the cose signature.
    let len = cose_signature.len();
    cose_signature[len - 15] = !cose_signature[len - 15];

    // Verify signature.
    let verify_result = verify_signature(payload, cose_signature);
    assert!(verify_result.is_err());
    assert_eq!(verify_result, Err(CoseError::VerificationFailed));
}

// This can be used with inconsistent parameters that make the verification fail.
// In particular, the signing key does not match the certificate used to verify.
fn test_verify_verification_fails(
    payload: &[u8],
    cert_chain: &[&[u8]],
    params_vec: Vec<SignatureParameters>,
) {
    test::setup();
    let cose_signature = sign(payload, cert_chain, &params_vec);
    assert!(cose_signature.is_ok());
    let cose_signature = cose_signature.unwrap();

    // Verify signature.
    let verify_result = verify_signature(payload, cose_signature);
    assert!(verify_result.is_err());
    assert_eq!(verify_result, Err(CoseError::VerificationFailed));
}

fn test_cose_sign_verify() {
    let payload = b"This is the content.";

    // P256
    let certs: [&[u8]; 2] = [&test::P256_ROOT,
                             &test::P256_INT];
    let params_vec = vec![P256_PARAMS];
    test_verify(payload, &certs, params_vec);

    // P256, no other certs.
    let certs: [&[u8]; 0] = [];
    let params_vec = vec![P256_PARAMS];
    test_verify(payload, &certs, params_vec);

    // P384
    let params_vec = vec![P384_PARAMS];
    test_verify(payload, &certs, params_vec);

    // P521
    let params_vec = vec![P521_PARAMS];
    test_verify(payload, &certs, params_vec);
}

fn test_cose_verify_xpi_signature() {
    // This signature was created with sign_app.py from m-c.
    test::setup();
    assert!(verify_signature(&test::XPI_PAYLOAD, test::XPI_SIGNATURE.to_vec()).is_ok());
}

fn test_cose_sign_verify_modified_payload() {
    let mut payload = String::from_str("This is the content.")
        .unwrap()
        .into_bytes();
    let certs: [&[u8]; 2] = [&test::P256_ROOT,
                             &test::P256_INT];
    let params_vec = vec![P256_PARAMS];
    test_verify_modified_payload(&mut payload, &certs, params_vec);
}

fn test_cose_sign_verify_wrong_cert() {
    let payload = b"This is the content.";
    let certs: [&[u8]; 2] = [&test::P256_ROOT,
                             &test::P256_INT];
    let params = SignatureParameters {
        certificate: &test::P384_EE,
        algorithm: SignatureAlgorithm::ES256,
        pkcs8: &test::PKCS8_P256_EE,
    };
    let params_vec = vec![params];
    test_verify_verification_fails(payload, &certs, params_vec);
}

fn test_cose_sign_verify_tampered_signature() {
    let payload = b"This is the content.";
    let certs: [&[u8]; 2] = [&test::P256_ROOT,
                             &test::P256_INT];
    let params_vec = vec![P256_PARAMS];
    test_verify_modified_signature(payload, &certs, params_vec);
}

const RSA_PARAMS: SignatureParameters = SignatureParameters {
    certificate: &test::RSA_EE,
    algorithm: SignatureAlgorithm::PS256,
    pkcs8: &test::PKCS8_RSA_EE,
};

fn test_cose_sign_verify_rsa() {
    let payload = b"This is the RSA-signed content.";
    let certs: [&[u8]; 2] = [&test::RSA_ROOT,
                             &test::RSA_INT];
    let params_vec = vec![RSA_PARAMS];
    test_verify(payload, &certs, params_vec);
}

fn test_cose_sign_verify_rsa_modified_payload() {
    let mut payload = String::from_str("This is the RSA-signed content.")
        .unwrap()
        .into_bytes();
    let certs: [&[u8]; 2] = [&test::RSA_ROOT,
                             &test::RSA_INT];
    let params_vec = vec![RSA_PARAMS];
    test_verify_modified_payload(&mut payload, &certs, params_vec);
}

fn test_cose_sign_verify_rsa_tampered_signature() {
    let payload = b"This is the RSA-signed content.";
    let certs: [&[u8]; 2] = [&test::RSA_ROOT,
                             &test::RSA_INT];
    let params_vec = vec![RSA_PARAMS];
    test_verify_modified_signature(payload, &certs, params_vec);
}

fn test_cose_sign_verify_two_signatures() {
    let payload = b"This is the content.";
    let certs: [&[u8]; 4] = [&test::P256_ROOT,
                             &test::P256_INT,
                             &test::RSA_ROOT,
                             &test::RSA_INT];
    let params_vec = vec![P256_PARAMS,
                          RSA_PARAMS];
    test_verify(payload, &certs, params_vec);
}

fn test_cose_sign_verify_two_signatures_tampered_payload() {
    let mut payload = String::from_str("This is the content.")
        .unwrap()
        .into_bytes();
    let certs: [&[u8]; 4] = [&test::P256_ROOT,
                             &test::P256_INT,
                             &test::RSA_ROOT,
                             &test::RSA_INT];
    let params_vec = vec![P256_PARAMS,
                          RSA_PARAMS];
    test_verify_modified_payload(&mut payload, &certs, params_vec);
}

fn test_cose_sign_verify_two_signatures_tampered_signature() {
    let payload = b"This is the content.";
    let certs: [&[u8]; 4] = [&test::P256_ROOT,
                             &test::P256_INT,
                             &test::RSA_ROOT,
                             &test::RSA_INT];
    let params_vec = vec![P256_PARAMS,
                          RSA_PARAMS];
    test_verify_modified_signature(payload, &certs, params_vec);
}

fn main() {
    // Basic NSS exmaple usage.
    test_nss::test_nss_sign_verify();
    test_nss::test_nss_sign_verify_different_payload();
    test_nss::test_nss_sign_verify_wrong_cert();

    // COSE sign/verify example usages.
    test_cose_sign_verify_two_signatures_tampered_signature();
    test_cose_sign_verify_two_signatures_tampered_payload();
    test_cose_sign_verify_two_signatures();
    test_cose_sign_verify_rsa_tampered_signature();
    test_cose_sign_verify_rsa_modified_payload();
    test_cose_sign_verify_rsa();
    test_cose_sign_verify_tampered_signature();
    test_cose_sign_verify_wrong_cert();
    test_cose_sign_verify_modified_payload();
    test_cose_verify_xpi_signature();
    test_cose_sign_verify();
}
