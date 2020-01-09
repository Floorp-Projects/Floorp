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

const gExpiredEEPEM = `-----BEGIN CERTIFICATE-----
MIIDHDCCAgSgAwIBAgIUY9ERAIKj0js/YbhJoMrcLnj++uowDQYJKoZIhvcNAQEL
BQAwEjEQMA4GA1UEAwwHVGVzdCBDQTAiGA8yMDEzMDEwMTAwMDAwMFoYDzIwMTQw
MTAxMDAwMDAwWjAiMSAwHgYDVQQDDBdFeHBpcmVkIFRlc3QgRW5kLWVudGl0eTCC
ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALqIUahEjhbWQf1utogGNhA9
PBPZ6uQ1SrTs9WhXbCR7wcclqODYH72xnAabbhqG8mvir1p1a2pkcQh6pVqnRYf3
HNUknAJ+zUP8HmnQOCApk6sgw0nk27lMwmtsDu0Vgg/xfq1pGrHTAjqLKkHup3Dg
Dw2N/WYLK7AkkqR9uYhheZCxV5A90jvF4LhIH6g304hD7ycW2FW3ZlqqfgKQLzp7
EIAGJMwcbJetlmFbt+KWEsB1MaMMkd20yvf8rR0l0wnvuRcOp2jhs3svIm9p47SK
lWEd7ibWJZ2rkQhONsscJAQsvxaLL+Xxj5kXMbiz/kkj+nJRxDHVA6zaGAo17Y0C
AwEAAaNWMFQwHgYDVR0RBBcwFYITZXhwaXJlZC5leGFtcGxlLmNvbTAyBggrBgEF
BQcBAQQmMCQwIgYIKwYBBQUHMAGGFmh0dHA6Ly9sb2NhbGhvc3Q6ODg4OC8wDQYJ
KoZIhvcNAQELBQADggEBAImiFuy275T6b+Ud6gl/El6qpgWHUXeYiv2sp7d+HVzf
T+ow5WVsxI/GMKhdA43JaKT9gfMsbnP1qiI2zel3U+F7IAMO1CEr5FVdCOVTma5h
mu/81rkJLmZ8RQDWWOhZKyn/7aD7TH1C1e768yCt5E2DDl8mHil9zR8BPsoXwuS3
L9zJ2JqNc60+hB8l297ZaSl0nbKffb47ukvn5kSJ7tI9n/fSXdj1JrukwjZP+74V
kQyNobaFzDZ+Zr3QmfbejEsY2EYnq8XuENgIO4DuYrm80/p6bMO6laB0Uv5W6uXZ
gBZdRTe1WMdYWGhmvnFFQmf+naeOOl6ryFwWwtnoK7I=
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
  let certList = [];
  for (let pem of pemList) {
    let cert = certdb.constructX509FromBase64(pemToBase64(pem));
    certList.push(cert);
  }
  return certList;
}

function test_cert_pkcs7_export() {
  // This was generated by running BadCertAndPinningServer locally on the bad_certs
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

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let pkcs7DefaultEE = certdb.asPKCS7Blob(certListDefaultEE);

  equal(
    btoa(pkcs7DefaultEE),
    expectedPKCS7ForDefaultEE,
    "PKCS7 export should work as expected for default-ee chain"
  );

  // This was generated by running BadCertAndPinningServer locally on the bad_certs
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
  let pkcs7UnknownIssuer = certdb.asPKCS7Blob(certListUnknownIssuer);
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
    "MIAGCSqGSIb3DQEHAqCAMIACAQExADCABgkqhkiG9w0BBwEAAKCCCPEwggLTMIIBu6ADAgE" +
    "CAhQpoXAjALAddSApG46EBfimNiyZuDANBgkqhkiG9w0BAQsFADASMRAwDgYDVQQDDAdUZX" +
    "N0IENBMCIYDzIwMTcxMTI3MDAwMDAwWhgPMjAyMDAyMDUwMDAwMDBaMBIxEDAOBgNVBAMMB" +
    "1Rlc3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6iFGoRI4W1kH9braI" +
    "BjYQPTwT2erkNUq07PVoV2wke8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHEIeqVap0WH9xz" +
    "VJJwCfs1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw4A8Njf1mCy" +
    "uwJJKkfbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86exCABiTMHGyXrZZhW" +
    "7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0ipVhHe4m1iWdq5EITjbLHCQE" +
    "LL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAGjHTAbMAwGA1UdEwQFMAMBAf8" +
    "wCwYDVR0PBAQDAgEGMA0GCSqGSIb3DQEBCwUAA4IBAQAgyCfLAcVs/MkERxunH9pZA4ja1Q" +
    "WWjsxSg9KgAIfOgj8c5RPHbl4oeWk0raNKWMu5+FR3/94IJeD45C3h/Y3+1HDyC6ZuzdgMX" +
    "v63dk0a36JDFlPA3swqwYhnL7pHnbdcfDyWnMVfmLNeAhL7QA+Vf5fJmTsxEJwFaHo9JpKo" +
    "Q469RdWno6aHeK3TfiQFaebzT1MRabCJXDeyw8OalQICt0M0wx29B6HNof3px2NxKyC6qlf" +
    "01wwNSaaIbsctDaLL5ZLN6T1LjpJsooMvDwRt69+SXo8SmD4YO6Wr4Q9drI3cCwVeQXwxoU" +
    "uB96muQQ2M3WDiMz5ZLI3oMLu8KSPsMIIC3TCCAcWgAwIBAgIUa0X7/7DlTaedpgrIJg25i" +
    "BPOkIMwDQYJKoZIhvcNAQELBQAwEjEQMA4GA1UEAwwHVGVzdCBDQTAiGA8yMDE1MDEwMTAw" +
    "MDAwMFoYDzIwMjUwMTAxMDAwMDAwWjAcMRowGAYDVQQDDBFUZXN0IEludGVybWVkaWF0ZTC" +
    "CASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALqIUahEjhbWQf1utogGNhA9PBPZ6u" +
    "Q1SrTs9WhXbCR7wcclqODYH72xnAabbhqG8mvir1p1a2pkcQh6pVqnRYf3HNUknAJ+zUP8H" +
    "mnQOCApk6sgw0nk27lMwmtsDu0Vgg/xfq1pGrHTAjqLKkHup3DgDw2N/WYLK7AkkqR9uYhh" +
    "eZCxV5A90jvF4LhIH6g304hD7ycW2FW3ZlqqfgKQLzp7EIAGJMwcbJetlmFbt+KWEsB1MaM" +
    "Mkd20yvf8rR0l0wnvuRcOp2jhs3svIm9p47SKlWEd7ibWJZ2rkQhONsscJAQsvxaLL+Xxj5" +
    "kXMbiz/kkj+nJRxDHVA6zaGAo17Y0CAwEAAaMdMBswDAYDVR0TBAUwAwEB/zALBgNVHQ8EB" +
    "AMCAQYwDQYJKoZIhvcNAQELBQADggEBAILNZM9yT9ylMpjyi0tXaDORzpHiJ8vEoVKk98bC" +
    "2BQF0kMEEB547p+Ms8zdJY00Bxe9qigT8rQwKprXq5RvgIZ32QLn/yMPiCp/e6zBdsx77Tk" +
    "fmnSnxvPi+0nlA+eM8JYN0UST4vWD4vPPX9GgZDVoGQTiF3hUivJ5R8sHb/ozcSukMKQQ22" +
    "+AIU7w6wyAIbCAG7Pab4k2XFAeEnUZsl9fCym5jsPN9Pnv9rlBi6h8shHw1R2ROXjgxubji" +
    "Mr3B456vFTJImLJjyA1iTSlr/+VXGUYg6Z0/HYnsO00+8xUKM71dPxGAfIFNaSscpykrGFL" +
    "vocT/kym6r8galxCJUowggM1MIICHaADAgECAhRnruFLuUdWcJC1fHuhcWXOKGqHRzANBgk" +
    "qhkiG9w0BAQsFADAcMRowGAYDVQQDDBFUZXN0IEludGVybWVkaWF0ZTAiGA8yMDE3MTEyNz" +
    "AwMDAwMFoYDzIwMjAwMjA1MDAwMDAwWjAsMSowKAYDVQQDDCFUZXN0IEVuZC1lbnRpdHkgd" +
    "2l0aCBJbnRlcm1lZGlhdGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6iFGo" +
    "RI4W1kH9braIBjYQPTwT2erkNUq07PVoV2wke8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHE" +
    "IeqVap0WH9xzVJJwCfs1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7q" +
    "dw4A8Njf1mCyuwJJKkfbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86exCAB" +
    "iTMHGyXrZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0ipVhHe4m1iWd" +
    "q5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAGjWzBZMCMGA1U" +
    "dEQQcMBqCCWxvY2FsaG9zdIINKi5leGFtcGxlLmNvbTAyBggrBgEFBQcBAQQmMCQwIgYIKw" +
    "YBBQUHMAGGFmh0dHA6Ly9sb2NhbGhvc3Q6ODg4OC8wDQYJKoZIhvcNAQELBQADggEBACjTj" +
    "eEzgaJS2hco5AHa/QLycXhjLOl/6LQuXFAIVmSIti72ki5C7XojdNstBEHCEILSaXtOokEB" +
    "uvLMgtOisJA/wDVZBrWS3pxvfSMLJctxacMD9Qs97Gf1xibojRjIsWWeRxXMwYsVz4vJZMA" +
    "8Rbmsxq67mI46n/ovmsEXQz1uDnJdIY0O/yCTclz6iklsENOsC4oM0uklISniuqi80WeKvO" +
    "UlYNL5JwCxYe2gtysfqCdBE+wRXVYVCdivceQBqRhCiT3GzkCKQFLqaDPj9wUG2zxtIZK5f" +
    "Fy2Q3ZKztjmXdiZNgoLE/Pqt61V+uGL+KyUfhbfP0em7dmrIAI7otsxAAAAAAAAAA==";
  let certListWithIntermediate = build_cert_list_from_pem_list([
    gOCSPEEWithIntermediatePEM,
    gTestIntPEM,
    gTestCAPEM,
  ]);
  let pkcs7WithIntermediate = certdb.asPKCS7Blob(certListWithIntermediate);
  equal(
    btoa(pkcs7WithIntermediate),
    expectedPKCS7WithIntermediate,
    "PKCS7 export should work as expected for chain with intermediate"
  );
}

function test_cert_pkcs7_empty_array() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );

  throws(
    () => certdb.asPKCS7Blob([]),
    /NS_ERROR_ILLEGAL_VALUE/,
    "trying to convert an empty array to pkcs7 should throw"
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

// In Bug 1580315, nsNSSCertList/nsIX509CertList was replaced by
// Array<nsIX509Cert>, so the serialization of the certList changed. This
// test is used to make sure we can still deserialize the transportSecurityInfo
// binary string which has the old certList binary.
function test_old_succeeded_certlist_deseralization_v1() {
  // This was the serialized output of test "good.include-subdomains.pinning.example.com"
  // in security/manager/ssl/tests/unit/test_cert_chains.js
  // The serialized output was generated before we replace nsIX509CertList with
  // Array<nsIX509Cert>, so it had the old version of transportSecurityInfo.
  const serialized =
    "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";

  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );
  // deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsITransportSecurityInfo);

  equal(
    deserialized.failedCertChain.length,
    0,
    "failedCertChain for a successful connection should be empty"
  );
  let certChain = build_cert_list_from_pem_list([gDefaultEEPEM, gTestCAPEM]);
  ok(
    areCertArraysEqual(certChain, deserialized.succeededCertChain),
    "succeededCertChain should be deserialized correctly"
  );
}

// Same as the above test, however, this is the v2 version of the
// serialization.
function test_old_succeeded_certlist_deseralization_v2() {
  const serialized =
    "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";

  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );
  // deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsITransportSecurityInfo);

  equal(
    deserialized.failedCertChain.length,
    [],
    "failedCertChain for a successful connection should be empty"
  );
  let certChain = build_cert_list_from_pem_list([gDefaultEEPEM, gTestCAPEM]);
  ok(
    areCertArraysEqual(certChain, deserialized.succeededCertChain),
    "succeededCertChain should be deserialized correctly"
  );
}

// In Bug 1580315, nsNSSCertList/nsIX509CertList was replaced by
// Array<nsIX509Cert>, so the serialization of the certList changed. This
// test is used to make sure we can still deserialize the TransportSecurityInfo
// binary string which has the old certList binary.
function test_old_failed_certlist_deseralization_v1() {
  // This was the serialized output of test "expired.example.com"
  // in security/manager/ssl/tests/unit/test_cert_chains.js
  // The serialized output was generated before we replace nsIX509CertList with
  // Array<nsIX509Cert>, so it had the old version of transportSecurityInfo.
  const serialized =
    "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" +
    "96muQQ2M3WDiMz5ZLI3oMLu8KSPs";

  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );
  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsITransportSecurityInfo);

  equal(
    deserialized.succeededCertChain.length,
    0,
    "succeededCertChain should be empty"
  );
  let certChain = build_cert_list_from_pem_list([gExpiredEEPEM, gTestCAPEM]);
  ok(
    areCertArraysEqual(certChain, deserialized.failedCertChain),
    "failedCertChain should be deserialized correctly"
  );
}

// Same as the above test, however, this is the v2 version of the
// serialization.
function test_old_failed_certlist_deseralization_v2() {
  const serialized =
    "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" +
    "96muQQ2M3WDiMz5ZLI3oMLu8KSPsAA==";

  let serHelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );
  // Deserialize from the string and compare to the original object
  let deserialized = serHelper.deserializeObject(serialized);
  deserialized.QueryInterface(Ci.nsITransportSecurityInfo);

  let certChain = build_cert_list_from_pem_list([gExpiredEEPEM, gTestCAPEM]);
  ok(
    areCertArraysEqual(certChain, deserialized.failedCertChain),
    "failedCertChain should be deserialized correctly"
  );
}
function run_test() {
  do_get_profile();
  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

  // Test nsIX509Cert.equals
  add_test(function() {
    test_cert_equals();
    run_next_test();
  });

  add_test(function() {
    test_cert_pkcs7_export();
    run_next_test();
  });

  add_test(function() {
    test_cert_pkcs7_empty_array();
    run_next_test();
  });

  add_test(function() {
    test_old_succeeded_certlist_deseralization_v2();
    run_next_test();
  });

  add_test(function() {
    test_old_failed_certlist_deseralization_v2();
    run_next_test();
  });

  add_test(function() {
    test_old_succeeded_certlist_deseralization_v1();
    run_next_test();
  });

  add_test(function() {
    test_old_failed_certlist_deseralization_v1();
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
        aTransportSecurityInfo.failedCertChain.length,
        0,
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
        areCertArraysEqual(originalCertChain, securityInfo.failedCertChain),
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
        areCertArraysEqual(originalCertChain, securityInfo.failedCertChain),
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
        areCertArraysEqual(originalCertChain, securityInfo.failedCertChain),
        "failedCertChain should equal the original cert chain for a" +
          " non-overrideable connection failure"
      );
    }
  );

  run_next_test();
}
