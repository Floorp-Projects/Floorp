## DESCRIPTION OF THE PROJECT

PKIjs designed to be a helper for everyone making any PKI-related applications. 
Currently PKI defined by a set of documents, and usually these documents have form in RFC (Requst For Comments) managed by IETF.
PKIjs respects this situation and provide to user a flexible environment based on existing set of RFCs, related to PKI.

| RFC number     | RFC name       |
|----------------|----------------|
|RFC5280|Internet X.509 Public Key Infrastructure Certificate and Certificate Revocation List (CRL) Profile|
|RFC3161|Internet X.509 Public Key Infrastructure Time-Stamp Protocol (TSP)|
|RFC5652|Cryptographic Message Syntax (CMS)|
|RFC3447|Public-Key Cryptography Standards (PKCS) #1: RSA Cryptography Specifications Version 2.1|
|RFC5753|Use of Elliptic Curve Cryptography (ECC) Algorithms in Cryptographic Message Syntax (CMS)|
|RFC2898|PKCS #5: Password-Based Cryptography Specification|
|RFC6960|X.509 Internet Public Key Infrastructure Online Certificate Status Protocol - OCSP|
|RFC2986|PKCS #10: Certification Request Syntax Specification Version 1.7|
|RFC7292|PKCS #12: Personal Information Exchange Syntax v1.1|
|RFC6318|Suite B in Secure/Multipurpose Internet Mail Extensions (S/MIME)|
|RFC5915|Elliptic Curve Private Key Structure|
|RFC5480|Elliptic Curve Cryptography Subject Public Key Information|
|RFC5208|Public-Key Cryptography Standards (PKCS) #8: Private-Key Information Syntax Specification Version 1.2|
|RFC4055|Additional Algorithms and Identifiers for RSA Cryptography for use in the Internet X.509 Public Key Infrastructure Certificate and Certificate Revocation List (CRL) Profile|

PKI.js made of many specialized classes, each of them is responsible for handling one structure from specific RFC. For example, in order to handle X.509 certificate, described in RFC5280, PKI.js has class "Certificate". Each class inside PKI.js is inside separate file.
Name of each class equals to name from RFC document. Here is a table with PKI.js class names and related RFCs:
 
| Class Name     | RFC number     |
|----------------|----------------|
|AccessDescription|RFC5280|
|Accuracy|RFC3161|
|AlgorithmIdentifier|RFC5280|
|AltName|RFC5280|
|Attribute|RFC2986|
|AttributeTypeAndValue|RFC5280|
|AuthenticatedSafe|RFC7292|
|AuthorityKeyIdentifier|RFC5280|
|BasicConstraints|RFC5280|
|BasicOCSPResponse|RFC6960|
|CRLBag|RFC7292|
|CRLDistributionPoints|RFC5280|
|CertBag|RFC7292|
|CertID|RFC6960|
|Certificate|RFC5280|
|CertificatePolicies|RFC5280|
|CertificateRevocationList|RFC5280|
|CertificateSet|RFC5652|
|CertificationRequest|RFC2986|
|ContentInfo|RFC5652|
|DigestInfo|RFC3447|
|DistributionPoint|RFC5280|
|ECCCMSSharedInfo|RFC6318|
|ECPrivateKey|RFC5915|
|ECPublicKey|RFC5480|
|EncapsulatedContentInfo|RFC5652|
|EncryptedContentInfo|RFC5652|
|EncryptedData|RFC5652|
|EnvelopedData|RFC5652|
|ExtKeyUsage|RFC5280|
|Extension|RFC5280|
|Extensions|RFC5280|
|GeneralName|RFC5280|
|GeneralNames|RFC5280|
|GeneralSubtree|RFC5280|
|InfoAccess|RFC5280|
|IssuerAndSerialNumber|RFC5652|
|IssuingDistributionPoint|RFC5280|
|KEKIdentifier|RFC5652|
|KEKRecipientInfo|RFC5652|
|KeyAgreeRecipientIdentifier|RFC5652|
|KeyAgreeRecipientInfo|RFC5652|
|KeyBag|RFC5208|
|KeyTransRecipientInfo|RFC5652|
|MacData|RFC7292|
|MessageImprint|RFC3161|
|NameConstraints|RFC5280|
|OCSPRequest|RFC6960|
|OCSPResponse|RFC6960|
|OriginatorIdentifierOrKey|RFC5652|
|OriginatorInfo|RFC5652|
|OriginatorPublicKey|RFC5652|
|OtherCertificateFormat|RFC5652|
|OtherKeyAttribute|RFC5652|
|OtherPrimeInfo|RFC3447|
|OtherRecipientInfo|RFC5652|
|OtherRevocationInfoFormat|RFC5652|
|PBES2Params|RFC2898|
|PBKDF2Params|RFC2898|
|PFX|RFC7292|
|PKCS8ShroudedKeyBag|RFC7292|
|PKIStatusInfo|RFC3161|
|PasswordRecipientinfo|RFC5652|
|PolicyConstraints|RFC5280|
|PolicyInformation|RFC5280|
|PolicyMapping|RFC5280|
|PolicyMappings|RFC5280|
|PolicyQualifierInfo|RFC5280|
|PrivateKeyInfo|RFC5208|
|PrivateKeyUsagePeriod|RFC5280|
|PublicKeyInfo|RFC5280|
|RSAESOAEPParams|RFC3447|
|RSAPrivateKey|RFC3447|
|RSAPublicKey|RFC3447|
|RSASSAPSSParams|RFC4055|
|RecipientEncryptedKey|RFC5652|
|RecipientEncryptedKeys|RFC5652|
|RecipientIdentifier|RFC5652|
|RecipientInfo|RFC5652|
|RecipientKeyIdentifier|RFC5652|
|RelativeDistinguishedNames|RFC5280|
|Request|RFC6960|
|ResponseBytes|RFC6960|
|ResponseData|RFC6960|
|RevocationInfoChoices|RFC5652|
|RevokedCertificate|RFC5280|
|SafeBag|RFC7292|
|SafeContents|RFC7292|
|SecretBag|RFC7292|
|Signature|RFC6960|
|SignedAndUnsignedAttributes|RFC5652|
|SignedData|RFC5652|
|SignerInfo|RFC5652|
|SingleResponse|RFC6960|
|SubjectDirectoryAttributes|RFC5280|
|TBSRequest|RFC6960|
|TSTInfo|RFC3161|
|Time|RFC5280
|TimeStampReq|RFC3161|
|TimeStampResp|RFC3161|

PKI.js library could be extended very easily to handle additional types from any RFC. If you have a special need for any RFC's new types please create issue on GitHub.
