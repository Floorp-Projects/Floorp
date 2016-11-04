/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the LoginImport object.
 */

"use strict";

// Globals

Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginImport",
                                  "resource://gre/modules/LoginImport.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginStore",
                                  "resource://gre/modules/LoginStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gLoginManagerCrypto",
                                   "@mozilla.org/login-manager/crypto/SDR;1",
                                   "nsILoginManagerCrypto");
XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

/**
 * Creates empty login data tables in the given SQLite connection, resembling
 * the most recent schema version (excluding indices).
 */
function promiseCreateDatabaseSchema(aConnection)
{
  return Task.spawn(function* () {
    yield aConnection.setSchemaVersion(5);
    yield aConnection.execute("CREATE TABLE moz_logins (" +
                              "id                  INTEGER PRIMARY KEY," +
                              "hostname            TEXT NOT NULL,"       +
                              "httpRealm           TEXT,"                +
                              "formSubmitURL       TEXT,"                +
                              "usernameField       TEXT NOT NULL,"       +
                              "passwordField       TEXT NOT NULL,"       +
                              "encryptedUsername   TEXT NOT NULL,"       +
                              "encryptedPassword   TEXT NOT NULL,"       +
                              "guid                TEXT,"                +
                              "encType             INTEGER,"             +
                              "timeCreated         INTEGER,"             +
                              "timeLastUsed        INTEGER,"             +
                              "timePasswordChanged INTEGER,"             +
                              "timesUsed           INTEGER)");
    yield aConnection.execute("CREATE TABLE moz_disabledHosts ("         +
                              "id                  INTEGER PRIMARY KEY," +
                              "hostname            TEXT UNIQUE)");
    yield aConnection.execute("CREATE TABLE moz_deleted_logins ("        +
                              "id                  INTEGER PRIMARY KEY," +
                              "guid                TEXT,"                +
                              "timeDeleted         INTEGER)");
  });
}

/**
 * Inserts a new entry in the database resembling the given nsILoginInfo object.
 */
function promiseInsertLoginInfo(aConnection, aLoginInfo)
{
  aLoginInfo.QueryInterface(Ci.nsILoginMetaInfo);

  // We can't use the aLoginInfo object directly in the execute statement
  // because the bind code in Sqlite.jsm doesn't allow objects with extra
  // properties beyond those being binded. So we might as well use an array as
  // it is simpler.
  let values = [
    aLoginInfo.hostname,
    aLoginInfo.httpRealm,
    aLoginInfo.formSubmitURL,
    aLoginInfo.usernameField,
    aLoginInfo.passwordField,
    gLoginManagerCrypto.encrypt(aLoginInfo.username),
    gLoginManagerCrypto.encrypt(aLoginInfo.password),
    aLoginInfo.guid,
    aLoginInfo.encType,
    aLoginInfo.timeCreated,
    aLoginInfo.timeLastUsed,
    aLoginInfo.timePasswordChanged,
    aLoginInfo.timesUsed,
  ];

  return aConnection.execute("INSERT INTO moz_logins (hostname, " +
                             "httpRealm, formSubmitURL, usernameField, " +
                             "passwordField, encryptedUsername, " +
                             "encryptedPassword, guid, encType, timeCreated, " +
                             "timeLastUsed, timePasswordChanged, timesUsed) " +
                             "VALUES (?" + ",?".repeat(12) + ")", values);
}

/**
 * Inserts a new disabled host entry in the database.
 */
function promiseInsertDisabledHost(aConnection, aHostname)
{
  return aConnection.execute("INSERT INTO moz_disabledHosts (hostname) " +
                             "VALUES (?)", [aHostname]);
}

// Tests

/**
 * Imports login data from a SQLite file constructed using the test data.
 */
