/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "US"}');
  // funnelcake builds start with "mozilla"
  Services.prefs.setCharPref("distribution.id", 'mozilla38');
  setUpGeoDefaults();

  Services.search.init(() => {
    equal(Services.search.defaultEngine.name, "A second test engine");

    do_test_finished();
    run_next_test();
  });
  do_test_pending();
}
