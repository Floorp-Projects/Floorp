/* import-globals-from head.js */

"use strict";

const TEST_URI = TEST_DOMAIN + TEST_PATH + "file_stripping.html";

const QUERY_STRIPPING_COUNT = "QUERY_STRIPPING_COUNT";
const QUERY_STRIPPING_PARAM_COUNT = "QUERY_STRIPPING_PARAM_COUNT";
const QUERY_STRIPPING_COUNT_BY_PARAM = "QUERY_STRIPPING_COUNT_BY_PARAM";

const histogramLabels = Services.telemetry.getCategoricalLabels()
  .QUERY_STRIPPING_COUNT_BY_PARAM;

async function clearTelemetry() {
  // There's an arbitrary interval of 2 seconds in which the content
  // processes sync their data with the parent process, we wait
  // this out to ensure that we clear everything.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 2000));

  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry.getHistogramById(QUERY_STRIPPING_COUNT).clear();
  Services.telemetry.getHistogramById(QUERY_STRIPPING_PARAM_COUNT).clear();
  Services.telemetry.getHistogramById(QUERY_STRIPPING_COUNT_BY_PARAM).clear();

  let isCleared = () => {
    let histograms = Services.telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    ).content;

    return (
      !histograms ||
      (!histograms[QUERY_STRIPPING_COUNT] &&
        !histograms[QUERY_STRIPPING_PARAM_COUNT] &&
        !histograms.QUERY_STRIPPING_COUNT_BY_PARAM)
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

function testTelemetry(queryParamToCount) {
  const histogram = Services.telemetry.getHistogramById(
    QUERY_STRIPPING_COUNT_BY_PARAM
  );

  let snapshot = histogram.snapshot();

  let indexToCount = {};
  Object.entries(queryParamToCount).forEach(([key, value]) => {
    let index = histogramLabels.indexOf(`param_${key}`);

    // In debug builds we perform additional stripping for testing, which
    // results in telemetry being recorded twice. This does not impact
    // production builds.
    if (SpecialPowers.isDebugBuild) {
      indexToCount[index] = value * 2;
    } else {
      indexToCount[index] = value;
    }
  });

  for (let [i, val] of Object.entries(snapshot.values)) {
    let expectedCount = indexToCount[i] || 0;

    is(
      val,
      expectedCount,
      `Histogram ${QUERY_STRIPPING_COUNT_BY_PARAM} should have expected value for label ${histogramLabels[i]}.`
    );
  }
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.enabled", true],
      [
        "privacy.query_stripping.strip_list",
        "foo mc_eid oly_anon_id oly_enc_id __s vero_id _hsenc mkt_tok fbclid",
      ],
    ],
  });

  // Clear Telemetry probes before testing.
  await clearTelemetry();
});

/**
 * Tests the QUERY_STRIPPING_COUNT_BY_PARAM histogram telemetry which counts how
 * often query params from a predefined lists are stripped.
 */
add_task(async function test_queryParamCountTelemetry() {
  info("Test with a query params to be stripped and recoded in telemetry.");
  let url = new URL(TEST_URI);
  url.searchParams.set("mc_eid", "myValue");

  // Open a new tab and trigger the query stripping.
  await BrowserTestUtils.withNewTab(url.href, async browser => {
    // Verify that the tracking query param has been stripped.
    await verifyQueryString(browser, "");
  });

  testTelemetry({ mc_eid: 1 });

  // Repeat this with the same query parameter, the respective histogram bucket
  // should be incremented.
  await BrowserTestUtils.withNewTab(url.href, async browser => {
    await verifyQueryString(browser, "");
  });

  testTelemetry({ mc_eid: 2 });

  url = new URL(TEST_URI);
  url.searchParams.set("fbclid", "myValue2");
  url.searchParams.set("mkt_tok", "myValue3");
  url.searchParams.set("bar", "foo");

  info("Test with multiple query params to be stripped.");
  await BrowserTestUtils.withNewTab(url.href, async browser => {
    await verifyQueryString(browser, "bar=foo");
  });
  testTelemetry({ mc_eid: 2, fbclid: 1, mkt_tok: 1 });

  info(
    "Test with query param on the strip-list, which should not be recoded in telemetry."
  );
  url = new URL(TEST_URI);
  url.searchParams.set("foo", "bar");
  url.searchParams.set("__s", "myValue4");
  await BrowserTestUtils.withNewTab(url.href, async browser => {
    await verifyQueryString(browser, "");
  });
  testTelemetry({ mc_eid: 2, fbclid: 1, mkt_tok: 1, __s: 1 });

  url = new URL(TEST_URI);
  url.searchParams.set("foo", "bar");
  await BrowserTestUtils.withNewTab(url.href, async browser => {
    await verifyQueryString(browser, "");
  });
  testTelemetry({ mc_eid: 2, fbclid: 1, mkt_tok: 1, __s: 1 });

  await clearTelemetry();
});
