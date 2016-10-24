/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the handling of nsILoginMetaInfo by methods that add, remove, modify,
 * and find logins.
 */

"use strict";

// //////////////////////////////////////////////////////////////////////////////
// // Globals

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

var gLooksLikeUUIDRegex = /^\{\w{8}-\w{4}-\w{4}-\w{4}-\w{12}\}$/;

/**
 * Retrieves the only login among the current data that matches the hostname of
 * the given nsILoginInfo.  In case there is more than one login for the
 * hostname, the test fails.
 */
function retrieveLoginMatching(aLoginInfo)
{
  let logins = Services.logins.findLogins({}, aLoginInfo.hostname, "", "");
  do_check_eq(logins.length, 1);
  return logins[0].QueryInterface(Ci.nsILoginMetaInfo);
}

/**
 * Checks that the nsILoginInfo and nsILoginMetaInfo properties of two different
 * login instances are equal.
 */
function assertMetaInfoEqual(aActual, aExpected)
{
  do_check_neq(aActual, aExpected);

  // Check the nsILoginInfo properties.
  do_check_true(aActual.equals(aExpected));

  // Check the nsILoginMetaInfo properties.
  do_check_eq(aActual.guid, aExpected.guid);
  do_check_eq(aActual.timeCreated, aExpected.timeCreated);
  do_check_eq(aActual.timeLastUsed, aExpected.timeLastUsed);
  do_check_eq(aActual.timePasswordChanged, aExpected.timePasswordChanged);
  do_check_eq(aActual.timesUsed, aExpected.timesUsed);
}

/**
 * nsILoginInfo instances with or without nsILoginMetaInfo properties.
 */
var gLoginInfo1;
var gLoginInfo2;
var gLoginInfo3;

/**
 * nsILoginInfo instances reloaded with all the nsILoginMetaInfo properties.
 * These are often used to provide the reference values to test against.
 */
var gLoginMetaInfo1;
var gLoginMetaInfo2;
var gLoginMetaInfo3;

// //////////////////////////////////////////////////////////////////////////////
// // Tests

/**
 * Prepare the test objects that will be used by the following tests.
 */
add_task(function test_initialize()
{
  // Use a reference time from ten minutes ago to initialize one instance of
  // nsILoginMetaInfo, to test that reference times are updated when needed.
  let baseTimeMs = Date.now() - 600000;

  gLoginInfo1 = TestData.formLogin();
  gLoginInfo2 = TestData.formLogin({
    hostname: "http://other.example.com",
    guid: gUUIDGenerator.generateUUID().toString(),
    timeCreated: baseTimeMs,
    timeLastUsed: baseTimeMs + 2,
    timePasswordChanged: baseTimeMs + 1,
    timesUsed: 2,
  });
  gLoginInfo3 = TestData.authLogin();
});

/**
 * Tests the behavior of addLogin with regard to metadata.  The logins added
 * here are also used by the following tests.
 */
add_task(function test_addLogin_metainfo()
{
  // Add a login without metadata to the database.
  Services.logins.addLogin(gLoginInfo1);

  // The object provided to addLogin should not have been modified.
  do_check_eq(gLoginInfo1.guid, null);
  do_check_eq(gLoginInfo1.timeCreated, 0);
  do_check_eq(gLoginInfo1.timeLastUsed, 0);
  do_check_eq(gLoginInfo1.timePasswordChanged, 0);
  do_check_eq(gLoginInfo1.timesUsed, 0);

  // A login with valid metadata should have been stored.
  gLoginMetaInfo1 = retrieveLoginMatching(gLoginInfo1);
  do_check_true(gLooksLikeUUIDRegex.test(gLoginMetaInfo1.guid));
  let creationTime = gLoginMetaInfo1.timeCreated;
  LoginTestUtils.assertTimeIsAboutNow(creationTime);
  do_check_eq(gLoginMetaInfo1.timeLastUsed, creationTime);
  do_check_eq(gLoginMetaInfo1.timePasswordChanged, creationTime);
  do_check_eq(gLoginMetaInfo1.timesUsed, 1);

  // Add a login without metadata to the database.
  let originalLogin = gLoginInfo2.clone().QueryInterface(Ci.nsILoginMetaInfo);
  Services.logins.addLogin(gLoginInfo2);

  // The object provided to addLogin should not have been modified.
  assertMetaInfoEqual(gLoginInfo2, originalLogin);

  // A login with the provided metadata should have been stored.
  gLoginMetaInfo2 = retrieveLoginMatching(gLoginInfo2);
  assertMetaInfoEqual(gLoginMetaInfo2, gLoginInfo2);

  // Add an authentication login to the database before continuing.
  Services.logins.addLogin(gLoginInfo3);
  gLoginMetaInfo3 = retrieveLoginMatching(gLoginInfo3);
  LoginTestUtils.checkLogins([gLoginInfo1, gLoginInfo2, gLoginInfo3]);
});

