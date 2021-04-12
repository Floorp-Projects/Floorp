/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_upper_case_globals)]

use libloading::{Library, Symbol};
use pkcs11::types::*;
use sha2::{Digest, Sha256};
use std::collections::BTreeMap;
use std::convert::TryInto;
use std::os::raw::c_void;

use core_foundation::array::*;
use core_foundation::base::*;
use core_foundation::boolean::*;
use core_foundation::data::*;
use core_foundation::dictionary::*;
use core_foundation::error::*;
use core_foundation::number::*;
use core_foundation::string::*;

// Normally we would generate this with a build script, but macos is
// cross-compiled on linux, and we'd have to figure out e.g. include paths,
// etc.. This is easier.
include!("bindings_macos.rs");

use crate::manager::SlotType;
use crate::util::*;

#[repr(C)]
pub struct __SecIdentity(c_void);
pub type SecIdentityRef = *const __SecIdentity;
declare_TCFType!(SecIdentity, SecIdentityRef);
impl_TCFType!(SecIdentity, SecIdentityRef, SecIdentityGetTypeID);

#[repr(C)]
pub struct __SecCertificate(c_void);
pub type SecCertificateRef = *const __SecCertificate;
declare_TCFType!(SecCertificate, SecCertificateRef);
impl_TCFType!(SecCertificate, SecCertificateRef, SecCertificateGetTypeID);

#[repr(C)]
pub struct __SecKey(c_void);
pub type SecKeyRef = *const __SecKey;
declare_TCFType!(SecKey, SecKeyRef);
impl_TCFType!(SecKey, SecKeyRef, SecKeyGetTypeID);

#[repr(C)]
pub struct __SecPolicy(c_void);
pub type SecPolicyRef = *const __SecPolicy;
declare_TCFType!(SecPolicy, SecPolicyRef);
impl_TCFType!(SecPolicy, SecPolicyRef, SecPolicyGetTypeID);

#[repr(C)]
pub struct __SecTrust(c_void);
pub type SecTrustRef = *const __SecTrust;
declare_TCFType!(SecTrust, SecTrustRef);
impl_TCFType!(SecTrust, SecTrustRef, SecTrustGetTypeID);

type SecKeyCreateSignatureType =
    unsafe extern "C" fn(SecKeyRef, SecKeyAlgorithm, CFDataRef, *mut CFErrorRef) -> CFDataRef;
type SecKeyCopyAttributesType = unsafe extern "C" fn(SecKeyRef) -> CFDictionaryRef;
type SecKeyCopyExternalRepresentationType =
    unsafe extern "C" fn(SecKeyRef, *mut CFErrorRef) -> CFDataRef;
type SecCertificateCopyNormalizedIssuerSequenceType =
    unsafe extern "C" fn(SecCertificateRef) -> CFDataRef;
type SecCertificateCopyNormalizedSubjectSequenceType =
    unsafe extern "C" fn(SecCertificateRef) -> CFDataRef;
type SecCertificateCopyKeyType = unsafe extern "C" fn(SecCertificateRef) -> SecKeyRef;
type SecTrustEvaluateWithErrorType =
    unsafe extern "C" fn(trust: SecTrustRef, error: *mut CFErrorRef) -> bool;

#[derive(Ord, Eq, PartialOrd, PartialEq)]
enum SecStringConstant {
    // These are available in macOS 10.12
    SecKeyAlgorithmECDSASignatureDigestX962SHA1,
    SecKeyAlgorithmECDSASignatureDigestX962SHA256,
    SecKeyAlgorithmECDSASignatureDigestX962SHA384,
    SecKeyAlgorithmECDSASignatureDigestX962SHA512,
    SecKeyAlgorithmRSASignatureDigestPKCS1v15Raw,
    SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
    SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
    SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
    SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
    SecAttrKeyTypeECSECPrimeRandom,
    // These are available in macOS 10.13
    SecKeyAlgorithmRSASignatureDigestPSSSHA1,
    SecKeyAlgorithmRSASignatureDigestPSSSHA256,
    SecKeyAlgorithmRSASignatureDigestPSSSHA384,
    SecKeyAlgorithmRSASignatureDigestPSSSHA512,
}

