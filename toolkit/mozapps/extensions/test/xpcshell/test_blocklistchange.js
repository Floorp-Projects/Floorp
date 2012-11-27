/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that changes that cause an add-on to become unblocked or blocked have
// the right effect

// The tests follow a mostly common pattern. First they start with the add-ons
// unblocked, then they make a change that causes the add-ons to become blocked
// then they make a similar change that keeps the add-ons blocked then they make
// a change that unblocks the add-ons. Some tests skip the initial part and
// start with add-ons detected as blocked.

// softblock1 is enabled/disabled by the blocklist changes so its softDisabled
// property should always match its userDisabled property

// softblock2 gets manually enabled then disabled after it becomes blocked so
// its softDisabled property should never become true after that

// softblock3 does the same as softblock2 however it remains disabled

// softblock4 is disabled while unblocked and so should never have softDisabled
// set to true and stay userDisabled. This add-on is not used in tests that
// start with add-ons blocked as it would be identical to softblock3

// softblock5 is a theme. Currently themes just get disabled when they become
// softblocked and have to be manually re-enabled if they become completely
// unblocked (bug 657520)

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

Cu.import("resource://gre/modules/NetUtil.jsm");

// Allow insecure updates
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false)

Cu.import("resource://testing-common/httpd.js");
var testserver;

var default_theme = {
  id: "default@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  internalName: "classic/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock1_1 = {
  id: "softblock1@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock1_2 = {
  id: "softblock1@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock1_3 = {
  id: "softblock1@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock2_1 = {
  id: "softblock2@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock2_2 = {
  id: "softblock2@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock2_3 = {
  id: "softblock2@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock3_1 = {
  id: "softblock3@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock3_2 = {
  id: "softblock3@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock3_3 = {
  id: "softblock3@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock4_1 = {
  id: "softblock4@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock4_2 = {
  id: "softblock4@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock4_3 = {
  id: "softblock4@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock5_1 = {
  id: "softblock5@tests.mozilla.org",
  version: "1.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update1.rdf",
  internalName: "test/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock5_2 = {
  id: "softblock5@tests.mozilla.org",
  version: "2.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update2.rdf",
  internalName: "test/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var softblock5_3 = {
  id: "softblock5@tests.mozilla.org",
  version: "3.0",
  name: "Softblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update3.rdf",
  internalName: "test/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var hardblock_1 = {
  id: "hardblock@tests.mozilla.org",
  version: "1.0",
  name: "Hardblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var hardblock_2 = {
  id: "hardblock@tests.mozilla.org",
  version: "2.0",
  name: "Hardblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var hardblock_3 = {
  id: "hardblock@tests.mozilla.org",
  version: "3.0",
  name: "Hardblocked add-on",
  updateURL: "http://localhost:4444/data/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var regexpblock_1 = {
  id: "regexpblock@tests.mozilla.org",
  version: "1.0",
  name: "RegExp-blocked add-on",
  updateURL: "http://localhost:4444/data/addon_update1.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var regexpblock_2 = {
  id: "regexpblock@tests.mozilla.org",
  version: "2.0",
  name: "RegExp-blocked add-on",
  updateURL: "http://localhost:4444/data/addon_update2.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

var regexpblock_3 = {
  id: "regexpblock@tests.mozilla.org",
  version: "3.0",
  name: "RegExp-blocked add-on",
  updateURL: "http://localhost:4444/data/addon_update3.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "3"
  }]
};

