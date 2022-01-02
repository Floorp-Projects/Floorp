/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_USER_SCRIPT_INJECTION_MS";
const HISTOGRAM_KEYED = "WEBEXT_USER_SCRIPT_INJECTION_MS_BY_ADDONID";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_userScripts_telemetry() {
  function apiScript() {
    browser.userScripts.onBeforeScript.addListener(userScript => {
      const scriptMetadata = userScript.metadata;

      userScript.defineGlobals({
        US_test_sendMessage(msg, data) {
          browser.test.sendMessage(msg, { data, scriptMetadata });
        },
      });
    });
  }

  async function background() {
    const code = `
      US_test_sendMessage("userScript-run", {location: window.location.href});
    `;
    await browser.userScripts.register({
      js: [{ code }],
      matches: ["http://*/*/file_sample.html"],
      runAt: "document_end",
      scriptMetadata: {
        name: "test-user-script-telemetry",
      },
    });

    browser.test.sendMessage("userScript-registered");
  }

  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  let testExtensionDef = {
    manifest: {
      permissions: ["http://*/*/file_sample.html"],
      user_scripts: {
        api_script: "api-script.js",
      },
    },
    background,
    files: {
      "api-script.js": apiScript,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(testExtensionDef);
  let extension2 = ExtensionTestUtils.loadExtension(testExtensionDef);
  let contentPage = await ExtensionTestUtils.loadContentPage("about:blank");

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

  await extension.startup();
  await extension.awaitMessage("userScript-registered");

  let extensionId = extension.extension.id;

  ok(
    !(HISTOGRAM in getSnapshots(process)),
    `No data recorded for histogram after startup: ${HISTOGRAM}.`
  );
  ok(
    !(HISTOGRAM_KEYED in getKeyedSnapshots(process)),
    `No data recorded for keyed histogram: ${HISTOGRAM_KEYED}.`
  );

  let url = `${BASE_URL}/file_sample.html`;
  contentPage.loadURL(url);
  const res = await extension.awaitMessage("userScript-run");
  Assert.deepEqual(
    res,
    {
      data: { location: url },
      scriptMetadata: { name: "test-user-script-telemetry" },
    },
    "The userScript has been executed on the content page as expected"
  );

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
  await extension.unload();

  await extension2.startup();
  await extension2.awaitMessage("userScript-registered");
  let extensionId2 = extension2.extension.id;

  equal(
    valueSum(getSnapshots(process)[HISTOGRAM].values),
    1,
    `No data recorded for histogram after startup: ${HISTOGRAM}.`
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

  contentPage = await ExtensionTestUtils.loadContentPage(url);
  const res2 = await extension2.awaitMessage("userScript-run");
  Assert.deepEqual(
    res2,
    {
      data: { location: url },
      scriptMetadata: { name: "test-user-script-telemetry" },
    },
    "The userScript has been executed on the content page as expected"
  );

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
