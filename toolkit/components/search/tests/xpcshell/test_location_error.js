/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);
});

add_task(async function test_location_error() {
  // We use an invalid port that parses but won't open
  let url = "http://localhost:0";

  Services.prefs.setCharPref("browser.region.network.url", url);
  await Services.search.init();
  try {
    Services.prefs.getCharPref("browser.search.region");
    ok(false, "not expecting region to be set");
  } catch (ex) {}
  // should have an error recorded.
  checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.ERROR);
});
