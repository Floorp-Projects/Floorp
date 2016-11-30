"use strict";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LoginHelper.jsm");

const HOST1 = "https://www.example.com/";
const HOST2 = "https://www.mozilla.org/";

const USER1 = "myuser";
const USER2 = "anotheruser";

const PASS1 = "mypass";
const PASS2 = "anotherpass";
const PASS3 = "yetanotherpass";

add_task(function test_new_logins() {
  let importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
    formSubmitURL: HOST1,
  });
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should be 1 login for ${HOST1}`);

  importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST2,
    formSubmitURL: HOST2,
  });

  Assert.ok(importedLogin, "Return value should indicate another imported login.");
  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should still be 1 login for ${HOST1}`);

  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST2});
  Assert.equal(matchingLogins.length, 1, `There should also be 1 login for ${HOST2}`);
  Assert.equal(Services.logins.getAllLogins().length, 2, "There should be 2 logins in total");
  Services.logins.removeAllLogins();
});

add_task(function test_duplicate_logins() {
  let importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
    formSubmitURL: HOST1,
  });
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should be 1 login for ${HOST1}`);

  importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
    formSubmitURL: HOST1,
  });
  Assert.ok(!importedLogin, "Return value should indicate no new login was imported.");
  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should still be 1 login for ${HOST1}`);
  Services.logins.removeAllLogins();
});

add_task(function test_different_passwords() {
  let importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
    formSubmitURL: HOST1,
    timeCreated: new Date(Date.now() - 1000),
  });
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should be 1 login for ${HOST1}`);

  // This item will be newer, so its password should take precedence.
  importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS2,
    hostname: HOST1,
    formSubmitURL: HOST1,
    timeCreated: new Date(),
  });
  Assert.ok(!importedLogin, "Return value should not indicate imported login (as we updated an existing one).");
  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should still be 1 login for ${HOST1}`);
  Assert.equal(matchingLogins[0].password, PASS2, "We should have updated the password for this login.");

  // Now try to update with an older password:
  importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS3,
    hostname: HOST1,
    formSubmitURL: HOST1,
    timeCreated: new Date(Date.now() - 1000000),
  });
  Assert.ok(!importedLogin, "Return value should not indicate imported login (as we didn't update anything).");
  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should still be 1 login for ${HOST1}`);
  Assert.equal(matchingLogins[0].password, PASS2, "We should NOT have updated the password for this login.");

  Services.logins.removeAllLogins();
});

add_task(function test_different_usernames() {
  let importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
    formSubmitURL: HOST1,
  });
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should be 1 login for ${HOST1}`);

  importedLogin = LoginHelper.maybeImportLogin({
    username: USER2,
    password: PASS1,
    hostname: HOST1,
    formSubmitURL: HOST1,
  });
  Assert.ok(importedLogin, "Return value should indicate another imported login.");
  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 2, `There should now be 2 logins for ${HOST1}`);

  Services.logins.removeAllLogins();
});

add_task(function test_different_targets() {
  let importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
    formSubmitURL: HOST1,
  });
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should be 1 login for ${HOST1}`);

  // Not passing either a formSubmitURL or a httpRealm should be treated as
  // the same as the previous login
  importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
  });
  Assert.ok(!importedLogin, "Return value should NOT indicate imported login " +
    "(because a missing formSubmitURL and httpRealm should be duped to the existing login).");
  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 1, `There should still be 1 login for ${HOST1}`);
  Assert.equal(matchingLogins[0].formSubmitURL, HOST1, "The form submission URL should have been kept.");

  importedLogin = LoginHelper.maybeImportLogin({
    username: USER1,
    password: PASS1,
    hostname: HOST1,
    httpRealm: HOST1,
  });
  Assert.ok(importedLogin, "Return value should indicate another imported login " +
    "as an httpRealm login shouldn't be duped.");
  matchingLogins = LoginHelper.searchLoginsWithObject({hostname: HOST1});
  Assert.equal(matchingLogins.length, 2, `There should now be 2 logins for ${HOST1}`);

  Services.logins.removeAllLogins();
});

