/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests X509.jsm functionality.

var { X509 } = Cu.import("resource://gre/modules/psm/X509.jsm", {});

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
  certificate.parse(readPEMToBytes("bad_certs/default-ee.pem"));

  equal(certificate.tbsCertificate.version, 3,
        "default-ee.pem should be x509v3");

  // serialNumber
  deepEqual(certificate.tbsCertificate.serialNumber,
            [ 0x1b, 0x27, 0x62, 0x4d, 0xc3, 0x70, 0xbf, 0x3d, 0xb6, 0x66,
              0x98, 0x33, 0xd8, 0x3c, 0x74, 0xd9, 0xee, 0x2c, 0x56, 0xc1 ],
            "default-ee.pem should have expected serialNumber");

  deepEqual(certificate.tbsCertificate.signature.algorithm._values,
            [ 1, 2, 840, 113549, 1, 1, 11 ], // sha256WithRSAEncryption
            "default-ee.pem should have sha256WithRSAEncryption signature");
  // TODO: there should actually be an explicit encoded NULL here, but it looks
  // like pycert doesn't include it.
  deepEqual(certificate.tbsCertificate.signature.parameters, null,
            "default-ee.pem should have NULL parameters for signature");

  equal(certificate.tbsCertificate.issuer.rdns.length, 1,
        "default-ee.pem should have one RDN in issuer");
  equal(certificate.tbsCertificate.issuer.rdns[0].avas.length, 1,
        "default-ee.pem should have one AVA in RDN in issuer");
  deepEqual(certificate.tbsCertificate.issuer.rdns[0].avas[0].value.value,
            stringToBytes("Test CA"),
            "default-ee.pem should have issuer 'Test CA'");

  equal(certificate.tbsCertificate.validity.notBefore.time.getTime(),
        Date.parse("2014-11-27T00:00:00.000Z"),
        "default-ee.pem should have the correct value for notBefore");
  equal(certificate.tbsCertificate.validity.notAfter.time.getTime(),
        Date.parse("2017-02-04T00:00:00.000Z"),
        "default-ee.pem should have the correct value for notAfter");

  equal(certificate.tbsCertificate.subject.rdns.length, 1,
        "default-ee.pem should have one RDN in subject");
  equal(certificate.tbsCertificate.subject.rdns[0].avas.length, 1,
        "default-ee.pem should have one AVA in RDN in subject");
  deepEqual(certificate.tbsCertificate.subject.rdns[0].avas[0].value.value,
            stringToBytes("Test End-entity"),
            "default-ee.pem should have subject 'Test End-entity'");

  deepEqual(certificate.tbsCertificate.subjectPublicKeyInfo.algorithm.algorithm._values,
            [ 1, 2, 840, 113549, 1, 1, 1 ], // rsaEncryption
            "default-ee.pem should have a spki algorithm of rsaEncryption");

  equal(certificate.tbsCertificate.extensions.length, 2,
        "default-ee.pem should have two extensions");

  deepEqual(certificate.signatureAlgorithm.algorithm._values,
            [ 1, 2, 840, 113549, 1, 1, 11 ], // sha256WithRSAEncryption
            "default-ee.pem should have sha256WithRSAEncryption signatureAlgorithm");
  // TODO: there should actually be an explicit encoded NULL here, but it looks
  // like pycert doesn't include it.
  deepEqual(certificate.signatureAlgorithm.parameters, null,
            "default-ee.pem should have NULL parameters for signatureAlgorithm");

  equal(certificate.signatureValue.length, 2048 / 8,
        "length of signature on default-ee.pem should be 2048 bits");
}
