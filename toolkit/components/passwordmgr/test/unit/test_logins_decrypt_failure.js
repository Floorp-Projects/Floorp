/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the case where there are logins that cannot be decrypted.
 */

"use strict";

// //////////////////////////////////////////////////////////////////////////////
// // Globals

/**
 * Resets the token used to decrypt logins.  This is equivalent to resetting the
 * master password when it is not known.
 */
function resetMasterPassword()
{
  let token = Cc["@mozilla.org/security/pk11tokendb;1"]
                .getService(Ci.nsIPK11TokenDB).getInternalKeyToken();
  token.reset();
  token.changePassword("", "");
}

// //////////////////////////////////////////////////////////////////////////////
// // Tests

/**
 * Resets the master password after some logins were added to the database.
 */
add_task(function test_logins_decrypt_failure()
{
  let logins = TestData.loginList();
  for (let loginInfo of logins) {
    Services.logins.addLogin(loginInfo);
  }

  // This makes the existing logins non-decryptable.
  resetMasterPassword();

  // These functions don't see the non-decryptable entries anymore.
  do_check_eq(Services.logins.getAllLogins().length, 0);
  do_check_eq(Services.logins.findLogins({}, "", "", "").length, 0);
  do_check_eq(Services.logins.searchLogins({}, newPropertyBag()).length, 0);
  Assert.throws(() => Services.logins.modifyLogin(logins[0], newPropertyBag()),
                      /No matching logins/);
  Assert.throws(() => Services.logins.removeLogin(logins[0]),
                      /No matching logins/);

  // The function that counts logins sees the non-decryptable entries also.
  do_check_eq(Services.logins.countLogins("", "", ""), logins.length);

  // Equivalent logins can be added.
  for (let loginInfo of logins) {
    Services.logins.addLogin(loginInfo);
  }
  LoginTestUtils.checkLogins(logins);
  do_check_eq(Services.logins.countLogins("", "", ""), logins.length * 2);

  // Finding logins doesn't return the non-decryptable duplicates.
  do_check_eq(Services.logins.findLogins({}, "http://www.example.com",
                                         "", "").length, 1);
  let matchData = newPropertyBag({ hostname: "http://www.example.com" });
  do_check_eq(Services.logins.searchLogins({}, matchData).length, 1);

  // Removing single logins does not remove non-decryptable logins.
  for (let loginInfo of TestData.loginList()) {
    Services.logins.removeLogin(loginInfo);
  }
  do_check_eq(Services.logins.getAllLogins().length, 0);
  do_check_eq(Services.logins.countLogins("", "", ""), logins.length);

  // Removing all logins removes the non-decryptable entries also.
  Services.logins.removeAllLogins();
  do_check_eq(Services.logins.getAllLogins().length, 0);
  do_check_eq(Services.logins.countLogins("", "", ""), 0);
});
