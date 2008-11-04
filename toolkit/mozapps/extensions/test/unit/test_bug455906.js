/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */
const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

do_import_script("netwerk/test/httpserver/httpd.js");

var ADDONS = [{
  // Tests how the blocklist affects a disabled add-on
  id: "test_bug455906_1@tests.mozilla.org",
  name: "Bug 455906 Addon Test 1",
  version: "5",
  appVersion: "3"
}, {
  // Tests how the blocklist affects an enabled add-on
  id: "test_bug455906_2@tests.mozilla.org",
  name: "Bug 455906 Addon Test 2",
  version: "5",
  appVersion: "3"
}, {
  // Tests how the blocklist affects an enabled add-on, to be disabled by the notification
  id: "test_bug455906_3@tests.mozilla.org",
  name: "Bug 455906 Addon Test 3",
  version: "5",
  appVersion: "3"
}, {
  // Tests how the blocklist affects a disabled add-on that was already warned about
  id: "test_bug455906_4@tests.mozilla.org",
  name: "Bug 455906 Addon Test 4",
  version: "5",
  appVersion: "3"
}, {
  // Tests how the blocklist affects an enabled add-on that was already warned about
  id: "test_bug455906_5@tests.mozilla.org",
  name: "Bug 455906 Addon Test 5",
  version: "5",
  appVersion: "3"
}, {
  // Tests how the blocklist affects an already blocked add-on
  id: "test_bug455906_6@tests.mozilla.org",
  name: "Bug 455906 Addon Test 6",
  version: "5",
  appVersion: "3"
}, {
  // Tests how the blocklist affects an incompatible add-on
  id: "test_bug455906_7@tests.mozilla.org",
  name: "Bug 455906 Addon Test 7",
  version: "5",
  appVersion: "2"
}, {
  // Spare add-on used to ensure we get a notification when switching lists
  id: "dummy_bug455906_1@tests.mozilla.org",
  name: "Dummy Addon 1",
  version: "5",
  appVersion: "3"
}, {
  // Spare add-on used to ensure we get a notification when switching lists
  id: "dummy_bug455906_2@tests.mozilla.org",
  name: "Dummy Addon 2",
  version: "5",
  appVersion: "3"
}];

var PLUGINS = [{
  // Tests how the blocklist affects a disabled plugin
  name: "test_bug455906_1",
  version: "5",
  disabled: true,
  blocklisted: false
}, {
  // Tests how the blocklist affects an enabled plugin
  name: "test_bug455906_2",
  version: "5",
  disabled: false,
  blocklisted: false
}, {
  // Tests how the blocklist affects an enabled plugin, to be disabled by the notification
  name: "test_bug455906_3",
  version: "5",
  disabled: false,
  blocklisted: false
}, {
  // Tests how the blocklist affects a disabled plugin that was already warned about
  name: "test_bug455906_4",
  version: "5",
  disabled: true,
  blocklisted: false
}, {
  // Tests how the blocklist affects an enabled plugin that was already warned about
  name: "test_bug455906_5",
  version: "5",
  disabled: false,
  blocklisted: false
}, {
  // Tests how the blocklist affects an already blocked plugin
  name: "test_bug455906_6",
  version: "5",
  disabled: false,
  blocklisted: true
}];

var gNotificationCheck = null;
var gTestCheck = null;
var gTestserver = null;

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

var PluginHostFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return PluginHost.QueryInterface(iid);
  }
};

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow: function(parent, url, name, features, arguments) {
    // Should be called to list the newly blocklisted items
    do_check_eq(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    if (gNotificationCheck) {
      var args = arguments.wrappedJSObject;
      gNotificationCheck(args);
    }

    // Call the next test after the blocklist has finished up
    do_timeout(0, "gTestCheck()");
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
var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{721c3e73-969e-474b-a6dc-059fd288c428}"),
                          "Fake Plugin Host",
                          "@mozilla.org/plugin/host;1", PluginHostFactory);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1", WindowWatcherFactory);

function create_addon(addon) {
  var installrdf = "<?xml version=\"1.0\"?>\n" +
                   "\n" +
                   "<RDF xmlns=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n" +
                   "     xmlns:em=\"http://www.mozilla.org/2004/em-rdf#\">\n" +
                   "  <Description about=\"urn:mozilla:install-manifest\">\n" +
                   "    <em:id>" + addon.id + "</em:id>\n" +
                   "    <em:version>" + addon.version + "</em:version>\n" +
                   "    <em:targetApplication>\n" +
                   "      <Description>\n" +
                   "        <em:id>xpcshell@tests.mozilla.org</em:id>\n" +
                   "        <em:minVersion>" + addon.appVersion + "</em:minVersion>\n" +
                   "        <em:maxVersion>" + addon.appVersion + "</em:maxVersion>\n" +
                   "      </Description>\n" +
                   "    </em:targetApplication>\n" +
                   "    <em:name>" + addon.name + "</em:name>\n" +
                   "  </Description>\n" +
                   "</RDF>\n";
  var target = gProfD.clone();
  target.append("extensions");
  target.append(addon.id);
  target.append("install.rdf");
  target.create(target.NORMAL_FILE_TYPE, 0644);
  var stream = Cc["@mozilla.org/network/file-output-stream;1"].
               createInstance(Ci.nsIFileOutputStream);
  stream.init(target, 0x04 | 0x08 | 0x20, 0664, 0); // write, create, truncate
  stream.write(installrdf, installrdf.length);
  stream.close();
}

function load_blocklist(file) {
  gPrefs.setCharPref("extensions.blocklist.url", "http://localhost:4444/data/" + file);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}

function isUserDisabled(id) {
  return getManifestProperty(id, "userDisabled") == "true";
}

function isAppDisabled(id) {
  return getManifestProperty(id, "appDisabled") == "true";
}

function check_addon_state(id) {
  return isUserDisabled(id) + "," + isAppDisabled(id);
}

function check_plugin_state(plugin) {
  return plugin.disabled + "," + plugin.blocklisted;
}

// Performs the initial setup
function run_test() {
  // Setup for test
  dump("Setting up tests\n");
  // Rather than keeping lots of identical add-ons in version control, just
  // write them into the profile.
  for (var i = 0; i < ADDONS.length; i++)
    create_addon(ADDONS[i]);

  // Copy the initial blocklist into the profile to check add-ons start in the
  // right state.
  var blocklist = do_get_file("toolkit/mozapps/extensions/test/unit/data/bug455906_start.xml")
  blocklist.copyTo(gProfD, "blocklist.xml");

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  startupEM();

  gTestserver = new nsHttpServer();
  gTestserver.registerDirectory("/data/", do_get_file("toolkit/mozapps/extensions/test/unit/data"));
  gTestserver.start(4444);

  do_test_pending();
  check_test_pt1();
}

// Before every main test this is the state the add-ons are meant to be in
function check_initial_state() {
  do_check_eq(check_addon_state(ADDONS[0].id), "true,false");
  do_check_eq(check_addon_state(ADDONS[1].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[2].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[3].id), "true,false");
  do_check_eq(check_addon_state(ADDONS[4].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[5].id), "false,true");
  do_check_eq(check_addon_state(ADDONS[6].id), "false,true");

  do_check_eq(check_plugin_state(PLUGINS[0]), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[1]), "false,false");
  do_check_eq(check_plugin_state(PLUGINS[2]), "false,false");
  do_check_eq(check_plugin_state(PLUGINS[3]), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[4]), "false,false");
  do_check_eq(check_plugin_state(PLUGINS[5]), "false,true");
}

