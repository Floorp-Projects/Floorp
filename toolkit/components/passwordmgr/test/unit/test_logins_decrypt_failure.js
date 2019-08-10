/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the case where there are logins that cannot be decrypted.
 */

"use strict";

// Globals

/**
 * Resets the token used to decrypt logins.  This is equivalent to resetting the
 * master password when it is not known.
 */
function resetMasterPassword() {
  let token = Cc["@mozilla.org/security/pk11tokendb;1"]
    .getService(Ci.nsIPK11TokenDB)
    .getInternalKeyToken();
  token.reset();
  token.initPassword("");
}

// Tests

/**
 * Resets the master password after some logins were added to the database.
 */
add_task(function test_logins_decrypt_failure() {
  let logins = TestData.loginList();
  for (let loginInfo of logins) {
    Services.logins.addLogin(loginInfo);
  }

  // This makes the existing logins non-decryptable.
  resetMasterPassword();

  // These functions don't see the non-decryptable entries anymore.
  Assert.equal(Services.logins.getAllLogins().length, 0);
  Assert.equal(Services.logins.findLogins("", "", "").length, 0);
  Assert.equal(Services.logins.searchLogins(newPropertyBag()).length, 0);
  Assert.throws(
    () => Services.logins.modifyLogin(logins[0], newPropertyBag()),
    /No matching logins/
  );
  Assert.throws(
    () => Services.logins.removeLogin(logins[0]),
    /No matching logins/
  );

  // The function that counts logins sees the non-decryptable entries also.
  Assert.equal(Services.logins.countLogins("", "", ""), logins.length);

  // Equivalent logins can be added.
  for (let loginInfo of logins) {
    Services.logins.addLogin(loginInfo);
  }
  LoginTestUtils.checkLogins(logins);
  Assert.equal(Services.logins.countLogins("", "", ""), logins.length * 2);

  // Finding logins doesn't return the non-decryptable duplicates.
  Assert.equal(
    Services.logins.findLogins("http://www.example.com", "", "").length,
    1
  );
  let matchData = newPropertyBag({ origin: "http://www.example.com" });
  Assert.equal(Services.logins.searchLogins(matchData).length, 1);

  // Removing single logins does not remove non-decryptable logins.
  for (let loginInfo of TestData.loginList()) {
    Services.logins.removeLogin(loginInfo);
  }
  Assert.equal(Services.logins.getAllLogins().length, 0);
  Assert.equal(Services.logins.countLogins("", "", ""), logins.length);

  // Removing all logins removes the non-decryptable entries also.
  Services.logins.removeAllLogins();
  Assert.equal(Services.logins.getAllLogins().length, 0);
  Assert.equal(Services.logins.countLogins("", "", ""), 0);
});

// Bug 621846 - If a login has a GUID but can't be decrypted, a search for
// that GUID will (correctly) fail. Ensure we can add a new login with that
// same GUID.
add_task(function test_add_logins_with_decrypt_failure() {
  // a login with a GUID.
  let login = new LoginInfo(
    "http://www.example2.com",
    "http://www.example2.com",
    null,
    "the username",
    "the password for www.example.com",
    "form_field_username",
    "form_field_password"
  );

  login.QueryInterface(Ci.nsILoginMetaInfo);
  login.guid = "{4bc50d2f-dbb6-4aa3-807c-c4c2065a2c35}";

  // A different login but with the same GUID.
  let loginDupeGuid = new LoginInfo(
    "http://www.example3.com",
    "http://www.example3.com",
    null,
    "the username",
    "the password",
    "form_field_username",
    "form_field_password"
  );
  loginDupeGuid.QueryInterface(Ci.nsILoginMetaInfo);
  loginDupeGuid.guid = login.guid;

  Services.logins.addLogin(login);

  // We can search for this login by GUID.
  let searchProp = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag2
  );
  searchProp.setPropertyAsAUTF8String("guid", login.guid);

  equal(Services.logins.searchLogins(searchProp).length, 1);

  // We should fail to re-add it as it remains good.
  Assert.throws(
    () => Services.logins.addLogin(login),
    /This login already exists./
  );
  // We should fail to re-add a different login with the same GUID.
  Assert.throws(
    () => Services.logins.addLogin(loginDupeGuid),
    /specified GUID already exists/
  );

  // This makes the existing login non-decryptable.
  resetMasterPassword();

  // We can no longer find it in our search.
  equal(Services.logins.searchLogins(searchProp).length, 0);

  // So we should be able to re-add a login with that same GUID.
  Services.logins.addLogin(login);
  equal(Services.logins.searchLogins(searchProp).length, 1);

  Services.logins.removeAllLogins();
});
