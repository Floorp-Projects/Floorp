/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();

  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bug675371"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);

    prepare_test({
      "bug675371@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test);
    install.install();
  });
}

function check_test() {
  AddonManager.getAddonByID("bug675371@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    do_check_true(addon.isActive);

    // Tests that chrome.manifest is registered when the addon is installed.
    var target = { active: false };
    Services.scriptloader.loadSubScript("chrome://bug675371/content/test.js", target);
    do_check_true(target.active);

    prepare_test({
      "bug675371@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    // Tests that chrome.manifest is unregistered when the addon is disabled.
    addon.userDisabled = true;
    target.active = false;
    try {
      Services.scriptloader.loadSubScript("chrome://bug675371/content/test.js", target);
      do_throw("Chrome file should not have been found");
    } catch (e) {
      do_check_false(target.active);
    }

    prepare_test({
      "bug675371@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    // Tests that chrome.manifest is registered when the addon is enabled.
    addon.userDisabled = false;
    target.active = false;
    Services.scriptloader.loadSubScript("chrome://bug675371/content/test.js", target);
    do_check_true(target.active);

    prepare_test({
      "bug675371@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    // Tests that chrome.manifest is unregistered when the addon is uninstalled.
    addon.uninstall();
    target.active = false;
    try {
      Services.scriptloader.loadSubScript("chrome://bug675371/content/test.js", target);
      do_throw("Chrome file should not have been found");
    } catch (e) {
      do_check_false(target.active);
    }

    do_test_finished();
  });
}
