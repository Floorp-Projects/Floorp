/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the metadata request includes startup time measurements

Components.utils.import("resource://gre/modules/AddonRepository.jsm");

var gManagerWindow;
var gProvider;

function parseParams(aQuery) {
  let params = {};

  aQuery.split("&").forEach(function(aParam) {
    let [key, value] = aParam.split("=");
    params[key] = value;
  });

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
    if (url.filePath != "/extensions-dummy/metadata")
      return;
    info(url.query);

    let params = parseParams(url.query);

    is(params.appOS, Services.appinfo.OS, "OS should be correct");
    is(params.appVersion, Services.appinfo.version, "Version should be correct");
    ok(params.tMain >= 0, "Should be a sensible tMain");
    ok(params.tFirstPaint >= 0, "Should be a sensible tFirstPaint");
    ok(params.tSessionRestored >= 0, "Should be a sensible tSessionRestored");

    gSeenRequest = true;
  }

  // Watch HTTP requests
  Services.obs.addObserver(observe, "http-on-modify-request", false);
  Services.prefs.setCharPref("extensions.getAddons.get.url",
    "http://127.0.0.1:8888/extensions-dummy/metadata?appOS=%OS%&appVersion=%VERSION%&tMain=%TIME_MAIN%&tFirstPaint=%TIME_FIRST_PAINT%&tSessionRestored=%TIME_SESSION_RESTORED%");

  registerCleanupFunction(function() {
    Services.obs.removeObserver(observe, "http-on-modify-request");
    Services.prefs.clearUserPref("extensions.getAddons.get.url");
  });

  AddonRepository.getAddonsByIDs(["test1@tests.mozilla.org"], {
    searchFailed: function() {
      ok(gSeenRequest, "Should have seen metadata request");
      finish();
    }
  });
}
