/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const { promiseStartupManager, promiseShutdownManager } = AddonTestUtils;

const openSearchEngineDescriptions = [
  {
    file: "secure-and-securely-updated1.xml",
    name: "secure-and-securely-updated1",
    description: "Secure and securely updated 1",
    searchForm: "https://example.com/ss1",
    searchUrl: "https://example.com/ss1?q=foo",
    updateUrl: "https://example.com/ss1.xml",
  },
  {
    file: "secure-and-securely-updated2.xml",
    name: "secure-and-securely-updated2",
    description: "Secure and securely updated 2",
    searchForm: "https://example.com/ss2",
    searchUrl: "https://example.com/ss2?q=foo",
    updateUrl: "https://example.com/ss2.xml",
  },
  {
    file: "secure-and-securely-updated3.xml",
    name: "secure-and-securely-updated3",
    description: "Secure and securely updated 3",
    searchForm: "https://example.com/ss3",
    searchUrl: "https://example.com/ss3?q=foo",
    updateUrl: "https://example.com/ss3.xml",
  },
  {
    file: "secure-and-insecurely-updated1.xml",
    name: "secure-and-insecurely-updated1",
    description: "Secure and insecurely updated 1",
    searchForm: "https://example.com/si1",
    searchUrl: "https://example.com/si1?q=foo",
    updateUrl: "https://example.com/si1.xml",
  },
  {
    file: "secure-and-insecurely-updated2.xml",
    name: "secure-and-insecurely-updated2",
    description: "Secure and insecurely updated 2",
    searchForm: "https://example.com/si2",
    searchUrl: "https://example.com/si2?q=foo",
    updateUrl: "https://example.com/si2.xml",
  },
  {
    file: "insecure-and-securely-updated1.xml",
    name: "insecure-and-securely-updated1",
    description: "Insecure and securely updated 1",
    searchForm: "https://example.com/is1",
    searchUrl: "https://example.com/is1?q=foo",
    updateUrl: "https://example.com/is1.xml",
  },
  {
    file: "insecure-and-insecurely-updated1.xml",
    name: "insecure-and-insecurely-updated1",
    description: "Insecure and insecurely updated 1",
    searchForm: "https://example.com/ii1",
    searchUrl: "https://example.com/ii1?q=foo",
    updateUrl: "https://example.com/ii1.xml",
  },
  {
    file: "insecure-and-insecurely-updated2.xml",
    name: "insecure-and-insecurely-updated2",
    description: "Insecure and insecurely updated 2",
    searchForm: "https://example.com/ii2",
    searchUrl: "https://example.com/ii2?q=foo",
    updateUrl: "https://example.com/ii2.xml",
  },
  {
    file: "secure-and-no-update-url1.xml",
    name: "secure-and-no-update-url1",
    description: "Secure and no update URL 1",
    searchForm: "https://example.com/snu1",
    searchUrl: "https://example.com/snu1?q=foo",
    updateUrl: null,
  },
  {
    file: "insecure-and-no-update-url1.xml",
    name: "insecure-and-no-update-url1",
    description: "Insecure and no update URL 1",
    searchForm: "http://example.com/inu1",
    searchUrl: "http://example.com/inu1?q=foo",
    updateUrl: null,
  },
  {
    file: "secure-localhost.xml",
    name: "secure-localhost",
    description: "Secure localhost",
    searchForm: "http://localhost:8080/sl",
    searchUrl: "http://localhost:8080/sl?q=foo",
    updateUrl: null,
  },
  {
    file: "secure-onionv2.xml",
    name: "secure-onionv2",
    description: "Secure onion v2",
    searchForm: "http://s3zkf3ortukqklec.onion/sov2",
    searchUrl: "http://s3zkf3ortukqklec.onion/sov2?q=foo",
    updateUrl: null,
  },
  {
    file: "secure-onionv3.xml",
    name: "secure-onionv3",
    description: "Secure onion v3",
    searchForm:
      "http://ydemw5wg5cseltau22u4fjfrmfshopaldpoznsirb3rgo2gv6uh4s2y5.onion/sov3",
    searchUrl:
      "http://ydemw5wg5cseltau22u4fjfrmfshopaldpoznsirb3rgo2gv6uh4s2y5.onion/sov3?q=foo",
    updateUrl: null,
  },
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

add_task(async function setup() {
  useHttpServer("opensearch");

  await promiseStartupManager();
  await Services.search.init();

  for (let engine of openSearchEngineDescriptions) {
    await Services.search.addOpenSearchEngine(gDataUrl + engine.file, null);
  }

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
  });
});

add_task(async function() {
  verifyTelemetry("secure_opensearch_engine_count", 9, "secure");
  verifyTelemetry("insecure_opensearch_engine_count", 4, "insecure");
  verifyTelemetry("secure_opensearch_update_count", 4, "securely updated");
  verifyTelemetry("insecure_opensearch_update_count", 4, "insecurely updated");
});
