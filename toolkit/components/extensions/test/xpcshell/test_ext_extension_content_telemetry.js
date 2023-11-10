/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_CONTENT_SCRIPT_INJECTION_MS";
const HISTOGRAM_KEYED = "WEBEXT_CONTENT_SCRIPT_INJECTION_MS_BY_ADDONID";
const GLEAN_METRIC_ID = "contentScriptInjection";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

add_task(async function test_telemetry() {
  const { GleanTimingDistribution } = globalThis;

  function contentScript() {
    browser.test.sendMessage("content-script-run");
  }

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

  // Make sure to force flushing glean fog data from child processes before
  // resetting the already collected data.
  await Services.fog.testFlushAllChildren();
  resetTelemetryData();

  let process = "content";

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
    ok(
      !(HISTOGRAM in getSnapshots(process)),
      `No data recorded for histogram: ${HISTOGRAM}.`
    );
    ok(
      !(HISTOGRAM_KEYED in getKeyedSnapshots(process)),
      `No data recorded for keyed histogram: ${HISTOGRAM_KEYED}.`
    );
  }

  // Assert glean telemetry data.
  assertGleanMetricsNoSamples({
    metricId: GLEAN_METRIC_ID,
    gleanMetric: Glean.extensionsTiming[GLEAN_METRIC_ID],
    gleanMetricConstructor: GleanTimingDistribution,
  });

  await extension1.startup();
  let extensionId = extension1.extension.id;

  info(`Started extension with id ${extensionId}`);

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
    ok(
      !(HISTOGRAM in getSnapshots(process)),
      `No data recorded for histogram after startup: ${HISTOGRAM}.`
    );
    ok(
      !(HISTOGRAM_KEYED in getKeyedSnapshots(process)),
      `No data recorded for keyed histogram: ${HISTOGRAM_KEYED}.`
    );
  }

  // Assert glean telemetry data.
  assertGleanMetricsNoSamples({
    metricId: GLEAN_METRIC_ID,
    gleanMetric: Glean.extensionsTiming[GLEAN_METRIC_ID],
    gleanMetricConstructor: GleanTimingDistribution,
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );
  await extension1.awaitMessage("content-script-run");

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
    await promiseTelemetryRecorded(HISTOGRAM, process, 1);
    await promiseKeyedTelemetryRecorded(
      HISTOGRAM_KEYED,
      process,
      extensionId,
      1
    );

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
  }

  // Assert glean telemetry data.
  await Services.fog.testFlushAllChildren();
  assertGleanMetricsSamplesCount({
    metricId: GLEAN_METRIC_ID,
    gleanMetric: Glean.extensionsTiming[GLEAN_METRIC_ID],
    gleanMetricConstructor: GleanTimingDistribution,
    expectedSamplesCount: 1,
  });

  await contentPage.close();
  await extension1.unload();

  await extension2.startup();
  let extensionId2 = extension2.extension.id;

  info(`Started extension with id ${extensionId2}`);

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
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
  }

  // Assert glean telemetry data.
  await Services.fog.testFlushAllChildren();
  assertGleanMetricsSamplesCount({
    metricId: GLEAN_METRIC_ID,
    gleanMetric: Glean.extensionsTiming[GLEAN_METRIC_ID],
    gleanMetricConstructor: GleanTimingDistribution,
    expectedSamplesCount: 1,
    message: "No new data recorded yet after extension 2 startup",
  });

  contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );
  await extension2.awaitMessage("content-script-run");

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
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
      valueSum(
        getKeyedSnapshots(process)[HISTOGRAM_KEYED][extensionId2].values
      ),
      1,
      `Data recorded for histogram: ${HISTOGRAM_KEYED} with key ${extensionId2}.`
    );
  }

  // Assert glean telemetry data.
  await Services.fog.testFlushAllChildren();
  assertGleanMetricsSamplesCount({
    metricId: GLEAN_METRIC_ID,
    gleanMetric: Glean.extensionsTiming[GLEAN_METRIC_ID],
    gleanMetricConstructor: GleanTimingDistribution,
    expectedSamplesCount: 2,
    message: "New data recorded after extension 2 content script injection",
  });

  await contentPage.close();
  await extension2.unload();
});
