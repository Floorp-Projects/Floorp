"use strict";

/* import-globals-from trr_common.js */

// Allow telemetry probes which may otherwise be disabled for some
// applications (e.g. Thunderbird).
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

function setup() {
  h2Port = trr_test_setup();
}

let TRR_OK = 1;

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

async function trrLookup(mode, rolloutMode) {
  let hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "TRR_SKIP_REASON_TRR_FIRST2"
  );

  if (rolloutMode) {
    info("Testing doh-rollout.mode");
    setModeAndURI(0, "doh?responseIP=2.2.2.2");
    Services.prefs.setIntPref("doh-rollout.mode", rolloutMode);
  } else {
    setModeAndURI(mode, "doh?responseIP=2.2.2.2");
  }

  Services.dns.clearCache(true);
  await new TRRDNSListener("test.example.com", "2.2.2.2");
  let expectedKey = `(other)_${mode}`;
  if (mode == 0) {
    expectedKey = "(other)";
  }

  let snapshot = hist.snapshot();
  await TestUtils.waitForCondition(() => {
    snapshot = hist.snapshot();
    info("snapshot:" + JSON.stringify(snapshot));
    return snapshot;
  });
  TelemetryTestUtils.assertKeyedHistogramValue(hist, expectedKey, TRR_OK, 1);
}

add_task(async function test_trr_lookup_mode_2() {
  await trrLookup(2);
});

add_task(async function test_trr_lookup_mode_3() {
  await trrLookup(3);
});

add_task(async function test_trr_lookup_mode_0() {
  await trrLookup(0, 2);
});