const ADDON_IDS = ["softblock1@tests.mozilla.org",
                   "softblock2@tests.mozilla.org",
                   "softblock3@tests.mozilla.org",
                   "softblock4@tests.mozilla.org",
                   "softblock5@tests.mozilla.org",
                   "hardblock@tests.mozilla.org",
                   "regexpblock@tests.mozilla.org"];

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow: function(parent, url, name, features, arguments) {
    // Should be called to list the newly blocklisted items
    do_check_eq(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    // Simulate auto-disabling any softblocks
    var list = arguments.wrappedJSObject.list;
    list.forEach(function(aItem) {
      if (!aItem.blocked)
        aItem.disable = true;
    });

    //run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed", null);

  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};

var InstallConfirm = {
  confirm: function(aWindow, aUrl, aInstalls, aInstallCount) {
    aInstalls.forEach(function(aInstall) {
      aInstall.install();
    });
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.amIWebInstallPrompt)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var InstallConfirmFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return InstallConfirm.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1", WindowWatcherFactory);
registrar.registerFactory(Components.ID("{f0863905-4dde-42e2-991c-2dc8209bc9ca}"),
                          "Fake Install Prompt",
                          "@mozilla.org/addons/web-install-prompt;1", InstallConfirmFactory);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function load_blocklist(aFile, aCallback) {
  Services.obs.addObserver(function() {
    Services.obs.removeObserver(arguments.callee, "blocklist-updated");

    do_execute_soon(aCallback);
  }, "blocklist-updated", false);

  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:4444/data/" + aFile);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}

// Does a background update check for add-ons and waits for any started installs
// to complete
function background_update(aCallback) {
  var installCount = 0;
  var backgroundCheckCompleted = false;

  AddonManager.addInstallListener({
    onNewInstall: function(aInstall) {
      installCount++;
    },

    onInstallEnded: function(aInstall) {
      installCount--;
      // Wait until all started installs have completed
      if (installCount)
        return;

      AddonManager.removeInstallListener(this);

      // If the background check hasn't yet completed then let that call the
      // callback when it is done
      if (!backgroundCheckCompleted)
        return;

      do_execute_soon(aCallback);
    }
  });

  Services.obs.addObserver(function() {
    Services.obs.removeObserver(arguments.callee, "addons-background-update-complete");
    backgroundCheckCompleted = true;

    // If any new installs have started then we'll call the callback once they
    // are completed
    if (installCount)
      return;

    do_execute_soon(aCallback);
  }, "addons-background-update-complete", false);

  AddonManagerPrivate.backgroundUpdateCheck();
}

// Manually updates the test add-ons to the given version
function manual_update(aVersion, aCallback) {
  var installs = [];
  AddonManager.getInstallForURL("http://localhost:4444/addons/blocklist_soft1_" + aVersion + ".xpi",
                                function(aInstall) {
    installs.push(aInstall);
    AddonManager.getInstallForURL("http://localhost:4444/addons/blocklist_soft2_" + aVersion + ".xpi",
                                  function(aInstall) {
      installs.push(aInstall);
      AddonManager.getInstallForURL("http://localhost:4444/addons/blocklist_soft3_" + aVersion + ".xpi",
                                    function(aInstall) {
        installs.push(aInstall);
        AddonManager.getInstallForURL("http://localhost:4444/addons/blocklist_soft4_" + aVersion + ".xpi",
                                      function(aInstall) {
          installs.push(aInstall);
          AddonManager.getInstallForURL("http://localhost:4444/addons/blocklist_soft5_" + aVersion + ".xpi",
                                        function(aInstall) {
            installs.push(aInstall);
            AddonManager.getInstallForURL("http://localhost:4444/addons/blocklist_hard1_" + aVersion + ".xpi",
                                          function(aInstall) {
              installs.push(aInstall);
              AddonManager.getInstallForURL("http://localhost:4444/addons/blocklist_regexp1_" + aVersion + ".xpi",
                                            function(aInstall) {
                installs.push(aInstall);

                Services.obs.addObserver(function(aSubject, aTopic, aData) {
                  Services.obs.removeObserver(arguments.callee, "addon-install-blocked");

                  aSubject.QueryInterface(Ci.amIWebInstallInfo);

                  var installCount = aSubject.installs.length;

                  var listener = {
                    installComplete: function() {
                      installCount--;
                      if (installCount)
                        return;

                      do_execute_soon(aCallback);
                    },

                    onDownloadCancelled: function(aInstall) {
                      this.installComplete();
                    },

                    onInstallEnded: function(aInstall) {
                      this.installComplete();
                    }
                  };

                  aSubject.installs.forEach(function(aInstall) {
                    aInstall.addListener(listener);
                  });

                  aSubject.install();
                }, "addon-install-blocked", false);

                AddonManager.installAddonsFromWebpage("application/x-xpinstall", null,
                                                      NetUtil.newURI("http://localhost:4444/"),
                                                      installs);
              }, "application/x-xpinstall");
            }, "application/x-xpinstall");
          }, "application/x-xpinstall");
        }, "application/x-xpinstall");
      }, "application/x-xpinstall");
    }, "application/x-xpinstall");
  }, "application/x-xpinstall");
}

// Checks that an add-ons properties match expected values
function check_addon(aAddon, aExpectedVersion, aExpectedUserDisabled,
                     aExpectedSoftDisabled, aExpectedState) {
  dump("Testing " + aAddon.id + " version " + aAddon.version + "\n");
  dump(aAddon.userDisabled + " " + aAddon.softDisabled + "\n");

  do_check_neq(aAddon, null);
  do_check_eq(aAddon.version, aExpectedVersion);
  do_check_eq(aAddon.blocklistState, aExpectedState);
  do_check_eq(aAddon.userDisabled, aExpectedUserDisabled);
  do_check_eq(aAddon.softDisabled, aExpectedSoftDisabled);
  if (aAddon.softDisabled)
    do_check_true(aAddon.userDisabled);

  if (aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED) {
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
  }
  else if (aAddon.userDisabled) {
    do_check_true(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
  }
  else {
    do_check_false(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_ENABLE));
    if (aAddon.type != "theme")
      do_check_true(hasFlag(aAddon.permissions, AddonManager.PERM_CAN_DISABLE));
  }
  do_check_eq(aAddon.appDisabled, aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED);

  let willBeActive = aAddon.isActive;
  if (hasFlag(aAddon.pendingOperations, AddonManager.PENDING_DISABLE))
    willBeActive = false;
  else if (hasFlag(aAddon.pendingOperations, AddonManager.PENDING_ENABLE))
    willBeActive = true;

  if (aExpectedUserDisabled || aExpectedState == Ci.nsIBlocklistService.STATE_BLOCKED) {
    do_check_false(willBeActive);
  }
  else {
    do_check_true(willBeActive);
  }
}

function run_test() {
  // Create and configure the HTTP server.
  testserver = new HttpServer();
  testserver.registerDirectory("/data/", do_get_file("data/blocklistchange"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  writeInstallRDFForExtension(default_theme, profileDir);
  writeInstallRDFForExtension(softblock1_1, profileDir);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  writeInstallRDFForExtension(softblock5_1, profileDir);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  writeInstallRDFForExtension(regexpblock_1, profileDir);
  startupManager();

  AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {
    s4.userDisabled = true;
    s5.userDisabled = false;
    restartManager();

    run_app_update_test();
  });
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Starts with add-ons unblocked and then switches application versions to
// change add-ons to blocked and back
function run_app_update_test() {
  dump(arguments.callee.name + "\n");
  load_blocklist("app_update.xml", function() {
    restartManager();

    AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

      check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");

      restartManager("2");

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

        s2.userDisabled = false;
        s2.userDisabled = true;
        check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        s3.userDisabled = false;
        check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        restartManager();

        restartManager("2.5");

        AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

          check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

          restartManager("1");

          AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

            check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

            s1.userDisabled = false;
            s2.userDisabled = false;
            s5.userDisabled = false;
            run_app_update_schema_test();
          });
        });
      });
    });
  });
}

