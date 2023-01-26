// Extensions
export const id_SubjectDirectoryAttributes = "2.5.29.9";
export const id_SubjectKeyIdentifier = "2.5.29.14";
export const id_KeyUsage = "2.5.29.15";
export const id_PrivateKeyUsagePeriod = "2.5.29.16";
export const id_SubjectAltName = "2.5.29.17";
export const id_IssuerAltName = "2.5.29.18";
export const id_BasicConstraints = "2.5.29.19";
export const id_CRLNumber = "2.5.29.20";
export const id_BaseCRLNumber = "2.5.29.27"; // BaseCRLNumber (delta CRL indicator)
export const id_CRLReason = "2.5.29.21";
export const id_InvalidityDate = "2.5.29.24";
export const id_IssuingDistributionPoint = "2.5.29.28";
export const id_CertificateIssuer = "2.5.29.29";
export const id_NameConstraints = "2.5.29.30";
export const id_CRLDistributionPoints = "2.5.29.31";
export const id_FreshestCRL = "2.5.29.46";
export const id_CertificatePolicies = "2.5.29.32";
export const id_AnyPolicy = "2.5.29.32.0";
export const id_MicrosoftAppPolicies = "1.3.6.1.4.1.311.21.10"; // szOID_APPLICATION_CERT_POLICIES - Microsoft-specific OID
export const id_PolicyMappings = "2.5.29.33";
export const id_AuthorityKeyIdentifier = "2.5.29.35";
export const id_PolicyConstraints = "2.5.29.36";
export const id_ExtKeyUsage = "2.5.29.37";
export const id_InhibitAnyPolicy = "2.5.29.54";
export const id_AuthorityInfoAccess = "1.3.6.1.5.5.7.1.1";
export const id_SubjectInfoAccess = "1.3.6.1.5.5.7.1.11";
export const id_SignedCertificateTimestampList = "1.3.6.1.4.1.11129.2.4.2";
export const id_MicrosoftCertTemplateV1 = "1.3.6.1.4.1.311.20.2"; // szOID_ENROLL_CERTTYPE_EXTENSION - Microsoft-specific extension
export const id_MicrosoftPrevCaCertHash = "1.3.6.1.4.1.311.21.2"; // szOID_CERTSRV_PREVIOUS_CERT_HASH - Microsoft-specific extension
export const id_MicrosoftCertTemplateV2 = "1.3.6.1.4.1.311.21.7"; // szOID_CERTIFICATE_TEMPLATE - Microsoft-specific extension
export const id_MicrosoftCaVersion = "1.3.6.1.4.1.311.21.1"; // szOID_CERTSRV_CA_VERSION - Microsoft-specific extension
export const id_QCStatements = "1.3.6.1.5.5.7.1.3";

// ContentType
export const id_ContentType_Data = "1.2.840.113549.1.7.1";
export const id_ContentType_SignedData = "1.2.840.113549.1.7.2";
export const id_ContentType_EnvelopedData = "1.2.840.113549.1.7.3";
export const id_ContentType_EncryptedData = "1.2.840.113549.1.7.6";

export const id_eContentType_TSTInfo = "1.2.840.113549.1.9.16.1.4";

// CertBag
export const id_CertBag_X509Certificate = "1.2.840.113549.1.9.22.1";
export const id_CertBag_SDSICertificate = "1.2.840.113549.1.9.22.2";
export const id_CertBag_AttributeCertificate = "1.2.840.113549.1.9.22.3";

// CRLBag
export const id_CRLBag_X509CRL = "1.2.840.113549.1.9.23.1";

export const id_pkix = "1.3.6.1.5.5.7";
export const id_ad = `${id_pkix}.48`;

export const id_PKIX_OCSP_Basic = `${id_ad}.1.1`;

export const id_ad_caIssuers = `${id_ad}.2`;
export const id_ad_ocsp = `${id_ad}.1`;

// Algorithms

export const id_sha1 = "1.3.14.3.2.26";
export const id_sha256 = "2.16.840.1.101.3.4.2.1";
export const id_sha384 = "2.16.840.1.101.3.4.2.2";
export const id_sha512 = "2.16.840.1.101.3.4.2.3";
