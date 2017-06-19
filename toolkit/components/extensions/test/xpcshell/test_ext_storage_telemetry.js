/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM_IDS = [
  "WEBEXT_STORAGE_LOCAL_SET_MS", "WEBEXT_STORAGE_LOCAL_GET_MS",
];

function arraySum(arr) {
  return arr.reduce((a, b) => a + b, 0);
}

add_task(async function test_telemetry_background() {
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

  // Initialize and clear histograms.
  let histograms = {};
  for (let id of HISTOGRAM_IDS) {
    histograms[id] = Services.telemetry.getHistogramById(id);
    histograms[id].clear();
    equal(arraySum(histograms[id].snapshot().counts), 0,
          `No data recorded for histogram: ${id}.`);
  }

  await extension1.startup();
  await extension1.awaitMessage("backgroundDone");

  // Telemetry from extension1's background page should be recorded.
  for (let id in histograms) {
    equal(arraySum(histograms[id].snapshot().counts), 1,
          `Data recorded for histogram: ${id}.`);
  }

  await extension2.startup();
  await extension2.awaitMessage("backgroundDone");

  // Telemetry from extension2's background page should be recorded.
  for (let id in histograms) {
    equal(arraySum(histograms[id].snapshot().counts), 2,
          `Additional data recorded for histogram: ${id}.`);
  }

  await extension2.unload();

  // Run a content script.
  let contentScriptPromise = extension1.awaitMessage("contentDone");
  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);
  await contentScriptPromise;
  await contentPage.close();

  // Telemetry from extension1's content script should be recorded.
  for (let id in histograms) {
    equal(arraySum(histograms[id].snapshot().counts), 3,
          `Data recorded in content script for histogram: ${id}.`);
  }

  await extension1.unload();
});
