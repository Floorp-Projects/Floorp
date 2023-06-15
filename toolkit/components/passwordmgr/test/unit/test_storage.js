/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the default storage module attached to the Login
 * Manager service is able to save and reload nsILoginInfo properties correctly,
 * even when they include special characters.
 */

"use strict";

// Globals

async function reloadAndCheckLoginsGen(aExpectedLogins) {
  await LoginTestUtils.reloadData();
  await LoginTestUtils.checkLogins(aExpectedLogins);
  LoginTestUtils.clearData();
}

// Tests

/**
 * Tests addLogin with valid non-ASCII characters.
 */
add_task(async function test_storage_addLogin_nonascii() {
  let origin = "http://" + String.fromCharCode(355) + ".example.com";

  // Store the strings "user" and "pass" using similarly looking glyphs.
  let loginInfo = TestData.formLogin({
    origin,
    formActionOrigin: origin,
    username: String.fromCharCode(533, 537, 7570, 345),
    password: String.fromCharCode(421, 259, 349, 537),
    usernameField: "field_" + String.fromCharCode(533, 537, 7570, 345),
    passwordField: "field_" + String.fromCharCode(421, 259, 349, 537),
  });
  await Services.logins.addLoginAsync(loginInfo);
  await reloadAndCheckLoginsGen([loginInfo]);

  // Store the string "test" using similarly looking glyphs.
  loginInfo = TestData.authLogin({
    httpRealm: String.fromCharCode(355, 277, 349, 357),
  });
  await Services.logins.addLoginAsync(loginInfo);
  await reloadAndCheckLoginsGen([loginInfo]);
});

/**
 * Tests addLogin with newline characters in the username and password.
 */
add_task(async function test_storage_addLogin_newlines() {
  let loginInfo = TestData.formLogin({
    username: "user\r\nname",
    password: "password\r\n",
  });
  await Services.logins.addLoginAsync(loginInfo);
  await reloadAndCheckLoginsGen([loginInfo]);
});

/**
 * Tests addLogin with a single dot in fields where it is allowed.
 *
 * These tests exist to verify the legacy "signons.txt" storage format.
 */
add_task(async function test_storage_addLogin_dot() {
  let loginInfo = TestData.formLogin({ origin: ".", passwordField: "." });
  await Services.logins.addLoginAsync(loginInfo);
  await reloadAndCheckLoginsGen([loginInfo]);

  loginInfo = TestData.authLogin({ httpRealm: "." });
  await Services.logins.addLoginAsync(loginInfo);
  await reloadAndCheckLoginsGen([loginInfo]);
});

/**
 * Tests addLogin with parentheses in origins.
 *
 * These tests exist to verify the legacy "signons.txt" storage format.
 */
add_task(async function test_storage_addLogin_parentheses() {
  let loginList = [
    TestData.authLogin({ httpRealm: "(realm" }),
    TestData.authLogin({ httpRealm: "realm)" }),
    TestData.authLogin({ httpRealm: "(realm)" }),
    TestData.authLogin({ httpRealm: ")realm(" }),
    TestData.authLogin({ origin: "http://parens(.example.com" }),
    TestData.authLogin({ origin: "http://parens).example.com" }),
    TestData.authLogin({ origin: "http://parens(example).example.com" }),
    TestData.authLogin({ origin: "http://parens)example(.example.com" }),
  ];
  await Services.logins.addLogins(loginList);
  await reloadAndCheckLoginsGen(loginList);
});
