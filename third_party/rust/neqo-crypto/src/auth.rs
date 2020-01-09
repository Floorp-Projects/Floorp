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
    #[must_use]
    fn into(self) -> PRErrorCode {
        match self {
            Self::Ok => 0,
            Self::CaInvalid => sec::SEC_ERROR_CA_CERT_INVALID,
            Self::CaNotV3 => mozpkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA,
            Self::CertAlgorithmDisabled => sec::SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED,
            Self::CertExpired => sec::SEC_ERROR_EXPIRED_CERTIFICATE,
            Self::CertInvalidTime => sec::SEC_ERROR_INVALID_TIME,
            Self::CertIsCa => mozpkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY,
            Self::CertKeyUsage => sec::SEC_ERROR_INADEQUATE_KEY_USAGE,
            Self::CertMitm => mozpkix::MOZILLA_PKIX_ERROR_MITM_DETECTED,
            Self::CertNotYetValid => mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE,
            Self::CertRevoked => sec::SEC_ERROR_REVOKED_CERTIFICATE,
            Self::CertSelfSigned => mozpkix::MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT,
            Self::CertSubjectInvalid => ssl::SSL_ERROR_BAD_CERT_DOMAIN,
            Self::CertUntrusted => sec::SEC_ERROR_UNTRUSTED_CERT,
            Self::CertWeakKey => mozpkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE,
            Self::IssuerEmptyName => mozpkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME,
            Self::IssuerExpired => sec::SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE,
            Self::IssuerNotYetValid => mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE,
            Self::IssuerUnknown => sec::SEC_ERROR_UNKNOWN_ISSUER,
            Self::IssuerUntrusted => sec::SEC_ERROR_UNTRUSTED_ISSUER,
            Self::PolicyRejection => {
                mozpkix::MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED
            }
            Self::Unknown => sec::SEC_ERROR_LIBRARY_FAILURE,
        }
    }
}

// Note that this mapping should be removed after gecko eventually learns how to
// map into the enumerated type.
impl From<PRErrorCode> for AuthenticationStatus {
    #[must_use]
    fn from(v: PRErrorCode) -> Self {
        match v {
            0 => Self::Ok,
            sec::SEC_ERROR_CA_CERT_INVALID => Self::CaInvalid,
            mozpkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA => Self::CaNotV3,
            sec::SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED => Self::CertAlgorithmDisabled,
            sec::SEC_ERROR_EXPIRED_CERTIFICATE => Self::CertExpired,
            sec::SEC_ERROR_INVALID_TIME => Self::CertInvalidTime,
            mozpkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY => Self::CertIsCa,
            sec::SEC_ERROR_INADEQUATE_KEY_USAGE => Self::CertKeyUsage,
            mozpkix::MOZILLA_PKIX_ERROR_MITM_DETECTED => Self::CertMitm,
            mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE => Self::CertNotYetValid,
            sec::SEC_ERROR_REVOKED_CERTIFICATE => Self::CertRevoked,
            mozpkix::MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT => Self::CertSelfSigned,
            ssl::SSL_ERROR_BAD_CERT_DOMAIN => Self::CertSubjectInvalid,
            sec::SEC_ERROR_UNTRUSTED_CERT => Self::CertUntrusted,
            mozpkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE => Self::CertWeakKey,
            mozpkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME => Self::IssuerEmptyName,
            sec::SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE => Self::IssuerExpired,
            mozpkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE => Self::IssuerNotYetValid,
            sec::SEC_ERROR_UNKNOWN_ISSUER => Self::IssuerUnknown,
            sec::SEC_ERROR_UNTRUSTED_ISSUER => Self::IssuerUntrusted,
            mozpkix::MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED => {
                Self::PolicyRejection
            }
            _ => Self::Unknown,
        }
    }
}
