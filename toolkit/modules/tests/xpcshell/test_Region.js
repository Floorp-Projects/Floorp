"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const REGION_PREF = "browser.region.network.url";

const RESPONSE_DELAY = 500;
const RESPONSE_TIMEOUT = 100;

const histogram = Services.telemetry.getHistogramById(
  "SEARCH_SERVICE_COUNTRY_FETCH_RESULT"
);

Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);

add_task(async function test_basic() {
  let srv = useHttpServer();
  srv.registerPathHandler("/geo", (req, res) => {
    res.setStatusLine("1.1", 200, "OK");
    send(res, { country_code: "UK" });
  });

  await Region._fetchRegion();
  Assert.ok(true, "Region fetch should succeed");
  Assert.equal(Region.home, "UK", "Region fetch should return correct result");

  await new Promise(r => srv.stop(r));
});

add_task(async function test_invalid_url() {
  histogram.clear();
  Services.prefs.setCharPref(REGION_PREF, "http://localhost:0");
  let result = await Region._fetchRegion();
  Assert.ok(!result, "Should return no result");
  checkTelemetry(Region.TELEMETRY.ERROR);
});

add_task(async function test_invalid_json() {
  histogram.clear();
  Services.prefs.setCharPref(
    REGION_PREF,
    'data:application/json,{"country_code"'
  );
  let result = await Region._fetchRegion();
  Assert.ok(!result, "Should return no result");
  checkTelemetry(Region.TELEMETRY.NO_RESULT);
});

add_task(async function test_mismatched_probe() {
  let probeDetails = await getExpectedHistogramDetails();
  let probeHistogram;
  if (probeDetails) {
    probeHistogram = Services.telemetry.getHistogramById(probeDetails.probeId);
    probeHistogram.clear();
  }
  histogram.clear();

  Services.prefs.setCharPref(
    REGION_PREF,
    'data:application/json,{"country_code": "AU"}'
  );
  await Region._fetchRegion();
  Assert.equal(Region.home, "AU", "Should have correct region");
  checkTelemetry(Region.TELEMETRY.SUCCESS);

  // We dont store probes for linux and on treeherder +
  // Mac there is no plaform countryCode so in these cases
  // skip the rest of the checks.
  if (!probeDetails) {
    return;
  }
  let snapshot = probeHistogram.snapshot();
  deepEqual(snapshot.values, probeDetails.expectedResult);
});

function useHttpServer() {
  let server = new HttpServer();
  server.start(-1);
  Services.prefs.setCharPref(
    REGION_PREF,
    `http://localhost:${server.identity.primaryPort}/geo`
  );
  return server;
}

function send(res, json) {
  res.setStatusLine("1.1", 200, "OK");
  res.setHeader("content-type", "application/json", true);
  res.write(JSON.stringify(json));
}

async function checkTelemetry(aExpectedValue) {
  await TestUtils.waitForCondition(() => {
    return histogram.snapshot().sum == 1;
  });
  let snapshot = histogram.snapshot();
  Assert.equal(snapshot.values[aExpectedValue], 1);
}

// Define some checks for our platform-specific telemetry.
// We can't influence what they return (as we can't
// influence the countryCode the platform thinks we
// are in), but we can check the values are
// correct given reality.
async function getExpectedHistogramDetails() {
  let probeUSMismatched, probeNonUSMismatched;
  switch (AppConstants.platform) {
    case "macosx":
      probeUSMismatched = "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_OSX";
      probeNonUSMismatched =
        "SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_OSX";
      break;
    case "win":
      probeUSMismatched = "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_WIN";
      probeNonUSMismatched =
        "SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_WIN";
      break;
    default:
      break;
  }

  if (probeUSMismatched && probeNonUSMismatched) {
    let countryCode = await Services.sysinfo.countryCode;
    print("Platform says the country-code is", countryCode);
    if (!countryCode) {
      // On treeherder for Mac the countryCode is null, so the probes won't be
      // recorded.
      // We still let the test run for Mac, as a developer would likely
      // eventually pick up on the issue.
      info("No country code set on this machine, skipping rest of test");
      return false;
    }

    if (countryCode == "US") {
      // boolean probe so 3 buckets, expect 1 result for |1|.
      return {
        probeId: probeUSMismatched,
        expectedResult: { 0: 0, 1: 1, 2: 0 },
      };
    }
    // We are expecting probeNonUSMismatched with false if the platform
    // says AU (not a mismatch) and true otherwise.
    return {
      probeId: probeNonUSMismatched,
      expectedResult:
        countryCode == "AU" ? { 0: 1, 1: 0 } : { 0: 0, 1: 1, 2: 0 },
    };
  }
  return false;
}
