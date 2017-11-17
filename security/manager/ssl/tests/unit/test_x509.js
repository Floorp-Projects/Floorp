/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests X509.jsm functionality.

// Until X509.jsm is actually used in production code, this is where we have to
// import it from.
var { X509 } = Cu.import("resource://testing-common/psm/X509.jsm", {});

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
            [ 0x2d, 0xb0, 0x79, 0x13, 0x99, 0xaa, 0xf8, 0x1f, 0x3e, 0xce, 0xac,
              0xb7, 0x53, 0xc9, 0xc1, 0x09, 0x12, 0x9e, 0x95, 0x18 ],
            "default-ee.pem should have expected serialNumber");

  deepEqual(certificate.tbsCertificate.signature.algorithm._values,
            [ 1, 2, 840, 113549, 1, 1, 11 ], // sha256WithRSAEncryption
            "default-ee.pem should have sha256WithRSAEncryption signature");
  deepEqual(certificate.tbsCertificate.signature.parameters._contents, [],
            "default-ee.pem should have NULL parameters for signature");

  equal(certificate.tbsCertificate.issuer.rdns.length, 1,
        "default-ee.pem should have one RDN in issuer");
  equal(certificate.tbsCertificate.issuer.rdns[0].avas.length, 1,
        "default-ee.pem should have one AVA in RDN in issuer");
  deepEqual(certificate.tbsCertificate.issuer.rdns[0].avas[0].value.value,
            stringToBytes("Test CA"),
            "default-ee.pem should have issuer 'Test CA'");

  equal(certificate.tbsCertificate.validity.notBefore.time.getTime(),
        Date.parse("2015-11-28T00:00:00.000Z"),
        "default-ee.pem should have the correct value for notBefore");
  equal(certificate.tbsCertificate.validity.notAfter.time.getTime(),
        Date.parse("2018-02-05T00:00:00.000Z"),
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
  deepEqual(certificate.signatureAlgorithm.parameters._contents, [],
            "default-ee.pem should have NULL parameters for signatureAlgorithm");

  equal(certificate.signatureValue.length, 2048 / 8,
        "length of signature on default-ee.pem should be 2048 bits");
}