// Starts with add-ons unblocked and then switches application versions to
// change add-ons to blocked and back. A DB schema change is faked to force a
// rebuild when the application version changes
function run_app_update_schema_test() {
  dump(arguments.callee.name + "\n");
  restartManager();

  AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

    check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");

    shutdownManager();
    var dbfile = gProfD.clone();
    dbfile.append("extensions.sqlite");
    var db = Services.storage.openDatabase(dbfile);
    db.schemaVersion = 100;
    db.close();
    gAppInfo.version = "2";
    startupManager(true);

    AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

      check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
      check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
      do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

      s2.userDisabled = false;
      s2.userDisabled = true;
      check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      s3.userDisabled = false;
      check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      restartManager();

      shutdownManager();
      var dbfile = gProfD.clone();
      dbfile.append("extensions.sqlite");
      var db = Services.storage.openDatabase(dbfile);
      db.schemaVersion = 100;
      db.close();
      gAppInfo.version = "2.5";
      startupManager(true);

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

        shutdownManager();
        var dbfile = gProfD.clone();
        dbfile.append("extensions.sqlite");
        var db = Services.storage.openDatabase(dbfile);
        db.schemaVersion = 100;
        db.close();
        startupManager(false);

        AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

          check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

          shutdownManager();
          var dbfile = gProfD.clone();
          dbfile.append("extensions.sqlite");
          var db = Services.storage.openDatabase(dbfile);
          db.schemaVersion = 100;
          db.close();
          gAppInfo.version = "1";
          startupManager(true);

          AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

            check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

            s1.userDisabled = false;
            s2.userDisabled = false;
            s5.userDisabled = false;
            run_blocklist_update_test();
          });
        });
      });
    });
  });
}

