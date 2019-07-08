// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

function test_cert_equals() {
  let certA = constructCertFromFile("bad_certs/default-ee.pem");
  let certB = constructCertFromFile("bad_certs/default-ee.pem");
  let certC = constructCertFromFile("bad_certs/expired-ee.pem");

  ok(
    certA != certB,
    "Cert objects constructed from the same file should not be equal" +
      " according to the equality operators"
  );
  ok(
    certA.equals(certB),
    "equals() on cert objects constructed from the same cert file should" +
      " return true"
  );
  ok(
    !certA.equals(certC),
    "equals() on cert objects constructed from files for different certs" +
      " should return false"
  );
}

function test_bad_cert_list_serialization() {
  // Normally the serialization of an nsIX509CertList consists of some header
  // junk (IIDs and whatnot), 4 bytes representing how many nsIX509Cert follow,
  // and then the serialization of each nsIX509Cert. This serialization consists
  // of the header junk for an nsIX509CertList with 1 "nsIX509Cert", but then
  // instead of an nsIX509Cert, the subsequent bytes represent the serialization
  // of another nsIX509CertList (with 0 nsIX509Cert). This test ensures that
  // nsIX509CertList safely handles this unexpected input when deserializing.
  const badCertListSerialization =
    "lZ+xZWUXSH+rm9iRO+UxlwAAAAAAAAAAwAAAAAAAAEYAAAABlZ+xZWUXSH+rm9iRO+UxlwAAAAAA" +
    "AAAAwAAAAAAAAEYAAAAA";
  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );
  throws(
    () => serHelper.deserializeObject(badCertListSerialization),
    /NS_ERROR_UNEXPECTED/,
    "deserializing a bogus nsIX509CertList should throw NS_ERROR_UNEXPECTED"
  );
}

function test_cert_list_serialization() {
  let certList = build_cert_chain(["default-ee", "expired-ee"]);

  throws(
    () => certList.addCert(null),
    /NS_ERROR_ILLEGAL_VALUE/,
    "trying to add a null cert to an nsIX509CertList should throw"
  );

  // Serialize the cert list to a string
  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );
  certList.QueryInterface(Ci.nsISerializable);
  let serialized = serHelper.serializeToString(certList);

  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsIX509CertList);
  ok(
    certList.equals(deserialized),
    "Deserialized cert list should equal the original"
  );
}

// We hard-code the following certificates for the pkcs7 export tests so that we
// don't have to change the test data when the certificates change each year.
// Luckily these tests don't depend on the certificates being valid, so it's ok
// to let them expire.
const gDefaultEEPEM = `-----BEGIN CERTIFICATE-----
MIIDiTCCAnGgAwIBAgIUDUo/9G0rz7fJiWTw0hY6TIyPRSIwDQYJKoZIhvcNAQEL
BQAwEjEQMA4GA1UEAwwHVGVzdCBDQTAiGA8yMDE3MTEyNzAwMDAwMFoYDzIwMjAw
MjA1MDAwMDAwWjAaMRgwFgYDVQQDDA9UZXN0IEVuZC1lbnRpdHkwggEiMA0GCSqG
SIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTwT2erkNUq0
7PVoV2wke8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHEIeqVap0WH9xzVJJwCfs1D
/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw4A8Njf1mCyuw
JJKkfbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86exCABiTMHGyX
rZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0ipVhHe4m1iWd
q5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAGjgcow
gccwgZAGA1UdEQSBiDCBhYIJbG9jYWxob3N0gg0qLmV4YW1wbGUuY29tghUqLnBp
bm5pbmcuZXhhbXBsZS5jb22CKCouaW5jbHVkZS1zdWJkb21haW5zLnBpbm5pbmcu
ZXhhbXBsZS5jb22CKCouZXhjbHVkZS1zdWJkb21haW5zLnBpbm5pbmcuZXhhbXBs
ZS5jb20wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzABhhZodHRwOi8vbG9jYWxo
b3N0Ojg4ODgvMA0GCSqGSIb3DQEBCwUAA4IBAQCkguNhMyVCYhyYXfE22wNvlaob
K2YRb4OGMxySIKuQ80N0XlO+xpLJTs9YzFVY1+JTHNez1QfwP9KJeZznTzVzLh4s
v0swx/+oUxCfLb0VIl/kdUqLkbGYrAmtjeOKZLaqVtRH0BnmbPowLak1pi6nQYOU
+aL9QOuvT/j3rXoimcdo6X3TK1SN2/64fGMyG/pwas+JXehbReUf4n1ewk84ADtb
+ew8tRAKf/uxzKUj5t/UgqDsnTWq5wUc5IJKwoHT41sQnNqPg12x4+WGWiAsWCpR
/hKYHFGr7rb4JTGEPAJpWcv9WtZYAvwT78a2xpHp5XNglj16IjWEukvJuU1W
-----END CERTIFICATE-----`;

