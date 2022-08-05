/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  await Services.logins.initializationPromise;
});

add_task(async function test_vulnerable_password_methods() {
  const storageJSON = Services.logins.wrappedJSObject._storage.wrappedJSObject;

  let logins = TestData.loginList();
  Assert.greater(logins.length, 0, "Initial logins length should be > 0.");

  for (let loginInfo of logins) {
    Services.logins.addLogin(loginInfo);
    Assert.ok(
      !storageJSON.isPotentiallyVulnerablePassword(loginInfo),
      "No logins should be vulnerable until addVulnerablePasswords is called."
    );
  }

  const vulnerableLogin = logins.shift();
  storageJSON.addPotentiallyVulnerablePassword(vulnerableLogin);

  Assert.ok(
    storageJSON.isPotentiallyVulnerablePassword(vulnerableLogin),
    "Login should be vulnerable after calling addVulnerablePassword."
  );
  for (let loginInfo of logins) {
    Assert.ok(
      !storageJSON.isPotentiallyVulnerablePassword(loginInfo),
      "No other logins should be vulnerable when addVulnerablePassword is called" +
        " with a single argument"
    );
  }

  storageJSON.clearAllPotentiallyVulnerablePasswords();

  for (let loginInfo of logins) {
    Assert.ok(
      !storageJSON.isPotentiallyVulnerablePassword(loginInfo),
      "No logins should be vulnerable when clearAllPotentiallyVulnerablePasswords is called."
    );
  }
});
