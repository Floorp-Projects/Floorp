"use strict";

ChromeUtils.import("resource://gre/modules/ExtensionStorageIDB.jsm");

async function test_multiple_pages() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      function awaitMessage(expectedMsg, api = "test") {
        return new Promise(resolve => {
          browser[api].onMessage.addListener(function listener(msg) {
            if (msg === expectedMsg) {
              browser[api].onMessage.removeListener(listener);
              resolve();
            }
          });
        });
      }

      let tabReady = awaitMessage("tab-ready", "runtime");

      try {
        let storage = browser.storage.local;

        browser.test.sendMessage("load-page", browser.runtime.getURL("tab.html"));
        await awaitMessage("page-loaded");
        await tabReady;

        let result = await storage.get("key");
        browser.test.assertEq(undefined, result.key, "Key should be undefined");

        await browser.runtime.sendMessage("tab-set-key");

        result = await storage.get("key");
        browser.test.assertEq(JSON.stringify({foo: {bar: "baz"}}),
                              JSON.stringify(result.key),
                              "Key should be set to the value from the tab");

        browser.test.sendMessage("remove-page");
        await awaitMessage("page-removed");

        result = await storage.get("key");
        browser.test.assertEq(JSON.stringify({foo: {bar: "baz"}}),
                              JSON.stringify(result.key),
                              "Key should still be set to the value from the tab");

        browser.test.notifyPass("storage-multiple");
      } catch (e) {
        browser.test.fail(`Error: ${e} :: ${e.stack}`);
        browser.test.notifyFail("storage-multiple");
      }
    },

    files: {
      "tab.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="tab.js"></script>
          </head>
        </html>`,

      "tab.js"() {
        browser.test.log("tab");
        browser.runtime.onMessage.addListener(msg => {
          if (msg == "tab-set-key") {
            return browser.storage.local.set({key: {foo: {bar: "baz"}}});
          }
        });

        browser.runtime.sendMessage("tab-ready");
      },
    },

    manifest: {
      permissions: ["storage"],
    },
  });

  let contentPage;
  extension.onMessage("load-page", async url => {
    contentPage = await ExtensionTestUtils.loadContentPage(url, {extension});
    extension.sendMessage("page-loaded");
  });
  extension.onMessage("remove-page", async url => {
    await contentPage.close();
    extension.sendMessage("page-removed");
  });

  await extension.startup();
  await extension.awaitFinish("storage-multiple");
  await extension.unload();
}

add_task(async function test_storage_local_file_backend_from_tab() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, false]],
                      test_multiple_pages);
});

add_task(async function test_storage_local_idb_backend_from_tab() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, true]],
                      test_multiple_pages);
});
