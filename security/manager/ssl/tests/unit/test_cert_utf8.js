// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

function run_test() {
  // This certificate has a number of placeholder byte sequences that we can
  // replace with invalid UTF-8 to ensure that we handle these cases safely.
  let certificateToAlterFile = do_get_file(
    "test_cert_utf8/certificateToAlter.pem",
    false
  );
  let certificateBytesToAlter = atob(
    pemToBase64(readFile(certificateToAlterFile))
  );
  testUTF8InField("issuerName", "ISSUER CN", certificateBytesToAlter);
  testUTF8InField("issuerOrganization", "ISSUER O", certificateBytesToAlter);
  testUTF8InField(
    "issuerOrganizationUnit",
    "ISSUER OU",
    certificateBytesToAlter
  );
  testUTF8InField("issuerCommonName", "ISSUER CN", certificateBytesToAlter);
  testUTF8InField("organization", "SUBJECT O", certificateBytesToAlter);
  testUTF8InField("organizationalUnit", "SUBJECT OU", certificateBytesToAlter);
  testUTF8InField("subjectName", "SUBJECT CN", certificateBytesToAlter);
  testUTF8InField("displayName", "SUBJECT CN", certificateBytesToAlter);
  testUTF8InField("commonName", "SUBJECT CN", certificateBytesToAlter);
  testUTF8InField(
    "emailAddress",
    "SUBJECT EMAILADDRESS",
    certificateBytesToAlter
  );
}

// Every (issuer, serial number) pair must be unique. If NSS ever encounters two
// different (in terms of encoding) certificates with the same values for this
// pair, it will refuse to import it (even as a temporary certificate). Since
// we're creating a number of different certificates, we need to ensure this
// pair is always unique. The easiest way to do this is to change the issuer
// distinguished name each time. To make sure this doesn't introduce additional
// UTF8 issues, always use a printable ASCII value.
var gUniqueIssuerCounter = 32;

function testUTF8InField(field, replacementPrefix, certificateBytesToAlter) {
  let toReplace = `${replacementPrefix} REPLACE ME`;
  let replacement = "";
  for (let i = 0; i < toReplace.length; i++) {
    replacement += "\xEB";
  }
  let bytes = certificateBytesToAlter.replace(toReplace, replacement);
  let uniqueIssuerReplacement =
    "ALWAYS MAKE ME UNIQU" + String.fromCharCode(gUniqueIssuerCounter);
  bytes = bytes.replace("ALWAYS MAKE ME UNIQUE", uniqueIssuerReplacement);
  ok(
    gUniqueIssuerCounter < 127,
    "should have enough ASCII replacements to make a unique issuer DN"
  );
  gUniqueIssuerCounter++;
  let cert = gCertDB.constructX509(stringToArray(bytes));
  notEqual(cert[field], null, `accessing nsIX509Cert.${field} shouldn't fail`);
  notEqual(
    cert.getEmailAddresses(),
    null,
    "calling nsIX509Cert.getEmailAddresses() shouldn't assert"
  );
  ok(
    !cert.containsEmailAddress("test@test.test"),
    "calling nsIX509Cert.containsEmailAddress() shouldn't assert"
  );
}
