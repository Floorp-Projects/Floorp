/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file consists of unit tests for cert_storage (whereas test_cert_storage.js is more of an
// integration test).

do_get_profile();

if (AppConstants.MOZ_NEW_CERT_STORAGE) {
  this.certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
    Ci.nsICertStorage
  );
}

async function addCerts(certInfos) {
  let result = await new Promise(resolve => {
    certStorage.addCerts(certInfos, resolve);
  });
  Assert.equal(result, Cr.NS_OK, "addCerts should succeed");
}

async function removeCertsByHashes(hashesBase64) {
  let result = await new Promise(resolve => {
    certStorage.removeCertsByHashes(hashesBase64, resolve);
  });
  Assert.equal(result, Cr.NS_OK, "removeCertsByHashes should succeed");
}

function getLongString(uniquePart, length) {
  return String(uniquePart).padStart(length, "0");
}

class CertInfo {
  constructor(cert, subject) {
    this.cert = btoa(cert);
    this.subject = btoa(subject);
    this.trust = Ci.nsICertStorage.TRUST_INHERIT;
  }
}
if (AppConstants.MOZ_NEW_CERT_STORAGE) {
  CertInfo.prototype.QueryInterface = ChromeUtils.generateQI([Ci.nsICertInfo]);
}

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_common_subject() {
    let someCert1 = new CertInfo(
      "some certificate bytes 1",
      "some common subject"
    );
    let someCert2 = new CertInfo(
      "some certificate bytes 2",
      "some common subject"
    );
    let someCert3 = new CertInfo(
      "some certificate bytes 3",
      "some common subject"
    );
    await addCerts([someCert1, someCert2, someCert3]);
    let storedCerts = certStorage.findCertsBySubject(
      stringToArray("some common subject")
    );
    let storedCertsAsStrings = storedCerts.map(arrayToString);
    let expectedCerts = [
      "some certificate bytes 1",
      "some certificate bytes 2",
      "some certificate bytes 3",
    ];
    Assert.deepEqual(
      storedCertsAsStrings.sort(),
      expectedCerts.sort(),
      "should find expected certs"
    );

    await addCerts([
      new CertInfo("some other certificate bytes", "some other subject"),
    ]);
    storedCerts = certStorage.findCertsBySubject(
      stringToArray("some common subject")
    );
    storedCertsAsStrings = storedCerts.map(arrayToString);
    Assert.deepEqual(
      storedCertsAsStrings.sort(),
      expectedCerts.sort(),
      "should still find expected certs"
    );

    let storedOtherCerts = certStorage.findCertsBySubject(
      stringToArray("some other subject")
    );
    let storedOtherCertsAsStrings = storedOtherCerts.map(arrayToString);
    let expectedOtherCerts = ["some other certificate bytes"];
    Assert.deepEqual(
      storedOtherCertsAsStrings,
      expectedOtherCerts,
      "should have other certificate"
    );
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_many_entries() {
    const NUM_CERTS = 500;
    const CERT_LENGTH = 3000;
    const SUBJECT_LENGTH = 40;
    let certs = [];
    for (let i = 0; i < NUM_CERTS; i++) {
      certs.push(
        new CertInfo(
          getLongString(i, CERT_LENGTH),
          getLongString(i, SUBJECT_LENGTH)
        )
      );
    }
    await addCerts(certs);
    for (let i = 0; i < NUM_CERTS; i++) {
      let subject = stringToArray(getLongString(i, SUBJECT_LENGTH));
      let storedCerts = certStorage.findCertsBySubject(subject);
      Assert.equal(
        storedCerts.length,
        1,
        "should have 1 certificate (lots of data test)"
      );
      let storedCertAsString = arrayToString(storedCerts[0]);
      Assert.equal(
        storedCertAsString,
        getLongString(i, CERT_LENGTH),
        "certificate should be as expected (lots of data test)"
      );
    }
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_removal() {
    // As long as cert_storage is given valid base64, attempting to delete some nonexistent
    // certificate will "succeed" (it'll do nothing).
    await removeCertsByHashes([btoa("thishashisthewrongsize")]);

    let removalCert1 = new CertInfo(
      "removal certificate bytes 1",
      "common subject to remove"
    );
    let removalCert2 = new CertInfo(
      "removal certificate bytes 2",
      "common subject to remove"
    );
    let removalCert3 = new CertInfo(
      "removal certificate bytes 3",
      "common subject to remove"
    );
    await addCerts([removalCert1, removalCert2, removalCert3]);

    let storedCerts = certStorage.findCertsBySubject(
      stringToArray("common subject to remove")
    );
    let storedCertsAsStrings = storedCerts.map(arrayToString);
    let expectedCerts = [
      "removal certificate bytes 1",
      "removal certificate bytes 2",
      "removal certificate bytes 3",
    ];
    Assert.deepEqual(
      storedCertsAsStrings.sort(),
      expectedCerts.sort(),
      "should find expected certs before removing them"
    );

    // echo -n "removal certificate bytes 2" | sha256sum | xxd -r -p | base64
    await removeCertsByHashes(["2nUPHwl5TVr1mAD1FU9FivLTlTb0BAdnVUhsYgBccN4="]);
    storedCerts = certStorage.findCertsBySubject(
      stringToArray("common subject to remove")
    );
    storedCertsAsStrings = storedCerts.map(arrayToString);
    expectedCerts = [
      "removal certificate bytes 1",
      "removal certificate bytes 3",
    ];
    Assert.deepEqual(
      storedCertsAsStrings.sort(),
      expectedCerts.sort(),
      "should only have first and third certificates now"
    );

    // echo -n "removal certificate bytes 1" | sha256sum | xxd -r -p | base64
    await removeCertsByHashes(["8zoRqHYrklr7Zx6UWpzrPuL+ol8KL1Ml6XHBQmXiaTY="]);
    storedCerts = certStorage.findCertsBySubject(
      stringToArray("common subject to remove")
    );
    storedCertsAsStrings = storedCerts.map(arrayToString);
    expectedCerts = ["removal certificate bytes 3"];
    Assert.deepEqual(
      storedCertsAsStrings.sort(),
      expectedCerts.sort(),
      "should only have third certificate now"
    );

    // echo -n "removal certificate bytes 3" | sha256sum | xxd -r -p | base64
    await removeCertsByHashes(["vZn7GwDSabB/AVo0T+N26nUsfSXIIx4NgQtSi7/0p/w="]);
    storedCerts = certStorage.findCertsBySubject(
      stringToArray("common subject to remove")
    );
    Assert.equal(storedCerts.length, 0, "shouldn't have any certificates now");

    // echo -n "removal certificate bytes 3" | sha256sum | xxd -r -p | base64
    // Again, removing a nonexistent certificate should "succeed".
    await removeCertsByHashes(["vZn7GwDSabB/AVo0T+N26nUsfSXIIx4NgQtSi7/0p/w="]);
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_batched_removal() {
    let removalCert1 = new CertInfo(
      "batch removal certificate bytes 1",
      "batch subject to remove"
    );
    let removalCert2 = new CertInfo(
      "batch removal certificate bytes 2",
      "batch subject to remove"
    );
    let removalCert3 = new CertInfo(
      "batch removal certificate bytes 3",
      "batch subject to remove"
    );
    await addCerts([removalCert1, removalCert2, removalCert3]);
    let storedCerts = certStorage.findCertsBySubject(
      stringToArray("batch subject to remove")
    );
    let storedCertsAsStrings = storedCerts.map(arrayToString);
    let expectedCerts = [
      "batch removal certificate bytes 1",
      "batch removal certificate bytes 2",
      "batch removal certificate bytes 3",
    ];
    Assert.deepEqual(
      storedCertsAsStrings.sort(),
      expectedCerts.sort(),
      "should find expected certs before removing them"
    );
    // echo -n "batch removal certificate bytes 1" | sha256sum | xxd -r -p | base64
    // echo -n "batch removal certificate bytes 2" | sha256sum | xxd -r -p | base64
    // echo -n "batch removal certificate bytes 3" | sha256sum | xxd -r -p | base64
    await removeCertsByHashes([
      "EOEEUTuanHZX9NFVCoMKVT22puIJC6g+ZuNPpJgvaa8=",
      "Xz6h/Kvn35cCLJEZXkjPqk1GG36b56sreLyAXpO+0zg=",
      "Jr7XdiTT8ZONUL+ogNNMW2oxKxanvYOLQPKBPgH/has=",
    ]);
    storedCerts = certStorage.findCertsBySubject(
      stringToArray("batch subject to remove")
    );
    Assert.equal(storedCerts.length, 0, "shouldn't have any certificates now");
  }
);

class CRLiteState {
  constructor(subject, spkiHash, state) {
    this.subject = btoa(subject);
    this.spkiHash = spkiHash;
    this.state = state;
  }
}
if (AppConstants.MOZ_NEW_CERT_STORAGE) {
  CRLiteState.prototype.QueryInterface = ChromeUtils.generateQI([
    Ci.nsICRLiteState,
  ]);
}

async function addCRLiteState(state) {
  let result = await new Promise(resolve => {
    certStorage.setCRLiteState(state, resolve);
  });
  Assert.equal(result, Cr.NS_OK, "setCRLiteState should succeed");
}

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_state() {
    // echo -n "some spki 1" | sha256sum | xxd -r -p | base64
    let crliteState1 = new CRLiteState(
      "some subject 1",
      "bDlKlhR5ptlvuxclnZ3RQHznG8/3pgIybrRJ/Zvn9L8=",
      Ci.nsICertStorage.STATE_ENFORCE
    );
    // echo -n "some spki 2" | sha256sum | xxd -r -p | base64
    let crliteState2 = new CRLiteState(
      "some subject 2",
      "ZlXvlHhtdx4yKwkhZqg7Opv5T1ofwzorlsCoLf0wnlY=",
      Ci.nsICertStorage.STATE_UNSET
    );
    // echo -n "some spki 3" | sha256sum | xxd -r -p | base64
    let crliteState3 = new CRLiteState(
      "some subject 3",
      "pp1SRn6njaHX/c+b2uf82JPeBkWhPfTBp/Mxb3xkjRM=",
      Ci.nsICertStorage.STATE_ENFORCE
    );
    await addCRLiteState([crliteState1, crliteState2, crliteState3]);

    let state1 = certStorage.getCRLiteState(
      stringToArray("some subject 1"),
      stringToArray("some spki 1")
    );
    Assert.equal(state1, Ci.nsICertStorage.STATE_ENFORCE);
    let state2 = certStorage.getCRLiteState(
      stringToArray("some subject 2"),
      stringToArray("some spki 2")
    );
    Assert.equal(state2, Ci.nsICertStorage.STATE_UNSET);
    let state3 = certStorage.getCRLiteState(
      stringToArray("some subject 3"),
      stringToArray("some spki 3")
    );
    Assert.equal(state3, Ci.nsICertStorage.STATE_ENFORCE);

    // Check that if we never set the state of a particular subject/spki pair, we get "unset" when we
    // look it up.
    let stateNeverSet = certStorage.getCRLiteState(
      stringToArray("some unknown subject"),
      stringToArray("some unknown spki")
    );
    Assert.equal(stateNeverSet, Ci.nsICertStorage.STATE_UNSET);

    // "some subject 1"/"some spki 1" and "some subject 3"/"some spki 3" both have their CRLite state
    // set. However, the combination of "some subject 3"/"some spki1" should not.
    let stateDifferentSubjectSPKI = certStorage.getCRLiteState(
      stringToArray("some subject 3"),
      stringToArray("some spki 1")
    );
    Assert.equal(stateDifferentSubjectSPKI, Ci.nsICertStorage.STATE_UNSET);

    let anotherStateDifferentSubjectSPKI = certStorage.getCRLiteState(
      stringToArray("some subject 1"),
      stringToArray("some spki 2")
    );
    Assert.equal(
      anotherStateDifferentSubjectSPKI,
      Ci.nsICertStorage.STATE_UNSET
    );
    let yetAnotherStateDifferentSubjectSPKI = certStorage.getCRLiteState(
      stringToArray("some subject 2"),
      stringToArray("some spki 1")
    );
    Assert.equal(
      yetAnotherStateDifferentSubjectSPKI,
      Ci.nsICertStorage.STATE_UNSET
    );

    crliteState3 = new CRLiteState(
      "some subject 3",
      "pp1SRn6njaHX/c+b2uf82JPeBkWhPfTBp/Mxb3xkjRM=",
      Ci.nsICertStorage.STATE_UNSET
    );
    await addCRLiteState([crliteState3]);
    state3 = certStorage.getCRLiteState(
      stringToArray("some subject 3"),
      stringToArray("some spki 3")
    );
    Assert.equal(state3, Ci.nsICertStorage.STATE_UNSET);

    crliteState2 = new CRLiteState(
      "some subject 2",
      "ZlXvlHhtdx4yKwkhZqg7Opv5T1ofwzorlsCoLf0wnlY=",
      Ci.nsICertStorage.STATE_ENFORCE
    );
    await addCRLiteState([crliteState2]);
    state2 = certStorage.getCRLiteState(
      stringToArray("some subject 2"),
      stringToArray("some spki 2")
    );
    Assert.equal(state2, Ci.nsICertStorage.STATE_ENFORCE);

    // Inserting a subject/spki pair with a state value outside of our expected
    // values will succeed. However, since our data type is a signed 16-bit value,
    // values outside that range will be truncated. The least significant 16 bits
    // of 2013003773 are FFFD, which when interpreted as a signed 16-bit integer
    // comes out to -3.
    // echo -n "some spki 4" | sha256sum | xxd -r -p | base64
    let bogusValueState = new CRLiteState(
      "some subject 4",
      "1eA0++hCqzt8vpzREYSqHAqpEOLchZca1Gx8viCVYzc=",
      2013003773
    );
    await addCRLiteState([bogusValueState]);
    let bogusValueStateValue = certStorage.getCRLiteState(
      stringToArray("some subject 4"),
      stringToArray("some spki 4")
    );
    Assert.equal(bogusValueStateValue, -3);
  }
);