// NB: This is not meant to be used outside of this module. It has to be made public because
// `RentedSecurityFramework` must be public in the rental declaration.
pub struct SecurityFrameworkFunctions<'a> {
    sec_key_create_signature: Symbol<'a, SecKeyCreateSignatureType>,
    sec_key_copy_attributes: Symbol<'a, SecKeyCopyAttributesType>,
    sec_key_copy_external_representation: Symbol<'a, SecKeyCopyExternalRepresentationType>,
    sec_certificate_copy_normalized_issuer_sequence:
        Symbol<'a, SecCertificateCopyNormalizedIssuerSequenceType>,
    sec_certificate_copy_normalized_subject_sequence:
        Symbol<'a, SecCertificateCopyNormalizedSubjectSequenceType>,
    sec_certificate_copy_key: Symbol<'a, SecCertificateCopyKeyType>,
    sec_trust_evaluate_with_error: Symbol<'a, SecTrustEvaluateWithErrorType>,
    sec_string_constants: BTreeMap<SecStringConstant, String>,
}

rental! {
    mod rent_libloading {
        use super::*;

        #[rental]
        pub struct RentedSecurityFramework {
            library: Box<Library>, // Library needs to be StableDeref, hence the Box
            functions: SecurityFrameworkFunctions<'library>,
        }
    }
}

/// This implementation uses security framework functions and constants that
/// are not provided by the version of the SDK we build with. To work around
/// this, we attempt to open and dynamically load these functions and symbols
/// at runtime. Unfortunately this does mean that if a user is not on a new
/// enough version of macOS, they will not be able to use client certificates
/// from their keychain in Firefox until they upgrade.
struct SecurityFramework {
    rental: Option<rent_libloading::RentedSecurityFramework>,
}

impl SecurityFramework {
    fn new() -> SecurityFramework {
        let library = match Library::new("/System/Library/Frameworks/Security.framework/Security") {
            Ok(library) => library,
            Err(_) => return SecurityFramework { rental: None },
        };
        match rent_libloading::RentedSecurityFramework::try_new::<_, ()>(
            Box::new(library),
            |library| unsafe {
                let sec_key_create_signature = library
                    .get::<SecKeyCreateSignatureType>(b"SecKeyCreateSignature\0")
                    .map_err(|_| ())?;
                let sec_key_copy_attributes = library
                    .get::<SecKeyCopyAttributesType>(b"SecKeyCopyAttributes\0")
                    .map_err(|_| ())?;
                let sec_key_copy_external_representation = library
                    .get::<SecKeyCopyExternalRepresentationType>(
                        b"SecKeyCopyExternalRepresentation\0",
                    )
                    .map_err(|_| ())?;
                let sec_certificate_copy_normalized_issuer_sequence = library
                    .get::<SecCertificateCopyNormalizedIssuerSequenceType>(
                        b"SecCertificateCopyNormalizedIssuerSequence\0",
                    )
                    .map_err(|_| ())?;
                let sec_certificate_copy_normalized_subject_sequence = library
                    .get::<SecCertificateCopyNormalizedSubjectSequenceType>(
                        b"SecCertificateCopyNormalizedSubjectSequence\0",
                    )
                    .map_err(|_| ())?;
                let sec_certificate_copy_key = library
                    .get::<SecCertificateCopyKeyType>(b"SecCertificateCopyKey\0")
                    .map_err(|_| ())?;
                let sec_trust_evaluate_with_error = library
                    .get::<SecTrustEvaluateWithErrorType>(b"SecTrustEvaluateWithError\0")
                    .map_err(|_| ())?;
                let mut sec_string_constants = BTreeMap::new();
                let strings_to_load = vec![
                    (
                        b"kSecKeyAlgorithmECDSASignatureDigestX962SHA1\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA1,
                    ),
                    (
                        b"kSecKeyAlgorithmECDSASignatureDigestX962SHA256\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA256,
                    ),
                    (
                        b"kSecKeyAlgorithmECDSASignatureDigestX962SHA384\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA384,
                    ),
                    (
                        b"kSecKeyAlgorithmECDSASignatureDigestX962SHA512\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA512,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15Raw,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPSSSHA1\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA1,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPSSSHA256\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA256,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPSSSHA384\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA384,
                    ),
                    (
                        b"kSecKeyAlgorithmRSASignatureDigestPSSSHA512\0".as_ref(),
                        SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA512,
                    ),
                    (
                        b"kSecAttrKeyTypeECSECPrimeRandom\0".as_ref(),
                        SecStringConstant::SecAttrKeyTypeECSECPrimeRandom,
                    ),
                ];
                for (symbol_name, sec_string_constant) in strings_to_load {
                    let cfstring_symbol = library
                        .get::<*const CFStringRef>(symbol_name)
                        .map_err(|_| ())?;
                    let cfstring = CFString::wrap_under_create_rule(**cfstring_symbol);
                    sec_string_constants.insert(sec_string_constant, cfstring.to_string());
                }
                Ok(SecurityFrameworkFunctions {
                    sec_key_create_signature,
                    sec_key_copy_attributes,
                    sec_key_copy_external_representation,
                    sec_certificate_copy_normalized_issuer_sequence,
                    sec_certificate_copy_normalized_subject_sequence,
                    sec_certificate_copy_key,
                    sec_trust_evaluate_with_error,
                    sec_string_constants,
                })
            },
        ) {
            Ok(rental) => SecurityFramework {
                rental: Some(rental),
            },
            Err(_) => SecurityFramework { rental: None },
        }
    }

