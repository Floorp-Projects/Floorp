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
  TelemetryTestUtils.assertKeyedHistogramValue(
    hist,
    expectedKey,
    Ci.nsITRRSkipReason.TRR_OK,
    1
  );
}

add_task(async function test_trr_lookup_mode_2() {
  await trrLookup(Ci.nsIDNSService.MODE_TRRFIRST);
});

add_task(async function test_trr_lookup_mode_3() {
  await trrLookup(Ci.nsIDNSService.MODE_TRRONLY);
});

add_task(async function test_trr_lookup_mode_0() {
  await trrLookup(
    Ci.nsIDNSService.MODE_NATIVEONLY,
    Ci.nsIDNSService.MODE_TRRFIRST
  );
});

async function trrByTypeLookup(trrURI, expectedSuccess, expectedSkipReason) {
  Services.prefs.setIntPref(
    "doh-rollout.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );

  let hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "TRR_RELEVANT_SKIP_REASON_TRR_FIRST_TYPE_REC"
  );

  setModeAndURI(Ci.nsIDNSService.MODE_TRRFIRST, trrURI);

  Services.dns.clearCache(true);
  await new TRRDNSListener("test.httpssvc.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess,
  });
  let expectedKey = `(other)_2`;

  let snapshot = hist.snapshot();
  await TestUtils.waitForCondition(() => {
    snapshot = hist.snapshot();
    info("snapshot:" + JSON.stringify(snapshot));
    return snapshot;
  });

  TelemetryTestUtils.assertKeyedHistogramValue(
    hist,
    expectedKey,
    expectedSkipReason,
    1
  );
}

add_task(async function test_trr_by_type_lookup_success() {
  await trrByTypeLookup("httpssvc", true, Ci.nsITRRSkipReason.TRR_OK);
});

add_task(async function test_trr_by_type_lookup_fail() {
  await trrByTypeLookup(
    "doh?responseIP=none",
    false,
    Ci.nsITRRSkipReason.TRR_NO_ANSWERS
  );
});