const gTestCAPEM = `-----BEGIN CERTIFICATE-----
MIIC0zCCAbugAwIBAgIUKaFwIwCwHXUgKRuOhAX4pjYsmbgwDQYJKoZIhvcNAQEL
BQAwEjEQMA4GA1UEAwwHVGVzdCBDQTAiGA8yMDE3MTEyNzAwMDAwMFoYDzIwMjAw
MjA1MDAwMDAwWjASMRAwDgYDVQQDDAdUZXN0IENBMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAuohRqESOFtZB/W62iAY2ED08E9nq5DVKtOz1aFdsJHvB
xyWo4NgfvbGcBptuGobya+KvWnVramRxCHqlWqdFh/cc1SScAn7NQ/weadA4ICmT
qyDDSeTbuUzCa2wO7RWCD/F+rWkasdMCOosqQe6ncOAPDY39ZgsrsCSSpH25iGF5
kLFXkD3SO8XguEgfqDfTiEPvJxbYVbdmWqp+ApAvOnsQgAYkzBxsl62WYVu34pYS
wHUxowyR3bTK9/ytHSXTCe+5Fw6naOGzey8ib2njtIqVYR3uJtYlnauRCE42yxwk
BCy/Fosv5fGPmRcxuLP+SSP6clHEMdUDrNoYCjXtjQIDAQABox0wGzAMBgNVHRME
BTADAQH/MAsGA1UdDwQEAwIBBjANBgkqhkiG9w0BAQsFAAOCAQEAIMgnywHFbPzJ
BEcbpx/aWQOI2tUFlo7MUoPSoACHzoI/HOUTx25eKHlpNK2jSljLufhUd//eCCXg
+OQt4f2N/tRw8gumbs3YDF7+t3ZNGt+iQxZTwN7MKsGIZy+6R523XHw8lpzFX5iz
XgIS+0APlX+XyZk7MRCcBWh6PSaSqEOOvUXVp6Omh3it034kBWnm809TEWmwiVw3
ssPDmpUCArdDNMMdvQehzaH96cdjcSsguqpX9NcMDUmmiG7HLQ2iy+WSzek9S46S
bKKDLw8Ebevfkl6PEpg+GDulq+EPXayN3AsFXkF8MaFLgfeprkENjN1g4jM+WSyN
6DC7vCkj7A==
-----END CERTIFICATE-----`;