// Tests the add-ons were installed and the initial blocklist applied as expected
function check_test_pt1() {
  dump("Checking pt 1\n");
  for (var i = 0; i < ADDONS.length; i++) {
    if (!gEM.getItemForID(ADDONS[i].id))
      do_throw("Addon " + (i + 1) + " did not get installed correctly");
  }

  do_check_eq(check_addon_state(ADDONS[0].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[1].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[2].id), "false,false");

  // Warn add-ons should be user disabled automatically
  do_check_eq(check_addon_state(ADDONS[3].id), "true,false");
  do_check_eq(check_addon_state(ADDONS[4].id), "true,false");

  // Blocked and incompatible should be app disabled only
  do_check_eq(check_addon_state(ADDONS[5].id), "false,true");
  do_check_eq(check_addon_state(ADDONS[6].id), "false,true");

  // We've overridden the plugin host so we cannot tell what that would have
  // initialised the plugins as

  // Put the add-ons into the base state
  gEM.disableItem(ADDONS[0].id);
  gEM.enableItem(ADDONS[4].id);
  restartEM();
  check_initial_state();

  gNotificationCheck = check_notification_pt2;
  gTestCheck = check_test_pt2;
  load_blocklist("bug455906_warn.xml");
}

function check_notification_pt2(args) {
  dump("Checking notification pt 2\n");
  do_check_eq(args.list.length, 4);

  for (let i = 0; i < args.list.length; i++) {
    let addon = args.list[i];
    if (addon.item instanceof Ci.nsIUpdateItem) {
      switch (addon.item.id) {
        case "test_bug455906_2@tests.mozilla.org":
          do_check_false(addon.blocked);
          break;
        case "test_bug455906_3@tests.mozilla.org":
          do_check_false(addon.blocked);
          addon.disable = true;
          break;
        default:
          do_throw("Unknown addon: " + addon.item.id);
      }
    }
    else if (addon.item instanceof Ci.nsIPluginTag) {
      switch (addon.item.name) {
        case "test_bug455906_2":
          do_check_false(addon.blocked);
          break;
        case "test_bug455906_3":
          do_check_false(addon.blocked);
          addon.disable = true;
          break;
        default:
          do_throw("Unknown addon: " + addon.item.name);
      }
    }
    else {
      do_throw("Unknown add-on type " + addon.item);
    }
  }
}

function check_test_pt2() {
  restartEM();
  dump("Checking results pt 2\n");

  // Should have disabled this add-on as requested
  do_check_eq(check_addon_state(ADDONS[2].id), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[2]), "true,false");

  // The blocked add-on should have changed to user disabled
  do_check_eq(check_addon_state(ADDONS[5].id), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[5]), "true,false");

  // These should have been unchanged
  do_check_eq(check_addon_state(ADDONS[0].id), "true,false");
  do_check_eq(check_addon_state(ADDONS[1].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[3].id), "true,false");
  do_check_eq(check_addon_state(ADDONS[4].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[6].id), "false,true");
  do_check_eq(check_plugin_state(PLUGINS[0]), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[1]), "false,false");
  do_check_eq(check_plugin_state(PLUGINS[3]), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[4]), "false,false");

  // Back to starting state
  gEM.enableItem(ADDONS[2].id);
  gEM.enableItem(ADDONS[5].id);
  PLUGINS[2].disabled = false;
  PLUGINS[5].disabled = false;
  restartEM();
  gNotificationCheck = null;
  gTestCheck = run_test_pt3;
  load_blocklist("bug455906_start.xml");
}

function run_test_pt3() {
  restartEM();
  check_initial_state();

  gNotificationCheck = check_notification_pt3;
  gTestCheck = check_test_pt3;
  load_blocklist("bug455906_block.xml");
}

