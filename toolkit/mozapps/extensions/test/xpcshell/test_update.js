/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update checks work

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

do_load_httpd_js();
var testserver;
var profileDir;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  profileDir = gProfD.clone();
  profileDir.append("extensions");

  var dest = profileDir.clone();
  dest.append("addon1@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_update.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, dest);

  dest = profileDir.clone();
  dest.append("addon2@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "addon2@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_update.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "0",
      maxVersion: "0"
    }],
    name: "Test Addon 2",
  }, dest);

  startupManager(1);

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Verify that an update is available and can be installed.
function run_test_1() {
  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "1.0");

    prepare_test({}, [
      "onNewInstall",
    ]);

    a1.findUpdates({
      onCompatibilityUpdated: function(addon) {
        do_throw("Should not have seen a compatibility update");
      },

      onUpdateAvailable: function(addon, install) {
        ensure_test_completed();

        do_check_eq(addon, a1);
        do_check_eq(install.name, addon.name);
        do_check_eq(install.version, "2.0");
        do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
        do_check_eq(install.existingAddon, addon);

        prepare_test({}, [
          "onDownloadStarted",
          "onDownloadEnded",
        ], check_test_1);
        install.install();
      },

      onNoUpdateAvailable: function(addon) {
        do_throw("Should have seen an update");
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_1(install) {
  ensure_test_completed();
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  run_test_2();
}

// Continue installing the update.
function run_test_2() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_2);
}

function check_test_2() {
  ensure_test_completed();

  AddonManager.getAddon("addon1@tests.mozilla.org", function(olda1) {
    do_check_neq(olda1, null);
    do_check_eq(olda1.version, "1.0");
    restartManager(1);

    AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
      do_check_neq(a1, null);
      do_check_eq(a1.version, "2.0");
      do_check_true(isExtensionInAddonsList(profileDir, a1.id));
      a1.uninstall();
      restartManager(0);

      run_test_3();
    });
  });
}


// Check that an update check finds compatibility updates and applies them
function run_test_3() {
  AddonManager.getAddon("addon2@tests.mozilla.org", function(a2) {
    do_check_neq(a2, null);
    do_check_false(a2.isActive);
    do_check_false(a2.isCompatible);
    do_check_true(a2.appDisabled);
    a2.findUpdates({
      onCompatibilityUpdated: function(addon) {
        do_check_true(a2.isCompatible);
        do_check_false(a2.appDisabled);
        do_check_false(a2.isActive);
      },

      onUpdateAvailable: function(addon, install) {
        do_throw("Should not have seen an available update");
      },

      onNoUpdateAvailable: function(addon) {
        do_check_eq(addon, a2);
        restartManager(0);
        check_test_3();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

function check_test_3() {
  AddonManager.getAddon("addon2@tests.mozilla.org", function(a2) {
    do_check_neq(a2, null);
    do_check_true(a2.isActive);
    do_check_true(a2.isCompatible);
    do_check_false(a2.appDisabled);
    a2.uninstall();

    run_test_4();
  });
}

// Test that background update checks work
function run_test_4() {
  var dest = profileDir.clone();
  dest.append("addon1@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_update.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, dest);
  restartManager(1);

  prepare_test({}, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded"
  ], continue_test_4);

  // Fake a timer event to cause a background update and wait for the magic to
  // happen
  gInternalManager.notify(null);
}

function continue_test_4(install) {
  do_check_neq(install.existingAddon, null);
  do_check_eq(install.existingAddon.id, "addon1@tests.mozilla.org");

  prepare_test({
    "addon1@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_4);
}

function check_test_4(install) {
  do_check_eq(install.existingAddon.pendingUpgrade.install, install);

  restartManager(1);
  AddonManager.getAddon("addon1@tests.mozilla.org", function(a1) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "2.0");
    a1.uninstall();
    restartManager(0);

    run_test_5();
  });
}

// Test that background update checks work for lightweight themes
function run_test_5() {
  LightweightThemeManager.currentTheme = {
    id: "1",
    version: "1",
    name: "Test LW Theme",
    description: "A test theme",
    author: "Mozilla",
    homepageURL: "http://localhost:4444/data/index.html",
    headerURL: "http://localhost:4444/data/header.png",
    footerURL: "http://localhost:4444/data/footer.png",
    previewURL: "http://localhost:4444/data/preview.png",
    iconURL: "http://localhost:4444/data/icon.png",
    updateURL: "http://localhost:4444/data/lwtheme.js"
  };

  // XXX The lightweight theme manager strips non-https updateURLs so hack it
  // back in.
  let themes = JSON.parse(Services.prefs.getCharPref("lightweightThemes.usedThemes"));
  do_check_eq(themes.length, 1);
  themes[0].updateURL = "http://localhost:4444/data/lwtheme.js";
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));

  testserver.registerPathHandler("/data/lwtheme.js", function(request, response) {
    response.write(JSON.stringify({
      id: "1",
      version: "2",
      name: "Updated Theme",
      description: "A test theme",
      author: "Mozilla",
      homepageURL: "http://localhost:4444/data/index2.html",
      headerURL: "http://localhost:4444/data/header.png",
      footerURL: "http://localhost:4444/data/footer.png",
      previewURL: "http://localhost:4444/data/preview.png",
      iconURL: "http://localhost:4444/data/icon2.png",
      updateURL: "http://localhost:4444/data/lwtheme.js"
    }));
  });

  AddonManager.getAddon("1@personas.mozilla.org", function(p1) {
    do_check_neq(p1, null);
    do_check_eq(p1.version, "1");
    do_check_eq(p1.name, "Test LW Theme");
    do_check_true(p1.isActive);

    prepare_test({
      "1@personas.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onExternalInstall"
    ], check_test_5);
  
    // Fake a timer event to cause a background update and wait for the magic to
    // happen
    gInternalManager.notify(null);
  });
}

function check_test_5() {
  AddonManager.getAddon("1@personas.mozilla.org", function(p1) {
    do_check_neq(p1, null);
    do_check_eq(p1.version, "2");
    do_check_eq(p1.name, "Updated Theme");

    end_test();
  });
}
