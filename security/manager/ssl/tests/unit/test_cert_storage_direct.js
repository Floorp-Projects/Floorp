/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file consists of unit tests for cert_storage (whereas test_cert_storage.js is more of an
// integration test).

do_get_profile();

if (AppConstants.MOZ_NEW_CERT_STORAGE) {
  this.certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(Ci.nsICertStorage);
}

async function addCertBySubject(cert, subject) {
  let result = await new Promise((resolve) => {
    certStorage.addCertBySubject(btoa(cert), btoa(subject), Ci.nsICertStorage.TRUST_INHERIT,
                                 resolve);
  });
  Assert.equal(result, Cr.NS_OK, "addCertBySubject should succeed");
}

async function removeCertByHash(hashBase64) {
  let result = await new Promise((resolve) => {
    certStorage.removeCertByHash(hashBase64, resolve);
  });
  Assert.equal(result, Cr.NS_OK, "removeCertByHash should succeed");
}

function stringToArray(s) {
  let a = [];
  for (let i = 0; i < s.length; i++) {
    a.push(s.charCodeAt(i));
  }
  return a;
}

function arrayToString(a) {
  let s = "";
  for (let b of a) {
    s += String.fromCharCode(b);
  }
  return s;
}

function getLongString(uniquePart, length) {
  return String(uniquePart).padStart(length, "0");
}

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_common_subject() {
  await addCertBySubject("some certificate bytes 1", "some common subject");
  await addCertBySubject("some certificate bytes 2", "some common subject");
  await addCertBySubject("some certificate bytes 3", "some common subject");
  let storedCerts = certStorage.findCertsBySubject(stringToArray("some common subject"));
  let storedCertsAsStrings = storedCerts.map(arrayToString);
  let expectedCerts = ["some certificate bytes 1", "some certificate bytes 2",
                       "some certificate bytes 3"];
  Assert.deepEqual(storedCertsAsStrings.sort(), expectedCerts.sort(), "should find expected certs");

  await addCertBySubject("some other certificate bytes", "some other subject");
  storedCerts = certStorage.findCertsBySubject(stringToArray("some common subject"));
  storedCertsAsStrings = storedCerts.map(arrayToString);
  Assert.deepEqual(storedCertsAsStrings.sort(), expectedCerts.sort(),
                   "should still find expected certs");

  let storedOtherCerts = certStorage.findCertsBySubject(stringToArray("some other subject"));
  let storedOtherCertsAsStrings = storedOtherCerts.map(arrayToString);
  let expectedOtherCerts = ["some other certificate bytes"];
  Assert.deepEqual(storedOtherCertsAsStrings, expectedOtherCerts, "should have other certificate");
});

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_many_entries() {
  const NUM_CERTS = 500;
  const CERT_LENGTH = 3000;
  const SUBJECT_LENGTH = 40;
  for (let i = 0; i < NUM_CERTS; i++) {
    await addCertBySubject(getLongString(i, CERT_LENGTH), getLongString(i, SUBJECT_LENGTH));
  }
  for (let i = 0; i < NUM_CERTS; i++) {
    let subject = stringToArray(getLongString(i, SUBJECT_LENGTH));
    let storedCerts = certStorage.findCertsBySubject(subject);
    Assert.equal(storedCerts.length, 1, "should have 1 certificate (lots of data test)");
    let storedCertAsString = arrayToString(storedCerts[0]);
    Assert.equal(storedCertAsString, getLongString(i, CERT_LENGTH),
                 "certificate should be as expected (lots of data test)");
  }
});

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function test_removal() {
  // As long as cert_storage is given valid base64, attempting to delete some nonexistent
  // certificate will "succeed" (it'll do nothing).
  await removeCertByHash(btoa("thishashisthewrongsize"));

  await addCertBySubject("removal certificate bytes 1", "common subject to remove");
  await addCertBySubject("removal certificate bytes 2", "common subject to remove");
  await addCertBySubject("removal certificate bytes 3", "common subject to remove");

  let storedCerts = certStorage.findCertsBySubject(stringToArray("common subject to remove"));
  let storedCertsAsStrings = storedCerts.map(arrayToString);
  let expectedCerts = ["removal certificate bytes 1", "removal certificate bytes 2",
                       "removal certificate bytes 3"];
  Assert.deepEqual(storedCertsAsStrings.sort(), expectedCerts.sort(),
                   "should find expected certs before removing them");

  // echo -n "removal certificate bytes 2" | sha256sum | xxd -r -p | base64
  await removeCertByHash("2nUPHwl5TVr1mAD1FU9FivLTlTb0BAdnVUhsYgBccN4=");
  storedCerts = certStorage.findCertsBySubject(stringToArray("common subject to remove"));
  storedCertsAsStrings = storedCerts.map(arrayToString);
  expectedCerts = ["removal certificate bytes 1", "removal certificate bytes 3"];
  Assert.deepEqual(storedCertsAsStrings.sort(), expectedCerts.sort(),
                   "should only have first and third certificates now");

  // echo -n "removal certificate bytes 1" | sha256sum | xxd -r -p | base64
  await removeCertByHash("8zoRqHYrklr7Zx6UWpzrPuL+ol8KL1Ml6XHBQmXiaTY=");
  storedCerts = certStorage.findCertsBySubject(stringToArray("common subject to remove"));
  storedCertsAsStrings = storedCerts.map(arrayToString);
  expectedCerts = ["removal certificate bytes 3"];
  Assert.deepEqual(storedCertsAsStrings.sort(), expectedCerts.sort(),
                   "should only have third certificate now");

  // echo -n "removal certificate bytes 3" | sha256sum | xxd -r -p | base64
  await removeCertByHash("vZn7GwDSabB/AVo0T+N26nUsfSXIIx4NgQtSi7/0p/w=");
  storedCerts = certStorage.findCertsBySubject(stringToArray("common subject to remove"));
  Assert.equal(storedCerts.length, 0, "shouldn't have any certificates now");

  // echo -n "removal certificate bytes 3" | sha256sum | xxd -r -p | base64
  // Again, removing a nonexistent certificate should "succeed".
  await removeCertByHash("vZn7GwDSabB/AVo0T+N26nUsfSXIIx4NgQtSi7/0p/w=");
});