function check_notification_pt3(args) {
  dump("Checking notification pt 3\n");
  do_check_eq(args.list.length, 6);

  for (let i = 0; i < args.list.length; i++) {
    let addon = args.list[i];
    if (addon.item instanceof Ci.nsIUpdateItem) {
      switch (addon.item.id) {
        case "test_bug455906_2@tests.mozilla.org":
          do_check_true(addon.blocked);
          break;
        case "test_bug455906_3@tests.mozilla.org":
          do_check_true(addon.blocked);
          break;
        case "test_bug455906_5@tests.mozilla.org":
          do_check_true(addon.blocked);
          break;
        default:
          do_throw("Unknown addon: " + addon.item.id);
      }
    }
    else if (addon.item instanceof Ci.nsIPluginTag) {
      switch (addon.item.name) {
        case "test_bug455906_2":
          do_check_true(addon.blocked);
          break;
        case "test_bug455906_3":
          do_check_true(addon.blocked);
          break;
        case "test_bug455906_5":
          do_check_true(addon.blocked);
          break;
        default:
          do_throw("Unknown addon: " + addon.item.name);
      }
    }
    else {
      do_throw("Unknown add-on type " + addon.item);
    }
  }
}

function check_test_pt3() {
  restartEM();
  dump("Checking results pt 3\n");

  // All should have gained the blocklist state, user disabled as previously
  do_check_eq(check_addon_state(ADDONS[0].id), "true,true");
  do_check_eq(check_addon_state(ADDONS[1].id), "false,true");
  do_check_eq(check_addon_state(ADDONS[2].id), "false,true");
  do_check_eq(check_addon_state(ADDONS[3].id), "true,true");
  do_check_eq(check_addon_state(ADDONS[4].id), "false,true");
  do_check_eq(check_plugin_state(PLUGINS[0]), "true,true");
  do_check_eq(check_plugin_state(PLUGINS[1]), "false,true");
  do_check_eq(check_plugin_state(PLUGINS[2]), "false,true");
  do_check_eq(check_plugin_state(PLUGINS[3]), "true,true");
  do_check_eq(check_plugin_state(PLUGINS[4]), "false,true");

  // Shouldn't be changed
  do_check_eq(check_addon_state(ADDONS[5].id), "false,true");
  do_check_eq(check_addon_state(ADDONS[6].id), "false,true");
  do_check_eq(check_plugin_state(PLUGINS[5]), "false,true");

  // Back to starting state
  gNotificationCheck = null;
  gTestCheck = run_test_pt4;
  load_blocklist("bug455906_start.xml");
}

function run_test_pt4() {
  gEM.enableItem(ADDONS[4].id);
  PLUGINS[4].disabled = false;
  restartEM();
  check_initial_state();

  gNotificationCheck = check_notification_pt4;
  gTestCheck = check_test_pt4;
  load_blocklist("bug455906_empty.xml");
}

function check_notification_pt4(args) {
  dump("Checking notification pt 4\n");

  // Should be just the dummy add-on to force this notification
  do_check_eq(args.list.length, 1);
  do_check_true(args.list[0].item instanceof Ci.nsIUpdateItem);
  do_check_eq(args.list[0].item.id, "dummy_bug455906_2@tests.mozilla.org");
}

function check_test_pt4() {
  restartEM();
  dump("Checking results pt 4\n");

  // This should have become unblocked
  do_check_eq(check_addon_state(ADDONS[5].id), "false,false");
  do_check_eq(check_plugin_state(PLUGINS[5]), "false,false");

  // No change for anything else
  do_check_eq(check_addon_state(ADDONS[0].id), "true,false");
  do_check_eq(check_addon_state(ADDONS[1].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[2].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[3].id), "true,false");
  do_check_eq(check_addon_state(ADDONS[4].id), "false,false");
  do_check_eq(check_addon_state(ADDONS[6].id), "false,true");
  do_check_eq(check_plugin_state(PLUGINS[0]), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[1]), "false,false");
  do_check_eq(check_plugin_state(PLUGINS[2]), "false,false");
  do_check_eq(check_plugin_state(PLUGINS[3]), "true,false");
  do_check_eq(check_plugin_state(PLUGINS[4]), "false,false");

  finish();
}

function finish() {
  gTestserver.stop();
  do_test_finished();
}
