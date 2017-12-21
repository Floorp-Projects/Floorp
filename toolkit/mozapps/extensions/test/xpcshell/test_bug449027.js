/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/MockRegistrar.jsm");

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

function MockPluginTag(name, version, start, appBlocks, toolkitBlocks) {
  this.name = name;
  this.version = version;
  this.start = start;
  this.appBlocks = appBlocks;
  this.toolkitBlocks = toolkitBlocks;
}
Object.defineProperty(MockPluginTag.prototype, "blocklisted", {
  get: function MockPluginTag_getBlocklisted() {
    return Services.blocklist.getPluginBlocklistState(this) == Services.blocklist.STATE_BLOCKED;
  }
});

var PLUGINS = [
  new MockPluginTag("test_bug449027_1", "5", false, false, false),
  new MockPluginTag("test_bug449027_2", "5", false, true, false),
  new MockPluginTag("test_bug449027_3", "5", false, true, false),
  new MockPluginTag("test_bug449027_4", "5", false, false, false),
  new MockPluginTag("test_bug449027_5", "5", false, false, false),
  new MockPluginTag("test_bug449027_6", "5", false, true, false),
  new MockPluginTag("test_bug449027_7", "5", false, true, false),
  new MockPluginTag("test_bug449027_8", "5", false, true, false),
  new MockPluginTag("test_bug449027_9", "5", false, true, false),
  new MockPluginTag("test_bug449027_10", "5", false, true, false),
  new MockPluginTag("test_bug449027_11", "5", false, true, false),
  new MockPluginTag("test_bug449027_12", "5", false, true, false),
  new MockPluginTag("test_bug449027_13", "5", false, true, false),
  new MockPluginTag("test_bug449027_14", "5", false, false, false),
  new MockPluginTag("test_bug449027_15", "5", false, true, true),
  new MockPluginTag("test_bug449027_16", "5", false, true, true),
  new MockPluginTag("test_bug449027_17", "5", false, false, false),
  new MockPluginTag("test_bug449027_18", "5", false, false, false),
  new MockPluginTag("test_bug449027_19", "5", false, true, true),
  new MockPluginTag("test_bug449027_20", "5", false, true, true),
  new MockPluginTag("test_bug449027_21", "5", false, true, true),
  new MockPluginTag("test_bug449027_22", "5", false, true, true),
  new MockPluginTag("test_bug449027_23", "5", false, true, true),
  new MockPluginTag("test_bug449027_24", "5", false, true, true),
  new MockPluginTag("test_bug449027_25", "5", false, true, true)
];

var gCallback = null;
var gTestserver = null;
var gNewBlocks = [];

// A fake plugin host for the blocklist service to use
var PluginHost = {
  getPluginTags(countRef) {
    countRef.value = PLUGINS.length;
    return PLUGINS;
  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIPluginHost)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow(parent, url, name, features, args) {
    // Should be called to list the newly blocklisted items
    Assert.equal(url, URI_EXTENSION_BLOCKLIST_DIALOG);
    Assert.notEqual(gCallback, null);

    args = args.wrappedJSObject;

    gNewBlocks = [];
    var list = args.list;
    for (let listItem of list)
      gNewBlocks.push(listItem.name + " " + listItem.version);

    // Call the callback after the blocklist has finished up
    do_timeout(0, gCallback);
  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

MockRegistrar.register("@mozilla.org/plugin/host;1", PluginHost);
MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

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
  target.create(target.NORMAL_FILE_TYPE, 0o644);
  var stream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                         .createInstance(Ci.nsIFileOutputStream);
  stream.init(target, 0x04 | 0x08 | 0x20, 0o664, 0); // write, create, truncate
  stream.write(installrdf, installrdf.length);
  stream.close();
}

/**
 * Checks that items are blocklisted correctly according to the current test.
 * If a lastTest is provided checks that the notification dialog got passed
 * the newly blocked items compared to the previous test.
 */
function check_state(test, lastTest, callback) {
  AddonManager.getAddonsByIDs(ADDONS.map(a => a.id), function(addons) {
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

      Assert.equal(expected, gNewBlocks.length);
    }
    do_execute_soon(callback);
  });
}

function load_blocklist(file) {
  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" + gPort + "/data/" + file);
  var blocklist = Components.classes["@mozilla.org/extensions/blocklist;1"]
                            .getService(Ci.nsITimerCallback);
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

  gTestserver = new HttpServer();
  gTestserver.registerDirectory("/data/", do_get_file("data"));
  gTestserver.start(-1);
  gPort = gTestserver.identity.primaryPort;

  do_test_pending();
  check_test_pt1();
}

/**
 * Checks the initial state is correct
 */
function check_test_pt1() {
  dump("Checking pt 1\n");

  AddonManager.getAddonsByIDs(ADDONS.map(a => a.id), function(addons) {
    for (var i = 0; i < ADDONS.length; i++) {
      if (!addons[i])
        do_throw("Addon " + (i + 1) + " did not get installed correctly");
    }

    do_execute_soon(function checkstate1() { check_state("start", null, run_test_pt2); });
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
