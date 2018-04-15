/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that all bundled add-ons are compatible.

async function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
  ok(AddonManager.strictCompatibility, "Strict compatibility should be enabled");

  let aAddons = await AddonManager.getAllAddons();
  aAddons.sort(function compareTypeName(a, b) {
    return a.type.localeCompare(b.type) || a.name.localeCompare(b.name);
  });

  let allCompatible = true;
  for (let a of aAddons) {
    // Ignore plugins.
    if (a.type == "plugin")
      continue;

    ok(a.isCompatible, a.type + " " + a.name + " " + a.version + " should be compatible");
    allCompatible = allCompatible && a.isCompatible;
  }

  finish();
}
