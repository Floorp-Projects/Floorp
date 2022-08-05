/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);

const TEST_DOMAIN = "https://example.com/";
const TEST_EMAIL_WEBAPP_DOMAIN = "https://test1.example.com/";
const EMAIL_TRACKER_DOMAIN = "https://email-tracking.example.org/";
const TEST_PATH = "browser/toolkit/components/url-classifier/tests/browser/";

const TEST_PAGE = TEST_DOMAIN + TEST_PATH + "page.html";
const TEST_EMAIL_WEBAPP_PAGE =
  TEST_EMAIL_WEBAPP_DOMAIN + TEST_PATH + "page.html";

const EMAIL_TRACKER_PAGE = EMAIL_TRACKER_DOMAIN + TEST_PATH + "page.html";
const EMAIL_TRACKER_IMAGE = EMAIL_TRACKER_DOMAIN + TEST_PATH + "raptor.jpg";

const TELEMETRY_EMAIL_TRACKER_COUNT = "EMAIL_TRACKER_COUNT";
const TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB =
  "EMAIL_TRACKER_EMBEDDED_PER_TAB";

const LABEL_BASE_NORMAL = 0;
const LABEL_CONTENT_NORMAL = 1;
const LABEL_BASE_EMAIL_WEBAPP = 2;
const LABEL_CONTENT_EMAIL_WEBAPP = 3;

const KEY_BASE_NORMAL = "base_normal";
const KEY_CONTENT_NORMAL = "content_normal";
const KEY_ALL_NORMAL = "all_normal";
const KEY_BASE_EMAILAPP = "base_emailapp";
const KEY_CONTENT_EMAILAPP = "content_emailapp";
const KEY_ALL_EMAILAPP = "all_emailapp";

async function clearTelemetry() {
  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry.getHistogramById(TELEMETRY_EMAIL_TRACKER_COUNT).clear();
  Services.telemetry
    .getKeyedHistogramById(TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB)
    .clear();
}

async function loadImage(browser, url) {
  return SpecialPowers.spawn(browser, [url], page => {
    return new Promise(resolve => {
      let image = new content.Image();
      image.src = page + "?" + Math.random();
      image.onload = _ => resolve(true);
      image.onerror = _ => resolve(false);
    });
  });
}

async function getTelemetryProbe(key, label, checkCntFn) {
  let histogram;

  // Wait until the telemetry probe appears.
  await TestUtils.waitForCondition(() => {
    let histograms = Services.telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).parent;

    histogram = histograms[key];

    let checkRes = false;

    if (histogram) {
      checkRes = checkCntFn ? checkCntFn(histogram.values[label]) : true;
    }

    return checkRes;
  });

  return histogram.values[label] || 0;
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

async function checkTelemetryProbe(key, label, expectedCnt) {
  let cnt = await getTelemetryProbe(key, label, cnt => {
    if (cnt === undefined) {
      cnt = 0;
    }

    return cnt == expectedCnt;
  });

  is(cnt, expectedCnt, "There should be expected count in telemetry.");
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

function checkNoTelemetryProbe(key) {
  let histograms = Services.telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;

  let histogram = histograms[key];

  ok(!histogram, `No Telemetry has been recorded for ${key}`);
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "urlclassifier.features.emailtracking.datacollection.blocklistTables",
        "mochitest5-track-simple",
      ],
      [
        "urlclassifier.features.emailtracking.datacollection.allowlistTables",
        "",
      ],
      [
        "urlclassifier.features.emailtracking.blocklistTables",
        "mochitest5-track-simple",
      ],
      ["urlclassifier.features.emailtracking.allowlistTables", ""],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.annotate_channels", false],
      ["privacy.trackingprotection.cryptomining.enabled", false],
      ["privacy.trackingprotection.emailtracking.enabled", true],
      [
        "privacy.trackingprotection.emailtracking.data_collection.enabled",
        true,
      ],
      ["privacy.trackingprotection.fingerprinting.enabled", false],
      ["privacy.trackingprotection.socialtracking.enabled", false],
      [
        "privacy.trackingprotection.emailtracking.webapp.domains",
        "test1.example.com",
      ],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(function() {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  await clearTelemetry();
});

add_task(async function test_email_tracking_telemetry() {
  // Open a non email webapp tab.
  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    // Load a image from the email tracker
    let res = await loadImage(browser, EMAIL_TRACKER_IMAGE);

    is(res, false, "The image is blocked.");

    // Verify the telemetry of the email tracker count.
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_BASE_NORMAL,
      1
    );
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_CONTENT_NORMAL,
      0
    );
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_BASE_EMAIL_WEBAPP,
      0
    );
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_CONTENT_EMAIL_WEBAPP,
      0
    );
  });

  // Open an email webapp tab.
  await BrowserTestUtils.withNewTab(TEST_EMAIL_WEBAPP_PAGE, async browser => {
    // Load a image from the email tracker
    let res = await loadImage(browser, EMAIL_TRACKER_IMAGE);

    is(res, false, "The image is blocked.");

    // Verify the telemetry of the email tracker count.
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_BASE_NORMAL,
      1
    );
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_CONTENT_NORMAL,
      0
    );
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_BASE_EMAIL_WEBAPP,
      1
    );
    await checkTelemetryProbe(
      TELEMETRY_EMAIL_TRACKER_COUNT,
      LABEL_CONTENT_EMAIL_WEBAPP,
      0
    );
  });
  // Make sure the tab was closed properly before clearing Telemetry.
  await BrowserUtils.promiseObserved("window-global-destroyed");

  await clearTelemetry();
});

