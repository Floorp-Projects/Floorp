/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const { promiseStartupManager, promiseShutdownManager } = AddonTestUtils;

const openSearchEngineFiles = [
  "secure-and-securely-updated1.xml",
  "secure-and-securely-updated2.xml",
  "secure-and-securely-updated3.xml",
  // An insecure search form should not affect telemetry.
  "secure-and-securely-updated-insecure-form.xml",
  "secure-and-insecurely-updated1.xml",
  "secure-and-insecurely-updated2.xml",
  "insecure-and-securely-updated1.xml",
  "insecure-and-insecurely-updated1.xml",
  "insecure-and-insecurely-updated2.xml",
  "secure-and-no-update-url1.xml",
  "insecure-and-no-update-url1.xml",
  "secure-localhost.xml",
  "secure-onionv2.xml",
  "secure-onionv3.xml",
];

async function verifyTelemetry(probeNameFragment, engineCount, type) {
  Services.telemetry.clearScalars();
  await Services.search.runBackgroundChecks();

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    `browser.searchinit.${probeNameFragment}`,
    engineCount,
    `Count of ${type} engines: ${engineCount}`
  );
}

add_setup(async function () {
  useHttpServer("opensearch");

  await promiseStartupManager();
  await Services.search.init();

  for (let file of openSearchEngineFiles) {
    await SearchTestUtils.promiseNewSearchEngine({ url: gDataUrl + file });
  }

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
  });
});

add_task(async function () {
  verifyTelemetry("secure_opensearch_engine_count", 10, "secure");
  verifyTelemetry("insecure_opensearch_engine_count", 4, "insecure");
  verifyTelemetry("secure_opensearch_update_count", 5, "securely updated");
  verifyTelemetry("insecure_opensearch_update_count", 4, "insecurely updated");
});
