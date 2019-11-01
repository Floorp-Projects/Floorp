// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::err::{mozpkix, sec, ssl, PRErrorCode};

/// The outcome of authentication.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum AuthenticationStatus {
    Ok,
    CaInvalid,
    CaNotV3,
    CertAlgorithmDisabled,
    CertExpired,
    CertInvalidTime,
    CertIsCa,
    CertKeyUsage,
    CertMitm,
    CertNotYetValid,
    CertRevoked,
    CertSelfSigned,
    CertSubjectInvalid,
    CertUntrusted,
    CertWeakKey,
    IssuerEmptyName,
    IssuerExpired,
    IssuerNotYetValid,
    IssuerUnknown,
    IssuerUntrusted,
    PolicyRejection,
    Unknown,
}

impl Into<PRErrorCode> for AuthenticationStatus {
    fn into(self) -> PRErrorCode {
        match self {
            AuthenticationStatus::Ok => 0,
            AuthenticationStatus::CaInvalid => sec::SEC_ERROR_CA_CERT_INVALID,
            AuthenticationStatus::CaNotV3 => mozpkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA,
            AuthenticationStatus::CertAlgorithmDisabled => {
                sec::SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
            }
            AuthenticationStatus::CertExpired => sec::SEC_ERROR_EXPIRED_CERTIFICATE,
            AuthenticationStatus::CertInvalidTime => sec::SEC_ERROR_INVALID_TIME,
            AuthenticationStatus::CertIsCa => {
                mozpkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY
            }
            AuthenticationStatus::CertKeyUsage => sec::SEC_ERROR_INADEQUATE_KEY_USAGE,
            AuthenticationStatus::CertMitm => mozpkix::MOZILLA_PKIX_ERROR_MITM_DETECTED,
            AuthenticationStatus::CertNotYetValid => {
                mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE
            }
            AuthenticationStatus::CertRevoked => sec::SEC_ERROR_REVOKED_CERTIFICATE,
            AuthenticationStatus::CertSelfSigned => mozpkix::MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT,
            AuthenticationStatus::CertSubjectInvalid => ssl::SSL_ERROR_BAD_CERT_DOMAIN,
            AuthenticationStatus::CertUntrusted => sec::SEC_ERROR_UNTRUSTED_CERT,
            AuthenticationStatus::CertWeakKey => mozpkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE,
            AuthenticationStatus::IssuerEmptyName => mozpkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME,
            AuthenticationStatus::IssuerExpired => sec::SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE,
            AuthenticationStatus::IssuerNotYetValid => {
                mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE
            }
            AuthenticationStatus::IssuerUnknown => sec::SEC_ERROR_UNKNOWN_ISSUER,
            AuthenticationStatus::IssuerUntrusted => sec::SEC_ERROR_UNTRUSTED_ISSUER,
            AuthenticationStatus::PolicyRejection => {
                mozpkix::MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED
            }
            AuthenticationStatus::Unknown => sec::SEC_ERROR_LIBRARY_FAILURE,
        }
    }
}

// Note that this mapping should be removed after gecko eventually learns how to
// map into the enumerated type.
impl From<PRErrorCode> for AuthenticationStatus {
    fn from(v: PRErrorCode) -> Self {
        match v {
            0 => AuthenticationStatus::Ok,
            sec::SEC_ERROR_CA_CERT_INVALID => AuthenticationStatus::CaInvalid,
            mozpkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA => AuthenticationStatus::CaNotV3,
            sec::SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED => {
                AuthenticationStatus::CertAlgorithmDisabled
            }
            sec::SEC_ERROR_EXPIRED_CERTIFICATE => AuthenticationStatus::CertExpired,
            sec::SEC_ERROR_INVALID_TIME => AuthenticationStatus::CertInvalidTime,
            mozpkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY => {
                AuthenticationStatus::CertIsCa
            }
            sec::SEC_ERROR_INADEQUATE_KEY_USAGE => AuthenticationStatus::CertKeyUsage,
            mozpkix::MOZILLA_PKIX_ERROR_MITM_DETECTED => AuthenticationStatus::CertMitm,
            mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE => {
                AuthenticationStatus::CertNotYetValid
            }
            sec::SEC_ERROR_REVOKED_CERTIFICATE => AuthenticationStatus::CertRevoked,
            mozpkix::MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT => AuthenticationStatus::CertSelfSigned,
            ssl::SSL_ERROR_BAD_CERT_DOMAIN => AuthenticationStatus::CertSubjectInvalid,
            sec::SEC_ERROR_UNTRUSTED_CERT => AuthenticationStatus::CertUntrusted,
            mozpkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE => AuthenticationStatus::CertWeakKey,
            mozpkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME => AuthenticationStatus::IssuerEmptyName,
            sec::SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE => AuthenticationStatus::IssuerExpired,
            mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE => {
                AuthenticationStatus::IssuerNotYetValid
            }
            sec::SEC_ERROR_UNKNOWN_ISSUER => AuthenticationStatus::IssuerUnknown,
            sec::SEC_ERROR_UNTRUSTED_ISSUER => AuthenticationStatus::IssuerUntrusted,
            mozpkix::MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED => {
                AuthenticationStatus::PolicyRejection
            }
            _ => AuthenticationStatus::Unknown,
        }
    }
}