add_task(async function test_no_telemetry_for_first_party_email_tracker() {
  // Open a email tracker tab.
  await BrowserTestUtils.withNewTab(EMAIL_TRACKER_PAGE, async browser => {
    // Load a image from the first-party email tracker
    let res = await loadImage(browser, EMAIL_TRACKER_IMAGE);

    is(res, true, "The image is loaded.");

    // Verify that there was no telemetry recorded.
    checkNoTelemetryProbe(TELEMETRY_EMAIL_TRACKER_COUNT);
  });
  // Make sure the tab was closed properly before clearing Telemetry.
  await BrowserUtils.promiseObserved("window-global-destroyed");

  await clearTelemetry();
});

add_task(async function test_disable_email_data_collection() {
  // Disable Email Tracking Data Collection.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "privacy.trackingprotection.emailtracking.data_collection.enabled",
        false,
      ],
    ],
  });

  // Open an email webapp tab.
  await BrowserTestUtils.withNewTab(TEST_EMAIL_WEBAPP_PAGE, async browser => {
    // Load a image from the email tracker
    let res = await loadImage(browser, EMAIL_TRACKER_IMAGE);

    is(res, false, "The image is blocked.");

    // Verify that there was no telemetry recorded.
    checkNoTelemetryProbe(TELEMETRY_EMAIL_TRACKER_COUNT);
  });
  // Make sure the tab was closed properly before clearing Telemetry.
  await BrowserUtils.promiseObserved("window-global-destroyed");

  await SpecialPowers.popPrefEnv();
  await clearTelemetry();
});

add_task(async function test_email_tracker_embedded_telemetry() {
  // First, we open a page without loading any email trackers.
  await BrowserTestUtils.withNewTab(TEST_PAGE, async _ => {});
  // Make sure the tab was closed properly before checking Telemetry.
  await BrowserUtils.promiseObserved("window-global-destroyed");

  // Check that the telemetry has been record properly for normal page. The
  // telemetry should show there was no email tracker loaded.
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_BASE_NORMAL,
    0,
    1
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_CONTENT_NORMAL,
    0,
    1
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_ALL_NORMAL,
    0,
    1
  );

  // Second, Open a email webapp tab that doesn't a load email tracker.
  await BrowserTestUtils.withNewTab(TEST_EMAIL_WEBAPP_PAGE, async _ => {});
  // Make sure the tab was closed properly before checking Telemetry.
  await BrowserUtils.promiseObserved("window-global-destroyed");

  // Check that the telemetry has been record properly for the email webapp. The
  // telemetry should show there was no email tracker loaded.
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_BASE_EMAILAPP,
    0,
    1
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_CONTENT_EMAILAPP,
    0,
    1
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_ALL_EMAILAPP,
    0,
    1
  );

  // Third, open a page with one email tracker loaded.
  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    // Load a image from the email tracker
    let res = await loadImage(browser, EMAIL_TRACKER_IMAGE);

    is(res, false, "The image is blocked.");
  });
  // Make sure the tab was closed properly before checking Telemetry.
  await BrowserUtils.promiseObserved("window-global-destroyed");

  // Verify that the telemetry has been record properly, The telemetry should
  // show there was one base email tracker loaded.
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_BASE_NORMAL,
    1,
    1
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_CONTENT_NORMAL,
    0,
    2
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_ALL_NORMAL,
    0,
    1
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_ALL_NORMAL,
    1,
    1
  );

  // Open a page and load the same email tracker multiple times. There
  // should be only one count for the same tracker.
  await BrowserTestUtils.withNewTab(TEST_PAGE, async browser => {
    // Load a image from the email tracker two times.
    await loadImage(browser, EMAIL_TRACKER_IMAGE);
    await loadImage(browser, EMAIL_TRACKER_IMAGE);
  });
  // Make sure the tab was closed properly before checking Telemetry.
  await BrowserUtils.promiseObserved("window-global-destroyed");

  // Verify that there is still only one count when loading the same tracker
  // multiple times.
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_BASE_NORMAL,
    1,
    2
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_CONTENT_NORMAL,
    0,
    3
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_ALL_NORMAL,
    0,
    1
  );
  await checkKeyedHistogram(
    TELEMETRY_EMAIL_TRACKER_EMBEDDED_PER_TAB,
    KEY_ALL_NORMAL,
    1,
    2
  );

  await clearTelemetry();
});
