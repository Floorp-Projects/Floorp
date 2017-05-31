/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_EXTENSION_STARTUP_MS";

add_task(async function test_telemetry() {
  let extension1 = ExtensionTestUtils.loadExtension({});
  let extension2 = ExtensionTestUtils.loadExtension({});

  let histogram = Services.telemetry.getHistogramById(HISTOGRAM);
  histogram.clear();
  equal(histogram.snapshot().sum, 0,
        `No data recorded for histogram: ${HISTOGRAM}.`);

  await extension1.startup();

  let histogramSum = histogram.snapshot().sum;
  ok(histogramSum > 0,
     `Data recorded for first extension for histogram: ${HISTOGRAM}.`);

  await extension2.startup();

  ok(histogram.snapshot().sum > histogramSum,
     `Data recorded for second extension for histogram: ${HISTOGRAM}.`);

  await extension1.unload();
  await extension2.unload();
});