const gUnknownIssuerPEM = `
-----BEGIN CERTIFICATE-----
MIIDqTCCApGgAwIBAgIUMRiJ9TrwqTOoVFU+j5FDWDWS1X8wDQYJKoZIhvcNAQEL
BQAwJjEkMCIGA1UEAwwbVGVzdCBJbnRlcm1lZGlhdGUgdG8gZGVsZXRlMCIYDzIw
MTcxMTI3MDAwMDAwWhgPMjAyMDAyMDUwMDAwMDBaMC4xLDAqBgNVBAMMI1Rlc3Qg
RW5kLWVudGl0eSBmcm9tIHVua25vd24gaXNzdWVyMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAuohRqESOFtZB/W62iAY2ED08E9nq5DVKtOz1aFdsJHvB
xyWo4NgfvbGcBptuGobya+KvWnVramRxCHqlWqdFh/cc1SScAn7NQ/weadA4ICmT
qyDDSeTbuUzCa2wO7RWCD/F+rWkasdMCOosqQe6ncOAPDY39ZgsrsCSSpH25iGF5
kLFXkD3SO8XguEgfqDfTiEPvJxbYVbdmWqp+ApAvOnsQgAYkzBxsl62WYVu34pYS
wHUxowyR3bTK9/ytHSXTCe+5Fw6naOGzey8ib2njtIqVYR3uJtYlnauRCE42yxwk
BCy/Fosv5fGPmRcxuLP+SSP6clHEMdUDrNoYCjXtjQIDAQABo4HCMIG/MIGIBgNV
HREEgYAwfoIZdW5rbm93bmlzc3Vlci5leGFtcGxlLmNvbYI0dW5rbm93bmlzc3Vl
ci5pbmNsdWRlLXN1YmRvbWFpbnMucGlubmluZy5leGFtcGxlLmNvbYIrdW5rbm93
bmlzc3Vlci50ZXN0LW1vZGUucGlubmluZy5leGFtcGxlLmNvbTAyBggrBgEFBQcB
AQQmMCQwIgYIKwYBBQUHMAGGFmh0dHA6Ly9sb2NhbGhvc3Q6ODg4OC8wDQYJKoZI
hvcNAQELBQADggEBALAnJjBJ+MOc7kMRzmESYZRSxKak7A1K67xBXWzWmK3t3WXv
e/RLjV/RhbyTN20h2ZjSVcuDzgNYC/RJ/z3Xd5Q9QEGoi1ly84HeaeHw/3kUSHxv
J3JnbPu2lk96U5y7tXEVfbEVZYpx4Us72fuURPWriVldILH2lgrEg+iKZWbY/wcT
vfu1j/flMkGEOpc1HytlmR9fkCDnqzFfcmv7Eh3X1BiSBOIemGnUHxONwlthSE68
IItE5l3c82G8oQGmve6r0N9h7t6opIjH1koFWMck/pzDA01FmWey4ASdlmjE8NSJ
Al1zsF8EiLOZeI1rvurcXwVOd0Olk9/QT5hwTkk=
-----END CERTIFICATE-----`;

const gOCSPEEWithIntermediatePEM = `
-----BEGIN CERTIFICATE-----
MIIDNTCCAh2gAwIBAgIUZ67hS7lHVnCQtXx7oXFlzihqh0cwDQYJKoZIhvcNAQEL
BQAwHDEaMBgGA1UEAwwRVGVzdCBJbnRlcm1lZGlhdGUwIhgPMjAxNzExMjcwMDAw
MDBaGA8yMDIwMDIwNTAwMDAwMFowLDEqMCgGA1UEAwwhVGVzdCBFbmQtZW50aXR5
IHdpdGggSW50ZXJtZWRpYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC
AQEAuohRqESOFtZB/W62iAY2ED08E9nq5DVKtOz1aFdsJHvBxyWo4NgfvbGcBptu
Gobya+KvWnVramRxCHqlWqdFh/cc1SScAn7NQ/weadA4ICmTqyDDSeTbuUzCa2wO
7RWCD/F+rWkasdMCOosqQe6ncOAPDY39ZgsrsCSSpH25iGF5kLFXkD3SO8XguEgf
qDfTiEPvJxbYVbdmWqp+ApAvOnsQgAYkzBxsl62WYVu34pYSwHUxowyR3bTK9/yt
HSXTCe+5Fw6naOGzey8ib2njtIqVYR3uJtYlnauRCE42yxwkBCy/Fosv5fGPmRcx
uLP+SSP6clHEMdUDrNoYCjXtjQIDAQABo1swWTAjBgNVHREEHDAagglsb2NhbGhv
c3SCDSouZXhhbXBsZS5jb20wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzABhhZo
dHRwOi8vbG9jYWxob3N0Ojg4ODgvMA0GCSqGSIb3DQEBCwUAA4IBAQAo043hM4Gi
UtoXKOQB2v0C8nF4Yyzpf+i0LlxQCFZkiLYu9pIuQu16I3TbLQRBwhCC0ml7TqJB
AbryzILTorCQP8A1WQa1kt6cb30jCyXLcWnDA/ULPexn9cYm6I0YyLFlnkcVzMGL
Fc+LyWTAPEW5rMauu5iOOp/6L5rBF0M9bg5yXSGNDv8gk3Jc+opJbBDTrAuKDNLp
JSEp4rqovNFnirzlJWDS+ScAsWHtoLcrH6gnQRPsEV1WFQnYr3HkAakYQok9xs5A
ikBS6mgz4/cFBts8bSGSuXxctkN2Ss7Y5l3YmTYKCxPz6retVfrhi/islH4W3z9H
pu3ZqyACO6Lb
-----END CERTIFICATE-----`;

