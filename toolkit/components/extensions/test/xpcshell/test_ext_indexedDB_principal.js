"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function test_indexedDB_principal() {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);

  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {},
    async background() {
      browser.test.onMessage.addListener(async msg => {
        if (msg == "create-storage") {
          let request = window.indexedDB.open("TestDatabase");
          request.onupgradeneeded = function(e) {
            let db = e.target.result;
            db.createObjectStore("TestStore");
          };
          request.onsuccess = function(e) {
            let db = e.target.result;
            let tx = db.transaction("TestStore", "readwrite");
            let store = tx.objectStore("TestStore");
            tx.oncomplete = () => browser.test.sendMessage("storage-created");
            store.add("foo", "bar");
            tx.onerror = function(e) {
              browser.test.fail(`Failed with error ${tx.error.message}`);
              // Don't wait for timeout
              browser.test.sendMessage("storage-created");
            };
          };
          request.onerror = function(e) {
            browser.test.fail(`Failed with error ${request.error.message}`);
            // Don't wait for timeout
            browser.test.sendMessage("storage-created");
          };
          return;
        }
        if (msg == "check-storage") {
          let dbRequest = window.indexedDB.open("TestDatabase");
          dbRequest.onupgradeneeded = function() {
            browser.test.fail("Database should exist");
            browser.test.notifyFail("done");
          };
          dbRequest.onsuccess = function(e) {
            let db = e.target.result;
            let transaction = db.transaction("TestStore");
            transaction.onerror = function(e) {
              browser.test.fail(
                `Failed with error ${transaction.error.message}`
              );
              browser.test.notifyFail("done");
            };
            let objectStore = transaction.objectStore("TestStore");
            let request = objectStore.get("bar");
            request.onsuccess = function(event) {
              browser.test.assertEq(
                request.result,
                "foo",
                "Got the expected data"
              );
              browser.test.notifyPass("done");
            };
            request.onerror = function(e) {
              browser.test.fail(`Failed with error ${request.error.message}`);
              browser.test.notifyFail("done");
            };
          };
          dbRequest.onerror = function(e) {
            browser.test.fail(`Failed with error ${dbRequest.error.message}`);
            browser.test.notifyFail("done");
          };
        }
      });
    },
  });

  await extension.startup();
  extension.sendMessage("create-storage");
  await extension.awaitMessage("storage-created");

  await extension.addon.disable();

  Services.prefs.setBoolPref("privacy.firstparty.isolate", false);

  await extension.addon.enable();
  await extension.awaitStartup();

  extension.sendMessage("check-storage");
  await extension.awaitFinish("done");

  await extension.unload();
  Services.prefs.clearUserPref("privacy.firstparty.isolate");
});
