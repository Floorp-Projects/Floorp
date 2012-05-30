/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

do_load_httpd_js();

var ADDONS = [{
  id: "test_bug449027_1@tests.mozilla.org",
  name: "Bug 449027 Addon Test 1",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_2@tests.mozilla.org",
  name: "Bug 449027 Addon Test 2",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_3@tests.mozilla.org",
  name: "Bug 449027 Addon Test 3",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_4@tests.mozilla.org",
  name: "Bug 449027 Addon Test 4",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_5@tests.mozilla.org",
  name: "Bug 449027 Addon Test 5",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_6@tests.mozilla.org",
  name: "Bug 449027 Addon Test 6",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_7@tests.mozilla.org",
  name: "Bug 449027 Addon Test 7",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_8@tests.mozilla.org",
  name: "Bug 449027 Addon Test 8",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_9@tests.mozilla.org",
  name: "Bug 449027 Addon Test 9",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_10@tests.mozilla.org",
  name: "Bug 449027 Addon Test 10",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_11@tests.mozilla.org",
  name: "Bug 449027 Addon Test 11",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_12@tests.mozilla.org",
  name: "Bug 449027 Addon Test 12",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_13@tests.mozilla.org",
  name: "Bug 449027 Addon Test 13",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_14@tests.mozilla.org",
  name: "Bug 449027 Addon Test 14",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_15@tests.mozilla.org",
  name: "Bug 449027 Addon Test 15",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_16@tests.mozilla.org",
  name: "Bug 449027 Addon Test 16",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_17@tests.mozilla.org",
  name: "Bug 449027 Addon Test 17",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_18@tests.mozilla.org",
  name: "Bug 449027 Addon Test 18",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_19@tests.mozilla.org",
  name: "Bug 449027 Addon Test 19",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_20@tests.mozilla.org",
  name: "Bug 449027 Addon Test 20",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_21@tests.mozilla.org",
  name: "Bug 449027 Addon Test 21",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_22@tests.mozilla.org",
  name: "Bug 449027 Addon Test 22",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_23@tests.mozilla.org",
  name: "Bug 449027 Addon Test 23",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_24@tests.mozilla.org",
  name: "Bug 449027 Addon Test 24",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_25@tests.mozilla.org",
  name: "Bug 449027 Addon Test 25",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}];

var PLUGINS = [{
  name: "test_bug449027_1",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  name: "test_bug449027_2",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_3",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_4",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  name: "test_bug449027_5",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  name: "test_bug449027_6",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_7",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_8",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_9",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_10",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_11",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_12",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_13",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  name: "test_bug449027_14",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  name: "test_bug449027_15",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_16",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_17",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  name: "test_bug449027_18",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  name: "test_bug449027_19",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_20",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_21",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_22",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_23",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_24",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  name: "test_bug449027_25",
  version: "5",
  blocklisted: false,
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}];

var gCallback = null;
var gTestserver = null;
var gNewBlocks = [];

// A fake plugin host for the blocklist service to use
var PluginHost = {
  getPluginTags: function(countRef) {
    countRef.value = PLUGINS.length;
    return PLUGINS;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIPluginHost)
     || iid.equals(Components.interfaces.nsISupports))
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
    do_check_neq(gCallback, null);

    var args = arguments.wrappedJSObject;

    gNewBlocks = [];
    var list = args.list;
    for (let listItem of list)
      gNewBlocks.push(listItem.name + " " + listItem.version);

    // Call the callback after the blocklist has finished up
    do_timeout(0, gCallback);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIWindowWatcher)
     || iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
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
                   "        <em:minVersion>3</em:minVersion>\n" +
                   "        <em:maxVersion>3</em:maxVersion>\n" +
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
  var stream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                         .createInstance(Components.interfaces.nsIFileOutputStream);
  stream.init(target, 0x04 | 0x08 | 0x20, 0664, 0); // write, create, truncate
  stream.write(installrdf, installrdf.length);
  stream.close();
}

/**
 * Checks that items are blocklisted correctly according to the current test.
 * If a lastTest is provided checks that the notification dialog got passed
 * the newly blocked items compared to the previous test.
 */
function check_state(test, lastTest, callback) {
  AddonManager.getAddonsByIDs([a.id for each (a in ADDONS)], function(addons) {
    for (var i = 0; i < ADDONS.length; i++) {
      var blocked = addons[i].blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED;
      if (blocked != ADDONS[i][test])
        do_throw("Blocklist state did not match expected for extension " + (i + 1) + ", test " + test);
    }

    for (i = 0; i < PLUGINS.length; i++) {
      if (PLUGINS[i].blocklisted != PLUGINS[i][test])
        do_throw("Blocklist state did not match expected for plugin " + (i + 1) + ", test " + test);
    }

    if (lastTest) {
      var expected = 0;
      for (i = 0; i < ADDONS.length; i++) {
        if (ADDONS[i][test] && !ADDONS[i][lastTest]) {
          if (gNewBlocks.indexOf(ADDONS[i].name + " " + ADDONS[i].version) < 0)
            do_throw("Addon " + (i + 1) + " should have been listed in the blocklist notification for test " + test);
          expected++;
        }
      }

      for (i = 0; i < PLUGINS.length; i++) {
        if (PLUGINS[i][test] && !PLUGINS[i][lastTest]) {
          if (gNewBlocks.indexOf(PLUGINS[i].name + " " + PLUGINS[i].version) < 0)
            do_throw("Plugin " + (i + 1) + " should have been listed in the blocklist notification for test " + test);
          expected++;
        }
      }

      do_check_eq(expected, gNewBlocks.length);
    }
    callback();
  });
}

function load_blocklist(file) {
  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:4444/data/" + file);
  var blocklist = Components.classes["@mozilla.org/extensions/blocklist;1"]
                            .getService(Components.interfaces.nsITimerCallback);
  blocklist.notify(null);
}

function run_test() {
  // Setup for test
  dump("Setting up tests\n");
  // Rather than keeping lots of identical add-ons in version control, just
  // write them into the profile.
  for (let addon of ADDONS)
    create_addon(addon);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  startupManager();

  gTestserver = new nsHttpServer();
  gTestserver.registerDirectory("/data/", do_get_file("data"));
  gTestserver.start(4444);

  do_test_pending();
  check_test_pt1();
}

/**
 * Checks the initial state is correct
 */
function check_test_pt1() {
  dump("Checking pt 1\n");

  AddonManager.getAddonsByIDs([a.id for each (a in ADDONS)], function(addons) {
    for (var i = 0; i < ADDONS.length; i++) {
      if (!addons[i])
        do_throw("Addon " + (i + 1) + " did not get installed correctly");
    }

    check_state("start", null, run_test_pt2);
  });
}

/**
 * Load the toolkit based blocks
 */
function run_test_pt2() {
  dump("Running test pt 2\n");
  gCallback = check_test_pt2;
  load_blocklist("test_bug449027_toolkit.xml");
}

function check_test_pt2() {
  dump("Checking pt 2\n");
  check_state("toolkitBlocks", "start", run_test_pt3);
}

/**
 * Load the application based blocks
 */
function run_test_pt3() {
  dump("Running test pt 3\n");
  gCallback = check_test_pt3;
  load_blocklist("test_bug449027_app.xml");
}

function check_test_pt3() {
  dump("Checking pt 3\n");
  check_state("appBlocks", "toolkitBlocks", end_test);
}

function end_test() {
  gTestserver.stop(do_test_finished);
}
