/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const TEST_PAGE_NORMAL = TEST_PATH + "empty.html";
const TEST_PAGE_FINGERPRINTER = TEST_PATH + "font-fingerprinter.html";

const TELEMETRY_FONT_FINGERPRINTING_PER_TAB = "FONT_FINGERPRINTING_PER_TAB";

async function clearTelemetry() {
  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry
    .getHistogramById(TELEMETRY_FONT_FINGERPRINTING_PER_TAB)
    .clear();
}

async function getHistogram(histogram_id, bucket, checkCntFn) {
  let histogram;

  // Wait until the telemetry probe appears.
  await TestUtils.waitForCondition(() => {
    let histograms = Services.telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;

    histogram = histograms[histogram_id];

    let checkRes = false;

    if (histogram) {
      checkRes = checkCntFn ? checkCntFn(histogram.values[bucket]) : true;
    }

    return checkRes;
  });

  return histogram.values[bucket] || 0;
}

async function checkHistogram(histogram_id, bucket, expectedCnt) {
  let cnt = await getHistogram(histogram_id, bucket, cnt => {
    if (cnt === undefined) {
      cnt = 0;
    }

    return cnt == expectedCnt;
  });

  is(cnt, expectedCnt, "There should be expected count in telemetry.");
}

add_setup(async function () {
  await clearTelemetry();
});

add_task(async function test_canvas_fingerprinting_telemetry() {
  let promiseWindowDestroyed = BrowserUtils.promiseObserved(
    "window-global-destroyed"
  );

  // First, we open a page without any canvas fingerprinters
  await BrowserTestUtils.withNewTab(TEST_PAGE_NORMAL, async _ => {});

  // Make sure the tab was closed properly before checking Telemetry.
  await promiseWindowDestroyed;

  // Check that the telemetry has been record properly for normal page. The
  // telemetry should show there was no known fingerprinting attempt.
  await checkHistogram(TELEMETRY_FONT_FINGERPRINTING_PER_TAB, 0 /* false */, 1);

  await clearTelemetry();
});

add_task(async function test_canvas_fingerprinting_telemetry() {
  let promiseWindowDestroyed = BrowserUtils.promiseObserved(
    "window-global-destroyed"
  );

  // Now open a page with a canvas fingerprinter
  await BrowserTestUtils.withNewTab(TEST_PAGE_FINGERPRINTER, async _ => {});

  // Make sure the tab was closed properly before checking Telemetry.
  await promiseWindowDestroyed;

  // The telemetry should show one font fingerprinting attempt.
  await checkHistogram(TELEMETRY_FONT_FINGERPRINTING_PER_TAB, 1 /* true */, 1);

  await clearTelemetry();
});
