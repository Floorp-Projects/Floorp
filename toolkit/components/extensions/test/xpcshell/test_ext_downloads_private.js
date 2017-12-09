/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Downloads.jsm");

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE = `http://localhost:${server.identity.primaryPort}/data`;
const TXT_FILE = "file_download.txt";
const TXT_URL = BASE + "/" + TXT_FILE;

function setup() {
  let downloadDir = FileUtils.getDir("TmpD", ["downloads"]);
  downloadDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_print(`Using download directory ${downloadDir.path}`);

  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, downloadDir);

  do_register_cleanup(() => {
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");

    let entries = downloadDir.directoryEntries;
    while (entries.hasMoreElements()) {
      let entry = entries.getNext().QueryInterface(Ci.nsIFile);
      ok(false, `Leftover file ${entry.path} in download directory`);
      entry.remove(false);
    }

    downloadDir.remove(false);
  });
}

add_task(async function test_private_download() {
  setup();

  let extension = ExtensionTestUtils.loadExtension({
    background: async function() {
      function promiseEvent(eventTarget, accept) {
        return new Promise(resolve => {
          eventTarget.addListener(function listener(data) {
            if (accept && !accept(data)) {
              return;
            }
            eventTarget.removeListener(listener);
            resolve(data);
          });
        });
      }
      let startTestPromise = promiseEvent(browser.test.onMessage);
      let onCreatedPromise = promiseEvent(browser.downloads.onCreated);
      let onDonePromise = promiseEvent(
        browser.downloads.onChanged,
        delta => delta.state && delta.state.current === "complete");

      browser.test.sendMessage("ready");
      let {url, filename} = await startTestPromise;

      browser.test.log("Starting private download");
      let downloadId = await browser.downloads.download({
        url,
        filename,
        incognito: true,
      });

      browser.test.log("Waiting for downloads.onCreated");
      let createdItem = await onCreatedPromise;

      browser.test.log("Waiting for completion notification");
      await onDonePromise;

      // test_ext_downloads_download.js already tests whether the file exists
      // in the file system. Here we will only verify that the  downloads API
      // behaves in a meaningful way.

      let [downloadItem] = await browser.downloads.search({id: downloadId});
      browser.test.assertEq(url, createdItem.url, "onCreated url should match");
      browser.test.assertEq(url, downloadItem.url, "download url should match");
      browser.test.assertTrue(createdItem.incognito,
                              "created download should be private");
      browser.test.assertTrue(downloadItem.incognito,
                              "stored download should be private");

      browser.test.log("Removing downloaded file");
      browser.test.assertTrue(downloadItem.exists, "downloaded file exists");
      await browser.downloads.removeFile(downloadId);

      // Disabled because the assertion fails - https://bugzil.la/1381031
      // let [downloadItem2] = await browser.downloads.search({id: downloadId});
      // browser.test.assertFalse(downloadItem2.exists, "file should be deleted");

      browser.test.log("Erasing private download from history");
      let erasePromise = promiseEvent(browser.downloads.onErased);
      await browser.downloads.erase({id: downloadId});
      browser.test.assertEq(downloadId, await erasePromise,
                            "onErased should be fired for the erased private download");

      browser.test.notifyPass("private download test done");
    },
    manifest: {
      permissions: ["downloads"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  extension.sendMessage({
    url: TXT_URL,
    filename: TXT_FILE,
  });

  await extension.awaitFinish("private download test done");
  await extension.unload();
});