// Starts with add-ons unblocked and then loads new blocklists to change add-ons
// to blocked and back again.
function run_blocklist_update_test() {
  dump(arguments.callee.name + "\n");
  load_blocklist("blocklist_update1.xml", function() {
    restartManager();

    AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

      check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");

      load_blocklist("blocklist_update2.xml", function() {
        restartManager();

        AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

          check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

          s2.userDisabled = false;
          s2.userDisabled = true;
          check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          s3.userDisabled = false;
          check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          restartManager();

          load_blocklist("blocklist_update2.xml", function() {
            restartManager();

            AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

              check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
              check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
              check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
              check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
              check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
              check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
              check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
              do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

              load_blocklist("blocklist_update1.xml", function() {
                restartManager();

                AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

                  check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
                  check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
                  check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
                  check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
                  check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
                  check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
                  check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
                  do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

                  s1.userDisabled = false;
                  s2.userDisabled = false;
                  s5.userDisabled = false;
                  run_addon_change_test();
                });
              });
            });
          });
        });
      });
    });
  });
}

// Starts with add-ons unblocked and then new versions are installed outside of
// the app to change them to blocked and back again.
function run_addon_change_test() {
  dump(arguments.callee.name + "\n");
  load_blocklist("addon_change.xml", function() {
    restartManager();

    AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

      check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "test/1.0");

      shutdownManager();

      writeInstallRDFForExtension(softblock1_2, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_2.id), Date.now() + 10000);
      writeInstallRDFForExtension(softblock2_2, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_2.id), Date.now() + 10000);
      writeInstallRDFForExtension(softblock3_2, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_2.id), Date.now() + 10000);
      writeInstallRDFForExtension(softblock4_2, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_2.id), Date.now() + 10000);
      writeInstallRDFForExtension(softblock5_2, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_2.id), Date.now() + 10000);
      writeInstallRDFForExtension(hardblock_2, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_2.id), Date.now() + 10000);
      writeInstallRDFForExtension(regexpblock_2, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_2.id), Date.now() + 10000);

      startupManager(false);

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s2, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s3, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s5, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(h, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        check_addon(r, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

        s2.userDisabled = false;
        s2.userDisabled = true;
        check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        s3.userDisabled = false;
        check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        restartManager();

        shutdownManager();

        writeInstallRDFForExtension(softblock1_3, profileDir);
        setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_3.id), Date.now() + 20000);
        writeInstallRDFForExtension(softblock2_3, profileDir);
        setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_3.id), Date.now() + 20000);
        writeInstallRDFForExtension(softblock3_3, profileDir);
        setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_3.id), Date.now() + 20000);
        writeInstallRDFForExtension(softblock4_3, profileDir);
        setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_3.id), Date.now() + 20000);
        writeInstallRDFForExtension(softblock5_3, profileDir);
        setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_3.id), Date.now() + 20000);
        writeInstallRDFForExtension(hardblock_3, profileDir);
        setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_3.id), Date.now() + 20000);
        writeInstallRDFForExtension(regexpblock_3, profileDir);
        setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_3.id), Date.now() + 20000);

        startupManager(false);

        AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

          check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s4, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s5, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

          shutdownManager();

          writeInstallRDFForExtension(softblock1_1, profileDir);
          setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_1.id), Date.now() + 30000);
          writeInstallRDFForExtension(softblock2_1, profileDir);
          setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_1.id), Date.now() + 30000);
          writeInstallRDFForExtension(softblock3_1, profileDir);
          setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_1.id), Date.now() + 30000);
          writeInstallRDFForExtension(softblock4_1, profileDir);
          setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_1.id), Date.now() + 30000);
          writeInstallRDFForExtension(softblock5_1, profileDir);
          setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_1.id), Date.now() + 30000);
          writeInstallRDFForExtension(hardblock_1, profileDir);
          setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_1.id), Date.now() + 30000);
          writeInstallRDFForExtension(regexpblock_1, profileDir);
          setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_1.id), Date.now() + 30000);

          startupManager(false);

          AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

            check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            do_check_eq(Services.prefs.getCharPref("general.skins.selectedSkin"), "classic/1.0");

            s1.userDisabled = false;
            s2.userDisabled = false;
            s5.userDisabled = false;
            run_addon_change_2_test();
          });
        });
      });
    });
  });
}

