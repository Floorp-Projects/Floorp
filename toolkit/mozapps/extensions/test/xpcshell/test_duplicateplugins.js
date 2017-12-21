/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://testing-common/MockRegistrar.jsm");

var Ci = Components.interfaces;

// This verifies that duplicate plugins are coalesced and maintain their ID
// across restarts.

var PLUGINS = [{
  name: "Duplicate Plugin 1",
  description: "A duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  filename: "/home/mozilla/.plugins/dupplugin1.so"
}, {
  name: "Duplicate Plugin 1",
  description: "A duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  filename: "/usr/lib/plugins/dupplugin1.so"
}, {
  name: "Duplicate Plugin 2",
  description: "Another duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  filename: "/home/mozilla/.plugins/dupplugin2.so"
}, {
  name: "Duplicate Plugin 2",
  description: "Another duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  filename: "/usr/lib/plugins/dupplugin2.so"
}, {
  name: "Non-duplicate Plugin", // 3
  description: "Not a duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  filename: "/home/mozilla/.plugins/dupplugin3.so"
}, {
  name: "Non-duplicate Plugin", // 4
  description: "Not a duplicate because the descriptions are different",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  filename: "/usr/lib/plugins/dupplugin4.so"
}, {
  name: "Another Non-duplicate Plugin", // 5
  description: "Not a duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  filename: "/home/mozilla/.plugins/dupplugin5.so"
}];

// A fake plugin host to return the plugins defined above
var PluginHost = {
  getPluginTags(countRef) {
    countRef.value = PLUGINS.length;
    return PLUGINS;
  },

  QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIPluginHost)
     || iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

MockRegistrar.register("@mozilla.org/plugin/host;1", PluginHost);

var gPluginIDs = [null, null, null, null, null];

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  Services.prefs.setBoolPref("media.gmp-provider.enabled", false);

  startupManager();

  run_test_1();
}

function found_plugin(aNum, aId) {
  if (gPluginIDs[aNum])
    do_throw("Found duplicate of plugin " + aNum);
  gPluginIDs[aNum] = aId;
}

// Test that the plugins were coalesced and all appear in the returned list
function run_test_1() {
  AddonManager.getAddonsByTypes(["plugin"], function(aAddons) {
    Assert.equal(aAddons.length, 5);
    aAddons.forEach(function(aAddon) {
      if (aAddon.name == "Duplicate Plugin 1") {
        found_plugin(0, aAddon.id);
        Assert.equal(aAddon.description, "A duplicate plugin");
      } else if (aAddon.name == "Duplicate Plugin 2") {
        found_plugin(1, aAddon.id);
        Assert.equal(aAddon.description, "Another duplicate plugin");
      } else if (aAddon.name == "Another Non-duplicate Plugin") {
        found_plugin(5, aAddon.id);
        Assert.equal(aAddon.description, "Not a duplicate plugin");
      } else if (aAddon.name == "Non-duplicate Plugin") {
        if (aAddon.description == "Not a duplicate plugin")
          found_plugin(3, aAddon.id);
        else if (aAddon.description == "Not a duplicate because the descriptions are different")
          found_plugin(4, aAddon.id);
        else
          do_throw("Found unexpected plugin with description " + aAddon.description);
      } else {
        do_throw("Found unexpected plugin " + aAddon.name);
      }
    });

    run_test_2();
  });
}

// Test that disabling a coalesced plugin disables all its tags
function run_test_2() {
  AddonManager.getAddonByID(gPluginIDs[0], function(p) {
    Assert.ok(!p.userDisabled);
    p.userDisabled = true;
    Assert.ok(PLUGINS[0].disabled);
    Assert.ok(PLUGINS[1].disabled);

    do_execute_soon(run_test_3);
  });
}

// Test that IDs persist across restart
function run_test_3() {
  restartManager();

  AddonManager.getAddonByID(gPluginIDs[0], callback_soon(function(p) {
    Assert.notEqual(p, null);
    Assert.equal(p.name, "Duplicate Plugin 1");
    Assert.equal(p.description, "A duplicate plugin");

    // Reorder the plugins and restart again
    [PLUGINS[0], PLUGINS[1]] = [PLUGINS[1], PLUGINS[0]];
    restartManager();

    AddonManager.getAddonByID(gPluginIDs[0], function(p_2) {
      Assert.notEqual(p_2, null);
      Assert.equal(p_2.name, "Duplicate Plugin 1");
      Assert.equal(p_2.description, "A duplicate plugin");

      do_execute_soon(do_test_finished);
    });
  }));
}
