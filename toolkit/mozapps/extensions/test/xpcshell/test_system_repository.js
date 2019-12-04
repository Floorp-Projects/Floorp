// Tests that AddonRepository doesn't download results for system add-ons

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

var gServer = new HttpServer();
gServer.start(-1);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");

// Test with a missing features directory
add_task(async function test_app_addons() {
  // Build the test set
  var distroDir = FileUtils.getDir("ProfD", ["sysfeatures"], true);
  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(distroDir, "system1@tests.mozilla.org.xpi");

  xpi = await getSystemAddonXPI(2, "1.0");
  xpi.copyTo(distroDir, "system2@tests.mozilla.org.xpi");

  xpi = await getSystemAddonXPI(3, "1.0");
  xpi.copyTo(distroDir, "system3@tests.mozilla.org.xpi");

  registerDirectory("XREAppFeat", distroDir);

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Services.prefs.setCharPref(
    PREF_GETADDONS_BYIDS,
    `http://localhost:${gServer.identity.primaryPort}/get?%IDS%`
  );

  gServer.registerPathHandler("/get", (request, response) => {
    do_throw("Unexpected request to server.");
  });

  await overrideBuiltIns({
    system: [
      "system1@tests.mozilla.org",
      "system2@tests.mozilla.org",
      "system3@tests.mozilla.org",
    ],
  });

  await promiseStartupManager();

  await AddonRepository.cacheAddons([
    "system1@tests.mozilla.org",
    "system2@tests.mozilla.org",
    "system3@tests.mozilla.org",
  ]);

  let cached = await AddonRepository.getCachedAddonByID(
    "system1@tests.mozilla.org"
  );
  Assert.equal(cached, null);

  cached = await AddonRepository.getCachedAddonByID(
    "system2@tests.mozilla.org"
  );
  Assert.equal(cached, null);

  cached = await AddonRepository.getCachedAddonByID(
    "system3@tests.mozilla.org"
  );
  Assert.equal(cached, null);

  await promiseShutdownManager();
  await new Promise(resolve => gServer.stop(resolve));
});
