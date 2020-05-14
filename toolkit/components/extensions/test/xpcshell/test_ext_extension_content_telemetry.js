/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_CONTENT_SCRIPT_INJECTION_MS";
const HISTOGRAM_KEYED = "WEBEXT_CONTENT_SCRIPT_INJECTION_MS_BY_ADDONID";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_telemetry() {
  function contentScript() {
    browser.test.sendMessage("content-script-run");
  }

  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  let extension1 = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_end",
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });
  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_end",
        },
      ],
    },

    files: {
      "content_script.js": contentScript,
    },
  });

  clearHistograms();

  let process = IS_OOP ? "content" : "parent";
  ok(
    !(HISTOGRAM in getSnapshots(process)),
    `No data recorded for histogram: ${HISTOGRAM}.`
  );
  ok(
    !(HISTOGRAM_KEYED in getKeyedSnapshots(process)),
    `No data recorded for keyed histogram: ${HISTOGRAM_KEYED}.`
  );

  await extension1.startup();
  let extensionId = extension1.extension.id;

  info(`Started extension with id ${extensionId}`);

  ok(
    !(HISTOGRAM in getSnapshots(process)),
    `No data recorded for histogram after startup: ${HISTOGRAM}.`
  );
  ok(
    !(HISTOGRAM_KEYED in getKeyedSnapshots(process)),
    `No data recorded for keyed histogram: ${HISTOGRAM_KEYED}.`
  );

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );
  await extension1.awaitMessage("content-script-run");
  await promiseTelemetryRecorded(HISTOGRAM, process, 1);
  await promiseKeyedTelemetryRecorded(HISTOGRAM_KEYED, process, extensionId, 1);

  equal(
    valueSum(getSnapshots(process)[HISTOGRAM].values),
    1,
    `Data recorded for histogram: ${HISTOGRAM}.`
  );
  equal(
    valueSum(getKeyedSnapshots(process)[HISTOGRAM_KEYED][extensionId].values),
    1,
    `Data recorded for histogram: ${HISTOGRAM_KEYED} with key ${extensionId}.`
  );

  await contentPage.close();
  await extension1.unload();

  await extension2.startup();
  let extensionId2 = extension2.extension.id;

  info(`Started extension with id ${extensionId2}`);

  equal(
    valueSum(getSnapshots(process)[HISTOGRAM].values),
    1,
    `No new data recorded for histogram after extension2 startup: ${HISTOGRAM}.`
  );
  equal(
    valueSum(getKeyedSnapshots(process)[HISTOGRAM_KEYED][extensionId].values),
    1,
    `No new data recorded for histogram after extension2 startup: ${HISTOGRAM_KEYED} with key ${extensionId}.`
  );
  ok(
    !(extensionId2 in getKeyedSnapshots(process)[HISTOGRAM_KEYED]),
    `No data recorded for histogram after startup: ${HISTOGRAM_KEYED} with key ${extensionId2}.`
  );

  contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );
  await extension2.awaitMessage("content-script-run");
  await promiseTelemetryRecorded(HISTOGRAM, process, 2);
  await promiseKeyedTelemetryRecorded(
    HISTOGRAM_KEYED,
    process,
    extensionId2,
    1
  );

  equal(
    valueSum(getSnapshots(process)[HISTOGRAM].values),
    2,
    `Data recorded for histogram: ${HISTOGRAM}.`
  );
  equal(
    valueSum(getKeyedSnapshots(process)[HISTOGRAM_KEYED][extensionId].values),
    1,
    `No new data recorded for histogram: ${HISTOGRAM_KEYED} with key ${extensionId}.`
  );
  equal(
    valueSum(getKeyedSnapshots(process)[HISTOGRAM_KEYED][extensionId2].values),
    1,
    `Data recorded for histogram: ${HISTOGRAM_KEYED} with key ${extensionId2}.`
  );

  await contentPage.close();
  await extension2.unload();
});
