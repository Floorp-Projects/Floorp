/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const ADDONS = {
  test_bootstrap_const: {
    "install.rdf": {
      "id": "bootstrap@tests.mozilla.org",
    },
    "bootstrap.js": "ChromeUtils.import(\"resource://gre/modules/Services.jsm\");\n\nconst install = function() {\n  Services.obs.notifyObservers(null, \"addon-install\");\n};\n"
  },
};

add_task(async function() {
  await promiseStartupManager();

  let sawInstall = false;
  Services.obs.addObserver(function() {
    sawInstall = true;
  }, "addon-install");

  await AddonTestUtils.promiseInstallXPI(ADDONS.test_bootstrap_const);

  ok(sawInstall);
});
