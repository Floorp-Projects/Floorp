/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_CONTENT_SCRIPT_INJECTION_MS";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_telemetry() {
  function contentScript() {
    browser.test.sendMessage("content-script-run");
  }

  let extension1 = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [{
        "matches": ["http://*/*/file_sample.html"],
        "js": ["content_script.js"],
        "run_at": "document_end",
      }],
    },

    files: {
      "content_script.js": contentScript,
    },
  });
  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [{
        "matches": ["http://*/*/file_sample.html"],
        "js": ["content_script.js"],
        "run_at": "document_end",
      }],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  let histogram = Services.telemetry.getHistogramById(HISTOGRAM);
  histogram.clear();
  equal(histogram.snapshot().sum, 0,
        `No data recorded for histogram: ${HISTOGRAM}.`);

  await extension1.startup();
  equal(histogram.snapshot().sum, 0,
        `No data recorded for histogram after startup: ${HISTOGRAM}.`);

  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);
  await extension1.awaitMessage("content-script-run");
  let histogramSum = histogram.snapshot().sum;
  ok(histogramSum > 0,
     `Data recorded for first extension for histogram: ${HISTOGRAM}.`);

  await contentPage.close();
  await extension1.unload();

  await extension2.startup();
  equal(histogram.snapshot().sum, histogramSum,
        `No data recorded for histogram after startup: ${HISTOGRAM}.`);

  contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);
  await extension2.awaitMessage("content-script-run");
  ok(histogram.snapshot().sum > histogramSum,
     `Data recorded for second extension for histogram: ${HISTOGRAM}.`);

  await contentPage.close();
  await extension2.unload();
});