const gTestIntPEM = `
-----BEGIN CERTIFICATE-----
MIIC3TCCAcWgAwIBAgIUa0X7/7DlTaedpgrIJg25iBPOkIMwDQYJKoZIhvcNAQEL
BQAwEjEQMA4GA1UEAwwHVGVzdCBDQTAiGA8yMDE1MDEwMTAwMDAwMFoYDzIwMjUw
MTAxMDAwMDAwWjAcMRowGAYDVQQDDBFUZXN0IEludGVybWVkaWF0ZTCCASIwDQYJ
KoZIhvcNAQEBBQADggEPADCCAQoCggEBALqIUahEjhbWQf1utogGNhA9PBPZ6uQ1
SrTs9WhXbCR7wcclqODYH72xnAabbhqG8mvir1p1a2pkcQh6pVqnRYf3HNUknAJ+
zUP8HmnQOCApk6sgw0nk27lMwmtsDu0Vgg/xfq1pGrHTAjqLKkHup3DgDw2N/WYL
K7AkkqR9uYhheZCxV5A90jvF4LhIH6g304hD7ycW2FW3ZlqqfgKQLzp7EIAGJMwc
bJetlmFbt+KWEsB1MaMMkd20yvf8rR0l0wnvuRcOp2jhs3svIm9p47SKlWEd7ibW
JZ2rkQhONsscJAQsvxaLL+Xxj5kXMbiz/kkj+nJRxDHVA6zaGAo17Y0CAwEAAaMd
MBswDAYDVR0TBAUwAwEB/zALBgNVHQ8EBAMCAQYwDQYJKoZIhvcNAQELBQADggEB
AILNZM9yT9ylMpjyi0tXaDORzpHiJ8vEoVKk98bC2BQF0kMEEB547p+Ms8zdJY00
Bxe9qigT8rQwKprXq5RvgIZ32QLn/yMPiCp/e6zBdsx77TkfmnSnxvPi+0nlA+eM
8JYN0UST4vWD4vPPX9GgZDVoGQTiF3hUivJ5R8sHb/ozcSukMKQQ22+AIU7w6wyA
IbCAG7Pab4k2XFAeEnUZsl9fCym5jsPN9Pnv9rlBi6h8shHw1R2ROXjgxubjiMr3
B456vFTJImLJjyA1iTSlr/+VXGUYg6Z0/HYnsO00+8xUKM71dPxGAfIFNaSscpyk
rGFLvocT/kym6r8galxCJUo=
-----END CERTIFICATE-----`;

function build_cert_list_from_pem_list(pemList) {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let certList = Cc["@mozilla.org/security/x509certlist;1"].createInstance(
    Ci.nsIX509CertList
  );
  for (let pem of pemList) {
    let cert = certdb.constructX509FromBase64(pemToBase64(pem));
    certList.addCert(cert);
  }
  return certList;
}

