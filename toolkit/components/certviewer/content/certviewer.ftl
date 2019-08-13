# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

certificate-viewer-certificate-section-title = Certificate

## Error messages

certificate-viewer-error-message = We were unable to find the certificate information, or the certificate is corrupted. Please try again.
certificate-viewer-error-title = Something went wrong.

## Certificate information labels

certificate-viewer-algorithm = Algorithm
certificate-viewer-certificate-authority = Certificate Authority
certificate-viewer-cipher-suite = Cipher Suite
certificate-viewer-common-name = Common Name 
# Inc. means Incorporated, e.g GitHub is incorporated in Delaware
certificate-viewer-inc-country = Inc. Country 
certificate-viewer-country = Country 
certificate-viewer-curve = Curve 
certificate-viewer-distribution-point = Distribution Point
certificate-viewer-dns-name = DNS Name
certificate-viewer-exponent = Exponent 
certificate-viewer-id = ID 
certificate-viewer-key-exchange-group = Key Exchange Group
certificate-viewer-key-id = Key ID
certificate-viewer-key-size = Key Size 
certificate-viewer-locality = Locality
certificate-viewer-location = Location
certificate-viewer-logid = Log ID 
certificate-viewer-method = Method
certificate-viewer-modulus = Modulus
certificate-viewer-name = Name 
certificate-viewer-not-after = Not After 
certificate-viewer-not-before = Not Before 
certificate-viewer-organization = Organization
certificate-viewer-organizational-unit = Organizational Unit 
certificate-viewer-policy = Policy 
certificate-viewer-protocol = Protocol
certificate-viewer-public-value = Public Value
certificate-viewer-purposes = Purposes
certificate-viewer-qualifier = Qualifier
certificate-viewer-qualifiers = Qualifiers 
certificate-viewer-required = Required
# Inc. means Incorporated, e.g GitHub is incorporated in Delaware
certificate-viewer-inc-state-province = Inc. State/Province
certificate-viewer-state-province = State/Province
certificate-viewer-sha-1 = SHA-1
certificate-viewer-sha-256 = SHA-256
certificate-viewer-serial-number = Serial Number
certificate-viewer-signature-algorithm = Signature Algorithm
certificate-viewer-signaturealgorithm = Signature Algorithm
certificate-viewer-signature-scheme = Signature Scheme 
certificate-viewer-timestamp = Timestamp
certificate-viewer-value = Value
certificate-viewer-version = Version

## Variables
##   $domain (String) - The common name from the certificate being displayed.

certificate-viewer-download-pem = PEM (cert)
  .download = { $domain }.pem
certificate-viewer-download-pem-chain = PEM (chain)
  .download = { $domain }-chain.pem

## Error codes

certificate-viewer-warning-section-title = Warning
certificate-viewer-sec-error-expired-certificate = This certificate has expired.
certificate-viewer-ssl-error-bad-cert-domain = This certificate is not valid for this host.
certificate-viewer-mozilla-pkix-error-self-signed-cert = This certificate is not trusted because it is self-signed.
certificate-viewer-mozilla-pkix-error-key-pinning-failure = No trusted certificate chain could be constructed that matches the pinset.
certificate-viewer-mozilla-pkix-error-mitm-detected = MITM software has been detected.
certificate-viewer-sec-error-unknown-issuer = This certificate’s issuer is unknown.
certificate-viewer-sec-error-revoked-certificate = Peer’s certificate has been revoked.
certificate-viewer-sec-error-cert-signature-algorithm-disabled = The certificate is not trusted because it was signed using a signature algorithm that was disabled because that algorithm is not secure.
certificate-viewer-business-category = Business Category
