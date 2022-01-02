/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the LoginStore object.
 */

"use strict";

// Globals

ChromeUtils.defineModuleGetter(
  this,
  "LoginStore",
  "resource://gre/modules/LoginStore.jsm"
);

const TEST_STORE_FILE_NAME = "test-logins.json";

const MAX_DATE_MS = 8640000000000000;

// Tests

/**
 * Saves login data to a file, then reloads it.
 */
add_task(async function test_save_reload() {
  let storeForSave = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  // The "load" method must be called before preparing the data to be saved.
  await storeForSave.load();

  let rawLoginData = {
    id: storeForSave.data.nextId++,
    hostname: "http://www.example.com",
    httpRealm: null,
    formSubmitURL: "http://www.example.com",
    usernameField: "field_" + String.fromCharCode(533, 537, 7570, 345),
    passwordField: "field_" + String.fromCharCode(421, 259, 349, 537),
    encryptedUsername: "(test)",
    encryptedPassword: "(test)",
    guid: "(test)",
    encType: Ci.nsILoginManagerCrypto.ENCTYPE_SDR,
    timeCreated: Date.now(),
    timeLastUsed: Date.now(),
    timePasswordChanged: Date.now(),
    timesUsed: 1,
  };
  storeForSave.data.logins.push(rawLoginData);

  await storeForSave._save();

  // Test the asynchronous initialization path.
  let storeForLoad = new LoginStore(storeForSave.path);
  await storeForLoad.load();

  Assert.equal(storeForLoad.data.logins.length, 1);
  Assert.deepEqual(storeForLoad.data.logins[0], rawLoginData);

  // Test the synchronous initialization path.
  storeForLoad = new LoginStore(storeForSave.path);
  storeForLoad.ensureDataReady();

  Assert.equal(storeForLoad.data.logins.length, 1);
  Assert.deepEqual(storeForLoad.data.logins[0], rawLoginData);
});

/**
 * Checks that loading from a missing file results in empty arrays.
 */
add_task(async function test_load_empty() {
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  Assert.equal(false, await OS.File.exists(store.path));

  await store.load();

  Assert.equal(false, await OS.File.exists(store.path));

  Assert.equal(store.data.logins.length, 0);
});

/**
 * Checks that saving empty data still overwrites any existing file.
 */
add_task(async function test_save_empty() {
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
add_task(async function test_load_string_predefined() {
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string =
    '{"logins":[{' +
    '"id":1,' +
    '"hostname":"http://www.example.com",' +
    '"httpRealm":null,' +
    '"formSubmitURL":"http://www.example.com",' +
    '"usernameField":"usernameField",' +
    '"passwordField":"passwordField",' +
    '"encryptedUsername":"(test)",' +
    '"encryptedPassword":"(test)",' +
    '"guid":"(test)",' +
    '"encType":1,' +
    '"timeCreated":1262304000000,' +
    '"timeLastUsed":1262390400000,' +
    '"timePasswordChanged":1262476800000,' +
    '"timesUsed":1}],"disabledHosts":[' +
    '"http://www.example.org"]}';

  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string), {
    tmpPath: store.path + ".tmp",
  });

  await store.load();

  Assert.equal(store.data.logins.length, 1);
  Assert.deepEqual(store.data.logins[0], {
    id: 1,
    hostname: "http://www.example.com",
    httpRealm: null,
    formSubmitURL: "http://www.example.com",
    usernameField: "usernameField",
    passwordField: "passwordField",
    encryptedUsername: "(test)",
    encryptedPassword: "(test)",
    guid: "(test)",
    encType: Ci.nsILoginManagerCrypto.ENCTYPE_SDR,
    timeCreated: 1262304000000,
    timeLastUsed: 1262390400000,
    timePasswordChanged: 1262476800000,
    timesUsed: 1,
  });
});

/**
 * Loads login data from a malformed JSON string.
 */
add_task(async function test_load_string_malformed() {
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string = '{"logins":[{"hostname":"http://www.example.com","id":1,';

  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string), {
    tmpPath: store.path + ".tmp",
  });

  await store.load();

  // A backup file should have been created.
  Assert.ok(await OS.File.exists(store.path + ".corrupt"));
  await OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  Assert.equal(store.data.logins.length, 0);
});

/**
 * Loads login data from a malformed JSON string, using the synchronous
 * initialization path.
 */
