/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const HISTOGRAM = "WEBEXT_BACKGROUND_PAGE_LOAD_MS";
const HISTOGRAM_KEYED = "WEBEXT_BACKGROUND_PAGE_LOAD_MS_BY_ADDONID";

add_task(async function test_telemetry() {
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

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

  clearHistograms();

  assertHistogramEmpty(HISTOGRAM);
  assertKeyedHistogramEmpty(HISTOGRAM_KEYED);

  await extension1.startup();
  await extension1.awaitMessage("loaded");

  const processSnapshot = snapshot => {
    return snapshot.sum > 0;
  };

  const processKeyedSnapshot = snapshot => {
    let res = {};
    for (let key of Object.keys(snapshot)) {
      res[key] = snapshot[key].sum > 0;
    }
    return res;
  };

  assertHistogramSnapshot(
    HISTOGRAM,
    { processSnapshot, expectedValue: true },
    `Data recorded for first extension for histogram: ${HISTOGRAM}.`
  );

  assertHistogramSnapshot(
    HISTOGRAM_KEYED,
    {
      keyed: true,
      processSnapshot: processKeyedSnapshot,
      expectedValue: {
        [extension1.extension.id]: true,
      },
    },
    `Data recorded for first extension for histogram ${HISTOGRAM_KEYED}`
  );

  let histogram = Services.telemetry.getHistogramById(HISTOGRAM);
  let histogramKeyed = Services.telemetry.getKeyedHistogramById(
    HISTOGRAM_KEYED
  );
  let histogramSum = histogram.snapshot().sum;
  let histogramSumExt1 = histogramKeyed.snapshot()[extension1.extension.id].sum;

  await extension2.startup();
  await extension2.awaitMessage("loaded");

  assertHistogramSnapshot(
    HISTOGRAM,
    {
      processSnapshot: snapshot => snapshot.sum > histogramSum,
      expectedValue: true,
    },
    `Data recorded for second extension for histogram: ${HISTOGRAM}.`
  );

  assertHistogramSnapshot(
    HISTOGRAM_KEYED,
    {
      keyed: true,
      processSnapshot: processKeyedSnapshot,
      expectedValue: {
        [extension1.extension.id]: true,
        [extension2.extension.id]: true,
      },
    },
    `Data recorded for second extension for histogram ${HISTOGRAM_KEYED}`
  );

  equal(
    histogramKeyed.snapshot()[extension1.extension.id].sum,
    histogramSumExt1,
    `Data recorder for first extension is unchanged on the keyed histogram ${HISTOGRAM_KEYED}`
  );

  await extension1.unload();
  await extension2.unload();
});
