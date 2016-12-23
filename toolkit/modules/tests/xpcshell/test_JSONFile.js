/**
 * Tests the JSONFile object.
 */

"use strict";

// Globals

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");

let gFileCounter = Math.floor(Math.random() * 1000000);

/**
 * Returns a reference to a temporary file, that is guaranteed not to exist, and
 * to have never been created before.
 *
 * @param aLeafName
 *        Suggested leaf name for the file to be created.
 *
 * @return nsIFile pointing to a non-existent file in a temporary directory.
 *
 * @note It is not enough to delete the file if it exists, or to delete the file
 *       after calling nsIFile.createUnique, because on Windows the delete
 *       operation in the file system may still be pending, preventing a new
 *       file with the same name to be created.
 */
function getTempFile(aLeafName)
{
  // Prepend a serial number to the extension in the suggested leaf name.
  let [base, ext] = DownloadPaths.splitBaseNameAndExtension(aLeafName);
  let leafName = base + "-" + gFileCounter + ext;
  gFileCounter++;

  // Get a file reference under the temporary directory for this test file.
  let file = FileUtils.getFile("TmpD", [leafName]);
  do_check_false(file.exists());

  do_register_cleanup(function() {
    if (file.exists()) {
      file.remove(false);
    }
  });

  return file;
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

add_task(function* test_save_reload()
{
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  yield storeForSave.load();

  do_check_true(storeForSave.dataReady);
  do_check_matches(storeForSave.data, {});

  Object.assign(storeForSave.data, TEST_DATA);

  yield new Promise((resolve) => {
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

  yield storeForLoad.load();

  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});

add_task(function* test_load_sync()
{
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path
  });
  yield storeForSave.load();
  Object.assign(storeForSave.data, TEST_DATA);
  yield storeForSave._save();

  let storeForLoad = new JSONFile({
    path: storeForSave.path,
  });
  storeForLoad.ensureDataReady();

  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});

add_task(function* test_load_with_dataPostProcessor()
{
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path
  });
  yield storeForSave.load();
  Object.assign(storeForSave.data, TEST_DATA);
  yield storeForSave._save();

  let random = Math.random();
  let storeForLoad = new JSONFile({
    path: storeForSave.path,
    dataPostProcessor: (data) => {
      Assert.deepEqual(data, TEST_DATA);

      data.test = random;
      return data;
    },
  });

  yield storeForLoad.load();

  do_check_eq(storeForLoad.data.test, random);
});

add_task(function* test_load_with_dataPostProcessor_fails()
{
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
    dataPostProcessor: () => {
      throw new Error("dataPostProcessor fails.");
    },
  });

  yield Assert.rejects(store.load(), /dataPostProcessor fails\./);

  do_check_false(store.dataReady);
});

add_task(function* test_load_sync_with_dataPostProcessor_fails()
{
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
    dataPostProcessor: () => {
      throw new Error("dataPostProcessor fails.");
    },
  });

  Assert.throws(() => store.ensureDataReady(), /dataPostProcessor fails\./);

  do_check_false(store.dataReady);
});

/**
 * Loads data from a string in a predefined format.  The purpose of this test is
 * to verify that the JSON format used in previous versions can be loaded.
 */
add_task(function* test_load_string_predefined()
{
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string =
    "{\"number\":123,\"string\":\"test\",\"object\":{\"prop1\":1,\"prop2\":2}}";

  yield OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  yield store.load();

  Assert.deepEqual(store.data, TEST_DATA);
});

/**
 * Loads data from a malformed JSON string.
 */
add_task(function* test_load_string_malformed()
{
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string = "{\"number\":123,\"string\":\"test\",\"object\":{\"prop1\":1,";

  yield OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  yield store.load();

  // A backup file should have been created.
  do_check_true(yield OS.File.exists(store.path + ".corrupt"));
  yield OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  do_check_true(store.dataReady);
  do_check_matches(store.data, {});
});

/**
 * Loads data from a malformed JSON string, using the synchronous initialization
 * path.
 */
add_task(function* test_load_string_malformed_sync()
{
  let store = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string = "{\"number\":123,\"string\":\"test\",\"object\":{\"prop1\":1,";

  yield OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  store.ensureDataReady();

  // A backup file should have been created.
  do_check_true(yield OS.File.exists(store.path + ".corrupt"));
  yield OS.File.remove(store.path + ".corrupt");

  // The store should be ready to accept new data.
  do_check_true(store.dataReady);
  do_check_matches(store.data, {});
});

add_task(function* test_overwrite_data()
{
  let storeForSave = new JSONFile({
    path: getTempFile(TEST_STORE_FILE_NAME).path,
  });

  let string = `{"number":456,"string":"tset","object":{"prop1":3,"prop2":4}}`;

  yield OS.File.writeAtomic(storeForSave.path, new TextEncoder().encode(string),
                            { tmpPath: storeForSave.path + ".tmp" });

  Assert.ok(!storeForSave.dataReady);
  storeForSave.data = TEST_DATA;
  Assert.ok(storeForSave.dataReady);
  Assert.equal(storeForSave.data, TEST_DATA);

  yield new Promise((resolve) => {
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

  yield storeForLoad.load();

  Assert.deepEqual(storeForLoad.data, TEST_DATA);
});

add_task(function* test_beforeSave()
{
  let store;
  let promiseBeforeSave = new Promise((resolve) => {
    store = new JSONFile({
      path: getTempFile(TEST_STORE_FILE_NAME).path,
      beforeSave: resolve,
      saveDelayMs: 250,
    });
  });

  store.saveSoon();

  yield promiseBeforeSave;
});

add_task(function* test_beforeSave_rejects()
{
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

  yield rejects(promiseSave, function(ex) {
    return ex.message == "oops";
  });
});
