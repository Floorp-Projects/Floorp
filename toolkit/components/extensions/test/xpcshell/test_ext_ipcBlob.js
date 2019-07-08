"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

add_task(async function test_parent_to_child() {
  async function background() {
    const dbName = "broken-blob";
    const dbStore = "blob-store";
    const dbVersion = 1;
    const blobContent = "Hello World!";

    let db = await new Promise((resolve, reject) => {
      let dbOpen = indexedDB.open(dbName, dbVersion);
      dbOpen.onerror = event => {
        browser.test.fail(`Error opening the DB: ${event.target.error}`);
        browser.test.notifyFail("test-completed");
        reject();
      };
      dbOpen.onsuccess = event => {
        resolve(event.target.result);
      };
      dbOpen.onupgradeneeded = event => {
        let dbobj = event.target.result;
        dbobj.onerror = error => {
          browser.test.fail(`Error updating the DB: ${error.target.error}`);
          browser.test.notifyFail("test-completed");
          reject();
        };
        dbobj.createObjectStore(dbStore);
      };
    });

    async function save(blob) {
      let txn = db.transaction([dbStore], "readwrite");
      let store = txn.objectStore(dbStore);
      let req = store.put(blob, "key");

      return new Promise((resolve, reject) => {
        req.onsuccess = () => {
          resolve();
        };
        req.onerror = event => {
          browser.test.fail(
            `Error saving the blob into the DB: ${event.target.error}`
          );
          browser.test.notifyFail("test-completed");
          reject();
        };
      });
    }

    async function load() {
      let txn = db.transaction([dbStore], "readonly");
      let store = txn.objectStore(dbStore);
      let req = store.getAll();

      return new Promise((resolve, reject) => {
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
      })
        .then(loadDetails => {
          let blobs = [];
          loadDetails.forEach(details => {
            blobs.push(details);
          });
          return blobs[0];
        })
        .catch(err => {
          browser.test.fail(
            `Error loading the blob from the DB: ${err} :: ${err.stack}`
          );
          browser.test.notifyFail("test-completed");
        });
    }

    browser.test.log("Blob creation");
    await save(new Blob([blobContent]));
    let blob = await load();

    db.close();

    browser.runtime.onMessage.addListener(([msg, what]) => {
      browser.test.log("Message received from content: " + msg);
      if (msg == "script-ready") {
        return Promise.resolve({ blob });
      }

      if (msg == "script-value") {
        browser.test.assertEq(blobContent, what, "blob content matches");
        browser.test.notifyPass("test-completed");
        return;
      }

      browser.test.fail(`Unexpected test message received: ${msg}`);
    });

    browser.test.sendMessage("bg-ready");
  }

  function contentScriptStart() {
    browser.runtime.sendMessage(["script-ready"], response => {
      let reader = new FileReader();
      reader.addEventListener(
        "load",
        () => {
          browser.runtime.sendMessage(["script-value", reader.result]);
        },
        { once: true }
      );
      reader.readAsText(response.blob);
    });
  }

  let extensionData = {
    background,
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script_start.js"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "content_script_start.js": contentScriptStart,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitMessage("bg-ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.awaitFinish("test-completed");

  await contentPage.close();
  await extension.unload();
});