    /// SecKeyCreateSignature is available in macOS 10.12
    fn sec_key_create_signature(
        &self,
        key: &SecKey,
        algorithm: SecKeyAlgorithm,
        data_to_sign: &CFData,
    ) -> Result<CFData, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| unsafe {
                let mut error = std::ptr::null_mut();
                let result = (framework.sec_key_create_signature)(
                    key.as_concrete_TypeRef(),
                    algorithm,
                    data_to_sign.as_concrete_TypeRef(),
                    &mut error,
                );
                if result.is_null() {
                    let error = CFError::wrap_under_create_rule(error);
                    error!("SecKeyCreateSignature failed: {}", error);
                    return Err(());
                }
                Ok(CFData::wrap_under_create_rule(result))
            }),
            None => Err(()),
        }
    }

    /// SecKeyCopyAttributes is available in macOS 10.12
    fn sec_key_copy_attributes<T>(&self, key: &SecKey) -> Result<CFDictionary<CFString, T>, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| unsafe {
                let result = (framework.sec_key_copy_attributes)(key.as_concrete_TypeRef());
                if result.is_null() {
                    error!("SecKeyCopyAttributes failed");
                    return Err(());
                }
                Ok(CFDictionary::wrap_under_create_rule(result))
            }),
            None => Err(()),
        }
    }

    /// SecKeyCopyExternalRepresentation is available in macOS 10.12
    fn sec_key_copy_external_representation(&self, key: &SecKey) -> Result<CFData, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| unsafe {
                let mut error = std::ptr::null_mut();
                let result = (framework.sec_key_copy_external_representation)(
                    key.as_concrete_TypeRef(),
                    &mut error,
                );
                if result.is_null() {
                    let error = CFError::wrap_under_create_rule(error);
                    error!("SecKeyCopyExternalRepresentation failed: {}", error);
                    return Err(());
                }
                Ok(CFData::wrap_under_create_rule(result))
            }),
            None => Err(()),
        }
    }

    /// SecCertificateCopyNormalizedIssuerSequence is available in macOS 10.12.4
    fn sec_certificate_copy_normalized_issuer_sequence(
        &self,
        certificate: &SecCertificate,
    ) -> Result<CFData, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| unsafe {
                let result = (framework.sec_certificate_copy_normalized_issuer_sequence)(
                    certificate.as_concrete_TypeRef(),
                );
                if result.is_null() {
                    error!("SecCertificateCopyNormalizedIssuerSequence failed");
                    return Err(());
                }
                Ok(CFData::wrap_under_create_rule(result))
            }),
            None => Err(()),
        }
    }

    /// SecCertificateCopyNormalizedSubjectSequence is available in macOS 10.12.4
    fn sec_certificate_copy_normalized_subject_sequence(
        &self,
        certificate: &SecCertificate,
    ) -> Result<CFData, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| unsafe {
                let result = (framework.sec_certificate_copy_normalized_subject_sequence)(
                    certificate.as_concrete_TypeRef(),
                );
                if result.is_null() {
                    error!("SecCertificateCopyNormalizedSubjectSequence failed");
                    return Err(());
                }
                Ok(CFData::wrap_under_create_rule(result))
            }),
            None => Err(()),
        }
    }

    /// SecCertificateCopyKey is available in macOS 10.14
    fn sec_certificate_copy_key(&self, certificate: &SecCertificate) -> Result<SecKey, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| unsafe {
                let result =
                    (framework.sec_certificate_copy_key)(certificate.as_concrete_TypeRef());
                if result.is_null() {
                    error!("SecCertificateCopyKey failed");
                    return Err(());
                }
                Ok(SecKey::wrap_under_create_rule(result))
            }),
            None => Err(()),
        }
    }

    /// SecTrustEvaluateWithError is available in macOS 10.14
    fn sec_trust_evaluate_with_error(&self, trust: &SecTrust) -> Result<bool, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| unsafe {
                Ok((framework.sec_trust_evaluate_with_error)(
                    trust.as_concrete_TypeRef(),
                    std::ptr::null_mut(),
                ))
            }),
            None => Err(()),
        }
    }

    fn get_sec_string_constant(
        &self,
        sec_string_constant: SecStringConstant,
    ) -> Result<CFString, ()> {
        match &self.rental {
            Some(rental) => rental.rent(|framework| {
                match framework.sec_string_constants.get(&sec_string_constant) {
                    Some(string) => Ok(CFString::new(string)),
                    None => Err(()),
                }
            }),
            None => Err(()),
        }
    }
}

