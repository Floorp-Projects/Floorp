"use strict";

const HOST1 = "https://www.example.com";
const HOST2 = "https://www.mozilla.org";

const USER1 = "myuser";
const USER2 = "anotheruser";

const PASS1 = "mypass";
const PASS2 = "anotherpass";
const PASS3 = "yetanotherpass";

add_task(async function test_invalid_logins() {
  let importedLogins = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: "example.com", // Not an origin
      formActionOrigin: HOST1,
    },
    {
      username: USER1,
      // no password
      origin: HOST1,
      formActionOrigin: HOST1,
    },
    {
      username: USER2,
      password: "", // Empty password
      origin: HOST1,
      formActionOrigin: HOST1,
    },
  ]);
  Assert.equal(
    importedLogins.length,
    0,
    `Return value should indicate no imported login: ${JSON.stringify(
      importedLogins,
      null,
      2
    )}`
  );
  let savedLogins = Services.logins.getAllLogins();
  Assert.equal(
    savedLogins.length,
    0,
    `Should have no logins in storage: ${JSON.stringify(savedLogins, null, 2)}`
  );
  Services.logins.removeAllLogins();
});

add_task(async function test_new_logins() {
  let [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1 + "/",
      formActionOrigin: HOST1 + "/",
    },
  ]);
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should be 1 login for ${HOST1}`
  );

  [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST2,
      formActionOrigin: HOST2,
    },
  ]);

  Assert.ok(
    importedLogin,
    "Return value should indicate another imported login."
  );
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should still be 1 login for ${HOST1}`
  );

  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST2 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should also be 1 login for ${HOST2}`
  );
  Assert.equal(
    Services.logins.getAllLogins().length,
    2,
    "There should be 2 logins in total"
  );
  Services.logins.removeAllLogins();
});

add_task(async function test_duplicate_logins() {
  let [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
      formActionOrigin: HOST1,
    },
  ]);
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should be 1 login for ${HOST1}`
  );

  [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
      formActionOrigin: HOST1,
    },
  ]);
  Assert.ok(
    !importedLogin,
    "Return value should indicate no new login was imported."
  );
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should still be 1 login for ${HOST1}`
  );
  Services.logins.removeAllLogins();
});

add_task(async function test_different_passwords() {
  let [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
      formActionOrigin: HOST1,
      timeCreated: new Date(Date.now() - 1000),
    },
  ]);
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should be 1 login for ${HOST1}`
  );

  // This item will be newer, so its password should take precedence.
  [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS2,
      origin: HOST1,
      formActionOrigin: HOST1,
      timeCreated: new Date(),
    },
  ]);
  Assert.ok(
    !importedLogin,
    "Return value should not indicate imported login (as we updated an existing one)."
  );
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should still be 1 login for ${HOST1}`
  );
  Assert.equal(
    matchingLogins[0].password,
    PASS2,
    "We should have updated the password for this login."
  );

  // Now try to update with an older password:
  [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS3,
      origin: HOST1,
      formActionOrigin: HOST1,
      timeCreated: new Date(Date.now() - 1000000),
    },
  ]);
  Assert.ok(
    !importedLogin,
    "Return value should not indicate imported login (as we didn't update anything)."
  );
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should still be 1 login for ${HOST1}`
  );
  Assert.equal(
    matchingLogins[0].password,
    PASS2,
    "We should NOT have updated the password for this login."
  );

  Services.logins.removeAllLogins();
});

add_task(async function test_different_usernames_without_guid() {
  let [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
      formActionOrigin: HOST1,
    },
  ]);
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should be 1 login for ${HOST1}`
  );

  [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER2,
      password: PASS1,
      origin: HOST1,
      formActionOrigin: HOST1,
    },
  ]);
  Assert.ok(
    importedLogin,
    "Return value should indicate another imported login."
  );
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    2,
    `There should now be 2 logins for ${HOST1}`
  );

  Services.logins.removeAllLogins();
});

add_task(async function test_different_usernames_with_guid() {
  let [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
      formActionOrigin: HOST1,
    },
  ]);
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should be 1 login for ${HOST1}`
  );

  info("Changing both the origin and username using the GUID");
  let importedLogins = await LoginHelper.maybeImportLogins([
    {
      username: USER2,
      password: PASS1,
      origin: HOST2,
      formActionOrigin: HOST1,
      guid: importedLogin.guid,
    },
  ]);
  Assert.ok(!importedLogins.length, "Return value should indicate an update");
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST2 });
  Assert.equal(
    matchingLogins.length,
    1,
    `The 1 login for ${HOST1} should have been updated`
  );
  let storageLogin = matchingLogins[0];
  Assert.equal(storageLogin.guid, importedLogin.guid, "Check same guid");
  Assert.equal(storageLogin.username, USER2, "Check username updated");
  Assert.equal(storageLogin.origin, HOST2, "Check origin updated");

  Services.logins.removeAllLogins();
});

add_task(async function test_different_targets() {
  let [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
      formActionOrigin: HOST1,
    },
  ]);
  Assert.ok(importedLogin, "Return value should indicate imported login.");
  let matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should be 1 login for ${HOST1}`
  );

  // Not passing either a formActionOrigin or a httpRealm should be treated as
  // the same as the previous login
  [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
    },
  ]);
  Assert.ok(
    !importedLogin,
    "Return value should NOT indicate imported login " +
      "(because a missing formActionOrigin and httpRealm should be duped to the existing login)."
  );
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    1,
    `There should still be 1 login for ${HOST1}`
  );
  Assert.equal(
    matchingLogins[0].formActionOrigin,
    HOST1,
    "The form submission URL should have been kept."
  );

  [importedLogin] = await LoginHelper.maybeImportLogins([
    {
      username: USER1,
      password: PASS1,
      origin: HOST1,
      httpRealm: HOST1,
    },
  ]);
  Assert.ok(
    importedLogin,
    "Return value should indicate another imported login " +
      "as an httpRealm login shouldn't be duped."
  );
  matchingLogins = LoginHelper.searchLoginsWithObject({ origin: HOST1 });
  Assert.equal(
    matchingLogins.length,
    2,
    `There should now be 2 logins for ${HOST1}`
  );

  Services.logins.removeAllLogins();
});
