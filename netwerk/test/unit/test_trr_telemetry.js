"use strict";

/* import-globals-from trr_common.js */

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

function setup() {
  h2Port = trr_test_setup();
  runningODoHTests = false;
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

let TRR_OK = 1;
let TRR_NOT_CONFIRMED = 14;
let TRR_DECODE_FAILED = 25;
let TRR_EXCLUDED = 26;
let TRR_NXDOMAIN = 30;

async function trr_retry_decode_failure() {
  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", true);
  let chan = makeChan(
    `https://foo.example.com:${h2Port}/reset-doh-request-count`,
    Ci.nsIRequest.TRR_DISABLED_MODE
  );
  await new Promise(resolve =>
    chan.asyncOpen(new ChannelListener(resolve, null))
  );

  setModeAndURI(2, "doh?responseIP=2.2.2.2&retryOnDecodeFailure=true");
  dns.clearCache(true);
  await new TRRDNSListener("retry_ok.example.com", "2.2.2.2");
}

add_task(async function test_trr_retry_decode_failure() {
  let hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "TRR_SKIP_REASON_STRICT_MODE"
  );

  await trr_retry_decode_failure();

  TelemetryTestUtils.assertKeyedHistogramValue(
    hist,
    `(other)|${TRR_DECODE_FAILED}`,
    TRR_OK,
    1
  );
});

async function trr_not_retry_nxdomain() {
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);

  info("Test nxdomain");
  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2&nxdomain=true");

  await new TRRDNSListener("nxdomain.example.com", {
    expectedAnswer: "127.0.0.1",
  });
}

add_task(async function check_nxdomain_not_retry() {
  let hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "TRR_SKIP_REASON_STRICT_MODE"
  );

  await trr_not_retry_nxdomain();

  TelemetryTestUtils.assertKeyedHistogramValue(
    hist,
    "(other)",
    TRR_NXDOMAIN,
    1
  );
});

async function trr_not_retry_confirmation_failed() {
  dns.clearCache(true);

  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2&nxdomain=true");
  Services.prefs.setCharPref("network.trr.confirmationNS", "example.com");

  await TestUtils.waitForCondition(
    // 3 => CONFIRM_FAILED
    () => dns.currentTrrConfirmationState == 3,
    `Timed out waiting for confirmation failed. Currently ${dns.currentTrrConfirmationState}`,
    1,
    5000
  );

  await new TRRDNSListener("not_confirm.example.com", {
    expectedAnswer: "127.0.0.1",
  });
}

add_task(async function check_confirmation_failure_not_retry() {
  let hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "TRR_SKIP_REASON_STRICT_MODE"
  );

  await trr_not_retry_confirmation_failed();

  TelemetryTestUtils.assertKeyedHistogramValue(
    hist,
    "(other)",
    TRR_NOT_CONFIRMED,
    1
  );
});

async function trr_not_retry_trr_excluded() {
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", true);
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  Services.prefs.setCharPref("network.trr.excluded-domains", "bar.example.com");

  await new TRRDNSListener("bar.example.com", "127.0.0.1");
}

add_task(async function check_trr_excluded_not_retry() {
  let hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "TRR_SKIP_REASON_STRICT_MODE"
  );

  await trr_not_retry_trr_excluded();

  TelemetryTestUtils.assertKeyedHistogramValue(
    hist,
    "(other)",
    TRR_EXCLUDED,
    1
  );
});
