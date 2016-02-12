// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// This test tests that the nsIX509Cert.dbKey and nsIX509CertDB.findCertByDBKey
// APIs work as expected. That is, getting a certificate's dbKey and using it
// in findCertByDBKey should return the same certificate. Also, for backwards
// compatibility, findCertByDBKey should ignore any whitespace in its input
// (even though now nsIX509Cert.dbKey will never have whitespace in it).

function hexStringToBytes(hex) {
  let bytes = [];
  for (let hexByteStr of hex.split(":")) {
    bytes.push(parseInt(hexByteStr, 16));
  }
  return bytes;
}

function encodeCommonNameAsBytes(commonName) {
  // The encoding will look something like this (in hex):
  // 30 (SEQUENCE) <length of contents>
  //    31 (SET) <length of contents>
  //       30 (SEQUENCE) <length of contents>
  //          06 (OID) 03 (length)
  //             55 04 03 (id-at-commonName)
  //          0C (UTF8String) <length of common name>
  //             <common name bytes>
  // To make things simple, it would be nice to have the length of each
  // component be less than 128 bytes (so we can have single-byte lengths).
  // For this to hold, the maximum length of the contents of the outermost
  // SEQUENCE must be 127. Everything not in the contents of the common name
  // will take up 11 bytes, so the value of the common name itself can be at
  // most 116 bytes.
  ok(commonName.length <= 116,
     "test assumption: common name can't be longer than 116 bytes (makes " +
     "DER encoding easier)");
  let commonNameOIDBytes = [ 0x06, 0x03, 0x55, 0x04, 0x03 ];
  let commonNameBytes = [ 0x0C, commonName.length ];
  for (let i = 0; i < commonName.length; i++) {
    commonNameBytes.push(commonName.charCodeAt(i));
  }
  let bytes = commonNameOIDBytes.concat(commonNameBytes);
  bytes.unshift(bytes.length);
  bytes.unshift(0x30); // SEQUENCE
  bytes.unshift(bytes.length);
  bytes.unshift(0x31); // SET
  bytes.unshift(bytes.length);
  bytes.unshift(0x30); // SEQUENCE
  return bytes;
}

function testInvalidDBKey(certDB, dbKey) {
  throws(() => certDB.findCertByDBKey(dbKey), /NS_ERROR_ILLEGAL_INPUT/,
         `findCertByDBKey(${dbKey}) should raise NS_ERROR_ILLEGAL_INPUT`);
}

function testDBKeyForNonexistentCert(certDB, dbKey) {
  let cert = certDB.findCertByDBKey(dbKey);
  ok(!cert, "shouldn't find cert for given dbKey");
}

function byteArrayToByteString(bytes) {
  let byteString = "";
  for (let b of bytes) {
    byteString += String.fromCharCode(b);
  }
  return byteString;
}

function run_test() {
  do_get_profile();
  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  let cert = constructCertFromFile("bad_certs/test-ca.pem");
  equal(cert.issuerName, "CN=" + cert.issuerCommonName,
        "test assumption: this certificate's issuer distinguished name " +
        "consists only of a common name");
  let issuerBytes = encodeCommonNameAsBytes(cert.issuerCommonName);
  ok(issuerBytes.length < 256,
     "test assumption: length of encoded issuer is less than 256 bytes");
  let serialNumberBytes = hexStringToBytes(cert.serialNumber);
  ok(serialNumberBytes.length < 256,
     "test assumption: length of encoded serial number is less than 256 bytes");
  let dbKeyHeader = [ 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, serialNumberBytes.length,
                      0, 0, 0, issuerBytes.length ];
  let expectedDbKeyBytes = dbKeyHeader.concat(serialNumberBytes, issuerBytes);
  let expectedDbKey = btoa(byteArrayToByteString(expectedDbKeyBytes));
  equal(cert.dbKey, expectedDbKey,
        "actual and expected dbKey values should match");

  let certFromDbKey = certDB.findCertByDBKey(expectedDbKey);
  ok(certFromDbKey.equals(cert),
     "nsIX509CertDB.findCertByDBKey should find the right certificate");

  ok(expectedDbKey.length > 64,
     "test assumption: dbKey should be longer than 64 characters");
  let expectedDbKeyWithCRLF = expectedDbKey.replace(/(.{64})/, "$1\r\n");
  ok(expectedDbKeyWithCRLF.indexOf("\r\n") == 64,
     "test self-check: adding CRLF to dbKey should succeed");
  certFromDbKey = certDB.findCertByDBKey(expectedDbKeyWithCRLF);
  ok(certFromDbKey.equals(cert),
     "nsIX509CertDB.findCertByDBKey should work with dbKey with CRLF");

  let expectedDbKeyWithSpaces = expectedDbKey.replace(/(.{64})/, "$1  ");
  ok(expectedDbKeyWithSpaces.indexOf("  ") == 64,
     "test self-check: adding spaces to dbKey should succeed");
  certFromDbKey = certDB.findCertByDBKey(expectedDbKeyWithSpaces);
  ok(certFromDbKey.equals(cert),
     "nsIX509CertDB.findCertByDBKey should work with dbKey with spaces");

  // Test some invalid dbKey values.
  testInvalidDBKey(certDB, "AAAA"); // Not long enough.
  // No header.
  testInvalidDBKey(certDB, btoa(byteArrayToByteString(
    [ 0, 0, 0, serialNumberBytes.length,
      0, 0, 0, issuerBytes.length ].concat(serialNumberBytes, issuerBytes))));
  testInvalidDBKey(certDB, btoa(byteArrayToByteString(
    [ 0, 0, 0, 0, 0, 0, 0, 0,
      255, 255, 255, 255, // serial number length is way too long
      255, 255, 255, 255, // issuer length is way too long
      0, 0, 0, 0 ])));
  // Truncated issuer.
  testInvalidDBKey(certDB, btoa(byteArrayToByteString(
    [ 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 1,
      0, 0, 0, 10,
      1,
      1, 2, 3 ])));
  // Issuer doesn't decode to valid common name.
  testDBKeyForNonexistentCert(certDB, btoa(byteArrayToByteString(
    [ 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 1,
      0, 0, 0, 3,
      1,
      1, 2, 3 ])));

  // zero-length serial number and issuer -> no such certificate
  testDBKeyForNonexistentCert(certDB, btoa(byteArrayToByteString(
    [ 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0 ])));
}
