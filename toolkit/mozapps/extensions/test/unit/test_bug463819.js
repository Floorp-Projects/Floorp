/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const INSTALLERROR_SUCCESS               = 0;
const INSTALLERROR_INCOMPATIBLE_VERSION  = -3;
const INSTALLERROR_BLOCKLISTED           = -6;
const INSTALLERROR_SOFTBLOCKED           = -10;

// Disables security checking our updates which haven't been signed
gPrefs.setBoolPref("extensions.checkUpdateSecurity", false);

// Get the HTTP server.
do_load_httpd_js();
var testserver;

// This allows the EM to attempt to display errors to the user without failing
var promptService = {
  alert: function(aParent, aDialogTitle, aText) {
  },
  
  alertCheck: function(aParent, aDialogTitle, aText, aCheckMsg, aCheckState) {
  },
  
  confirm: function(aParent, aDialogTitle, aText) {
  },
  
  confirmCheck: function(aParent, aDialogTitle, aText, aCheckMsg, aCheckState) {
  },
  
  confirmEx: function(aParent, aDialogTitle, aText, aButtonFlags, aButton0Title, aButton1Title, aButton2Title, aCheckMsg, aCheckState) {
  },
  
  prompt: function(aParent, aDialogTitle, aText, aValue, aCheckMsg, aCheckState) {
  },
  
  promptUsernameAndPassword: function(aParent, aDialogTitle, aText, aUsername, aPassword, aCheckMsg, aCheckState) {
  },

  promptPassword: function(aParent, aDialogTitle, aText, aPassword, aCheckMsg, aCheckState) {
  },
  
  select: function(aParent, aDialogTitle, aText, aCount, aSelectList, aOutSelection) {
  },
  
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIPromptService)
     || iid.equals(Components.interfaces.nsISupports))
      return this;
  
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

// This will be called to show the blocklist message, we just make it look like
// it was dismissed.
var WindowWatcher = {
  openWindow: function(parent, url, name, features, arguments) {
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};

var PromptServiceFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return promptService.QueryInterface(iid);
  }
};
var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{6cc9c9fe-bc0b-432b-a410-253ef8bcc699}"),
                          "PromptService",
                          "@mozilla.org/embedcomp/prompt-service;1", PromptServiceFactory);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1", WindowWatcherFactory);

var gNextTest, gLastStatus;

// nsIAddonInstallListener
var installListener = {
  onDownloadStarted: function(aAddon) {
  },

  onDownloadEnded: function(aAddon) {
  },

  onInstallStarted: function(aAddon) {
  },

  onCompatibilityCheckStarted: function(aAddon) {
  },

  onCompatibilityCheckEnded: function(aAddon, aStatus) {
  },

  onInstallEnded: function(aAddon, aStatus) {
    gLastStatus = aStatus;
  },

  onInstallsCompleted: function() {
    gNextTest();
  },

  onDownloadProgress: function onProgress(aAddon, aValue, aMaxValue) {
  }
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "1.9");

  // Install the blocklist
  var blocklist = do_get_file("data/test_bug463819.xml");
  blocklist.copyTo(gProfD, "blocklist.xml");

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/", do_get_file("data"));
  testserver.start(4444);

  startupEM();
  gEM.addInstallListener(installListener);

  // These add-ons require no update check so will complete installation immediately
  dump("Installing add-on 1\n");
  gNextTest = test_addon_1;
  gEM.installItemFromFile(do_get_addon("test_bug463819_1"), NS_INSTALL_LOCATION_APPPROFILE);
  dump("Installing add-on 4\n");
  gNextTest = test_addon_4;
  gEM.installItemFromFile(do_get_addon("test_bug463819_4"), NS_INSTALL_LOCATION_APPPROFILE);
  dump("Installing add-on 7\n");
  gNextTest = test_addon_7;
  gEM.installItemFromFile(do_get_addon("test_bug463819_7"), NS_INSTALL_LOCATION_APPPROFILE);

  do_test_pending();

  // These add-ons will perform a compatibility check before responding.
  dump("Installing add-on 2\n");
  gNextTest = test_addon_2;
  gEM.installItemFromFile(do_get_addon("test_bug463819_2"), NS_INSTALL_LOCATION_APPPROFILE);
}

// Compatible in install.rdf and not in blocklist
function test_addon_1() {
  do_check_eq(gLastStatus, INSTALLERROR_SUCCESS);
}

// Compatible in install.rdf and low severity in blocklist
function test_addon_4() {
  do_check_eq(gLastStatus, INSTALLERROR_SOFTBLOCKED);
}

// Compatible in install.rdf and high severity in blocklist
function test_addon_7() {
  do_check_eq(gLastStatus, INSTALLERROR_BLOCKLISTED);
}

// Incompatible in install.rdf, compatible in update.rdf, not in blocklist
function test_addon_2() {
  do_check_eq(gLastStatus, INSTALLERROR_SUCCESS);

  dump("Installing add-on 3\n");
  gNextTest = test_addon_3;
  gEM.installItemFromFile(do_get_addon("test_bug463819_3"), NS_INSTALL_LOCATION_APPPROFILE);
}

// Incompatible in install.rdf, incompatible in update.rdf, not in blocklist
function test_addon_3() {
  do_check_eq(gLastStatus, INSTALLERROR_INCOMPATIBLE_VERSION);

  dump("Installing add-on 5\n");
  gNextTest = test_addon_5;
  gEM.installItemFromFile(do_get_addon("test_bug463819_5"), NS_INSTALL_LOCATION_APPPROFILE);
}

// Incompatible in install.rdf, compatible in update.rdf, low severity in blocklist
function test_addon_5() {
  do_check_eq(gLastStatus, INSTALLERROR_SOFTBLOCKED);

  dump("Installing add-on 6\n");
  gNextTest = test_addon_6;
  gEM.installItemFromFile(do_get_addon("test_bug463819_6"), NS_INSTALL_LOCATION_APPPROFILE);
}

// Incompatible in install.rdf, incompatible in update.rdf, low severity in blocklist
function test_addon_6() {
  do_check_eq(gLastStatus, INSTALLERROR_INCOMPATIBLE_VERSION);

  dump("Installing add-on 8\n");
  gNextTest = test_addon_8;
  gEM.installItemFromFile(do_get_addon("test_bug463819_8"), NS_INSTALL_LOCATION_APPPROFILE);
}

// Incompatible in install.rdf, compatible in update.rdf, high severity in blocklist
function test_addon_8() {
  do_check_eq(gLastStatus, INSTALLERROR_BLOCKLISTED);

  dump("Installing add-on 9\n");
  gNextTest = test_addon_9;
  gEM.installItemFromFile(do_get_addon("test_bug463819_9"), NS_INSTALL_LOCATION_APPPROFILE);
}

// Incompatible in install.rdf, incompatible in update.rdf, high severity in blocklist
function test_addon_9() {
  do_check_eq(gLastStatus, INSTALLERROR_INCOMPATIBLE_VERSION);

  finish_test();
}

function finish_test() {
  testserver.stop(do_test_finished);
}
