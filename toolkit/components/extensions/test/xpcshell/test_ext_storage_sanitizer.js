/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Sanitizer",
  "resource:///modules/Sanitizer.jsm"
);

async function test_sanitize_offlineApps(storageHelpersScript) {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      background: {
        scripts: ["storageHelpers.js", "background.js"],
      },
    },
    files: {
      "storageHelpers.js": storageHelpersScript,
      "background.js": function() {
        browser.test.onMessage.addListener(async (msg, args) => {
          let result = {};
          switch (msg) {
            case "set-storage-data":
              await window.testWriteKey(...args);
              break;
            case "get-storage-data":
              const value = await window.testReadKey(args[0]);
              browser.test.assertEq(args[1], value, "Got the expected value");
              break;
            default:
              browser.test.fail(`Unexpected test message received: ${msg}`);
          }

          browser.test.sendMessage(`${msg}:done`, result);
        });
      },
    },
  });

  await extension.startup();

  extension.sendMessage("set-storage-data", ["aKey", "aValue"]);
  await extension.awaitMessage("set-storage-data:done");

  await extension.sendMessage("get-storage-data", ["aKey", "aValue"]);
  await extension.awaitMessage("get-storage-data:done");

  info("Verify the extension data not cleared by offlineApps Sanitizer");
  await Sanitizer.sanitize(["offlineApps"]);
  await extension.sendMessage("get-storage-data", ["aKey", "aValue"]);
  await extension.awaitMessage("get-storage-data:done");

  await extension.unload();
}

add_task(async function test_sanitize_offlineApps_extension_indexedDB() {
  await test_sanitize_offlineApps(function indexedDBStorageHelpers() {
    const getIDBStore = () =>
      new Promise(resolve => {
        let dbreq = window.indexedDB.open("TestDB");
        dbreq.onupgradeneeded = () =>
          dbreq.result.createObjectStore("TestStore");
        dbreq.onsuccess = () => resolve(dbreq.result);
      });

    // Export writeKey and readKey storage test helpers.
    window.testWriteKey = (k, v) =>
      getIDBStore().then(db => {
        const tx = db.transaction("TestStore", "readwrite");
        const store = tx.objectStore("TestStore");
        return new Promise((resolve, reject) => {
          tx.oncomplete = evt => resolve(evt.target.result);
          tx.onerror = evt => reject(evt.target.error);
          store.add(v, k);
        });
      });
    window.testReadKey = k =>
      getIDBStore().then(db => {
        const tx = db.transaction("TestStore");
        const store = tx.objectStore("TestStore");
        return new Promise((resolve, reject) => {
          const req = store.get(k);
          tx.oncomplete = evt => resolve(req.result);
          tx.onerror = evt => reject(evt.target.error);
        });
      });
  });
});

add_task(
  {
    // Skip this test if LSNG is not enabled (because this test is only
    // going to pass when nextgen local storage is being used).
    skip_if: () => !Services.prefs.getBoolPref("dom.storage.next_gen"),
  },
  async function test_sanitize_offlineApps_extension_localStorage() {
    await test_sanitize_offlineApps(function indexedDBStorageHelpers() {
      // Export writeKey and readKey storage test helpers.
      window.testWriteKey = (k, v) => window.localStorage.setItem(k, v);
      window.testReadKey = k => window.localStorage.getItem(k);
    });
  }
);