function test_cert_pkcs7_export() {
  // This was generated by running BadCertServer locally on the bad_certs
  // directory and visiting:
  // https://good.include-subdomains.pinning.example.com:8443/
  // and then viewing the certificate chain presented (in the page info dialog)
  // and exporting it.
  // (NB: test-ca must be imported and trusted for the connection to succeed)
  const expectedPKCS7ForDefaultEE =
    "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";
  let certListDefaultEE = build_cert_list_from_pem_list([
    gDefaultEEPEM,
    gTestCAPEM,
  ]);
  let pkcs7DefaultEE = certListDefaultEE.asPKCS7Blob();
  equal(
    btoa(pkcs7DefaultEE),
    expectedPKCS7ForDefaultEE,
    "PKCS7 export should work as expected for default-ee chain"
  );

  // This was generated by running BadCertServer locally on the bad_certs
  // directory and visiting:
  // https://unknownissuer.example.com:8443/
  // and then viewing the certificate presented (in the add certificate
  // exception dialog) and exporting it.
  const expectedPKCS7ForUnknownIssuer =
    "MIAGCSqGSIb3DQEHAqCAMIACAQExADCABgkqhkiG9w0BBwEAAKCCA60wggOpMIICkaADAgE" +
    "CAhQxGIn1OvCpM6hUVT6PkUNYNZLVfzANBgkqhkiG9w0BAQsFADAmMSQwIgYDVQQDDBtUZX" +
    "N0IEludGVybWVkaWF0ZSB0byBkZWxldGUwIhgPMjAxNzExMjcwMDAwMDBaGA8yMDIwMDIwN" +
    "TAwMDAwMFowLjEsMCoGA1UEAwwjVGVzdCBFbmQtZW50aXR5IGZyb20gdW5rbm93biBpc3N1" +
    "ZXIwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTw" +
    "T2erkNUq07PVoV2wke8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHEIeqVap0WH9xzVJJwCfs" +
    "1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw4A8Njf1mCyuwJJKkf" +
    "bmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86exCABiTMHGyXrZZhW7filhLA" +
    "dTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0ipVhHe4m1iWdq5EITjbLHCQELL8Wiy/" +
    "l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAGjgcIwgb8wgYgGA1UdEQSBgDB+ghl1bm" +
    "tub3duaXNzdWVyLmV4YW1wbGUuY29tgjR1bmtub3duaXNzdWVyLmluY2x1ZGUtc3ViZG9tY" +
    "Wlucy5waW5uaW5nLmV4YW1wbGUuY29tgit1bmtub3duaXNzdWVyLnRlc3QtbW9kZS5waW5u" +
    "aW5nLmV4YW1wbGUuY29tMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcwAYYWaHR0cDovL2x" +
    "vY2FsaG9zdDo4ODg4LzANBgkqhkiG9w0BAQsFAAOCAQEAsCcmMEn4w5zuQxHOYRJhlFLEpq" +
    "TsDUrrvEFdbNaYre3dZe979EuNX9GFvJM3bSHZmNJVy4POA1gL9En/Pdd3lD1AQaiLWXLzg" +
    "d5p4fD/eRRIfG8ncmds+7aWT3pTnLu1cRV9sRVlinHhSzvZ+5RE9auJWV0gsfaWCsSD6Ipl" +
    "Ztj/BxO9+7WP9+UyQYQ6lzUfK2WZH1+QIOerMV9ya/sSHdfUGJIE4h6YadQfE43CW2FITrw" +
    "gi0TmXdzzYbyhAaa97qvQ32Hu3qikiMfWSgVYxyT+nMMDTUWZZ7LgBJ2WaMTw1IkCXXOwXw" +
    "SIs5l4jWu+6txfBU53Q6WT39BPmHBOSTEAAAAAAAAA";
  let certListUnknownIssuer = build_cert_list_from_pem_list([
    gUnknownIssuerPEM,
  ]);
  let pkcs7UnknownIssuer = certListUnknownIssuer.asPKCS7Blob();
  equal(
    btoa(pkcs7UnknownIssuer),
    expectedPKCS7ForUnknownIssuer,
    "PKCS7 export should work as expected for unknown issuer"
  );

  // This was generated by running OCSPStaplingServer locally on the ocsp_certs
  // directory and visiting:
  // https://ocsp-stapling-with-intermediate.example.com:8443/
  // and then viewing the certificate chain presented (in the page info dialog)
  // and exporting it.
  // (NB: test-ca must be imported and trusted for the connection to succeed)
  const expectedPKCS7WithIntermediate =
    "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";
  let certListWithIntermediate = build_cert_list_from_pem_list([
    gOCSPEEWithIntermediatePEM,
    gTestIntPEM,
    gTestCAPEM,
  ]);
  let pkcs7WithIntermediate = certListWithIntermediate.asPKCS7Blob();
  equal(
    btoa(pkcs7WithIntermediate),
    expectedPKCS7WithIntermediate,
    "PKCS7 export should work as expected for chain with intermediate"
  );
}