lazy_static! {
    static ref SECURITY_FRAMEWORK: SecurityFramework = SecurityFramework::new();
}

fn sec_identity_copy_certificate(identity: &SecIdentity) -> Result<SecCertificate, ()> {
    let mut certificate = std::ptr::null();
    let status =
        unsafe { SecIdentityCopyCertificate(identity.as_concrete_TypeRef(), &mut certificate) };
    if status != errSecSuccess {
        error!("SecIdentityCopyCertificate failed: {}", status);
        return Err(());
    }
    if certificate.is_null() {
        error!("couldn't get certificate from identity?");
        return Err(());
    }
    Ok(unsafe { SecCertificate::wrap_under_create_rule(certificate) })
}

fn sec_certificate_copy_subject_summary(certificate: &SecCertificate) -> Result<CFString, ()> {
    let result = unsafe { SecCertificateCopySubjectSummary(certificate.as_concrete_TypeRef()) };
    if result.is_null() {
        error!("SecCertificateCopySubjectSummary failed");
        return Err(());
    }
    Ok(unsafe { CFString::wrap_under_create_rule(result) })
}

fn sec_certificate_copy_data(certificate: &SecCertificate) -> Result<CFData, ()> {
    let result = unsafe { SecCertificateCopyData(certificate.as_concrete_TypeRef()) };
    if result.is_null() {
        error!("SecCertificateCopyData failed");
        return Err(());
    }
    Ok(unsafe { CFData::wrap_under_create_rule(result) })
}

fn sec_identity_copy_private_key(identity: &SecIdentity) -> Result<SecKey, ()> {
    let mut key = std::ptr::null();
    let status = unsafe { SecIdentityCopyPrivateKey(identity.as_concrete_TypeRef(), &mut key) };
    if status != errSecSuccess {
        error!("SecIdentityCopyPrivateKey failed: {}", status);
        return Err(());
    }
    if key.is_null() {
        error!("SecIdentityCopyPrivateKey didn't set key?");
        return Err(());
    }
    Ok(unsafe { SecKey::wrap_under_create_rule(key) })
}

pub struct Cert {
    class: Vec<u8>,
    token: Vec<u8>,
    id: Vec<u8>,
    label: Vec<u8>,
    value: Vec<u8>,
    issuer: Vec<u8>,
    serial_number: Vec<u8>,
    subject: Vec<u8>,
}

impl Cert {
    fn new_from_identity(identity: &SecIdentity) -> Result<Cert, ()> {
        let certificate = sec_identity_copy_certificate(identity)?;
        Cert::new_from_certificate(&certificate)
    }

    fn new_from_certificate(certificate: &SecCertificate) -> Result<Cert, ()> {
        let label = sec_certificate_copy_subject_summary(certificate)?;
        let der = sec_certificate_copy_data(certificate)?;
        let der = der.bytes().to_vec();
        let id = Sha256::digest(&der).to_vec();
        let issuer =
            SECURITY_FRAMEWORK.sec_certificate_copy_normalized_issuer_sequence(certificate)?;
        let serial_number = read_encoded_serial_number(&der)?;
        let subject =
            SECURITY_FRAMEWORK.sec_certificate_copy_normalized_subject_sequence(certificate)?;
        Ok(Cert {
            class: serialize_uint(CKO_CERTIFICATE)?,
            token: serialize_uint(CK_TRUE)?,
            id,
            label: label.to_string().into_bytes(),
            value: der,
            issuer: issuer.bytes().to_vec(),
            serial_number,
            subject: subject.bytes().to_vec(),
        })
    }

    fn class(&self) -> &[u8] {
        &self.class
    }

    fn token(&self) -> &[u8] {
        &self.token
    }

    pub fn id(&self) -> &[u8] {
        &self.id
    }

    fn label(&self) -> &[u8] {
        &self.label
    }

    fn value(&self) -> &[u8] {
        &self.value
    }

    fn issuer(&self) -> &[u8] {
        &self.issuer
    }

    fn serial_number(&self) -> &[u8] {
        &self.serial_number
    }

    fn subject(&self) -> &[u8] {
        &self.subject
    }

