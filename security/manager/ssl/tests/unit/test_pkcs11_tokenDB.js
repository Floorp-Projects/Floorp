// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods for interfacing with the PKCS #11 token database.

// Ensure that the appropriate initialization has happened.
do_get_profile();

function run_test() {
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);

  notEqual(tokenDB.getInternalKeyToken(), null,
           "The internal token should be non-null");
}
