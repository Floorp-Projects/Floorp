/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests methods that add, remove, and modify logins.
 */

"use strict";

// Globals

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
function checkLoginInvalid(aLoginInfo, aExpectedError) {
  // Try to add the new login, and verify that no data is stored.
  Assert.throws(() => Services.logins.addLogin(aLoginInfo), aExpectedError);
  LoginTestUtils.checkLogins([]);

  // Add a login for the modification tests.
  let testLogin = TestData.formLogin({ origin: "http://modify.example.com" });
  Services.logins.addLogin(testLogin);

  // Try to modify the existing login using nsILoginInfo and nsIPropertyBag.
  Assert.throws(
    () => Services.logins.modifyLogin(testLogin, aLoginInfo),
    aExpectedError
  );
  Assert.throws(
    () =>
      Services.logins.modifyLogin(
        testLogin,
        newPropertyBag({
          origin: aLoginInfo.origin,
          formActionOrigin: aLoginInfo.formActionOrigin,
          httpRealm: aLoginInfo.httpRealm,
          username: aLoginInfo.username,
          password: aLoginInfo.password,
          usernameField: aLoginInfo.usernameField,
          passwordField: aLoginInfo.passwordField,
        })
      ),
    aExpectedError
  );

  // Verify that no data was stored by the previous calls.
  LoginTestUtils.checkLogins([testLogin]);
  Services.logins.removeLogin(testLogin);
}

/**
 * Verifies that two objects are not the same instance
 * but have equal attributes.
 *
 * @param {Object} objectA
 *        An object to compare.
 *
 * @param {Object} objectB
 *        Another object to compare.
 *
 * @param {string[]} attributes
 *        Attributes to compare.
 *
 * @return true if all passed attributes are equal for both objects, false otherwise.
 */
function compareAttributes(objectA, objectB, attributes) {
  // If it's the same object, we want to return false.
  if (objectA == objectB) {
    return false;
  }
  return attributes.every(attr => objectA[attr] == objectB[attr]);
}

// Tests

/**
 * Tests that adding logins to the database works.
 */
add_task(function test_addLogin_removeLogin() {
  // Each login from the test data should be valid and added to the list.
  for (let loginInfo of TestData.loginList()) {
    Services.logins.addLogin(loginInfo);
  }
  LoginTestUtils.checkLogins(TestData.loginList());

  // Trying to add each login again should result in an error.
  for (let loginInfo of TestData.loginList()) {
    Assert.throws(() => Services.logins.addLogin(loginInfo), /already exists/);
  }

  // Removing each login should succeed.
  for (let loginInfo of TestData.loginList()) {
    Services.logins.removeLogin(loginInfo);
  }

  LoginTestUtils.checkLogins([]);
});

/**
 * Tests invalid combinations of httpRealm and formActionOrigin.
 *
 * For an nsILoginInfo to be valid for storage, one of the two properties should
 * be strictly equal to null, and the other must not be null or an empty string.
 *
 * The legacy case of an empty string in formActionOrigin and a null value in
 * httpRealm is also supported for storage at the moment.
 */
add_task(function test_invalid_httpRealm_formActionOrigin() {
  // httpRealm === null, formActionOrigin === null
  checkLoginInvalid(
    TestData.formLogin({ formActionOrigin: null }),
    /without a httpRealm or formActionOrigin/
  );

  // httpRealm === "", formActionOrigin === null
  checkLoginInvalid(
    TestData.authLogin({ httpRealm: "" }),
    /without a httpRealm or formActionOrigin/
  );

  // httpRealm === null, formActionOrigin === ""
  // This is not enforced for now.
  // checkLoginInvalid(TestData.formLogin({ formActionOrigin: "" }),
  //                   /without a httpRealm or formActionOrigin/);

  // httpRealm === "", formActionOrigin === ""
  checkLoginInvalid(
    TestData.formLogin({ formActionOrigin: "", httpRealm: "" }),
    /both a httpRealm and formActionOrigin/
  );

  // !!httpRealm, !!formActionOrigin
  checkLoginInvalid(
    TestData.formLogin({ httpRealm: "The HTTP Realm" }),
    /both a httpRealm and formActionOrigin/
  );

  // httpRealm === "", !!formActionOrigin
  checkLoginInvalid(
    TestData.formLogin({ httpRealm: "" }),
    /both a httpRealm and formActionOrigin/
  );

  // !!httpRealm, formActionOrigin === ""
  checkLoginInvalid(
    TestData.authLogin({ formActionOrigin: "" }),
    /both a httpRealm and formActionOrigin/
  );
});

