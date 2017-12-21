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

    Assert.notEqual(install, null);

    prepare_test({
      "bug675371@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], callback_soon(check_test));
    install.install();
  });
}

function check_test() {
  AddonManager.getAddonByID("bug675371@tests.mozilla.org", do_exception_wrap(function(addon) {
    Assert.notEqual(addon, null);
    Assert.ok(addon.isActive);

    // Tests that chrome.manifest is registered when the addon is installed.
    var target = { };
    Services.scriptloader.loadSubScript("chrome://bug675371/content/test.js", target);
    Assert.ok(target.active);

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
      Assert.ok(!target.active);
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
    Assert.ok(target.active);

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
      Assert.ok(!target.active);
    }

    executeSoon(do_test_finished);
  }));
}
