/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);
  Services.prefs
    .getDefaultBranch(SearchUtils.BROWSER_SEARCH_PREF)
    .setCharPref("geoSpecificDefaults.url", "");
});

add_task(async function test_location() {
  Services.prefs.setCharPref(
    "geo.provider-country.network.url",
    'data:application/json,{"country_code": "AU"}'
  );
  await Services.search.init(true);
  equal(
    Services.prefs.getCharPref("browser.search.region"),
    "AU",
    "got the correct region."
  );
  // check we have "success" recorded in telemetry
  checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.SUCCESS);
  // simple checks for our platform-specific telemetry.  We can't influence
  // what they return (as we can't influence the countryCode the platform
  // thinks we are in), but we can check the values are correct given reality.
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
      return;
    }
    let expectedResult;
    let hid;
    // We know geoip said AU - if the platform thinks US then we expect
    // probeUSMismatched with true (ie, a mismatch)
    if (countryCode == "US") {
      hid = probeUSMismatched;
      expectedResult = { 0: 0, 1: 1, 2: 0 }; // boolean probe so 3 buckets, expect 1 result for |1|.
    } else {
      // We are expecting probeNonUSMismatched with false if the platform
      // says AU (not a mismatch) and true otherwise.
      hid = probeNonUSMismatched;
      expectedResult =
        countryCode == "AU" ? { 0: 1, 1: 0 } : { 0: 0, 1: 1, 2: 0 };
    }
    const histogram = Services.telemetry.getHistogramById(hid);
    let snapshot;
    await TestUtils.waitForCondition(() => {
      snapshot = histogram.snapshot();
      return snapshot.sum == 1;
    });

    deepEqual(snapshot.values, expectedResult);
  }
});
