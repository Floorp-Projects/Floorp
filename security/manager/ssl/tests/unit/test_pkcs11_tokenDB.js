// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods for interfacing with the PKCS #11 token database.

// Ensure that the appropriate initialization has happened.
do_get_profile();

function run_test() {
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);

  let tokenListPreLoad = tokenDB.listTokens();
  while (tokenListPreLoad.hasMoreElements()) {
    let token = tokenListPreLoad.getNext().QueryInterface(Ci.nsIPK11Token);
    notEqual(token.tokenLabel, "Test PKCS11 Tokeñ Label",
             "Test PKCS11 Token 1 should not be listed prior to module load");
    notEqual(token.tokenLabel, "Test PKCS11 Tokeñ 2 Label",
             "Test PKCS11 Token 2 should not be listed prior to module load");
  }

  throws(() => tokenDB.findTokenByName("Test PKCS11 Tokeñ Label"),
         /NS_ERROR_FAILURE/,
         "Non-present test token 1 should not be findable by name");
  throws(() => tokenDB.findTokenByName("Test PKCS11 Tokeñ 2 Label"),
         /NS_ERROR_FAILURE/,
         "Non-present test token 2 should not be findable by name");

  loadPKCS11TestModule(false);

  // Test Token 1 is simulated to insert and remove itself in a tight loop, so
  // we don't bother testing that it's present.
  let tokenListPostLoad = tokenDB.listTokens();
  let foundTokenNames = [];
  while (tokenListPostLoad.hasMoreElements()) {
    let token = tokenListPostLoad.getNext().QueryInterface(Ci.nsIPK11Token);
    foundTokenNames.push(token.tokenName);
  }
  ok(foundTokenNames.includes("Test PKCS11 Tokeñ 2 Label"),
     "Test PKCS11 Token 2 should be listed after module load");

  notEqual(tokenDB.findTokenByName("Test PKCS11 Tokeñ 2 Label"), null,
           "Test token 2 should be findable by name after loading test module");

  throws(() => tokenDB.findTokenByName(""), /NS_ERROR_ILLEGAL_VALUE/,
         "nsIPK11TokenDB.findTokenByName should throw given an empty name");
}
