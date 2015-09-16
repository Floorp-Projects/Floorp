/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the metadata request includes startup time measurements

var tmp = {};
Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm", tmp);
var AddonRepository = tmp.AddonRepository;

var gTelemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
var gManagerWindow;
var gProvider;

function parseParams(aQuery) {
  let params = {};

  for (let param of aQuery.split("&")) {
    let [key, value] = param.split("=");
    params[key] = value;
  }

  return params;
}

function test() {
  waitForExplicitFinish();

  var gSeenRequest = false;

  gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "test1@tests.mozilla.org",
    name: "Test add-on"
  }]);

  function observe(aSubject, aTopic, aData) {
    aSubject.QueryInterface(Ci.nsIChannel);
    let url = aSubject.URI.QueryInterface(Ci.nsIURL);
    if (url.filePath != "/extensions-dummy/metadata") {
      return;
    }
    info(url.query);

    // Check if we encountered telemetry errors and turn the tests for which
    // we don't have valid data into known failures.
    let snapshot = gTelemetry.getHistogramById("STARTUP_MEASUREMENT_ERRORS")
                             .snapshot();

    let tProcessValid = (snapshot.counts[0] == 0);
    let tMainValid = tProcessValid && (snapshot.counts[2] == 0);
    let tFirstPaintValid = tProcessValid && (snapshot.counts[5] == 0);
    let tSessionRestoredValid = tProcessValid && (snapshot.counts[6] == 0);

    let params = parseParams(url.query);

    is(params.appOS, Services.appinfo.OS, "OS should be correct");
    is(params.appVersion, Services.appinfo.version, "Version should be correct");

    if (tMainValid) {
      ok(params.tMain >= 0, "Should be a sensible tMain");
    } else {
      todo(false, "An error occurred while recording the startup timestamps, skipping this test");
    }

    if (tFirstPaintValid) {
      ok(params.tFirstPaint >= 0, "Should be a sensible tFirstPaint");
    } else {
      todo(false, "An error occurred while recording the startup timestamps, skipping this test");
    }

    if (tSessionRestoredValid) {
      ok(params.tSessionRestored >= 0, "Should be a sensible tSessionRestored");
    } else {
      todo(false, "An error occurred while recording the startup timestamps, skipping this test");
    }

    gSeenRequest = true;
  }

  const PREF = "extensions.getAddons.getWithPerformance.url";

  // Watch HTTP requests
  Services.obs.addObserver(observe, "http-on-modify-request", false);
  Services.prefs.setCharPref(PREF,
    "http://127.0.0.1:8888/extensions-dummy/metadata?appOS=%OS%&appVersion=%VERSION%&tMain=%TIME_MAIN%&tFirstPaint=%TIME_FIRST_PAINT%&tSessionRestored=%TIME_SESSION_RESTORED%");

  registerCleanupFunction(function() {
    Services.obs.removeObserver(observe, "http-on-modify-request");
  });

  AddonRepository._beginGetAddons(["test1@tests.mozilla.org"], {
    searchFailed: function() {
      ok(gSeenRequest, "Should have seen metadata request");
      finish();
    }
  }, true);
}

