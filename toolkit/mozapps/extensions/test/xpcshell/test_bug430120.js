/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const BLOCKLIST_TIMER                 = "blocklist-background-update-timer";
const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";
const PREF_BLOCKLIST_ENABLED          = "extensions.blocklist.enabled";
const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";
const PREF_APP_UPDATE_CHANNEL         = "app.update.channel";
const PREF_GENERAL_USERAGENT_LOCALE   = "general.useragent.locale";
const CATEGORY_UPDATE_TIMER           = "update-timer";

// Get the HTTP server.
Components.utils.import("resource://testing-common/httpd.js");
Components.utils.import("resource://testing-common/MockRegistrar.jsm");
var testserver;
var gOSVersion;
var gBlocklist;

// This is a replacement for the timer service so we can trigger timers
var timerService = {

  hasTimer: function(id) {
    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    var entries = catMan.enumerateCategory(CATEGORY_UPDATE_TIMER);
    while (entries.hasMoreElements()) {
      var entry = entries.getNext().QueryInterface(Components.interfaces.nsISupportsCString).data;
      var value = catMan.getCategoryEntry(CATEGORY_UPDATE_TIMER, entry);
      var timerID = value.split(",")[2];
      if (id == timerID) {
        return true;
      }
    }
    return false;
  },

  fireTimer: function(id) {
    gBlocklist.QueryInterface(Components.interfaces.nsITimerCallback).notify(null);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIUpdateTimerManager)
     || iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

MockRegistrar.register("@mozilla.org/updates/timer-manager;1", timerService);

function failHandler(metadata, response) {
  do_throw("Should not have attempted to retrieve the blocklist when it is disabled");
}

function pathHandler(metadata, response) {
  var ABI = "noarch-spidermonkey";
  // the blacklist service special-cases ABI for Universal binaries,
  // so do the same here.
  if ("@mozilla.org/xpcom/mac-utils;1" in Components.classes) {
    var macutils = Components.classes["@mozilla.org/xpcom/mac-utils;1"]
                             .getService(Components.interfaces.nsIMacUtils);
    if (macutils.isUniversalBinary)
      ABI += "-u-" + macutils.architecturesInBinary;
  }
  do_check_eq(metadata.queryString,
              "xpcshell@tests.mozilla.org&1&XPCShell&1&" +
              gAppInfo.appBuildID + "&" +
              "XPCShell_" + ABI + "&locale&updatechannel&" +
              gOSVersion + "&1.9&distribution&distribution-version");
  gBlocklist.observe(null, "quit-application", "");
  gBlocklist.observe(null, "xpcom-shutdown", "");
  testserver.stop(do_test_finished);
}

function run_test() {
  var osVersion;
  var sysInfo = Components.classes["@mozilla.org/system-info;1"]
                          .getService(Components.interfaces.nsIPropertyBag2);
  try {
    osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");
    if (osVersion) {
      try {
        osVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
      }
      catch (e) {
      }
      gOSVersion = encodeURIComponent(osVersion);
    }
  }
  catch (e) {
  }

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  testserver = new HttpServer();
  testserver.registerPathHandler("/1", failHandler);
  testserver.registerPathHandler("/2", pathHandler);
  testserver.start(-1);
  gPort = testserver.identity.primaryPort;

  // Initialise the blocklist service
  gBlocklist = Components.classes["@mozilla.org/extensions/blocklist;1"]
                         .getService(Components.interfaces.nsIBlocklistService)
                         .QueryInterface(Components.interfaces.nsIObserver);
  gBlocklist.observe(null, "profile-after-change", "");

  do_check_true(timerService.hasTimer(BLOCKLIST_TIMER));

  do_test_pending();

  // This should have no effect as the blocklist is disabled
  Services.prefs.setCharPref(PREF_BLOCKLIST_URL, "http://localhost:" + gPort + "/1");
  Services.prefs.setBoolPref(PREF_BLOCKLIST_ENABLED, false);
  timerService.fireTimer(BLOCKLIST_TIMER);

  // Some values have to be on the default branch to work
  var defaults = Services.prefs.QueryInterface(Components.interfaces.nsIPrefService)
                       .getDefaultBranch(null);
  defaults.setCharPref(PREF_APP_UPDATE_CHANNEL, "updatechannel");
  defaults.setCharPref(PREF_APP_DISTRIBUTION, "distribution");
  defaults.setCharPref(PREF_APP_DISTRIBUTION_VERSION, "distribution-version");
  defaults.setCharPref(PREF_GENERAL_USERAGENT_LOCALE, "locale");

  // This should correctly escape everything
  Services.prefs.setCharPref(PREF_BLOCKLIST_URL, "http://localhost:" + gPort + "/2?" +
                     "%APP_ID%&%APP_VERSION%&%PRODUCT%&%VERSION%&%BUILD_ID%&" +
                     "%BUILD_TARGET%&%LOCALE%&%CHANNEL%&" +
                     "%OS_VERSION%&%PLATFORM_VERSION%&%DISTRIBUTION%&%DISTRIBUTION_VERSION%");
  Services.prefs.setBoolPref(PREF_BLOCKLIST_ENABLED, true);
  timerService.fireTimer(BLOCKLIST_TIMER);
}