add_task(function* test_import()
{
  let store = new LoginStore(getTempFile("test-import.json").path);
  let loginsSqlite = getTempFile("test-logins.sqlite").path;

  // Prepare the logins to be imported, including the nsILoginMetaInfo data.
  let loginList = TestData.loginList();
  for (let loginInfo of loginList) {
    loginInfo.QueryInterface(Ci.nsILoginMetaInfo);
    loginInfo.guid = gUUIDGenerator.generateUUID().toString();
    loginInfo.timeCreated = Date.now();
    loginInfo.timeLastUsed = Date.now();
    loginInfo.timePasswordChanged = Date.now();
    loginInfo.timesUsed = 1;
  }

  // Create and populate the SQLite database first.
  let connection = yield Sqlite.openConnection({ path: loginsSqlite });
  try {
    yield promiseCreateDatabaseSchema(connection);
    for (let loginInfo of loginList) {
      yield promiseInsertLoginInfo(connection, loginInfo);
    }
    yield promiseInsertDisabledHost(connection, "http://www.example.com");
    yield promiseInsertDisabledHost(connection, "https://www.example.org");
  } finally {
    yield connection.close();
  }

  // The "load" method must be called before importing data.
  yield store.load();
  yield new LoginImport(store, loginsSqlite).import();

  // Verify that every login in the test data has a matching imported row.
  do_check_eq(loginList.length, store.data.logins.length);
  do_check_true(loginList.every(function (loginInfo) {
    return store.data.logins.some(function (loginDataItem) {
      let username = gLoginManagerCrypto.decrypt(loginDataItem.encryptedUsername);
      let password = gLoginManagerCrypto.decrypt(loginDataItem.encryptedPassword);
      return loginDataItem.hostname == loginInfo.hostname &&
             loginDataItem.httpRealm == loginInfo.httpRealm &&
             loginDataItem.formSubmitURL == loginInfo.formSubmitURL &&
             loginDataItem.usernameField == loginInfo.usernameField &&
             loginDataItem.passwordField == loginInfo.passwordField &&
             username == loginInfo.username &&
             password == loginInfo.password &&
             loginDataItem.guid == loginInfo.guid &&
             loginDataItem.encType == loginInfo.encType &&
             loginDataItem.timeCreated == loginInfo.timeCreated &&
             loginDataItem.timeLastUsed == loginInfo.timeLastUsed &&
             loginDataItem.timePasswordChanged == loginInfo.timePasswordChanged &&
             loginDataItem.timesUsed == loginInfo.timesUsed;
    });
  }));

  // Verify that disabled hosts have been imported.
  do_check_eq(store.data.disabledHosts.length, 2);
  do_check_true(store.data.disabledHosts.indexOf("http://www.example.com") != -1);
  do_check_true(store.data.disabledHosts.indexOf("https://www.example.org") != -1);
});

/**
 * Tests imports of NULL values due to a downgraded database.
 */
add_task(function* test_import_downgraded()
{
  let store = new LoginStore(getTempFile("test-import-downgraded.json").path);
  let loginsSqlite = getTempFile("test-logins-downgraded.sqlite").path;

  // Create and populate the SQLite database first.
  let connection = yield Sqlite.openConnection({ path: loginsSqlite });
  try {
    yield promiseCreateDatabaseSchema(connection);
    yield connection.setSchemaVersion(3);
    yield promiseInsertLoginInfo(connection, TestData.formLogin({
      guid: gUUIDGenerator.generateUUID().toString(),
      timeCreated: null,
      timeLastUsed: null,
      timePasswordChanged: null,
      timesUsed: 0,
    }));
  } finally {
    yield connection.close();
  }

  // The "load" method must be called before importing data.
  yield store.load();
  yield new LoginImport(store, loginsSqlite).import();

  // Verify that the missing metadata was generated correctly.
  let loginItem = store.data.logins[0];
  let creationTime = loginItem.timeCreated;
  LoginTestUtils.assertTimeIsAboutNow(creationTime);
  do_check_eq(loginItem.timeLastUsed, creationTime);
  do_check_eq(loginItem.timePasswordChanged, creationTime);
  do_check_eq(loginItem.timesUsed, 1);
});

/**
 * Verifies that importing from a SQLite file with database version 2 fails.
 */
add_task(function* test_import_v2()
{
  let store = new LoginStore(getTempFile("test-import-v2.json").path);
  let loginsSqlite = do_get_file("data/signons-v2.sqlite").path;

  // The "load" method must be called before importing data.
  yield store.load();
  try {
    yield new LoginImport(store, loginsSqlite).import();
    do_throw("The operation should have failed.");
  } catch (ex) { }
});

/**
 * Imports login data from a SQLite file, with database version 3.
 */
add_task(function* test_import_v3()
{
  let store = new LoginStore(getTempFile("test-import-v3.json").path);
  let loginsSqlite = do_get_file("data/signons-v3.sqlite").path;

  // The "load" method must be called before importing data.
  yield store.load();
  yield new LoginImport(store, loginsSqlite).import();

  // We only execute basic integrity checks.
  do_check_eq(store.data.logins[0].usernameField, "u1");
  do_check_eq(store.data.disabledHosts.length, 0);
});
