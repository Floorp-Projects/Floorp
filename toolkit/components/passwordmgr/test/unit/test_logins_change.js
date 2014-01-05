/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests methods that add, remove, and modify logins.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Verifies that the specified login is considered invalid by addLogin and by
 * modifyLogin with both nsILoginInfo and nsIPropertyBag arguments.
 *
 * This test requires that the login store is empty.
 *
 * @param aLoginInfo
 *        nsILoginInfo corresponding to an invalid login.
 * @param aExpectedError
 *        This argument is passed to the "Assert.throws" test to determine which
 *        error is expected from the modification functions.
 */
function checkLoginInvalid(aLoginInfo, aExpectedError)
{
  // Try to add the new login, and verify that no data is stored.
  Assert.throws(() => Services.logins.addLogin(aLoginInfo), aExpectedError);
  LoginTest.checkLogins([]);

  // Add a login for the modification tests.
  let testLogin = TestData.formLogin({ hostname: "http://modify.example.com" });
  Services.logins.addLogin(testLogin);

  // Try to modify the existing login using nsILoginInfo and nsIPropertyBag.
  Assert.throws(() => Services.logins.modifyLogin(testLogin, aLoginInfo),
                aExpectedError);
  Assert.throws(() => Services.logins.modifyLogin(testLogin, newPropertyBag({
    hostname: aLoginInfo.hostname,
    formSubmitURL: aLoginInfo.formSubmitURL,
    httpRealm: aLoginInfo.httpRealm,
    username: aLoginInfo.username,
    password: aLoginInfo.password,
    usernameField: aLoginInfo.usernameField,
    passwordField: aLoginInfo.passwordField,
  })), aExpectedError);

  // Verify that no data was stored by the previous calls.
  LoginTest.checkLogins([testLogin]);
  Services.logins.removeLogin(testLogin);
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Tests that adding logins to the database works.
 */
add_task(function test_addLogin_removeLogin()
{
  // Each login from the test data should be valid and added to the list.
  for (let loginInfo of TestData.loginList()) {
    Services.logins.addLogin(loginInfo);
  }
  LoginTest.checkLogins(TestData.loginList());

  // Trying to add each login again should result in an error.
  for (let loginInfo of TestData.loginList()) {
    Assert.throws(() => Services.logins.addLogin(loginInfo), /already exists/);
  }

  // Removing each login should succeed.
  for (let loginInfo of TestData.loginList()) {
    Services.logins.removeLogin(loginInfo);
  }

  LoginTest.checkLogins([]);
});

/**
 * Tests invalid combinations of httpRealm and formSubmitURL.
 *
 * For an nsILoginInfo to be valid for storage, one of the two properties should
 * be strictly equal to null, and the other must not be null or an empty string.
 *
 * The legacy case of an empty string in formSubmitURL and a null value in
 * httpRealm is also supported for storage at the moment.
 */
add_task(function test_invalid_httpRealm_formSubmitURL()
{
  // httpRealm === null, formSubmitURL === null
  checkLoginInvalid(TestData.formLogin({ formSubmitURL: null }),
                    /without a httpRealm or formSubmitURL/);

  // httpRealm === "", formSubmitURL === null
  checkLoginInvalid(TestData.authLogin({ httpRealm: "" }),
                    /without a httpRealm or formSubmitURL/);

  // httpRealm === null, formSubmitURL === ""
  // This is not enforced for now.
  // checkLoginInvalid(TestData.formLogin({ formSubmitURL: "" }),
  //                   /without a httpRealm or formSubmitURL/);

  // httpRealm === "", formSubmitURL === ""
  checkLoginInvalid(TestData.formLogin({ formSubmitURL: "", httpRealm: "" }),
                    /both a httpRealm and formSubmitURL/);

  // !!httpRealm, !!formSubmitURL
  checkLoginInvalid(TestData.formLogin({ httpRealm: "The HTTP Realm" }),
                    /both a httpRealm and formSubmitURL/);

  // httpRealm === "", !!formSubmitURL
  checkLoginInvalid(TestData.formLogin({ httpRealm: "" }),
                    /both a httpRealm and formSubmitURL/);

  // !!httpRealm, formSubmitURL === ""
  checkLoginInvalid(TestData.authLogin({ formSubmitURL: "" }),
                    /both a httpRealm and formSubmitURL/);
});

/**
 * Tests null or empty values in required login properties.
 */
add_task(function test_missing_properties()
{
  checkLoginInvalid(TestData.formLogin({ hostname: null }),
                    /null or empty hostname/);

  checkLoginInvalid(TestData.formLogin({ hostname: "" }),
                    /null or empty hostname/);

  checkLoginInvalid(TestData.formLogin({ username: null }),
                    /null username/);

  checkLoginInvalid(TestData.formLogin({ password: null }),
                    /null or empty password/);

  checkLoginInvalid(TestData.formLogin({ password: "" }),
                    /null or empty password/);
});

/**
 * Tests invalid NUL characters in nsILoginInfo properties.
 */
add_task(function test_invalid_characters()
{
  let loginList = [
    TestData.authLogin({ hostname: "http://null\0X.example.com" }),
    TestData.authLogin({ httpRealm: "realm\0" }),
    TestData.formLogin({ formSubmitURL: "http://null\0X.example.com" }),
    TestData.formLogin({ usernameField: "field\0_null" }),
    TestData.formLogin({ usernameField: ".\0" }), // Special single dot case
    TestData.formLogin({ passwordField: "field\0_null" }),
    TestData.formLogin({ username: "user\0name" }),
    TestData.formLogin({ password: "pass\0word" }),
  ];
  for (let loginInfo of loginList) {
    checkLoginInvalid(loginInfo, /login values can't contain nulls/);
  }
});

/**
 * Tests removing a login that does not exists.
 */
add_task(function test_removeLogin_nonexisting()
{
  Assert.throws(() => Services.logins.removeLogin(TestData.formLogin()),
                /No matching logins/);
});

/**
 * Tests removing all logins at once.
 */
add_task(function test_removeAllLogins()
{
  for (let loginInfo of TestData.loginList()) {
    Services.logins.addLogin(loginInfo);
  }
  Services.logins.removeAllLogins();
  LoginTest.checkLogins([]);

  // The function should also work when there are no logins to delete.
  Services.logins.removeAllLogins();
});

/**
 * Tests the modifyLogin function with an nsILoginInfo argument.
 */
add_task(function test_modifyLogin_nsILoginInfo()
{
  let loginInfo = TestData.formLogin();
  let updatedLoginInfo = TestData.formLogin({
    username: "new username",
    password: "new password",
    usernameField: "new_form_field_username",
    passwordField: "new_form_field_password",
  });
  let differentLoginInfo = TestData.authLogin();

  // Trying to modify a login that does not exist should throw.
  Assert.throws(() => Services.logins.modifyLogin(loginInfo, updatedLoginInfo),
                /No matching logins/);

  // Add the first form login, then modify it to match the second.
  Services.logins.addLogin(loginInfo);
  Services.logins.modifyLogin(loginInfo, updatedLoginInfo);

  // The data should now match the second login.
  LoginTest.checkLogins([updatedLoginInfo]);
  Assert.throws(() => Services.logins.modifyLogin(loginInfo, updatedLoginInfo),
                /No matching logins/);

  // The login can be changed to have a different type and hostname.
  Services.logins.modifyLogin(updatedLoginInfo, differentLoginInfo);
  LoginTest.checkLogins([differentLoginInfo]);

  // It is now possible to add a login with the old type and hostname.
  Services.logins.addLogin(loginInfo);
  LoginTest.checkLogins([loginInfo, differentLoginInfo]);

  // Modifying a login to match an existing one should not be possible.
  Assert.throws(
         () => Services.logins.modifyLogin(loginInfo, differentLoginInfo),
         /already exists/);
  LoginTest.checkLogins([loginInfo, differentLoginInfo]);

  LoginTest.clearData();
});

/**
 * Tests the modifyLogin function with an nsIPropertyBag argument.
 */
add_task(function test_modifyLogin_nsIProperyBag()
{
  let loginInfo = TestData.formLogin();
  let updatedLoginInfo = TestData.formLogin({
    username: "new username",
    password: "new password",
    usernameField: "",
    passwordField: "new_form_field_password",
  });
  let differentLoginInfo = TestData.authLogin();
  let differentLoginProperties = newPropertyBag({
    hostname: differentLoginInfo.hostname,
    formSubmitURL: differentLoginInfo.formSubmitURL,
    httpRealm: differentLoginInfo.httpRealm,
    username: differentLoginInfo.username,
    password: differentLoginInfo.password,
    usernameField: differentLoginInfo.usernameField,
    passwordField: differentLoginInfo.passwordField,
  });

  // Trying to modify a login that does not exist should throw.
  Assert.throws(() => Services.logins.modifyLogin(loginInfo, newPropertyBag()),
                /No matching logins/);

  // Add the first form login, then modify it to match the second, changing
  // only some of its properties and checking the behavior with an empty string.
  Services.logins.addLogin(loginInfo);
  Services.logins.modifyLogin(loginInfo, newPropertyBag({
    username: "new username",
    password: "new password",
    usernameField: "",
    passwordField: "new_form_field_password",
  }));

  // The data should now match the second login.
  LoginTest.checkLogins([updatedLoginInfo]);
  Assert.throws(() => Services.logins.modifyLogin(loginInfo, newPropertyBag()),
                /No matching logins/);

  // It is also possible to provide no properties to be modified.
  Services.logins.modifyLogin(updatedLoginInfo, newPropertyBag());

  // Specifying a null property for a required value should throw.
  Assert.throws(() => Services.logins.modifyLogin(loginInfo, newPropertyBag({
    usernameField: null,
  })));

  // The login can be changed to have a different type and hostname.
  Services.logins.modifyLogin(updatedLoginInfo, differentLoginProperties);
  LoginTest.checkLogins([differentLoginInfo]);

  // It is now possible to add a login with the old type and hostname.
  Services.logins.addLogin(loginInfo);
  LoginTest.checkLogins([loginInfo, differentLoginInfo]);

  // Modifying a login to match an existing one should not be possible.
  Assert.throws(
         () => Services.logins.modifyLogin(loginInfo, differentLoginProperties),
         /already exists/);
  LoginTest.checkLogins([loginInfo, differentLoginInfo]);

  LoginTest.clearData();
});
