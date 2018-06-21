/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function run_test() {
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "US"}');
  // funnelcake builds start with "mozilla"
  Services.prefs.getDefaultBranch("").setCharPref("distribution.id", "mozilla38");

  await setUpGeoDefaults();
  await asyncReInit();

  equal(Services.search.defaultEngine.name, "A second test engine");
});
