/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const Ci = Components.interfaces;

// This verifies that duplicate plugins are coalesced and maintain their ID
// across restarts.

var PLUGINS = [{
  name: "Duplicate Plugin 1",
  description: "A duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() this.enabledState == Ci.nsIPluginTag.STATE_DISABLED,
  filename: "/home/mozilla/.plugins/dupplugin1.so"
}, {
  name: "Duplicate Plugin 1",
  description: "A duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() this.enabledState == Ci.nsIPluginTag.STATE_DISABLED,
  filename: "",
  filename: "/usr/lib/plugins/dupplugin1.so"
}, {
  name: "Duplicate Plugin 2",
  description: "Another duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() this.enabledState == Ci.nsIPluginTag.STATE_DISABLED,
  filename: "/home/mozilla/.plugins/dupplugin2.so"
}, {
  name: "Duplicate Plugin 2",
  description: "Another duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() this.enabledState == Ci.nsIPluginTag.STATE_DISABLED,
  filename: "",
  filename: "/usr/lib/plugins/dupplugin2.so"
}, {
  name: "Non-duplicate Plugin", // 3
  description: "Not a duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() this.enabledState == Ci.nsIPluginTag.STATE_DISABLED,
  filename: "/home/mozilla/.plugins/dupplugin3.so"
}, {
  name: "Non-duplicate Plugin", // 4
  description: "Not a duplicate because the descriptions are different",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() this.enabledState == Ci.nsIPluginTag.STATE_DISABLED,
  filename: "",
  filename: "/usr/lib/plugins/dupplugin4.so"
}, {
  name: "Another Non-duplicate Plugin", // 5
  description: "Not a duplicate plugin",
  version: "1",
  blocklisted: false,
  enabledState: Ci.nsIPluginTag.STATE_ENABLED,
  get disabled() this.enabledState == Ci.nsIPluginTag.STATE_DISABLED,
  filename: "/home/mozilla/.plugins/dupplugin5.so"
}];

// A fake plugin host to return the plugins defined above
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

var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{721c3e73-969e-474b-a6dc-059fd288c428}"),
                          "Fake Plugin Host",
                          "@mozilla.org/plugin/host;1", PluginHostFactory);

var gPluginIDs = [null, null, null, null, null];

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

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
    do_check_eq(aAddons.length, 5);
    aAddons.forEach(function(aAddon) {
      if (aAddon.name == "Duplicate Plugin 1") {
        found_plugin(0, aAddon.id);
        do_check_eq(aAddon.description, "A duplicate plugin");
      }
      else if (aAddon.name == "Duplicate Plugin 2") {
        found_plugin(1, aAddon.id);
        do_check_eq(aAddon.description, "Another duplicate plugin");
      }
      else if (aAddon.name == "Another Non-duplicate Plugin") {
        found_plugin(5, aAddon.id);
        do_check_eq(aAddon.description, "Not a duplicate plugin");
      }
      else if (aAddon.name == "Non-duplicate Plugin") {
        if (aAddon.description == "Not a duplicate plugin")
          found_plugin(3, aAddon.id);
        else if (aAddon.description == "Not a duplicate because the descriptions are different")
          found_plugin(4, aAddon.id);
        else
          do_throw("Found unexpected plugin with description " + aAddon.description);
      }
      else {
        do_throw("Found unexpected plugin " + aAddon.name);
      }
    });

    run_test_2();
  });
}

// Test that disabling a coalesced plugin disables all its tags
function run_test_2() {
  AddonManager.getAddonByID(gPluginIDs[0], function(p) {
    do_check_false(p.userDisabled);
    p.userDisabled = true;
    do_check_true(PLUGINS[0].disabled);
    do_check_true(PLUGINS[1].disabled);

    do_execute_soon(run_test_3);
  });
}

// Test that IDs persist across restart
function run_test_3() {
  restartManager();

  AddonManager.getAddonByID(gPluginIDs[0], function(p) {
    do_check_neq(p, null);
    do_check_eq(p.name, "Duplicate Plugin 1");
    do_check_eq(p.description, "A duplicate plugin");

    // Reorder the plugins and restart again
    [PLUGINS[0], PLUGINS[1]] = [PLUGINS[1], PLUGINS[0]];
    restartManager();

    AddonManager.getAddonByID(gPluginIDs[0], function(p) {
      do_check_neq(p, null);
      do_check_eq(p.name, "Duplicate Plugin 1");
      do_check_eq(p.description, "A duplicate plugin");

      do_test_finished();
    });
  });
}
