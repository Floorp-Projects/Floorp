/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the LoginStore object.
 */

"use strict";

// Globals

XPCOMUtils.defineLazyModuleGetter(this, "LoginStore",
                                  "resource://gre/modules/LoginStore.jsm");

const TEST_STORE_FILE_NAME = "test-logins.json";

// Tests

/**
 * Saves login data to a file, then reloads it.
 */
add_task(async function test_save_reload()
{
  let storeForSave = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  // The "load" method must be called before preparing the data to be saved.
  await storeForSave.load();

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

  await storeForSave._save();

  // Test the asynchronous initialization path.
  let storeForLoad = new LoginStore(storeForSave.path);
  await storeForLoad.load();

  Assert.equal(storeForLoad.data.logins.length, 1);
  Assert.deepEqual(storeForLoad.data.logins[0], rawLoginData);
  Assert.equal(storeForLoad.data.disabledHosts.length, 1);
  Assert.equal(storeForLoad.data.disabledHosts[0], "http://www.example.org");

  // Test the synchronous initialization path.
  storeForLoad = new LoginStore(storeForSave.path);
  storeForLoad.ensureDataReady();

  Assert.equal(storeForLoad.data.logins.length, 1);
  Assert.deepEqual(storeForLoad.data.logins[0], rawLoginData);
  Assert.equal(storeForLoad.data.disabledHosts.length, 1);
  Assert.equal(storeForLoad.data.disabledHosts[0], "http://www.example.org");
});

/**
 * Checks that loading from a missing file results in empty arrays.
 */
add_task(async function test_load_empty()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  Assert.equal(false, await OS.File.exists(store.path));

  await store.load();

  Assert.equal(false, await OS.File.exists(store.path));

  Assert.equal(store.data.logins.length, 0);
  Assert.equal(store.data.disabledHosts.length, 0);
});

/**
 * Checks that saving empty data still overwrites any existing file.
 */
add_task(async function test_save_empty()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  await store.load();

  let createdFile = await OS.File.open(store.path, { create: true });
  await createdFile.close();

  await store._save();

  Assert.ok(await OS.File.exists(store.path));
});

/**
 * Loads data from a string in a predefined format.  The purpose of this test is
 * to verify that the JSON format used in previous versions can be loaded.
 */
add_task(async function test_load_string_predefined()
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

  await OS.File.writeAtomic(store.path,
                            new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  await store.load();

  Assert.equal(store.data.logins.length, 1);
  Assert.deepEqual(store.data.logins[0], {
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

  Assert.equal(store.data.disabledHosts.length, 1);
  Assert.equal(store.data.disabledHosts[0], "http://www.example.org");
});

/**
 * Loads login data from a malformed JSON string.
 */
add_task(async function test_load_string_malformed()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string = "{\"logins\":[{\"hostname\":\"http://www.example.com\"," +
                "\"id\":1,";

  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  await store.load();

  // A backup file should have been created.
  Assert.ok(await OS.File.exists(store.path + ".corrupt"));
  await OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  Assert.equal(store.data.logins.length, 0);
  Assert.equal(store.data.disabledHosts.length, 0);
});

/**
 * Loads login data from a malformed JSON string, using the synchronous
 * initialization path.
 */
add_task(async function test_load_string_malformed_sync()
{
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string = "{\"logins\":[{\"hostname\":\"http://www.example.com\"," +
                "\"id\":1,";

  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  store.ensureDataReady();

  // A backup file should have been created.
  Assert.ok(await OS.File.exists(store.path + ".corrupt"));
  await OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  Assert.equal(store.data.logins.length, 0);
  Assert.equal(store.data.disabledHosts.length, 0);
});