/**
 * Tests that adding a login with a duplicate GUID throws an exception.
 */
add_task(function test_addLogin_metainfo_duplicate()
{
  let loginInfo = TestData.formLogin({
    hostname: "http://duplicate.example.com",
    guid: gLoginMetaInfo2.guid,
  });
  Assert.throws(() => Services.logins.addLogin(loginInfo),
                /specified GUID already exists/);

  // Verify that no data was stored by the previous call.
  LoginTestUtils.checkLogins([gLoginInfo1, gLoginInfo2, gLoginInfo3]);
});

/**
 * Tests that the existing metadata is not changed when modifyLogin is called
 * with an nsILoginInfo argument.
 */
add_task(function test_modifyLogin_nsILoginInfo_metainfo_ignored()
{
  let newLoginInfo = gLoginInfo1.clone().QueryInterface(Ci.nsILoginMetaInfo);
  newLoginInfo.guid = gUUIDGenerator.generateUUID().toString();
  newLoginInfo.timeCreated = Date.now();
  newLoginInfo.timeLastUsed = Date.now();
  newLoginInfo.timePasswordChanged = Date.now();
  newLoginInfo.timesUsed = 12;
  Services.logins.modifyLogin(gLoginInfo1, newLoginInfo);

  newLoginInfo = retrieveLoginMatching(gLoginInfo1);
  assertMetaInfoEqual(newLoginInfo, gLoginMetaInfo1);
});

/**
 * Tests the modifyLogin function with an nsIProperyBag argument.
 */
