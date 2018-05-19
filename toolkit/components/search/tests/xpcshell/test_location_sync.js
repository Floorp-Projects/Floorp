/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getCountryCodePref() {
  try {
    return Services.prefs.getCharPref("browser.search.countryCode");
  } catch (_) {
    return undefined;
  }
}

// A console listener so we can listen for a log message from nsSearchService.
function promiseTimezoneMessage() {
  return new Promise(resolve => {
    let listener = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIConsoleListener]),
      observe(msg) {
        if (msg.message.startsWith("getIsUS() fell back to a timezone check with the result=")) {
          Services.console.unregisterListener(listener);
          resolve(msg);
        }
      }
    };
    Services.console.registerListener(listener);
  });
}

// Force a sync init and ensure the right thing happens (ie, that no xhr
// request is made and we fall back to the timezone-only trick)
add_task(async function test_simple() {
  deepEqual(getCountryCodePref(), undefined, "no countryCode pref");

  // Still set a geoip pref so we can (indirectly) check it wasn't used.
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "AU"}');

  ok(!Services.search.isInitialized);

  // setup a console listener for the timezone fallback message.
  let promiseTzMessage = promiseTimezoneMessage();

  // fetching the engines forces a sync init, and should have caused us to
  // check the timezone.
  Services.search.getEngines();
  ok(Services.search.isInitialized);

  // a little wait to check we didn't do the xhr thang.
  await new Promise(resolve => {
    do_timeout(500, resolve);
  });

  let msg = await promiseTzMessage;
  print("Timezone message:", msg.message);
  ok(msg.message.endsWith(isUSTimezone().toString()), "fell back to timezone and it matches our timezone");

  deepEqual(getCountryCodePref(), undefined, "didn't do the geoip xhr");
  // and no telemetry evidence of geoip.
  for (let hid of [
    "SEARCH_SERVICE_COUNTRY_FETCH_RESULT",
    "SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS",
    "SEARCH_SERVICE_COUNTRY_TIMEOUT",
    "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_TIMEZONE",
    "SEARCH_SERVICE_US_TIMEZONE_MISMATCHED_COUNTRY",
    "SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT",
    ]) {
      let histogram = Services.telemetry.getHistogramById(hid);
      let snapshot = histogram.snapshot();
      equal(snapshot.sum, 0, hid);
      switch (snapshot.histogram_type) {
        case Ci.nsITelemetry.HISTOGRAM_FLAG:
          // flags are a special case in that they are initialized with a default
          // of one |0|.
          deepEqual(snapshot.counts, [1, 0, 0], hid);
          break;
        case Ci.nsITelemetry.HISTOGRAM_BOOLEAN:
          // booleans aren't initialized at all, so should have all zeros.
          deepEqual(snapshot.counts, [0, 0, 0], hid);
          break;
        case Ci.nsITelemetry.HISTOGRAM_EXPONENTIAL:
        case Ci.nsITelemetry.HISTOGRAM_LINEAR:
          equal(snapshot.counts.reduce((a, b) => a + b), 0, hid);
          break;
        default:
          ok(false, "unknown histogram type " + snapshot.histogram_type + " for " + hid);
      }
    }
});
