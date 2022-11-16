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

const QUERY_STRIPPING_COUNT = "QUERY_STRIPPING_COUNT";
const QUERY_STRIPPING_PARAM_COUNT = "QUERY_STRIPPING_PARAM_COUNT";

async function clearTelemetry() {
  // There's an arbitrary interval of 2 seconds in which the content
  // processes sync their data with the parent process, we wait
  // this out to ensure that we clear everything.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 2000));

  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry.getHistogramById(QUERY_STRIPPING_COUNT).clear();
  Services.telemetry.getHistogramById(QUERY_STRIPPING_PARAM_COUNT).clear();

  let isCleared = () => {
    let histograms = Services.telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).content;

    return (
      !histograms ||
      (!histograms[QUERY_STRIPPING_COUNT] &&
        !histograms[QUERY_STRIPPING_PARAM_COUNT])
    );
  };

  // Check that the telemetry probes have been cleared properly. Do this check
  // sync first to avoid any race conditions where telemetry arrives after
  // clearing.
  if (!isCleared()) {
    await TestUtils.waitForCondition(isCleared);
  }

  ok(true, "Telemetry has been cleared.");
}

async function verifyQueryString(browser, expected) {
  await SpecialPowers.spawn(browser, [expected], expected => {
    // Strip the first question mark.
    let search = content.location.search.slice(1);

    is(search, expected, "The query string is correct.");
  });
}

async function getTelemetryProbe(probeInParent, key, label, checkCntFn) {
  let histogram;

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
    histogram = histograms[key];

    let checkRes = false;

    if (histogram) {
      checkRes = checkCntFn ? checkCntFn(histogram.values[label]) : true;
    }

    return checkRes;
  });

  return histogram.values[label];
}

async function checkTelemetryProbe(probeInParent, key, expectedCnt, label) {
  let cnt = await getTelemetryProbe(
    probeInParent,
    key,
    label,
    cnt => cnt == expectedCnt
  );

  is(cnt, expectedCnt, "There should be expected count in telemetry.");
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.enabled", true],
      [
        "privacy.query_stripping.strip_list",
        "paramToStrip paramToStripB paramToStripC paramToStripD",
      ],
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
  await checkTelemetryProbe(
    true,
    QUERY_STRIPPING_COUNT,
    1,
    LABEL_STRIP_FOR_NAVIGATION
  );
  await checkTelemetryProbe(true, QUERY_STRIPPING_PARAM_COUNT, 1, "1");

  // Because there would be some loading happening during the test and they
  // could interfere the count here. So, we only verify if the counter is
  // increased, but not the exact count.
  let newNavigationCnt = await getTelemetryProbe(
    true,
    QUERY_STRIPPING_COUNT,
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
  await checkTelemetryProbe(
    false,
    QUERY_STRIPPING_COUNT,
    1,
    LABEL_STRIP_FOR_NAVIGATION
  );
  await checkTelemetryProbe(false, QUERY_STRIPPING_PARAM_COUNT, 1, "1");

  // Check if the navigation count is increased.
  let newNavigationCnt = await getTelemetryProbe(
    false,
    QUERY_STRIPPING_COUNT,
    LABEL_NAVIGATION,
    cnt => cnt > 0
  );
  ok(newNavigationCnt > 0, "There is navigation count added.");

  await clearTelemetry();
});

add_task(async function testQueryStrippingNavigationInContentQueryCount() {
  let testThirdPartyURI =
    TEST_THIRD_PARTY_URI +
    "?paramToStrip=value&paramToStripB=valueB&paramToStripC=valueC&paramToStripD=valueD";

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
  await checkTelemetryProbe(
    false,
    QUERY_STRIPPING_COUNT,
    1,
    LABEL_STRIP_FOR_NAVIGATION
  );

  await getTelemetryProbe(false, QUERY_STRIPPING_PARAM_COUNT, "0", cnt => !cnt);
  await getTelemetryProbe(false, QUERY_STRIPPING_PARAM_COUNT, "1", cnt => !cnt);
  await getTelemetryProbe(false, QUERY_STRIPPING_PARAM_COUNT, "2", cnt => !cnt);
  await getTelemetryProbe(false, QUERY_STRIPPING_PARAM_COUNT, "3", cnt => !cnt);
  await getTelemetryProbe(
    false,
    QUERY_STRIPPING_PARAM_COUNT,
    "4",
    cnt => cnt == 1
  );
  await getTelemetryProbe(false, QUERY_STRIPPING_PARAM_COUNT, "5", cnt => !cnt);

  // Check if the navigation count is increased.
  let newNavigationCnt = await getTelemetryProbe(
    false,
    QUERY_STRIPPING_COUNT,
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
  await checkTelemetryProbe(
    true,
    QUERY_STRIPPING_COUNT,
    1,
    LABEL_STRIP_FOR_REDIRECT
  );
  await checkTelemetryProbe(true, QUERY_STRIPPING_COUNT, 1, LABEL_REDIRECT);
  await checkTelemetryProbe(true, QUERY_STRIPPING_PARAM_COUNT, 1, "1");

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
  await checkTelemetryProbe(
    true,
    QUERY_STRIPPING_COUNT,
    undefined,
    LABEL_STRIP_FOR_NAVIGATION
  );
  // Check if the navigation count is increased.
  let newNavigationCnt = await getTelemetryProbe(
    true,
    QUERY_STRIPPING_COUNT,
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
  await checkTelemetryProbe(
    false,
    QUERY_STRIPPING_COUNT,
    undefined,
    LABEL_STRIP_FOR_NAVIGATION
  );
  // Check if the navigation count is increased.
  newNavigationCnt = await getTelemetryProbe(
    false,
    QUERY_STRIPPING_COUNT,
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
  await checkTelemetryProbe(
    true,
    QUERY_STRIPPING_COUNT,
    undefined,
    LABEL_STRIP_FOR_REDIRECT
  );
  await checkTelemetryProbe(true, QUERY_STRIPPING_COUNT, 1, LABEL_REDIRECT);

  await clearTelemetry();
});