// Starts with add-ons blocked and then new versions are installed outside of
// the app to change them to unblocked.
function run_addon_change_2_test() {
  dump(arguments.callee.name + "\n");
  shutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);
  shutdownManager();

  writeInstallRDFForExtension(softblock1_2, profileDir);
  writeInstallRDFForExtension(softblock2_2, profileDir);
  writeInstallRDFForExtension(softblock3_2, profileDir);
  writeInstallRDFForExtension(softblock4_2, profileDir);
  writeInstallRDFForExtension(softblock5_2, profileDir);
  writeInstallRDFForExtension(hardblock_2, profileDir);
  writeInstallRDFForExtension(regexpblock_2, profileDir);

  startupManager(false);

  AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

    check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(s2, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(s3, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(h, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
    check_addon(r, "2.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

    s2.userDisabled = false;
    s2.userDisabled = true;
    check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    s3.userDisabled = false;
    check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    restartManager();

    shutdownManager();

    writeInstallRDFForExtension(softblock1_3, profileDir);
    setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_3.id), Date.now() + 10000);
    writeInstallRDFForExtension(softblock2_3, profileDir);
    setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_3.id), Date.now() + 10000);
    writeInstallRDFForExtension(softblock3_3, profileDir);
    setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_3.id), Date.now() + 10000);
    writeInstallRDFForExtension(softblock4_3, profileDir);
    setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_3.id), Date.now() + 10000);
    writeInstallRDFForExtension(softblock5_3, profileDir);
    setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_3.id), Date.now() + 10000);
    writeInstallRDFForExtension(hardblock_3, profileDir);
    setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_3.id), Date.now() + 10000);
    writeInstallRDFForExtension(regexpblock_3, profileDir);
    setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_3.id), Date.now() + 10000);

    startupManager(false);

    AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

      check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
      check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

      shutdownManager();

      writeInstallRDFForExtension(softblock1_1, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock1_1.id), Date.now() + 20000);
      writeInstallRDFForExtension(softblock2_1, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock2_1.id), Date.now() + 20000);
      writeInstallRDFForExtension(softblock3_1, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock3_1.id), Date.now() + 20000);
      writeInstallRDFForExtension(softblock4_1, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock4_1.id), Date.now() + 20000);
      writeInstallRDFForExtension(softblock5_1, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, softblock5_1.id), Date.now() + 20000);
      writeInstallRDFForExtension(hardblock_1, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, hardblock_1.id), Date.now() + 20000);
      writeInstallRDFForExtension(regexpblock_1, profileDir);
      setExtensionModifiedTime(getFileForAddon(profileDir, regexpblock_1.id), Date.now() + 20000);

      startupManager(false);

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

        s1.userDisabled = false;
        s2.userDisabled = false;
        s4.userDisabled = true;
        s5.userDisabled = false;
        run_background_update_test();
      });
    });
  });
}

// Add-ons are initially unblocked then attempts to upgrade to blocked versions
// in the background which should fail
function run_background_update_test() {
  dump(arguments.callee.name + "\n");
  restartManager();

  AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

    check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

    background_update(function() {
      restartManager();

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s2, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s5, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

        run_background_update_2_test();
      });
    });
  });
}

// Starts with add-ons blocked and then new versions are detected and installed
// automatically for unblocked versions.
function run_background_update_2_test() {
  dump(arguments.callee.name + "\n");
  shutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);
  shutdownManager();

  writeInstallRDFForExtension(softblock1_3, profileDir);
  writeInstallRDFForExtension(softblock2_3, profileDir);
  writeInstallRDFForExtension(softblock3_3, profileDir);
  writeInstallRDFForExtension(softblock4_3, profileDir);
  writeInstallRDFForExtension(softblock5_3, profileDir);
  writeInstallRDFForExtension(hardblock_3, profileDir);
  writeInstallRDFForExtension(regexpblock_3, profileDir);

  startupManager(false);

  AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

    check_addon(s1, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(s2, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(s3, "3.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
    check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

    s2.userDisabled = false;
    s2.userDisabled = true;
    check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    s3.userDisabled = false;
    check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    restartManager();

    background_update(function() {
      restartManager();

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
        check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

        s1.userDisabled = false;
        s2.userDisabled = false;
        s4.userDisabled = true;
        s5.userDisabled = true;
        run_manual_update_test();
      });
    });
  });
}

