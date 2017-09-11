/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that all bundled add-ons are compatible.

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
  ok(AddonManager.strictCompatibility, "Strict compatibility should be enabled");

  AddonManager.getAllAddons(function gAACallback(aAddons) {
    // Sort add-ons (by type and name) to improve output.
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
    // Add a reminder.
    if (!allCompatible)
      ok(false, "As this test failed, test browser_bug557956.js should have failed, too.");

    finish();
  });
}