function test_security_info_serialization(securityInfo, expectedErrorCode) {
  // Serialize the securityInfo to a string
  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );
  let serialized = serHelper.serializeToString(securityInfo);

  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsITransportSecurityInfo);
  equal(
    securityInfo.securityState,
    deserialized.securityState,
    "Original and deserialized security state should match"
  );
  equal(
    securityInfo.errorMessage,
    deserialized.errorMessage,
    "Original and deserialized error message should match"
  );
  equal(
    securityInfo.errorCode,
    expectedErrorCode,
    "Original and expected error code should match"
  );
  equal(
    deserialized.errorCode,
    expectedErrorCode,
    "Deserialized and expected error code should match"
  );
}

function run_test() {
  do_get_profile();
  add_tls_server_setup("BadCertServer", "bad_certs");

  // Test nsIX509Cert.equals
  add_test(function() {
    test_cert_equals();
    run_next_test();
  });

  add_test(function() {
    test_bad_cert_list_serialization();
    run_next_test();
  });

  // Test serialization of nsIX509CertList
  add_test(function() {
    test_cert_list_serialization();
    run_next_test();
  });

  add_test(function() {
    test_cert_pkcs7_export();
    run_next_test();
  });

  // Test successful connection (failedCertChain should be null)
  add_connection_test(
    // re-use pinning certs (keeler)
    "good.include-subdomains.pinning.example.com",
    PRErrorCodeSuccess,
    null,
    function withSecurityInfo(aTransportSecurityInfo) {
      aTransportSecurityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(aTransportSecurityInfo, 0);
      equal(
        aTransportSecurityInfo.failedCertChain,
        null,
        "failedCertChain for a successful connection should be null"
      );
    }
  );

  // Test overrideable connection failure (failedCertChain should be non-null)
  add_connection_test(
    "expired.example.com",
    SEC_ERROR_EXPIRED_CERTIFICATE,
    null,
    function withSecurityInfo(securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(
        securityInfo,
        SEC_ERROR_EXPIRED_CERTIFICATE
      );
      notEqual(
        securityInfo.failedCertChain,
        null,
        "failedCertChain should not be null for an overrideable" +
          " connection failure"
      );
      let originalCertChain = build_cert_chain(["expired-ee", "test-ca"]);
      ok(
        originalCertChain.equals(securityInfo.failedCertChain),
        "failedCertChain should equal the original cert chain for an" +
          " overrideable connection failure"
      );
    }
  );

  // Test overrideable connection failure (failedCertChain should be non-null)
  add_connection_test(
    "unknownissuer.example.com",
    SEC_ERROR_UNKNOWN_ISSUER,
    null,
    function withSecurityInfo(securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(securityInfo, SEC_ERROR_UNKNOWN_ISSUER);
      notEqual(
        securityInfo.failedCertChain,
        null,
        "failedCertChain should not be null for an overrideable" +
          " connection failure"
      );
      let originalCertChain = build_cert_chain(["unknownissuer"]);
      ok(
        originalCertChain.equals(securityInfo.failedCertChain),
        "failedCertChain should equal the original cert chain for an" +
          " overrideable connection failure"
      );
    }
  );

  // Test non-overrideable error (failedCertChain should be non-null)
  add_connection_test(
    "inadequatekeyusage.example.com",
    SEC_ERROR_INADEQUATE_KEY_USAGE,
    null,
    function withSecurityInfo(securityInfo) {
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      test_security_info_serialization(
        securityInfo,
        SEC_ERROR_INADEQUATE_KEY_USAGE
      );
      notEqual(
        securityInfo.failedCertChain,
        null,
        "failedCertChain should not be null for a non-overrideable" +
          " connection failure"
      );
      let originalCertChain = build_cert_chain([
        "inadequatekeyusage-ee",
        "test-ca",
      ]);
      ok(
        originalCertChain.equals(securityInfo.failedCertChain),
        "failedCertChain should equal the original cert chain for a" +
          " non-overrideable connection failure"
      );
    }
  );

  run_next_test();
}