// Starts with add-ons blocked and then simulates the user upgrading them to
// unblocked versions.
function run_manual_update_test() {
  dump(arguments.callee.name + "\n");
  restartManager();
  load_blocklist("manual_update.xml", function() {
    restartManager();

    AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

      check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s4, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(s5, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
      check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

      s2.userDisabled = false;
      s2.userDisabled = true;
      check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      s3.userDisabled = false;
      check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      restartManager();

      manual_update("2", function() {
        restartManager();

        AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

          check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s4, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          check_addon(s5, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
          // Can't manually update to a hardblocked add-on
          check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
          check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

          manual_update("3", function() {
            restartManager();

            AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

              check_addon(s1, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
              check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
              check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
              check_addon(s4, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
              check_addon(s5, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
              check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
              check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

              run_manual_update_2_test();
            });
          });
        });
      });
    });
  });
}

// Starts with add-ons blocked and then new versions are installed outside of
// the app to change them to unblocked.
function run_manual_update_2_test() {
  dump(arguments.callee.name + "\n");
  shutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);
  shutdownManager();

  writeInstallRDFForExtension(softblock1_1, profileDir);
  writeInstallRDFForExtension(softblock2_1, profileDir);
  writeInstallRDFForExtension(softblock3_1, profileDir);
  writeInstallRDFForExtension(softblock4_1, profileDir);
  writeInstallRDFForExtension(softblock5_1, profileDir);
  writeInstallRDFForExtension(hardblock_1, profileDir);
  writeInstallRDFForExtension(regexpblock_1, profileDir);

  startupManager(false);

  AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

    check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
    check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

    s2.userDisabled = false;
    s2.userDisabled = true;
    check_addon(s2, "1.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    s3.userDisabled = false;
    check_addon(s3, "1.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
    restartManager();

    manual_update("2", function() {
      restartManager();

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "2.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s2, "2.0", true, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s3, "2.0", false, false, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        // Can't manually update to a hardblocked add-on
        check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

        restartManager();

        manual_update("3", function() {
          restartManager();

          AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

            check_addon(s1, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s2, "3.0", true, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(s3, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(h, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
            check_addon(r, "3.0", false, false, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

            s1.userDisabled = false;
            s2.userDisabled = false;
            s4.userDisabled = true;
            run_local_install_test();
          });
        });
      });
    });
  });
}

// Uses the API to install blocked add-ons from the local filesystem
function run_local_install_test() {
  dump(arguments.callee.name + "\n");
  shutdownManager();

  getFileForAddon(profileDir, softblock1_1.id).remove(true);
  getFileForAddon(profileDir, softblock2_1.id).remove(true);
  getFileForAddon(profileDir, softblock3_1.id).remove(true);
  getFileForAddon(profileDir, softblock4_1.id).remove(true);
  getFileForAddon(profileDir, softblock5_1.id).remove(true);
  getFileForAddon(profileDir, hardblock_1.id).remove(true);
  getFileForAddon(profileDir, regexpblock_1.id).remove(true);

  startupManager(false);

  installAllFiles([
    do_get_file("addons/blocklist_soft1_1.xpi"),
    do_get_file("addons/blocklist_soft2_1.xpi"),
    do_get_file("addons/blocklist_soft3_1.xpi"),
    do_get_file("addons/blocklist_soft4_1.xpi"),
    do_get_file("addons/blocklist_soft5_1.xpi"),
    do_get_file("addons/blocklist_hard1_1.xpi"),
    do_get_file("addons/blocklist_regexp1_1.xpi")
  ], function() {
    AddonManager.getAllInstalls(function(aInstalls) {
      // Should have finished all installs without needing to restart
      do_check_eq(aInstalls.length, 0);

      AddonManager.getAddonsByIDs(ADDON_IDS, function([s1, s2, s3, s4, s5, h, r]) {

        check_addon(s1, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s2, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(s3, "1.0", true, true, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
        check_addon(h, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);
        check_addon(r, "1.0", false, false, Ci.nsIBlocklistService.STATE_BLOCKED);

        end_test();
      });
    });
  });
}