/**
 * Tests null or empty values in required login properties.
 */
add_task(function test_missing_properties() {
  checkLoginInvalid(
    TestData.formLogin({ origin: null }),
    /null or empty origin/
  );

  checkLoginInvalid(TestData.formLogin({ origin: "" }), /null or empty origin/);

  checkLoginInvalid(TestData.formLogin({ username: null }), /null username/);

  checkLoginInvalid(
    TestData.formLogin({ password: null }),
    /null or empty password/
  );

  checkLoginInvalid(
    TestData.formLogin({ password: "" }),
    /null or empty password/
  );
});

/**
 * Tests invalid NUL characters in nsILoginInfo properties.
 */
add_task(function test_invalid_characters() {
  let loginList = [
    TestData.authLogin({ origin: "http://null\0X.example.com" }),
    TestData.authLogin({ httpRealm: "realm\0" }),
    TestData.formLogin({ formActionOrigin: "http://null\0X.example.com" }),
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
add_task(function test_removeLogin_nonexisting() {
  Assert.throws(
    () => Services.logins.removeLogin(TestData.formLogin()),
    /No matching logins/
  );
});

/**
 * Tests removing all logins at once.
 */
add_task(function test_removeAllLogins() {
  for (let loginInfo of TestData.loginList()) {
    Services.logins.addLogin(loginInfo);
  }
  Services.logins.removeAllLogins();
  LoginTestUtils.checkLogins([]);

  // The function should also work when there are no logins to delete.
  Services.logins.removeAllLogins();
});

/**
 * Tests the modifyLogin function with an nsILoginInfo argument.
 */
add_task(function test_modifyLogin_nsILoginInfo() {
  let loginInfo = TestData.formLogin();
  let updatedLoginInfo = TestData.formLogin({
    username: "new username",
    password: "new password",
    usernameField: "new_form_field_username",
    passwordField: "new_form_field_password",
  });
  let differentLoginInfo = TestData.authLogin();

  // Trying to modify a login that does not exist should throw.
  Assert.throws(
    () => Services.logins.modifyLogin(loginInfo, updatedLoginInfo),
    /No matching logins/
  );

  // Add the first form login, then modify it to match the second.
  Services.logins.addLogin(loginInfo);
  Services.logins.modifyLogin(loginInfo, updatedLoginInfo);

  // The data should now match the second login.
  LoginTestUtils.checkLogins([updatedLoginInfo]);
  Assert.throws(
    () => Services.logins.modifyLogin(loginInfo, updatedLoginInfo),
    /No matching logins/
  );

  // The login can be changed to have a different type and origin.
  Services.logins.modifyLogin(updatedLoginInfo, differentLoginInfo);
  LoginTestUtils.checkLogins([differentLoginInfo]);

  // It is now possible to add a login with the old type and origin.
  Services.logins.addLogin(loginInfo);
  LoginTestUtils.checkLogins([loginInfo, differentLoginInfo]);

  // Modifying a login to match an existing one should not be possible.
  Assert.throws(
    () => Services.logins.modifyLogin(loginInfo, differentLoginInfo),
    /already exists/
  );
  LoginTestUtils.checkLogins([loginInfo, differentLoginInfo]);

  LoginTestUtils.clearData();
});

/**
 * Tests the modifyLogin function with an nsIPropertyBag argument.
 */
add_task(function test_modifyLogin_nsIProperyBag() {
  let loginInfo = TestData.formLogin();
  let updatedLoginInfo = TestData.formLogin({
    username: "new username",
    password: "new password",
    usernameField: "",
    passwordField: "new_form_field_password",
  });
  let differentLoginInfo = TestData.authLogin();
  let differentLoginProperties = newPropertyBag({
    origin: differentLoginInfo.origin,
    formActionOrigin: differentLoginInfo.formActionOrigin,
    httpRealm: differentLoginInfo.httpRealm,
    username: differentLoginInfo.username,
    password: differentLoginInfo.password,
    usernameField: differentLoginInfo.usernameField,
    passwordField: differentLoginInfo.passwordField,
  });

  // Trying to modify a login that does not exist should throw.
  Assert.throws(
    () => Services.logins.modifyLogin(loginInfo, newPropertyBag()),
    /No matching logins/
  );

  // Add the first form login, then modify it to match the second, changing
  // only some of its properties and checking the behavior with an empty string.
  Services.logins.addLogin(loginInfo);
  Services.logins.modifyLogin(
    loginInfo,
    newPropertyBag({
      username: "new username",
      password: "new password",
      usernameField: "",
      passwordField: "new_form_field_password",
    })
  );

  // The data should now match the second login.
  LoginTestUtils.checkLogins([updatedLoginInfo]);
  Assert.throws(
    () => Services.logins.modifyLogin(loginInfo, newPropertyBag()),
    /No matching logins/
  );

  // It is also possible to provide no properties to be modified.
  Services.logins.modifyLogin(updatedLoginInfo, newPropertyBag());

  // Specifying a null property for a required value should throw.
  Assert.throws(
    () =>
      Services.logins.modifyLogin(
        loginInfo,
        newPropertyBag({
          usernameField: null,
        })
      ),
    /No matching logins/
  );

  // The login can be changed to have a different type and origin.
  Services.logins.modifyLogin(updatedLoginInfo, differentLoginProperties);
  LoginTestUtils.checkLogins([differentLoginInfo]);

  // It is now possible to add a login with the old type and origin.
  Services.logins.addLogin(loginInfo);
  LoginTestUtils.checkLogins([loginInfo, differentLoginInfo]);

  // Modifying a login to match an existing one should not be possible.
  Assert.throws(
    () => Services.logins.modifyLogin(loginInfo, differentLoginProperties),
    /already exists/
  );
  LoginTestUtils.checkLogins([loginInfo, differentLoginInfo]);

  LoginTestUtils.clearData();
});

/**
 * Tests the login deduplication function.
 */
add_task(function test_deduplicate_logins() {
  // Different key attributes combinations and the amount of unique
  // results expected for the TestData login list.
  let keyCombinations = [
    {
      keyset: ["username", "password"],
      results: 17,
    },
    {
      keyset: ["origin", "username"],
      results: 21,
    },
    {
      keyset: ["origin", "username", "password"],
      results: 22,
    },
    {
      keyset: ["origin", "username", "password", "formActionOrigin"],
      results: 27,
    },
  ];

  let logins = TestData.loginList();

  for (let testCase of keyCombinations) {
    // Deduplicate the logins using the current testcase keyset.
    let deduped = LoginHelper.dedupeLogins(logins, testCase.keyset);
    Assert.equal(
      deduped.length,
      testCase.results,
      "Correct amount of results."
    );

    // Checks that every login after deduping is unique.
    Assert.ok(
      deduped.every(loginA =>
        deduped.every(
          loginB => !compareAttributes(loginA, loginB, testCase.keyset)
        )
      ),
      "Every login is unique."
    );
  }
});

/**
 * Ensure that the login deduplication function keeps the most recent login.
 */
add_task(function test_deduplicate_keeps_most_recent() {
  // Logins to deduplicate.
  let logins = [
    TestData.formLogin({ timeLastUsed: Date.UTC(2004, 11, 4, 0, 0, 0) }),
    TestData.formLogin({
      formActionOrigin: "http://example.com",
      timeLastUsed: Date.UTC(2015, 11, 4, 0, 0, 0),
    }),
  ];

  // Deduplicate the logins.
  let deduped = LoginHelper.dedupeLogins(logins);
  Assert.equal(deduped.length, 1, "Deduplicated the logins array.");

  // Verify that the remaining login have the most recent date.
  let loginTimeLastUsed = deduped[0].QueryInterface(Ci.nsILoginMetaInfo)
    .timeLastUsed;
  Assert.equal(
    loginTimeLastUsed,
    Date.UTC(2015, 11, 4, 0, 0, 0),
    "Most recent login was kept."
  );

  // Deduplicate the reverse logins array.
  deduped = LoginHelper.dedupeLogins(logins.reverse());
  Assert.equal(deduped.length, 1, "Deduplicated the reversed logins array.");

  // Verify that the remaining login have the most recent date.
  loginTimeLastUsed = deduped[0].QueryInterface(Ci.nsILoginMetaInfo)
    .timeLastUsed;
  Assert.equal(
    loginTimeLastUsed,
    Date.UTC(2015, 11, 4, 0, 0, 0),
    "Most recent login was kept."
  );
});
