/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/MockRegistrar.jsm");

const nsIBLS = Ci.nsIBlocklistService;
const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

var gBlocklist = null;
var gPrefs = null;
var gTestserver = null;

var gNextTestPart = null;


var PLUGINS = [{
  // Tests a plugin whose state goes from not-blocked, to outdated
  name: "test_bug514327_outdated",
  version: "5",
  disabled: false,
  blocklisted: false
}, {
  // Used to trigger the blocklist dialog, which indicates the blocklist has updated
  name: "test_bug514327_1",
  version: "5",
  disabled: false,
  blocklisted: false
}, {
  // Used to trigger the blocklist dialog, which indicates the blocklist has updated
  name: "test_bug514327_2",
  version: "5",
  disabled: false,
  blocklisted: false
} ];


// A fake plugin host for the blocklist service to use
var PluginHost = {
  getPluginTags: function(countRef) {
    countRef.value = PLUGINS.length;
    return PLUGINS;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIPluginHost)
     || iid.equals(Ci.nsISupports))
      return this;
  
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow: function(parent, url, name, features, args) {
    // Should be called to list the newly blocklisted items
    do_check_eq(url, URI_EXTENSION_BLOCKLIST_DIALOG);
    // Should only include one item
    do_check_eq(args.wrappedJSObject.list.length, 1);
    // And that item should be the blocked plugin, not the outdated one
    var item = args.wrappedJSObject.list[0];
    do_check_true(item.item instanceof Ci.nsIPluginTag);
    do_check_neq(item.name, "test_bug514327_outdated");

    // Call the next test after the blocklist has finished up
    do_timeout(0, gNextTestPart);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

MockRegistrar.register("@mozilla.org/plugin/host;1", PluginHost);
MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);


function do_update_blocklist(aDatafile, aNextPart) {
  gNextTestPart = aNextPart;

  gPrefs.setCharPref("extensions.blocklist.url", "http://localhost:" + gPort + "/data/" + aDatafile);
  gBlocklist.QueryInterface(Ci.nsITimerCallback).notify(null);
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  gTestserver = new HttpServer();
  gTestserver.registerDirectory("/data/", do_get_file("data"));
  gTestserver.start(-1);
  gPort = gTestserver.identity.primaryPort;

  startupManager();

  // initialize the blocklist with no entries
  copyBlocklistToProfile(do_get_file("data/test_bug514327_3_empty.xml"));
  
  gPrefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  gBlocklist = Cc["@mozilla.org/extensions/blocklist;1"].getService(nsIBLS);
  
  // should NOT be marked as outdated by the blocklist
  do_check_true(gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9") == nsIBLS.STATE_NOT_BLOCKED);
  
  do_test_pending();

  // update blocklist with data that marks the plugin as outdated
  do_update_blocklist("test_bug514327_3_outdated_1.xml", test_part_1);
}

function test_part_1() {
  // plugin should now be marked as outdated
  do_check_true(gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9") == nsIBLS.STATE_OUTDATED);
  // and the notifyUser pref should be set to true
  do_check_true(gPrefs.getBoolPref("plugins.update.notifyUser"));
  
  // preternd the user has been notified, reset the pref
  gPrefs.setBoolPref("plugins.update.notifyUser", false);
  
  // update blocklist with data that marks the plugin as outdated
  do_update_blocklist("test_bug514327_3_outdated_2.xml", test_part_2);
}

function test_part_2() {
  // plugin should still be marked as outdated
  do_check_true(gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9") == nsIBLS.STATE_OUTDATED);
  // and the notifyUser pref should NOT be set to true, as the plugin was already outdated
  do_check_false(gPrefs.getBoolPref("plugins.update.notifyUser"));

  finish();
}

function finish() {
  gTestserver.stop(do_test_finished);
}
