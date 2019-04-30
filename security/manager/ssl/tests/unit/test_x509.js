/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests X509.jsm functionality.

var { X509 } = ChromeUtils.import("resource://gre/modules/psm/X509.jsm");

function stringToBytes(s) {
  let b = [];
  for (let i = 0; i < s.length; i++) {
    b.push(s.charCodeAt(i));
  }
  return b;
}

function readPEMToBytes(filename) {
  return stringToBytes(atob(pemToBase64(readFile(do_get_file(filename)))));
}

function run_test() {
  let certificate = new X509.Certificate();
  // We use this certificate because it has a set validity period, which means that when
  // the test certificates get regenerated each year, the values in this test won't change.
  certificate.parse(readPEMToBytes("bad_certs/expired-ee.pem"));

  equal(certificate.tbsCertificate.version, 3,
        "expired-ee.pem should be x509v3");

  // serialNumber
  deepEqual(certificate.tbsCertificate.serialNumber,
            [ 0x63, 0xd1, 0x11, 0x00, 0x82, 0xa3, 0xd2, 0x3b, 0x3f, 0x61, 0xb8,
              0x49, 0xa0, 0xca, 0xdc, 0x2e, 0x78, 0xfe, 0xfa, 0xea ],
            "expired-ee.pem should have expected serialNumber");

  deepEqual(certificate.tbsCertificate.signature.algorithm._values,
            [ 1, 2, 840, 113549, 1, 1, 11 ], // sha256WithRSAEncryption
            "expired-ee.pem should have sha256WithRSAEncryption signature");
  deepEqual(certificate.tbsCertificate.signature.parameters._contents, [],
            "expired-ee.pem should have NULL parameters for signature");

  equal(certificate.tbsCertificate.issuer.rdns.length, 1,
        "expired-ee.pem should have one RDN in issuer");
  equal(certificate.tbsCertificate.issuer.rdns[0].avas.length, 1,
        "expired-ee.pem should have one AVA in RDN in issuer");
  deepEqual(certificate.tbsCertificate.issuer.rdns[0].avas[0].value.value,
            stringToBytes("Test CA"),
            "expired-ee.pem should have issuer 'Test CA'");

  equal(certificate.tbsCertificate.validity.notBefore.time.getTime(),
        Date.parse("2013-01-01T00:00:00.000Z"),
        "expired-ee.pem should have the correct value for notBefore");
  equal(certificate.tbsCertificate.validity.notAfter.time.getTime(),
        Date.parse("2014-01-01T00:00:00.000Z"),
        "expired-ee.pem should have the correct value for notAfter");

  equal(certificate.tbsCertificate.subject.rdns.length, 1,
        "expired-ee.pem should have one RDN in subject");
  equal(certificate.tbsCertificate.subject.rdns[0].avas.length, 1,
        "expired-ee.pem should have one AVA in RDN in subject");
  deepEqual(certificate.tbsCertificate.subject.rdns[0].avas[0].value.value,
            stringToBytes("Expired Test End-entity"),
            "expired-ee.pem should have subject 'Expired Test End-entity'");

  deepEqual(certificate.tbsCertificate.subjectPublicKeyInfo.algorithm.algorithm._values,
            [ 1, 2, 840, 113549, 1, 1, 1 ], // rsaEncryption
            "expired-ee.pem should have a spki algorithm of rsaEncryption");

  equal(certificate.tbsCertificate.extensions.length, 2,
        "expired-ee.pem should have two extensions");

  deepEqual(certificate.signatureAlgorithm.algorithm._values,
            [ 1, 2, 840, 113549, 1, 1, 11 ], // sha256WithRSAEncryption
            "expired-ee.pem should have sha256WithRSAEncryption signatureAlgorithm");
  deepEqual(certificate.signatureAlgorithm.parameters._contents, [],
            "expired-ee.pem should have NULL parameters for signatureAlgorithm");

  equal(certificate.signatureValue.length, 2048 / 8,
        "length of signature on expired-ee.pem should be 2048 bits");
}
