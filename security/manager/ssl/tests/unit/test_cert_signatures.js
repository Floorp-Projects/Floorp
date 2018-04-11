// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that certificates cannot be tampered with without being detected.
// Tests a combination of cases: RSA signatures, ECDSA signatures, certificate
// chains where the intermediate has been tampered with, chains where the
// end-entity has been tampered, tampering of the signature, and tampering in
// the rest of the certificate.

do_get_profile(); // must be called before getting nsIX509CertDB
var certdb = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

// Reads a PEM-encoded certificate, modifies the nth byte (0-indexed), and
// returns the base64-encoded bytes of the certificate. Negative indices may be
// specified to modify a byte from the end of the certificate.
function readAndTamperWithNthByte(certificatePath, n) {
  let pem = readFile(do_get_file(certificatePath, false));
  let der = atob(pemToBase64(pem));
  if (n < 0) {
    // remember, n is negative at this point
    n = der.length + n;
  }
  let replacement = "\x22";
  if (der.charCodeAt(n) == replacement) {
    replacement = "\x23";
  }
  der = der.substring(0, n) + replacement + der.substring(n + 1);
  return btoa(der);
}

// The signature on certificates appears last. This should modify the contents
// of the signature such that it no longer validates correctly while still
// resulting in a structurally valid certificate.
const BYTE_IN_SIGNATURE = -8;
function addSignatureTamperedCertificate(certificatePath) {
  let base64 = readAndTamperWithNthByte(certificatePath, BYTE_IN_SIGNATURE);
  certdb.addCertFromBase64(base64, ",,");
}

function ensureSignatureVerificationFailure(certificatePath) {
  let cert = constructCertFromFile(certificatePath);
  return checkCertErrorGeneric(certdb, cert, SEC_ERROR_BAD_SIGNATURE,
                               certificateUsageSSLServer);
}

function tamperWithSignatureAndEnsureVerificationFailure(certificatePath) {
  let base64 = readAndTamperWithNthByte(certificatePath, BYTE_IN_SIGNATURE);
  let cert = certdb.constructX509FromBase64(base64);
  return checkCertErrorGeneric(certdb, cert, SEC_ERROR_BAD_SIGNATURE,
                               certificateUsageSSLServer);
}

// The beginning of a certificate looks like this (in hex, using DER):
// 30 XX XX XX [the XX encode length - there are probably 3 bytes here]
//    30 XX XX XX [length again]
//       A0 03
//          02 01
//             02
//       02 XX [length again - 1 byte as long as we're using pycert]
//          XX XX ... [serial number - 20 bytes as long as we're using pycert]
// Since we want to modify the serial number, we need to change something from
// byte 15 to byte 34 (0-indexed). If it turns out that the two length sections
// we assumed were 3 bytes are shorter (they can't be longer), modifying
// something from byte 15 to byte 30 will still get us what we want. Since the
// serial number is a DER INTEGER and because it must be positive, it's best to
// skip the first two bytes of the serial number so as to not run into any
// issues there. Thus byte 17 is a good byte to modify.
const BYTE_IN_SERIAL_NUMBER = 17;
function addSerialNumberTamperedCertificate(certificatePath) {
  let base64 = readAndTamperWithNthByte(certificatePath,
                                        BYTE_IN_SERIAL_NUMBER);
  certdb.addCertFromBase64(base64, ",,");
}

function tamperWithSerialNumberAndEnsureVerificationFailure(certificatePath) {
  let base64 = readAndTamperWithNthByte(certificatePath,
                                        BYTE_IN_SERIAL_NUMBER);
  let cert = certdb.constructX509FromBase64(base64);
  return checkCertErrorGeneric(certdb, cert, SEC_ERROR_BAD_SIGNATURE,
                               certificateUsageSSLServer);
}

add_task(async function() {
  addCertFromFile(certdb, "test_cert_signatures/ca-rsa.pem", "CTu,,");
  addCertFromFile(certdb, "test_cert_signatures/ca-secp384r1.pem", "CTu,,");

  // Tamper with the signatures on intermediate certificates and ensure that
  // end-entity certificates issued by those intermediates do not validate
  // successfully.
  addSignatureTamperedCertificate("test_cert_signatures/int-rsa.pem");
  addSignatureTamperedCertificate("test_cert_signatures/int-secp384r1.pem");
  await ensureSignatureVerificationFailure("test_cert_signatures/ee-rsa.pem");
  await ensureSignatureVerificationFailure("test_cert_signatures/ee-secp384r1.pem");

  // Tamper with the signatures on end-entity certificates and ensure that they
  // do not validate successfully.
  await tamperWithSignatureAndEnsureVerificationFailure(
    "test_cert_signatures/ee-rsa-direct.pem");
  await tamperWithSignatureAndEnsureVerificationFailure(
    "test_cert_signatures/ee-secp384r1-direct.pem");

  // Tamper with the serial numbers of intermediate certificates and ensure
  // that end-entity certificates issued by those intermediates do not validate
  // successfully.
  addSerialNumberTamperedCertificate("test_cert_signatures/int-rsa.pem");
  addSerialNumberTamperedCertificate("test_cert_signatures/int-secp384r1.pem");
  await ensureSignatureVerificationFailure("test_cert_signatures/ee-rsa.pem");
  await ensureSignatureVerificationFailure("test_cert_signatures/ee-secp384r1.pem");

  // Tamper with the serial numbers of end-entity certificates and ensure that
  // they do not validate successfully.
  await tamperWithSerialNumberAndEnsureVerificationFailure(
    "test_cert_signatures/ee-rsa-direct.pem");
  await tamperWithSerialNumberAndEnsureVerificationFailure(
    "test_cert_signatures/ee-secp384r1-direct.pem");
});
