/**
 * Tests for passwords.
 */

"use strict";

const URL = "http://example.com";

const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
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
  await Services.logins.addLoginAsync(loginInfo);

  Assert.equal(await countLogins(URL), 1);

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

  Assert.equal(await countLogins(URL), 0);

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
  await Services.logins.addLoginAsync(loginInfo);

  Assert.equal(await countLogins(URL), 1);

  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PASSWORDS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  Assert.equal(await countLogins(URL), 0);

  LoginTestUtils.clearData();
});

async function countLogins(origin) {
  let count = 0;
  const logins = await Services.logins.getAllLogins();
  for (const login of logins) {
    if (login.origin == origin) {
      ++count;
    }
  }

  return count;
}
