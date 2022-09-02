/**
 * Tests the JSONFile object.
 */

"use strict";

// Globals
ChromeUtils.defineModuleGetter(
  this,
  "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileTestUtils",
  "resource://testing-common/FileTestUtils.jsm"
);

/**
 * Returns a reference to a temporary file that is guaranteed not to exist and
 * is cleaned up later. See FileTestUtils.getTempFile for details.
 */
function getTempFile(leafName) {
  return FileTestUtils.getTempFile(leafName);
}

const TEST_STORE_FILE_NAME = "test-store.json";

const TEST_DATA = {
  number: 123,
  string: "test",
  object: {
    prop1: 1,
    prop2: 2,
  },
};

// Tests

add_task(async function test_save_reload() {
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  await storeForSave.load();

  Assert.ok(storeForSave.dataReady);
  Assert.deepEqual(storeForSave.data, {});

  Object.assign(storeForSave.data, TEST_DATA);

  await new Promise(resolve => {
    let save = storeForSave._save.bind(storeForSave);
    storeForSave._save = () => {
      save();
      resolve();
    };
    storeForSave.saveSoon();
  });

  let storeForLoad = new JSONFile({
    path: storeForSave.path,
  });

  await storeForLoad.load();

  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});

add_task(async function test_load_sync() {
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });
  await storeForSave.load();
  Object.assign(storeForSave.data, TEST_DATA);
  await storeForSave._save();

  let storeForLoad = new JSONFile({
    path: storeForSave.path,
  });
  storeForLoad.ensureDataReady();

  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});

add_task(async function test_load_with_dataPostProcessor() {
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });
  await storeForSave.load();
  Object.assign(storeForSave.data, TEST_DATA);
  await storeForSave._save();

  let random = Math.random();
  let storeForLoad = new JSONFile({
    path: storeForSave.path,
    dataPostProcessor: data => {
      Assert.deepEqual(data, TEST_DATA);

      data.test = random;
      return data;
    },
  });

  await storeForLoad.load();

  Assert.equal(storeForLoad.data.test, random);
});

add_task(async function test_load_with_dataPostProcessor_fails() {
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
    dataPostProcessor: () => {
      throw new Error("dataPostProcessor fails.");
    },
  });

  await Assert.rejects(store.load(), /dataPostProcessor fails\./);

  Assert.ok(!store.dataReady);
});

add_task(async function test_load_sync_with_dataPostProcessor_fails() {
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
    dataPostProcessor: () => {
      throw new Error("dataPostProcessor fails.");
    },
  });

  Assert.throws(() => store.ensureDataReady(), /dataPostProcessor fails\./);

  Assert.ok(!store.dataReady);
});

/**
 * Loads data from a string in a predefined format.  The purpose of this test is
 * to verify that the JSON format used in previous versions can be loaded.
 */
add_task(async function test_load_string_predefined() {
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string = '{"number":123,"string":"test","object":{"prop1":1,"prop2":2}}';

  await IOUtils.writeUTF8(store.path, string, {
    tmpPath: store.path + ".tmp",
  });

  await store.load();

  Assert.deepEqual(store.data, TEST_DATA);
});

/**
 * Loads data from a malformed JSON string.
 */
add_task(async function test_load_string_malformed() {
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string = '{"number":123,"string":"test","object":{"prop1":1,';

  await IOUtils.writeUTF8(store.path, string, {
    tmpPath: store.path + ".tmp",
  });

  await store.load();

  // A backup file should have been created.
  Assert.ok(await IOUtils.exists(store.path + ".corrupt"));
  await IOUtils.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  Assert.ok(store.dataReady);
  Assert.deepEqual(store.data, {});
});

/**
 * Loads data from a malformed JSON string, using the synchronous initialization
 * path.
 */
add_task(async function test_load_string_malformed_sync() {
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string = '{"number":123,"string":"test","object":{"prop1":1,';

  await IOUtils.writeUTF8(store.path, string, {
    tmpPath: store.path + ".tmp",
  });

  store.ensureDataReady();

  // A backup file should have been created.
  Assert.ok(await IOUtils.exists(store.path + ".corrupt"));
  await IOUtils.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  Assert.ok(store.dataReady);
  Assert.deepEqual(store.data, {});
});

add_task(async function test_overwrite_data() {
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string = `{"number":456,"string":"tset","object":{"prop1":3,"prop2":4}}`;

  await IOUtils.writeUTF8(storeForSave.path, string, {
    tmpPath: storeForSave.path + ".tmp",
  });

  Assert.ok(!storeForSave.dataReady);
  storeForSave.data = TEST_DATA;
  Assert.ok(storeForSave.dataReady);
  Assert.equal(storeForSave.data, TEST_DATA);

  await new Promise(resolve => {
    let save = storeForSave._save.bind(storeForSave);
    storeForSave._save = () => {
      save();
      resolve();
    };
    storeForSave.saveSoon();
  });

  let storeForLoad = new JSONFile({
    path: storeForSave.path,
  });

  await storeForLoad.load();

  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});

add_task(async function test_beforeSave() {
  let store;
  let promiseBeforeSave = new Promise(resolve => {
    store = new JSONFile({
      path: getTempFile(TEST_STORE_FILE_NAME).path,
      beforeSave: resolve,
      saveDelayMs: 250,
    });
  });

  store.saveSoon();

  await promiseBeforeSave;
});

add_task(async function test_beforeSave_rejects() {
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
    beforeSave() {
      return Promise.reject(new Error("oops"));
    },
    saveDelayMs: 250,
  });

  let promiseSave = new Promise((resolve, reject) => {
    let save = storeForSave._save.bind(storeForSave);
    storeForSave._save = () => {
      save().then(resolve, reject);
    };
    storeForSave.saveSoon();
  });

  await Assert.rejects(promiseSave, function(ex) {
    return ex.message == "oops";
  });
});

add_task(async function test_finalize() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let barrier = new AsyncShutdown.Barrier("test-auto-finalize");
  let storeForSave = new JSONFile({
    path,
    saveDelayMs: 2000,
    finalizeAt: barrier.client,
  });
  await storeForSave.load();
  storeForSave.data = TEST_DATA;
  storeForSave.saveSoon();

  let promiseFinalize = storeForSave.finalize();
  await Assert.rejects(storeForSave.finalize(), /has already been finalized$/);
  await promiseFinalize;
  Assert.ok(!storeForSave.dataReady);

  // Finalization removes the blocker, so waiting should not log an unhandled
  // error even though the object has been explicitly finalized.
  await barrier.wait();

  let storeForLoad = new JSONFile({ path });
  await storeForLoad.load();
  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});

add_task(async function test_finalize_on_shutdown() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let barrier = new AsyncShutdown.Barrier("test-finalize-shutdown");
  let storeForSave = new JSONFile({
    path,
    saveDelayMs: 2000,
    finalizeAt: barrier.client,
  });
  await storeForSave.load();
  storeForSave.data = TEST_DATA;
  // Arm the saver, then simulate shutdown and ensure the file is
  // automatically finalized.
  storeForSave.saveSoon();

  await barrier.wait();
  // It's possible for `finalize` to reject when called concurrently with
  // shutdown. We don't distinguish between explicit `finalize` calls and
  // finalization on shutdown because we expect most consumers to rely on the
  // latter. However, this behavior can be safely changed if needed.
  await Assert.rejects(storeForSave.finalize(), /has already been finalized$/);
  Assert.ok(!storeForSave.dataReady);

  let storeForLoad = new JSONFile({ path });
  await storeForLoad.load();
  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});
