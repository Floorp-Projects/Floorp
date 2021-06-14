/**
 * Bug 1706616 - Testing the URL query string stripping telemetry.
 */

/* import-globals-from head.js */

"use strict";

const TEST_URI = TEST_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_THIRD_PARTY_URI = TEST_DOMAIN_2 + TEST_PATH + "file_stripping.html";
const TEST_REDIRECT_URI = TEST_DOMAIN + TEST_PATH + "redirect.sjs";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const LABEL_NAVIGATION = 0;
const LABEL_REDIRECT = 1;
const LABEL_STRIP_FOR_NAVIGATION = 2;
const LABEL_STRIP_FOR_REDIRECT = 3;

async function clearTelemetry() {
  // There's an arbitrary interval of 2 seconds in which the content
  // processes sync their data with the parent process, we wait
  // this out to ensure that we clear everything.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 2000));

  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry.getHistogramById("QUERY_STRIPPING_COUNT").clear();

  // Ensure the data is cleared in content.
  await TestUtils.waitForCondition(() => {
    let histograms = Services.telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).content;

    return !histograms || !histograms.QUERY_STRIPPING_COUNT;
  });
}

async function verifyQueryString(browser, expected) {
  await SpecialPowers.spawn(browser, [expected], expected => {
    // Strip the first question mark.
    let search = content.location.search.slice(1);

    is(search, expected, "The query string is correct.");
  });
}

async function getTelemetryProbe(probeInParent, label, checkCntFn) {
  let queryStrippingHistogram;

  // Wait until the telemetry probe appears.
  await TestUtils.waitForCondition(() => {
    let histograms;
    if (probeInParent) {
      histograms = Services.telemetry.getSnapshotForHistograms(
        "main",
        false /* clear */
      ).parent;
    } else {
      histograms = Services.telemetry.getSnapshotForHistograms(
        "main",
        false /* clear */
      ).content;
    }
    queryStrippingHistogram = histograms.QUERY_STRIPPING_COUNT;

    let checkRes = false;

    if (queryStrippingHistogram) {
      checkRes = checkCntFn
        ? checkCntFn(queryStrippingHistogram.values[label])
        : true;
    }

    return checkRes;
  });

  return queryStrippingHistogram.values[label];
}

async function checkTelemetryProbe(probeInParent, expectedCnt, label) {
  let cnt = await getTelemetryProbe(
    probeInParent,
    label,
    cnt => cnt == expectedCnt
  );

  is(cnt, expectedCnt, "There should be expected count in telemetry.");
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.enabled", true],
      ["privacy.query_stripping.strip_list", "paramToStrip"],
    ],
  });

  // Clear Telemetry probes before testing.
  await clearTelemetry();
});

add_task(async function testQueryStrippingNavigationInParent() {
  let testURI = TEST_URI + "?paramToStrip=value";

  // Open a new tab and trigger the query stripping.
  await BrowserTestUtils.withNewTab(testURI, async browser => {
    // Verify if the query string was happened.
    await verifyQueryString(browser, "");
  });

  // Verify the telemetry probe. The stripping for new tab loading would happen
  // in the parent process, so we check values in parent process.
  await checkTelemetryProbe(true, 1, LABEL_STRIP_FOR_NAVIGATION);

  // Because there would be some loading happening during the test and they
  // could interfere the count here. So, we only verify if the counter is
  // increased, but not the exact count.
  let newNavigationCnt = await getTelemetryProbe(
    true,
    LABEL_NAVIGATION,
    cnt => cnt > 0
  );
  ok(newNavigationCnt > 0, "There is navigation count added.");

  await clearTelemetry();
});

