/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_BACKGROUND_PAGE_LOAD_MS";

add_task(async function test_telemetry() {
  let extension1 = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("loaded");
    },
  });

  let extension2 = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("loaded");
    },
  });

  let histogram = Services.telemetry.getHistogramById(HISTOGRAM);
  histogram.clear();
  equal(histogram.snapshot().sum, 0,
        `No data recorded for histogram: ${HISTOGRAM}.`);

  await extension1.startup();
  await extension1.awaitMessage("loaded");

  let histogramSum = histogram.snapshot().sum;
  ok(histogramSum > 0,
     `Data recorded for first extension for histogram: ${HISTOGRAM}.`);

  await extension2.startup();
  await extension2.awaitMessage("loaded");

  ok(histogram.snapshot().sum > histogramSum,
     `Data recorded for second extension for histogram: ${HISTOGRAM}.`);

  await extension1.unload();
  await extension2.unload();
});
