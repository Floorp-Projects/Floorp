"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

Services.prefs.setCharPref("dom.securecontext.allowlist", "example.com");

var kTest103 =
  "https://example.com/browser/netwerk/test/browser/103_preload.html";
var kTestNo103 =
  "https://example.com/browser/netwerk/test/browser/no_103_preload.html";
var kTest404 =
  "https://example.com/browser/netwerk/test/browser/103_preload_and_404.html";

add_task(async function () {
  let hist_hints = TelemetryTestUtils.getAndClearHistogram(
    "EH_NUM_OF_HINTS_PER_PAGE"
  );
  let hist_final = TelemetryTestUtils.getAndClearHistogram("EH_FINAL_RESPONSE");
  let hist_time = TelemetryTestUtils.getAndClearHistogram(
    "EH_TIME_TO_FINAL_RESPONSE"
  );

  await BrowserTestUtils.openNewForegroundTab(gBrowser, kTest103, true);

  // This is a 200 response with 103:
  // EH_NUM_OF_HINTS_PER_PAGE should record 1.
  // EH_FINAL_RESPONSE should record 1 on position 0 (R2xx).
  // EH_TIME_TO_FINAL_RESPONSE should have a new value
  //   (we cannot determine what the timing will be therefore we only check that
  //    the histogram sum is > 0).
  TelemetryTestUtils.assertHistogram(hist_hints, 1, 1);
  TelemetryTestUtils.assertHistogram(hist_final, 0, 1);
  const snapshot = hist_time.snapshot();
  let found = false;
  for (let [val] of Object.entries(snapshot.values)) {
    if (val > 0) {
      found = true;
    }
  }
  Assert.ok(found);

  gBrowser.removeCurrentTab();
});

add_task(async function () {
  let hist_hints = TelemetryTestUtils.getAndClearHistogram(
    "EH_NUM_OF_HINTS_PER_PAGE"
  );
  let hist_final = TelemetryTestUtils.getAndClearHistogram("EH_FINAL_RESPONSE");
  let hist_time = TelemetryTestUtils.getAndClearHistogram(
    "EH_TIME_TO_FINAL_RESPONSE"
  );

  await BrowserTestUtils.openNewForegroundTab(gBrowser, kTestNo103, true);

  // This is a 200 response without 103:
  // EH_NUM_OF_HINTS_PER_PAGE should record 0.
  // EH_FINAL_RESPONSE andd EH_TIME_TO_FINAL_RESPONSE should not be recorded.
  TelemetryTestUtils.assertHistogram(hist_hints, 0, 1);
  const snapshot_final = hist_final.snapshot();
  Assert.equal(snapshot_final.sum, 0);
  const snapshot_time = hist_time.snapshot();
  let found = false;
  for (let [val] of Object.entries(snapshot_time.values)) {
    if (val > 0) {
      found = true;
    }
  }
  Assert.ok(!found);

  gBrowser.removeCurrentTab();
});

add_task(async function () {
  let hist_hints = TelemetryTestUtils.getAndClearHistogram(
    "EH_NUM_OF_HINTS_PER_PAGE"
  );
  let hist_final = TelemetryTestUtils.getAndClearHistogram("EH_FINAL_RESPONSE");
  let hist_time = TelemetryTestUtils.getAndClearHistogram(
    "EH_TIME_TO_FINAL_RESPONSE"
  );

  await BrowserTestUtils.openNewForegroundTab(gBrowser, kTest404, true);

  // This is a 404 response with 103:
  // EH_NUM_OF_HINTS_PER_PAGE and EH_TIME_TO_FINAL_RESPONSE should not be recorded.
  // EH_FINAL_RESPONSE should record 1 on index 2 (R4xx).
  const snapshot_hints = hist_hints.snapshot();
  Assert.equal(snapshot_hints.sum, 0);
  TelemetryTestUtils.assertHistogram(hist_final, 2, 1);
  const snapshot_time = hist_time.snapshot();
  let found = false;
  for (let [val] of Object.entries(snapshot_time.values)) {
    if (val > 0) {
      found = true;
    }
  }
  Assert.ok(!found);

  gBrowser.removeCurrentTab();
});

add_task(async function cleanup() {
  Services.prefs.clearUserPref("dom.securecontext.allowlist");
});