add_task(async function testQueryStrippingNavigationInContent() {
  let testThirdPartyURI = TEST_THIRD_PARTY_URI + "?paramToStrip=value";

  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_THIRD_PARTY_URI
    );

    // Trigger the navigation by script.
    await SpecialPowers.spawn(browser, [testThirdPartyURI], async url => {
      content.postMessage({ type: "script", url }, "*");
    });

    await locationChangePromise;

    // Verify if the query string was happened.
    await verifyQueryString(browser, "");
  });

  // Verify the telemetry probe in content process.
  await checkTelemetryProbe(false, 1, LABEL_STRIP_FOR_NAVIGATION);

  // Check if the navigation count is increased.
  let newNavigationCnt = await getTelemetryProbe(
    false,
    LABEL_NAVIGATION,
    cnt => cnt > 0
  );
  ok(newNavigationCnt > 0, "There is navigation count added.");

  await clearTelemetry();
});

add_task(async function testQueryStrippingRedirect() {
  let testThirdPartyURI = `${TEST_REDIRECT_URI}?${TEST_THIRD_PARTY_URI}?paramToStrip=value`;

  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_THIRD_PARTY_URI
    );

    // Trigger the redirect.
    await SpecialPowers.spawn(browser, [testThirdPartyURI], async url => {
      content.postMessage({ type: "script", url }, "*");
    });

    await locationChangePromise;

    // Verify if the query string was happened.
    await verifyQueryString(browser, "");
  });

  // Verify the telemetry probe in parent process. Note that there is no
  // non-test loading is using redirect. So, we can check the exact count here.
  await checkTelemetryProbe(true, 1, LABEL_STRIP_FOR_REDIRECT);
  await checkTelemetryProbe(true, 1, LABEL_REDIRECT);

  await clearTelemetry();
});

add_task(async function testQueryStrippingDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.enabled", false]],
  });

  // First, test the navigation in parent process.
  let testURI = TEST_URI + "?paramToStrip=value";

  // Open a new tab and trigger the query stripping.
  await BrowserTestUtils.withNewTab(testURI, async browser => {
    // Verify if the query string was not happened.
    await verifyQueryString(browser, "paramToStrip=value");
  });

  // Verify the telemetry probe. There should be no stripped navigation count in
  // parent.
  await checkTelemetryProbe(true, undefined, LABEL_STRIP_FOR_NAVIGATION);
  // Check if the navigation count is increased.
  let newNavigationCnt = await getTelemetryProbe(
    true,
    LABEL_NAVIGATION,
    cnt => cnt > 0
  );
  ok(newNavigationCnt > 0, "There is navigation count added.");

  // Second, test the navigation in content.
  let testThirdPartyURI = TEST_THIRD_PARTY_URI + "?paramToStrip=value";

  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      testThirdPartyURI
    );

    // Trigger the navigation by script.
    await SpecialPowers.spawn(browser, [testThirdPartyURI], async url => {
      content.postMessage({ type: "script", url }, "*");
    });

    await locationChangePromise;

    // Verify if the query string was happened.
    await verifyQueryString(browser, "paramToStrip=value");
  });

  // Verify the telemetry probe in content process. There should be no  stripped
  // navigation count in content.
  await checkTelemetryProbe(false, undefined, LABEL_STRIP_FOR_NAVIGATION);
  // Check if the navigation count is increased.
  newNavigationCnt = await getTelemetryProbe(
    false,
    LABEL_NAVIGATION,
    cnt => cnt > 0
  );
  ok(newNavigationCnt > 0, "There is navigation count added.");

  // Third, test the redirect.
  testThirdPartyURI = `${TEST_REDIRECT_URI}?${TEST_THIRD_PARTY_URI}?paramToStrip=value`;

  await BrowserTestUtils.withNewTab(TEST_URI, async browser => {
    // Create the promise to wait for the location change.
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      `${TEST_THIRD_PARTY_URI}?paramToStrip=value`
    );

    // Trigger the redirect.
    await SpecialPowers.spawn(browser, [testThirdPartyURI], async url => {
      content.postMessage({ type: "script", url }, "*");
    });

    await locationChangePromise;

    // Verify if the query string was happened.
    await verifyQueryString(browser, "paramToStrip=value");
  });

  // Verify the telemetry probe. The stripped redirect count should not exist.
  await checkTelemetryProbe(true, undefined, LABEL_STRIP_FOR_REDIRECT);
  await checkTelemetryProbe(true, 1, LABEL_REDIRECT);

  await clearTelemetry();
});
