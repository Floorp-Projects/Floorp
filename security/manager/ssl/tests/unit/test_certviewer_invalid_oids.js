// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Checks that invalid OID encodings are detected in the Cert Viewer Details tab.

do_get_profile(); // Must be called before getting nsIX509CertDB
const certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function certFromFile(filename) {
  return constructCertFromFile(`test_certviewer_invalid_oids/${filename}.pem`);
}

function test(certFilename, expectedOIDText) {
  let cert = certFromFile(certFilename);
  let certDumpTree = Cc["@mozilla.org/security/nsASN1Tree;1"]
                       .createInstance(Ci.nsIASN1Tree);
  certDumpTree.loadASN1Structure(cert.ASN1Structure);
  let actualOIDText = certDumpTree.getDisplayData(9);

  equal(actualOIDText, expectedOIDText,
        "Actual and expected OID text should match");
}

function run_test() {
  test("bug483440-attack2b",
       "Object Identifier (2 5 4 Unknown) = www.bank.com\n" +
       "OU = Hacking Division\n" +
       "CN = www.badguy.com\nO = Badguy Inc\n");

  test("bug483440-pk10oflo",
       "Object Identifier (2 5 4 Unknown) = www.bank.com\n" +
       "OU = Hacking Division\n" +
       "CN = www.badguy.com\nO = Badguy Inc\n");

  test("bug483440-attack7",

       // Check 88 80 80 80 01, not leading, have to pass
       "Object Identifier (2 5 4 2147483649) = attack1\n" +

       // Check 90 80 80 80 01, not leading, have to fail
       "Object Identifier (2 5 4 Unknown) = attack2\n" +

       // Check 80 80 80 80 80, not leading, have to fail
       "Object Identifier (2 5 4 Unknown) = attack3\n" +

       // Check 81 81, trailing, have to fail
       "Object Identifier (2 5 4 3 Unknown) = attack4\n" +

       // Check FF FF FF 7F, not leading, have to pass
       "Object Identifier (2 5 4 268435455) = attack5\n" +

       // Check 80 leading, have to fail
       "Object Identifier (Unknown 3) = attack6\n" +

       // Check 14757 = 2*40 + 14677 leading single byte encoded as F325,
       // have to pass
       "Object Identifier (2 14677 4 3) = attack7\n");
}