    fn matches(&self, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool {
        for (attr_type, attr_value) in attrs {
            let comparison = match *attr_type {
                CKA_CLASS => self.class(),
                CKA_TOKEN => self.token(),
                CKA_LABEL => self.label(),
                CKA_ID => self.id(),
                CKA_VALUE => self.value(),
                CKA_ISSUER => self.issuer(),
                CKA_SERIAL_NUMBER => self.serial_number(),
                CKA_SUBJECT => self.subject(),
                _ => return false,
            };
            if attr_value.as_slice() != comparison {
                return false;
            }
        }
        true
    }

    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]> {
        let result = match attribute {
            CKA_CLASS => self.class(),
            CKA_TOKEN => self.token(),
            CKA_LABEL => self.label(),
            CKA_ID => self.id(),
            CKA_VALUE => self.value(),
            CKA_ISSUER => self.issuer(),
            CKA_SERIAL_NUMBER => self.serial_number(),
            CKA_SUBJECT => self.subject(),
            _ => return None,
        };
        Some(result)
    }
}

#[derive(Clone, Copy, Debug)]
pub enum KeyType {
    EC(usize),
    RSA,
}

enum SignParams<'a> {
    EC(CFString, &'a [u8]),
    RSA(CFString, &'a [u8]),
}

impl<'a> SignParams<'a> {
    fn new(
        key_type: KeyType,
        data: &'a [u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<SignParams<'a>, ()> {
        match key_type {
            KeyType::EC(_) => SignParams::new_ec_params(data),
            KeyType::RSA => SignParams::new_rsa_params(params, data),
        }
    }

    fn new_ec_params(data: &'a [u8]) -> Result<SignParams<'a>, ()> {
        let algorithm_id = match data.len() {
            20 => SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA1,
            32 => SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA256,
            48 => SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA384,
            64 => SecStringConstant::SecKeyAlgorithmECDSASignatureDigestX962SHA512,
            _ => {
                error!(
                    "Unexpected digested signature input length for ECDSA: {}",
                    data.len()
                );
                return Err(());
            }
        };
        let algorithm = SECURITY_FRAMEWORK.get_sec_string_constant(algorithm_id)?;
        Ok(SignParams::EC(algorithm, data))
    }

    fn new_rsa_params(
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
        data: &'a [u8],
    ) -> Result<SignParams<'a>, ()> {
        if let Some(pss_params) = params {
            let algorithm = {
                let algorithm_id = match pss_params.hashAlg {
                    CKM_SHA_1 => SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA1,
                    CKM_SHA256 => SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA256,
                    CKM_SHA384 => SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA384,
                    CKM_SHA512 => SecStringConstant::SecKeyAlgorithmRSASignatureDigestPSSSHA512,
                    _ => {
                        error!(
                            "unsupported algorithm to use with RSA-PSS: {}",
                            unsafe_packed_field_access!(pss_params.hashAlg)
                        );
                        return Err(());
                    }
                };
                SECURITY_FRAMEWORK.get_sec_string_constant(algorithm_id)?
            };
            return Ok(SignParams::RSA(algorithm, data));
        }

        // Handle the case where this is a TLS 1.0 MD5/SHA1 hash.
        if data.len() == 36 {
            let algorithm_id = SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15Raw;
            let algorithm = SECURITY_FRAMEWORK.get_sec_string_constant(algorithm_id)?;
            return Ok(SignParams::RSA(algorithm, data));
        }
        // Otherwise, `data` should be a DigestInfo.
        let (digest_oid, hash) = read_digest_info(data)?;
        let algorithm_id = if digest_oid == OID_BYTES_SHA_256 {
            SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256
        } else if digest_oid == OID_BYTES_SHA_384 {
            SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384
        } else if digest_oid == OID_BYTES_SHA_512 {
            SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512
        } else if digest_oid == OID_BYTES_SHA_1 {
            SecStringConstant::SecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1
        } else {
            error!("unsupported digest algorithm: {:?}", digest_oid);
            return Err(());
        };

        Ok(SignParams::RSA(
            SECURITY_FRAMEWORK.get_sec_string_constant(algorithm_id)?,
            hash,
        ))
    }

    fn get_algorithm(&self) -> SecKeyAlgorithm {
        match self {
            SignParams::EC(algorithm, _) => algorithm.as_concrete_TypeRef(),
            SignParams::RSA(algorithm, _) => algorithm.as_concrete_TypeRef(),
        }
    }

    fn get_data_to_sign(&self) -> &'a [u8] {
        match self {
            SignParams::EC(_, data_to_sign) => data_to_sign,
            SignParams::RSA(_, data_to_sign) => data_to_sign,
        }
    }
}

pub struct Key {
    identity: SecIdentity,
    class: Vec<u8>,
    token: Vec<u8>,
    id: Vec<u8>,
    private: Vec<u8>,
    key_type: Vec<u8>,
    modulus: Option<Vec<u8>>,
    ec_params: Option<Vec<u8>>,
    key_type_enum: KeyType,
    key_handle: Option<SecKey>,
}

impl Key {
    fn new(identity: &SecIdentity) -> Result<Key, ()> {
        let certificate = sec_identity_copy_certificate(identity)?;
        let der = sec_certificate_copy_data(&certificate)?;
        let id = Sha256::digest(der.bytes()).to_vec();
        let key = SECURITY_FRAMEWORK.sec_certificate_copy_key(&certificate)?;
        let key_type: CFString = get_key_attribute(&key, unsafe { kSecAttrKeyType })?;
        let key_size_in_bits: CFNumber = get_key_attribute(&key, unsafe { kSecAttrKeySizeInBits })?;
        let mut modulus = None;
        let mut ec_params = None;
        let sec_attr_key_type_ec = SECURITY_FRAMEWORK
            .get_sec_string_constant(SecStringConstant::SecAttrKeyTypeECSECPrimeRandom)?;
        let (key_type_enum, key_type_attribute) =
            if key_type.as_concrete_TypeRef() == unsafe { kSecAttrKeyTypeRSA } {
                let public_key = SECURITY_FRAMEWORK.sec_key_copy_external_representation(&key)?;
                let modulus_value = read_rsa_modulus(public_key.bytes())?;
                modulus = Some(modulus_value);
                (KeyType::RSA, CKK_RSA)
            } else if key_type == sec_attr_key_type_ec {
                // Assume all EC keys are secp256r1, secp384r1, or secp521r1. This
                // is wrong, but the API doesn't seem to give us a way to determine
                // which curve this key is on.
                // This might not matter in practice, because it seems all NSS uses
                // this for is to get the signature size.
                let key_size_in_bits = match key_size_in_bits.to_i64() {
                    Some(value) => value,
                    None => return Err(()),
                };
                match key_size_in_bits {
                    256 => ec_params = Some(ENCODED_OID_BYTES_SECP256R1.to_vec()),
                    384 => ec_params = Some(ENCODED_OID_BYTES_SECP384R1.to_vec()),
                    521 => ec_params = Some(ENCODED_OID_BYTES_SECP521R1.to_vec()),
                    _ => {
                        error!("unsupported EC key");
                        return Err(());
                    }
                }
                let coordinate_width = (key_size_in_bits as usize + 7) / 8;
                (KeyType::EC(coordinate_width), CKK_EC)
            } else {
                error!("unsupported key type");
                return Err(());
            };

        Ok(Key {
            identity: identity.clone(),
            class: serialize_uint(CKO_PRIVATE_KEY)?,
            token: serialize_uint(CK_TRUE)?,
            id,
            private: serialize_uint(CK_TRUE)?,
            key_type: serialize_uint(key_type_attribute)?,
            modulus,
            ec_params,
            key_type_enum,
            key_handle: None,
        })
    }

    fn class(&self) -> &[u8] {
        &self.class
    }

    fn token(&self) -> &[u8] {
        &self.token
    }

    pub fn id(&self) -> &[u8] {
        &self.id
    }

    fn private(&self) -> &[u8] {
        &self.private
    }

    fn key_type(&self) -> &[u8] {
        &self.key_type
    }

    fn modulus(&self) -> Option<&[u8]> {
        match &self.modulus {
            Some(modulus) => Some(modulus.as_slice()),
            None => None,
        }
    }

    fn ec_params(&self) -> Option<&[u8]> {
        match &self.ec_params {
            Some(ec_params) => Some(ec_params.as_slice()),
            None => None,
        }
    }

    fn matches(&self, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool {
        for (attr_type, attr_value) in attrs {
            let comparison = match *attr_type {
                CKA_CLASS => self.class(),
                CKA_TOKEN => self.token(),
                CKA_ID => self.id(),
                CKA_PRIVATE => self.private(),
                CKA_KEY_TYPE => self.key_type(),
                CKA_MODULUS => {
                    if let Some(modulus) = self.modulus() {
                        modulus
                    } else {
                        return false;
                    }
                }
                CKA_EC_PARAMS => {
                    if let Some(ec_params) = self.ec_params() {
                        ec_params
                    } else {
                        return false;
                    }
                }
                _ => return false,
            };
            if attr_value.as_slice() != comparison {
                return false;
            }
        }
        true
    }

    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]> {
        match attribute {
            CKA_CLASS => Some(self.class()),
            CKA_TOKEN => Some(self.token()),
            CKA_ID => Some(self.id()),
            CKA_PRIVATE => Some(self.private()),
            CKA_KEY_TYPE => Some(self.key_type()),
            CKA_MODULUS => self.modulus(),
            CKA_EC_PARAMS => self.ec_params(),
            _ => None,
        }
    }

    pub fn get_signature_length(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<usize, ()> {
        // Unfortunately we don't have a way of getting the length of a signature without creating
        // one.
        let dummy_signature_bytes = self.sign(data, params)?;
        Ok(dummy_signature_bytes.len())
    }

    // The input data is a hash. What algorithm we use depends on the size of the hash.
    pub fn sign(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<Vec<u8>, ()> {
        // If this key hasn't been used for signing yet, there won't be a cached key handle. Obtain
        // and cache it if this is the case. Doing so can cause the underlying implementation to
        // show an authentication or pin prompt to the user. Caching the handle can avoid causing
        // multiple prompts to be displayed in some cases.
        if self.key_handle.is_none() {
            let _ = self
                .key_handle
                .replace(sec_identity_copy_private_key(&self.identity)?);
        }
        let key = match &self.key_handle {
            Some(key) => key,
            None => {
                error!("key_handle not set when it should have just been set?");
                return Err(());
            }
        };
        let sign_params = SignParams::new(self.key_type_enum, data, params)?;
        let signing_algorithm = sign_params.get_algorithm();
        let data_to_sign = CFData::from_buffer(sign_params.get_data_to_sign());
        let signature =
            SECURITY_FRAMEWORK.sec_key_create_signature(&key, signing_algorithm, &data_to_sign)?;
        let signature_value = match self.key_type_enum {
            KeyType::EC(coordinate_width) => {
                // We need to convert the DER Ecdsa-Sig-Value to the
                // concatenation of r and s, the coordinates of the point on
                // the curve. r and s must be 0-padded to be coordinate_width
                // total bytes.
                let (r, s) = read_ec_sig_point(signature.bytes())?;
                if r.len() > coordinate_width || s.len() > coordinate_width {
                    return Err(());
                }
                let mut signature_value = Vec::with_capacity(2 * coordinate_width);
                let r_padding = vec![0; coordinate_width - r.len()];
                signature_value.extend(r_padding);
                signature_value.extend_from_slice(r);
                let s_padding = vec![0; coordinate_width - s.len()];
                signature_value.extend(s_padding);
                signature_value.extend_from_slice(s);
                signature_value
            }
            KeyType::RSA => signature.bytes().to_vec(),
        };
        Ok(signature_value)
    }
}

pub enum Object {
    Cert(Cert),
    Key(Key),
}

impl Object {
    pub fn matches(&self, slot_type: SlotType, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool {
        // The modern/legacy slot distinction in theory enables differentiation
        // between keys that are from modules that can use modern cryptography
        // (namely EC keys and RSA-PSS signatures) and those that cannot.
        // However, the function that would enable this
        // (SecKeyIsAlgorithmSupported) causes a password dialog to appear on
        // our test machines, so this backend pretends that everything supports
        // modern crypto for now.
        if slot_type != SlotType::Modern {
            return false;
        }
        match self {
            Object::Cert(cert) => cert.matches(attrs),
            Object::Key(key) => key.matches(attrs),
        }
    }

    pub fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]> {
        match self {
            Object::Cert(cert) => cert.get_attribute(attribute),
            Object::Key(key) => key.get_attribute(attribute),
        }
    }
}

pub const SUPPORTED_ATTRIBUTES: &[CK_ATTRIBUTE_TYPE] = &[
    CKA_CLASS,
    CKA_TOKEN,
    CKA_LABEL,
    CKA_ID,
    CKA_VALUE,
    CKA_ISSUER,
    CKA_SERIAL_NUMBER,
    CKA_SUBJECT,
    CKA_PRIVATE,
    CKA_KEY_TYPE,
    CKA_MODULUS,
    CKA_EC_PARAMS,
];

fn get_key_attribute<T: TCFType + Clone>(key: &SecKey, attr: CFStringRef) -> Result<T, ()> {
    let attributes: CFDictionary<CFString, T> = SECURITY_FRAMEWORK.sec_key_copy_attributes(&key)?;
    match attributes.find(attr as *const _) {
        Some(value) => Ok((*value).clone()),
        None => Err(()),
    }
}

// Given a SecIdentity, attempts to build as much of a path to a trust anchor as possible, gathers
// the CA certificates from that path, and returns them. The purpose of this function is not to
// validate the given certificate but to find CA certificates that gecko may need to do path
// building when filtering client certificates according to the acceptable CA list sent by the
// server during client authentication.
fn get_issuers(identity: &SecIdentity) -> Result<Vec<SecCertificate>, ()> {
    let certificate = sec_identity_copy_certificate(identity)?;
    let policy = unsafe { SecPolicyCreateSSL(false, std::ptr::null()) };
    if policy.is_null() {
        error!("SecPolicyCreateSSL failed");
        return Err(());
    }
    let policy = unsafe { SecPolicy::wrap_under_create_rule(policy) };
    let mut trust = std::ptr::null();
    // Each of SecTrustCreateWithCertificates' input arguments can be either single items or an
    // array of items. Since we only want to specify one of each, we directly specify the arguments.
    let status = unsafe {
        SecTrustCreateWithCertificates(
            certificate.as_concrete_TypeRef(),
            policy.as_concrete_TypeRef(),
            &mut trust,
        )
    };
    if status != errSecSuccess {
        error!("SecTrustCreateWithCertificates failed: {}", status);
        return Err(());
    }
    if trust.is_null() {
        error!("trust is null?");
        return Err(());
    }
    let trust = unsafe { SecTrust::wrap_under_create_rule(trust) };
    // Disable AIA fetching so that SecTrustEvaluateWithError doesn't result in network I/O.
    let status = unsafe { SecTrustSetNetworkFetchAllowed(trust.as_concrete_TypeRef(), 0) };
    if status != errSecSuccess {
        error!("SecTrustSetNetworkFetchAllowed failed: {}", status);
        return Err(());
    }
    // We ignore the return value here because we don't care if the certificate is trusted or not -
    // we're only doing this to build its issuer chain as much as possible.
    let _ = SECURITY_FRAMEWORK.sec_trust_evaluate_with_error(&trust)?;
    let certificate_count = unsafe { SecTrustGetCertificateCount(trust.as_concrete_TypeRef()) };
    let mut certificates = Vec::with_capacity(certificate_count.try_into().map_err(|_| ())?);
    for i in 1..certificate_count {
        let certificate = unsafe { SecTrustGetCertificateAtIndex(trust.as_concrete_TypeRef(), i) };
        if certificate.is_null() {
            error!("SecTrustGetCertificateAtIndex returned null certificate?");
            continue;
        }
        let certificate = unsafe { SecCertificate::wrap_under_get_rule(certificate) };
        certificates.push(certificate);
    }
    Ok(certificates)
}

pub fn list_objects() -> Vec<Object> {
    let mut objects = Vec::new();
    let identities = unsafe {
        let class_key = CFString::wrap_under_get_rule(kSecClass);
        let class_value = CFString::wrap_under_get_rule(kSecClassIdentity);
        let return_ref_key = CFString::wrap_under_get_rule(kSecReturnRef);
        let return_ref_value = CFBoolean::wrap_under_get_rule(kCFBooleanTrue);
        let match_key = CFString::wrap_under_get_rule(kSecMatchLimit);
        let match_value = CFString::wrap_under_get_rule(kSecMatchLimitAll);
        let vals = vec![
            (class_key.as_CFType(), class_value.as_CFType()),
            (return_ref_key.as_CFType(), return_ref_value.as_CFType()),
            (match_key.as_CFType(), match_value.as_CFType()),
        ];
        let dict = CFDictionary::from_CFType_pairs(&vals);
        let mut result = std::ptr::null();
        let status = SecItemCopyMatching(dict.as_CFTypeRef() as CFDictionaryRef, &mut result);
        if status != errSecSuccess {
            error!("SecItemCopyMatching failed: {}", status);
            return objects;
        }
        if result.is_null() {
            debug!("no client certs?");
            return objects;
        }
        CFArray::<SecIdentityRef>::wrap_under_create_rule(result as CFArrayRef)
    };
    for identity in identities.get_all_values().iter() {
        let identity = unsafe { SecIdentity::wrap_under_get_rule(*identity as SecIdentityRef) };
        let cert = Cert::new_from_identity(&identity);
        let key = Key::new(&identity);
        if let (Ok(cert), Ok(key)) = (cert, key) {
            objects.push(Object::Cert(cert));
            objects.push(Object::Key(key));
        } else {
            continue;
        }
        if let Ok(issuers) = get_issuers(&identity) {
            for issuer in issuers {
                if let Ok(cert) = Cert::new_from_certificate(&issuer) {
                    objects.push(Object::Cert(cert));
                }
            }
        }
    }
    objects
}