add_task(function test_modifyLogin_nsIProperyBag_metainfo()
{
  // Use a new reference time that is two minutes from now.
  let newTimeMs = Date.now() + 120000;
  let newUUIDValue = gUUIDGenerator.generateUUID().toString();

  // Check that properties are changed as requested.
  Services.logins.modifyLogin(gLoginInfo1, newPropertyBag({
    guid: newUUIDValue,
    timeCreated: newTimeMs,
    timeLastUsed: newTimeMs + 2,
    timePasswordChanged: newTimeMs + 1,
    timesUsed: 2,
  }));

  gLoginMetaInfo1 = retrieveLoginMatching(gLoginInfo1);
  do_check_eq(gLoginMetaInfo1.guid, newUUIDValue);
  do_check_eq(gLoginMetaInfo1.timeCreated, newTimeMs);
  do_check_eq(gLoginMetaInfo1.timeLastUsed, newTimeMs + 2);
  do_check_eq(gLoginMetaInfo1.timePasswordChanged, newTimeMs + 1);
  do_check_eq(gLoginMetaInfo1.timesUsed, 2);

  // Check that timePasswordChanged is updated when changing the password.
  let originalLogin = gLoginInfo2.clone().QueryInterface(Ci.nsILoginMetaInfo);
  Services.logins.modifyLogin(gLoginInfo2, newPropertyBag({
    password: "new password",
  }));
  gLoginInfo2.password = "new password";

  gLoginMetaInfo2 = retrieveLoginMatching(gLoginInfo2);
  do_check_eq(gLoginMetaInfo2.password, gLoginInfo2.password);
  do_check_eq(gLoginMetaInfo2.timeCreated, originalLogin.timeCreated);
  do_check_eq(gLoginMetaInfo2.timeLastUsed, originalLogin.timeLastUsed);
  LoginTestUtils.assertTimeIsAboutNow(gLoginMetaInfo2.timePasswordChanged);

  // Check that timePasswordChanged is not set to the current time when changing
  // the password and specifying a new value for the property at the same time.
  Services.logins.modifyLogin(gLoginInfo2, newPropertyBag({
    password: "other password",
    timePasswordChanged: newTimeMs,
  }));
  gLoginInfo2.password = "other password";

  gLoginMetaInfo2 = retrieveLoginMatching(gLoginInfo2);
  do_check_eq(gLoginMetaInfo2.password, gLoginInfo2.password);
  do_check_eq(gLoginMetaInfo2.timeCreated, originalLogin.timeCreated);
  do_check_eq(gLoginMetaInfo2.timeLastUsed, originalLogin.timeLastUsed);
  do_check_eq(gLoginMetaInfo2.timePasswordChanged, newTimeMs);

  // Check the special timesUsedIncrement property.
  Services.logins.modifyLogin(gLoginInfo2, newPropertyBag({
    timesUsedIncrement: 2,
  }));

  gLoginMetaInfo2 = retrieveLoginMatching(gLoginInfo2);
  do_check_eq(gLoginMetaInfo2.timeCreated, originalLogin.timeCreated);
  do_check_eq(gLoginMetaInfo2.timeLastUsed, originalLogin.timeLastUsed);
  do_check_eq(gLoginMetaInfo2.timePasswordChanged, newTimeMs);
  do_check_eq(gLoginMetaInfo2.timesUsed, 4);
});

/**
 * Tests that modifying a login to a duplicate GUID throws an exception.
 */
add_task(function test_modifyLogin_nsIProperyBag_metainfo_duplicate()
{
  Assert.throws(() => Services.logins.modifyLogin(gLoginInfo1, newPropertyBag({
    guid: gLoginInfo2.guid,
  })), /specified GUID already exists/);
  LoginTestUtils.checkLogins([gLoginInfo1, gLoginInfo2, gLoginInfo3]);
});

/**
 * Tests searching logins using nsILoginMetaInfo properties.
 */
add_task(function test_searchLogins_metainfo()
{
  // Find by GUID.
  let logins = Services.logins.searchLogins({}, newPropertyBag({
    guid: gLoginMetaInfo1.guid,
  }));
  do_check_eq(logins.length, 1);
  let foundLogin = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  assertMetaInfoEqual(foundLogin, gLoginMetaInfo1);

  // Find by timestamp.
  logins = Services.logins.searchLogins({}, newPropertyBag({
    timePasswordChanged: gLoginMetaInfo2.timePasswordChanged,
  }));
  do_check_eq(logins.length, 1);
  foundLogin = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  assertMetaInfoEqual(foundLogin, gLoginMetaInfo2);

  // Find using two properties at the same time.
  logins = Services.logins.searchLogins({}, newPropertyBag({
    guid: gLoginMetaInfo3.guid,
    timePasswordChanged: gLoginMetaInfo3.timePasswordChanged,
  }));
  do_check_eq(logins.length, 1);
  foundLogin = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  assertMetaInfoEqual(foundLogin, gLoginMetaInfo3);
});

/**
 * Tests that the default nsILoginManagerStorage module attached to the Login
 * Manager service is able to save and reload nsILoginMetaInfo properties.
 */
add_task(function* test_storage_metainfo()
{
  yield* LoginTestUtils.reloadData();
  LoginTestUtils.checkLogins([gLoginInfo1, gLoginInfo2, gLoginInfo3]);

  assertMetaInfoEqual(retrieveLoginMatching(gLoginInfo1), gLoginMetaInfo1);
  assertMetaInfoEqual(retrieveLoginMatching(gLoginInfo2), gLoginMetaInfo2);
  assertMetaInfoEqual(retrieveLoginMatching(gLoginInfo3), gLoginMetaInfo3);
});
