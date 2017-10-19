/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.import("resource://gre/modules/ExtensionStorageIDB.jsm");

const HISTOGRAM_IDS = [
  "WEBEXT_STORAGE_LOCAL_SET_MS", "WEBEXT_STORAGE_LOCAL_GET_MS",
];

async function test_telemetry_background() {
  const server = createHttpServer();
  server.registerDirectory("/data/", do_get_file("data"));

  const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

  async function contentScript() {
    await browser.storage.local.set({a: "b"});
    await browser.storage.local.get("a");
    browser.runtime.sendMessage("contentDone");
  }

  let extInfo = {
    manifest: {
      permissions: ["storage"],
      content_scripts: [
        {
          "matches": ["http://*/*/file_sample.html"],
          "js": ["content_script.js"],
        },
      ],
    },
    async background() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.sendMessage(msg);
      });

      await browser.storage.local.set({a: "b"});
      await browser.storage.local.get("a");
      browser.test.sendMessage("backgroundDone");
    },
    files: {
      "content_script.js": contentScript,
    },
  };

  let extension1 = ExtensionTestUtils.loadExtension(extInfo);
  let extension2 = ExtensionTestUtils.loadExtension(extInfo);

  clearHistograms();

  let process = IS_OOP ? "extension" : "parent";
  let snapshots = getSnapshots(process);
  for (let id of HISTOGRAM_IDS) {
    ok(!(id in snapshots), `No data recorded for histogram: ${id}.`);
  }

  await extension1.startup();
  await extension1.awaitMessage("backgroundDone");
  for (let id of HISTOGRAM_IDS) {
    await promiseTelemetryRecorded(id, process, 1);
  }

  // Telemetry from extension1's background page should be recorded.
  snapshots = getSnapshots(process);
  for (let id of HISTOGRAM_IDS) {
    equal(arraySum(snapshots[id].counts), 1,
          `Data recorded for histogram: ${id}.`);
  }

  await extension2.startup();
  await extension2.awaitMessage("backgroundDone");
  for (let id of HISTOGRAM_IDS) {
    await promiseTelemetryRecorded(id, process, 2);
  }

  // Telemetry from extension2's background page should be recorded.
  snapshots = getSnapshots(process);
  for (let id of HISTOGRAM_IDS) {
    equal(arraySum(snapshots[id].counts), 2,
          `Additional data recorded for histogram: ${id}.`);
  }

  await extension2.unload();

  // Run a content script.
  process = IS_OOP ? "content" : "parent";
  let expectedCount = IS_OOP ? 1 : 3;
  let contentScriptPromise = extension1.awaitMessage("contentDone");
  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);
  await contentScriptPromise;
  await contentPage.close();

  for (let id of HISTOGRAM_IDS) {
    await promiseTelemetryRecorded(id, process, expectedCount);
  }

  // Telemetry from extension1's content script should be recorded.
  snapshots = getSnapshots(process);
  for (let id of HISTOGRAM_IDS) {
    equal(arraySum(snapshots[id].counts), expectedCount,
          `Data recorded in content script for histogram: ${id}.`);
  }

  await extension1.unload();
}

add_task(function test_telemetry_background_file_backend() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, false]],
                      test_telemetry_background);
});

add_task(function test_telemetry_background_idb_backend() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, true]],
                      test_telemetry_background);
});
