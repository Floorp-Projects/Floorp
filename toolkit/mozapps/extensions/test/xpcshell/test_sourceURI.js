/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);

const PREF_GETADDONS_CACHE_ENABLED       = "extensions.getAddons.cache.enabled";

const PORT          = gServer.identity.primaryPort;
const BASE_URL      = "http://localhost:" + PORT;
const DEFAULT_URL   = "about:blank";

var addon = {
  id: "addon@tests.mozilla.org",
  version: "1.0",
  name: "Test",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function backgroundUpdate(aCallback) {
  Services.obs.addObserver(function() {
    Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");
    aCallback();
  }, "addons-background-update-complete", false);

  AddonManagerPrivate.backgroundUpdateCheck();
}

function run_test() {
  do_test_pending();

  mapUrlToFile("/cache.xml", do_get_file("data/test_sourceURI.xml"), gServer);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, BASE_URL + "/cache.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS_PERFORMANCE, BASE_URL + "/cache.xml");
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  writeInstallRDFForExtension(addon, profileDir);
  startupManager();

  AddonManager.getAddonByID("addon@tests.mozilla.org", function(a) {
    do_check_neq(a, null);
    do_check_eq(a.sourceURI, null);

    backgroundUpdate(function() {
      restartManager();

      AddonManager.getAddonByID("addon@tests.mozilla.org", function(a) {
        do_check_neq(a, null);
        do_check_neq(a.sourceURI, null);
        do_check_eq(a.sourceURI.spec, "http://www.example.com/testaddon.xpi");

        do_test_finished();
      });
    });
  });
}
