/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/extension-storage.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://gre/modules/ExtensionStorageSync.jsm");

Service.engineManager.register(ExtensionStorageEngine);
const engine = Service.engineManager.get("extension-storage");
do_get_profile();   // so we can use FxAccounts
loadWebExtensionTestFunctions();

function mock(options) {
  let calls = [];
  let ret = function() {
    calls.push(arguments);
    return options.returns;
  }
  Object.setPrototypeOf(ret, {
    __proto__: Function.prototype,
    get calls() {
      return calls;
    }
  });
  return ret;
}

add_task(function* test_calling_sync_calls__sync() {
  let oldSync = ExtensionStorageEngine.prototype._sync;
  let syncMock = ExtensionStorageEngine.prototype._sync = mock({returns: true});
  try {
    // I wanted to call the main sync entry point for the entire
    // package, but that fails because it tries to sync ClientEngine
    // first, which fails.
    yield engine.sync();
  } finally {
    ExtensionStorageEngine.prototype._sync = oldSync;
  }
  equal(syncMock.calls.length, 1);
});

add_task(function* test_calling_sync_calls_ext_storage_sync() {
  const extension = {id: "my-extension"};
  let oldSync = ExtensionStorageSync.syncAll;
  let syncMock = ExtensionStorageSync.syncAll = mock({returns: Promise.resolve()});
  try {
    yield* withSyncContext(function* (context) {
      // Set something so that everyone knows that we're using storage.sync
      yield ExtensionStorageSync.set(extension, {"a": "b"}, context);

      yield engine._sync();
    });
  } finally {
    ExtensionStorageSync.syncAll = oldSync;
  }
  do_check_true(syncMock.calls.length >= 1);
});