add_task(async function test_load_string_malformed_sync() {
  let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);

  let string = '{"logins":[{"hostname":"http://www.example.com","id":1,';

  await OS.File.writeAtomic(store.path, new TextEncoder().encode(string), {
    tmpPath: store.path + ".tmp",
  });

  store.ensureDataReady();

  // A backup file should have been created.
  Assert.ok(await OS.File.exists(store.path + ".corrupt"));
  await OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  Assert.equal(store.data.logins.length, 0);
});

/**
 * Fix bad dates when loading login data
 */
add_task(async function test_load_bad_dates() {
  let rawLoginData = {
    encType: 1,
    encryptedPassword: "(test)",
    encryptedUsername: "(test)",
    formSubmitURL: "https://www.example.com",
    guid: "{2a97313f-873b-4048-9a3d-4f442b46c1e5}",
    hostname: "https://www.example.com",
    httpRealm: null,
    id: 1,
    passwordField: "pass",
    timesUsed: 1,
    usernameField: "email",
  };
  let rawStoreData = {
    dismissedBreachAlertsByLoginGUID: {},
    logins: [],
    nextId: 2,
    potentiallyVulnerablePasswords: [],
    version: 2,
  };

  /**
   * test that:
   * - bogus (0 or out-of-range) date values in any of the date fields are replaced with the
   *   earliest time marked by the other date fields
   * - bogus bogus (0 or out-of-range) date values in all date fields are replaced with the time of import
   */
  let tests = [
    {
      name: "Out-of-range time values",
      savedProps: {
        timePasswordChanged: MAX_DATE_MS + 1,
        timeLastUsed: MAX_DATE_MS + 1,
        timeCreated: MAX_DATE_MS + 1,
      },
      expectedProps: {
        timePasswordChanged: "now",
        timeLastUsed: "now",
        timeCreated: "now",
      },
    },
    {
      name: "All zero time values",
      savedProps: {
        timePasswordChanged: 0,
        timeLastUsed: 0,
        timeCreated: 0,
      },
      expectedProps: {
        timePasswordChanged: "now",
        timeLastUsed: "now",
        timeCreated: "now",
      },
    },
    {
      name: "Only timeCreated has value",
      savedProps: {
        timePasswordChanged: 0,
        timeLastUsed: 0,
        timeCreated: 946713600000,
      },
      expectedProps: {
        timePasswordChanged: 946713600000,
        timeLastUsed: 946713600000,
        timeCreated: 946713600000,
      },
    },
    {
      name: "timeCreated has 0 value",
      savedProps: {
        timePasswordChanged: 946713600000,
        timeLastUsed: 946713600000,
        timeCreated: 0,
      },
      expectedProps: {
        timePasswordChanged: 946713600000,
        timeLastUsed: 946713600000,
        timeCreated: 946713600000,
      },
    },
    {
      name: "timeCreated has out-of-range value",
      savedProps: {
        timePasswordChanged: 946713600000,
        timeLastUsed: 946713600000,
        timeCreated: MAX_DATE_MS + 1,
      },
      expectedProps: {
        timePasswordChanged: 946713600000,
        timeLastUsed: 946713600000,
        timeCreated: 946713600000,
      },
    },
    {
      name: "Use earliest time for missing value",
      savedProps: {
        timePasswordChanged: 0,
        timeLastUsed: 946713600000,
        timeCreated: 946540800000,
      },
      expectedProps: {
        timePasswordChanged: 946540800000,
        timeLastUsed: 946713600000,
        timeCreated: 946540800000,
      },
    },
  ];

  for (let testData of tests) {
    let store = new LoginStore(getTempFile(TEST_STORE_FILE_NAME).path);
    let string = JSON.stringify(
      Object.assign({}, rawStoreData, {
        logins: [Object.assign({}, rawLoginData, testData.savedProps)],
      })
    );
    await OS.File.writeAtomic(store.path, new TextEncoder().encode(string), {
      tmpPath: store.path + ".tmp",
    });
    let now = Date.now();
    await store.load();

    Assert.equal(
      store.data.logins.length,
      1,
      `${testData.name}: Expected a single login`
    );

    let login = store.data.logins[0];
    for (let pname of ["timeCreated", "timeLastUsed", "timePasswordChanged"]) {
      if (testData.expectedProps[pname] === "now") {
        Assert.ok(
          login[pname] >= now,
          `${testData.name}: Check ${pname} is at/near now`
        );
      } else {
        Assert.equal(
          login[pname],
          testData.expectedProps[pname],
          `${testData.name}: Check expected ${pname}`
        );
      }
    }
    Assert.equal(store.data.version, 3, "Check version was bumped");
  }
});
