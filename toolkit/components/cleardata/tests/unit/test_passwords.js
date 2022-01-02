/**
 * Tests for passwords.
 */

"use strict";

const URL = "http://example.com";

const { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);

add_task(async function test_principal_downloads() {
  // Store the strings "user" and "pass" using similarly looking glyphs.
  let loginInfo = LoginTestUtils.testData.formLogin({
    origin: URL,
    formActionOrigin: URL,
    username: "admin",
    password: "12345678",
    usernameField: "field_username",
    passwordField: "field_password",
  });
  Services.logins.addLogin(loginInfo);

  Assert.equal(countLogins(URL), 1);

  let uri = Services.io.newURI(URL);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_PASSWORDS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  Assert.equal(countLogins(URL), 0);

  LoginTestUtils.clearData();
});

add_task(async function test_all() {
  // Store the strings "user" and "pass" using similarly looking glyphs.
  let loginInfo = LoginTestUtils.testData.formLogin({
    origin: URL,
    formActionOrigin: URL,
    username: "admin",
    password: "12345678",
    usernameField: "field_username",
    passwordField: "field_password",
  });
  Services.logins.addLogin(loginInfo);

  Assert.equal(countLogins(URL), 1);

  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PASSWORDS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  Assert.equal(countLogins(URL), 0);

  LoginTestUtils.clearData();
});

function countLogins(origin) {
  let count = 0;
  const logins = Services.logins.getAllLogins();
  for (const login of logins) {
    if (login.origin == origin) {
      ++count;
    }
  }

  return count;
}
