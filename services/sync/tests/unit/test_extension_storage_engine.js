/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/extension-storage.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://gre/modules/ExtensionStorageSync.jsm");
/* globals extensionStorageSync */

let engine;

function mock(options) {
  let calls = [];
  let ret = function() {
    calls.push(arguments);
    return options.returns;
  };
  Object.setPrototypeOf(ret, {
    __proto__: Function.prototype,
    get calls() {
      return calls;
    }
  });
  return ret;
}

add_task(async function setup() {
  await Service.engineManager.register(ExtensionStorageEngine);
  engine = Service.engineManager.get("extension-storage");
  do_get_profile(); // so we can use FxAccounts
  loadWebExtensionTestFunctions();
});

add_task(async function test_calling_sync_calls__sync() {
  let oldSync = ExtensionStorageEngine.prototype._sync;
  let syncMock = ExtensionStorageEngine.prototype._sync = mock({returns: true});
  try {
    // I wanted to call the main sync entry point for the entire
    // package, but that fails because it tries to sync ClientEngine
    // first, which fails.
    await engine.sync();
  } finally {
    ExtensionStorageEngine.prototype._sync = oldSync;
  }
  equal(syncMock.calls.length, 1);
});

add_task(async function test_calling_sync_calls_ext_storage_sync() {
  const extension = {id: "my-extension"};
  let oldSync = extensionStorageSync.syncAll;
  let syncMock = extensionStorageSync.syncAll = mock({returns: Promise.resolve()});
  try {
    await withSyncContext(async function(context) {
      // Set something so that everyone knows that we're using storage.sync
      await extensionStorageSync.set(extension, {"a": "b"}, context);

      await engine._sync();
    });
  } finally {
    extensionStorageSync.syncAll = oldSync;
  }
  Assert.ok(syncMock.calls.length >= 1);
});
