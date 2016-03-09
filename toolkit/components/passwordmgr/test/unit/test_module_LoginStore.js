/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the LoginStore object.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

XPCOMUtils.defineLazyModuleGetter(this, "LoginStore",
                                  "resource://gre/modules/LoginStore.jsm");

const TEST_STORE_FILE_NAME = "test-logins.json";

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Saves login data to a file, then reloads it.
 */
add_task(function* test_save_reload()
{
  let storeForSave = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  // The "load" method must be called before preparing the data to be saved.
  yield storeForSave.load();

  let rawLoginData = {
    id:                  storeForSave.data.nextId++,
    hostname:            "http://www.example.com",
    httpRealm:           null,
    formSubmitURL:       "http://www.example.com/submit-url",
    usernameField:       "field_" + String.fromCharCode(533, 537, 7570, 345),
    passwordField:       "field_" + String.fromCharCode(421, 259, 349, 537),
    encryptedUsername:   "(test)",
    encryptedPassword:   "(test)",
    guid:                "(test)",
    encType:             Ci.nsILoginManagerCrypto.ENCTYPE_SDR,
    timeCreated:         Date.now(),
    timeLastUsed:        Date.now(),
    timePasswordChanged: Date.now(),
    timesUsed:           1,
  };
  storeForSave.data.logins.push(rawLoginData);

  storeForSave.data.disabledHosts.push("http://www.example.org");

  yield storeForSave.save();

  // Test the asynchronous initialization path.
  let storeForLoad = new LoginStore(storeForSave.path);
  yield storeForLoad.load();

  do_check_eq(storeForLoad.data.logins.length, 1);
  do_check_matches(storeForLoad.data.logins[0], rawLoginData);
  do_check_eq(storeForLoad.data.disabledHosts.length, 1);
  do_check_eq(storeForLoad.data.disabledHosts[0], "http://www.example.org");

  // Test the synchronous initialization path.
  storeForLoad = new LoginStore(storeForSave.path);
  storeForLoad.ensureDataReady();

  do_check_eq(storeForLoad.data.logins.length, 1);
  do_check_matches(storeForLoad.data.logins[0], rawLoginData);
  do_check_eq(storeForLoad.data.disabledHosts.length, 1);
  do_check_eq(storeForLoad.data.disabledHosts[0], "http://www.example.org");
});

/**
 * Checks that loading from a missing file results in empty arrays.
 */
add_task(function* test_load_empty()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  do_check_false(yield OS.File.exists(store.path));

  yield store.load();

  do_check_false(yield OS.File.exists(store.path));

  do_check_eq(store.data.logins.length, 0);
  do_check_eq(store.data.disabledHosts.length, 0);
});

/**
 * Checks that saving empty data still overwrites any existing file.
 */
add_task(function* test_save_empty()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  yield store.load();

  let createdFile = yield OS.File.open(store.path, { create: true });
  yield createdFile.close();

  yield store.save();

  do_check_true(yield OS.File.exists(store.path));
});

/**
 * Loads data from a string in a predefined format.  The purpose of this test is
 * to verify that the JSON format used in previous versions can be loaded.
 */
add_task(function* test_load_string_predefined()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string = "{\"logins\":[{" +
                "\"id\":1," +
                "\"hostname\":\"http://www.example.com\"," +
                "\"httpRealm\":null," +
                "\"formSubmitURL\":\"http://www.example.com/submit-url\"," +
                "\"usernameField\":\"usernameField\"," +
                "\"passwordField\":\"passwordField\"," +
                "\"encryptedUsername\":\"(test)\"," +
                "\"encryptedPassword\":\"(test)\"," +
                "\"guid\":\"(test)\"," +
                "\"encType\":1," +
                "\"timeCreated\":1262304000000," +
                "\"timeLastUsed\":1262390400000," +
                "\"timePasswordChanged\":1262476800000," +
                "\"timesUsed\":1}],\"disabledHosts\":[" +
                "\"http://www.example.org\"]}";

  yield OS.File.writeAtomic(store.path,
                            new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  yield store.load();

  do_check_eq(store.data.logins.length, 1);
  do_check_matches(store.data.logins[0], {
    id:                  1,
    hostname:            "http://www.example.com",
    httpRealm:           null,
    formSubmitURL:       "http://www.example.com/submit-url",
    usernameField:       "usernameField",
    passwordField:       "passwordField",
    encryptedUsername:   "(test)",
    encryptedPassword:   "(test)",
    guid:                "(test)",
    encType:             Ci.nsILoginManagerCrypto.ENCTYPE_SDR,
    timeCreated:         1262304000000,
    timeLastUsed:        1262390400000,
    timePasswordChanged: 1262476800000,
    timesUsed:           1,
  });

  do_check_eq(store.data.disabledHosts.length, 1);
  do_check_eq(store.data.disabledHosts[0], "http://www.example.org");
});

/**
 * Loads login data from a malformed JSON string.
 */
add_task(function* test_load_string_malformed()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string = "{\"logins\":[{\"hostname\":\"http://www.example.com\"," +
                "\"id\":1,";

  yield OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  yield store.load();

  // A backup file should have been created.
  do_check_true(yield OS.File.exists(store.path + ".corrupt"));
  yield OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  do_check_eq(store.data.logins.length, 0);
  do_check_eq(store.data.disabledHosts.length, 0);
});

/**
 * Loads login data from a malformed JSON string, using the synchronous
 * initialization path.
 */
add_task(function* test_load_string_malformed_sync()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string = "{\"logins\":[{\"hostname\":\"http://www.example.com\"," +
                "\"id\":1,";

  yield OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  store.ensureDataReady();

  // A backup file should have been created.
  do_check_true(yield OS.File.exists(store.path + ".corrupt"));
  yield OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  do_check_eq(store.data.logins.length, 0);
  do_check_eq(store.data.disabledHosts.length, 0);
});
