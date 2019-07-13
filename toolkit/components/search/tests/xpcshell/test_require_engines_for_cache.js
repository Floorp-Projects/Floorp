"strict";

// https://bugzilla.mozilla.org/show_bug.cgi?id=1255605
add_task(async function skip_writing_cache_without_engines() {
  Services.prefs.setCharPref("browser.search.region", "US");
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);
  await AddonTestUtils.promiseStartupManager();

  useTestEngines("no-extensions");
  Assert.strictEqual(
    0,
    (await Services.search.getEngines()).length,
    "no engines loaded"
  );
  Assert.ok(!removeCacheFile(), "empty cache file was not created.");
});
