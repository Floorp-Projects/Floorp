/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadStore object.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

XPCOMUtils.defineLazyModuleGetter(this, "DownloadStore",
                                  "resource://gre/modules/DownloadStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm")

/**
 * Returns a new DownloadList object with an associated DownloadStore.
 *
 * @param aStorePath
 *        String pointing to the file to be associated with the DownloadStore,
 *        or undefined to use a non-existing temporary file.  In this case, the
 *        temporary file is deleted when the test file execution finishes.
 *
 * @return {Promise}
 * @resolves Array [ Newly created DownloadList , associated DownloadStore ].
 * @rejects JavaScript exception.
 */
function promiseNewListAndStore(aStorePath) {
  return promiseNewDownloadList().then(function (aList) {
    let path = aStorePath || getTempFile(TEST_STORE_FILE_NAME).path;
    let store = new DownloadStore(aList, path);
    return [aList, store];
  });
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Saves downloads to a file, then reloads them.
 */
add_task(function test_save_reload()
{
  let [listForSave, storeForSave] = yield promiseNewListAndStore();
  let [listForLoad, storeForLoad] = yield promiseNewListAndStore(
                                                 storeForSave.path);

  listForSave.add(yield promiseSimpleDownload(TEST_SOURCE_URI));
  listForSave.add(yield Downloads.createDownload({
    source: { uri: TEST_EMPTY_URI,
              referrer: TEST_REFERRER_URI },
    target: { file: getTempFile(TEST_TARGET_FILE_NAME) },
    saver: { type: "copy" },
  }));

  yield storeForSave.save();
  yield storeForLoad.load();

  let itemsForSave = yield listForSave.getAll();
  let itemsForLoad = yield listForLoad.getAll();

  do_check_eq(itemsForSave.length, itemsForLoad.length);

  // Downloads should be reloaded in the same order.
  for (let i = 0; i < itemsForSave.length; i++) {
    // The reloaded downloads are different objects.
    do_check_neq(itemsForSave[i], itemsForLoad[i]);

    // The reloaded downloads have the same properties.
    do_check_true(itemsForSave[i].source.uri.equals(
                  itemsForLoad[i].source.uri));
    if (itemsForSave[i].source.referrer) {
      do_check_true(itemsForSave[i].source.referrer.equals(
                    itemsForLoad[i].source.referrer));
    } else {
      do_check_true(itemsForLoad[i].source.referrer === null);
    }
    do_check_true(itemsForSave[i].target.file.equals(
                  itemsForLoad[i].target.file));
    do_check_eq(itemsForSave[i].saver.type,
                itemsForLoad[i].saver.type);
  }
});

/**
 * Checks that saving an empty list deletes any existing file.
 */
add_task(function test_save_empty()
{
  let [list, store] = yield promiseNewListAndStore();

  let createdFile = yield OS.File.open(store.path, { create: true });
  yield createdFile.close();

  yield store.save();

  do_check_false(yield OS.File.exists(store.path));

  // If the file does not exist, saving should not generate exceptions.
  yield store.save();
});

/**
 * Checks that loading from a missing file results in an empty list.
 */
add_task(function test_load_empty()
{
  let [list, store] = yield promiseNewListAndStore();

  do_check_false(yield OS.File.exists(store.path));

  yield store.load();

  let items = yield list.getAll();
  do_check_eq(items.length, 0);
});

/**
 * Loads downloads from a string in a predefined format.  The purpose of this
 * test is to verify that the JSON format used in previous versions can be
 * loaded, assuming the file is reloaded on the same platform.
 */
add_task(function test_load_string_predefined()
{
  let [list, store] = yield promiseNewListAndStore();

  // The platform-dependent file name should be generated dynamically.
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  let filePathLiteral = JSON.stringify(targetFile.path);
  let sourceUriLiteral = JSON.stringify(TEST_SOURCE_URI.spec);
  let emptyUriLiteral = JSON.stringify(TEST_EMPTY_URI.spec);
  let referrerUriLiteral = JSON.stringify(TEST_REFERRER_URI.spec);

  let string = "[{\"source\":{\"uri\":" + sourceUriLiteral + "}," +
                "\"target\":{\"file\":" + filePathLiteral + "}," +
                "\"saver\":{\"type\":\"copy\"}}," +
                "{\"source\":{\"uri\":" + emptyUriLiteral + "," +
                "\"referrer\":" + referrerUriLiteral + "}," +
                "\"target\":{\"file\":" + filePathLiteral + "}," +
                "\"saver\":{\"type\":\"copy\"}}]";

  yield OS.File.writeAtomic(store.path,
                            new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  yield store.load();

  let items = yield list.getAll();

  do_check_eq(items.length, 2);

  do_check_true(items[0].source.uri.equals(TEST_SOURCE_URI));
  do_check_true(items[0].target.file.equals(targetFile));

  do_check_true(items[1].source.uri.equals(TEST_EMPTY_URI));
  do_check_true(items[1].source.referrer.equals(TEST_REFERRER_URI));
  do_check_true(items[1].target.file.equals(targetFile));
});

/**
 * Loads downloads from a well-formed JSON string containing unrecognized data.
 */
add_task(function test_load_string_unrecognized()
{
  let [list, store] = yield promiseNewListAndStore();

  // The platform-dependent file name should be generated dynamically.
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  let filePathLiteral = JSON.stringify(targetFile.path);
  let sourceUriLiteral = JSON.stringify(TEST_SOURCE_URI.spec);

  let string = "[{\"source\":null," +
                "\"target\":null}," +
                "{\"source\":{\"uri\":" + sourceUriLiteral + "}," +
                "\"target\":{\"file\":" + filePathLiteral + "}," +
                "\"saver\":{\"type\":\"copy\"}}]";

  yield OS.File.writeAtomic(store.path,
                            new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  yield store.load();

  let items = yield list.getAll();

  do_check_eq(items.length, 1);

  do_check_true(items[0].source.uri.equals(TEST_SOURCE_URI));
  do_check_true(items[0].target.file.equals(targetFile));
});

/**
 * Loads downloads from a malformed JSON string.
 */
add_task(function test_load_string_malformed()
{
  let [list, store] = yield promiseNewListAndStore();

  let string = "[{\"source\":null,\"target\":null}," +
                "{\"source\":{\"uri\":\"about:blank\"}}";

  yield OS.File.writeAtomic(store.path, new TextEncoder().encode(string),
                            { tmpPath: store.path + ".tmp" });

  try {
    yield store.load();
    do_throw("Exception expected when JSON data is malformed.");
  } catch (ex if ex.name == "SyntaxError") {
    do_print("The expected SyntaxError exception was thrown.");
  }

  let items = yield list.getAll();

  do_check_eq(items.length, 0);
});
