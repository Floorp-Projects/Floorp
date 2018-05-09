// Tests that AddonRepository doesn't download results for system add-ons

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

BootstrapMonitor.init();

ChromeUtils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);

// Build the test set
var distroDir = FileUtils.getDir("ProfD", ["sysfeatures"], true);
do_get_file("data/system_addons/system1_1.xpi").copyTo(distroDir, "system1@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system2_1.xpi").copyTo(distroDir, "system2@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system3_1.xpi").copyTo(distroDir, "system3@tests.mozilla.org.xpi");
registerDirectory("XREAppFeat", distroDir);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");

// Test with a missing features directory
add_task(async function test_app_addons() {
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, `http://localhost:${gServer.identity.primaryPort}/get?%IDS%`);
  Services.prefs.setCharPref(PREF_COMPAT_OVERRIDES, `http://localhost:${gServer.identity.primaryPort}/get?%IDS%`);

  gServer.registerPathHandler("/get", (request, response) => {
    do_throw("Unexpected request to server.");
  });

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org"] });

  await promiseStartupManager();

  await AddonRepository.cacheAddons(["system1@tests.mozilla.org",
                                     "system2@tests.mozilla.org",
                                     "system3@tests.mozilla.org"]);

  let cached = await AddonRepository.getCachedAddonByID("system1@tests.mozilla.org");
  Assert.equal(cached, null);

  cached = await AddonRepository.getCachedAddonByID("system2@tests.mozilla.org");
  Assert.equal(cached, null);

  cached = await AddonRepository.getCachedAddonByID("system3@tests.mozilla.org");
  Assert.equal(cached, null);

  await promiseShutdownManager();
  await new Promise(resolve => gServer.stop(resolve));
});
