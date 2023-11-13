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
const TEST_PAGE_FINGERPRINTER = TEST_PATH + "canvas-fingerprinter.html";

const TELEMETRY_CANVAS_FINGERPRINTING_PER_TAB = "CANVAS_FINGERPRINTING_PER_TAB";

const KEY_UNKNOWN = "unknown";
const KEY_KNOWN_TEXT = "known_text";

async function clearTelemetry() {
  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry
    .getKeyedHistogramById(TELEMETRY_CANVAS_FINGERPRINTING_PER_TAB)
    .clear();
}

async function getKeyedHistogram(histogram_id, key, bucket, checkCntFn) {
  let histogram;

  // Wait until the telemetry probe appears.
  await TestUtils.waitForCondition(() => {
    let histograms = Services.telemetry.getSnapshotForKeyedHistograms(
      "main",
      false /* clear */
    ).parent;

    histogram = histograms[histogram_id];

    let checkRes = false;

    if (histogram && histogram[key]) {
      checkRes = checkCntFn ? checkCntFn(histogram[key].values[bucket]) : true;
    }

    return checkRes;
  });

  return histogram[key].values[bucket] || 0;
}

async function checkKeyedHistogram(histogram_id, key, bucket, expectedCnt) {
  let cnt = await getKeyedHistogram(histogram_id, key, bucket, cnt => {
    if (cnt === undefined) {
      cnt = 0;
    }

    return cnt == expectedCnt;
  });

  is(cnt, expectedCnt, "There should be expected count in keyed telemetry.");
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
  await checkKeyedHistogram(
    TELEMETRY_CANVAS_FINGERPRINTING_PER_TAB,
    KEY_UNKNOWN,
    0,
    1
  );

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

  // The telemetry should show a a known fingerprinting text and a known
  // canvas fingerprinter.
  await checkKeyedHistogram(
    TELEMETRY_CANVAS_FINGERPRINTING_PER_TAB,
    KEY_KNOWN_TEXT,
    6, // CanvasFingerprinter::eVariant4
    1
  );

  await clearTelemetry();
});
