/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const LIST_UPDATED_TOPIC     = "plugins-list-updated";

// We need to use the same algorithm for generating IDs for plugins
var { getIDHashForString } = Components.utils.import("resource://gre/modules/addons/PluginProvider.jsm");

function PluginTag(name, description) {
  this.name = name;
  this.description = description;
}

PluginTag.prototype = {
  name: null,
  description: null,
  version: "1.0",
  filename: null,
  fullpath: null,
  disabled: false,
  blocklisted: false,
  clicktoplay: false,

  mimeTypes: [],

  getMimeTypes: function(count) {
    count.value = this.mimeTypes.length;
    return this.mimeTypes;
  }
};

PLUGINS = [
  // A standalone plugin
  new PluginTag("Java", "A mock Java plugin"),

  // A plugin made up of two plugin files
  new PluginTag("Flash", "A mock Flash plugin"),
  new PluginTag("Flash", "A mock Flash plugin")
];

gPluginHost = {
  // nsIPluginHost
  getPluginTags: function(count) {
    count.value = PLUGINS.length;
    return PLUGINS;
  },

  QueryInterface: XPCOMUtils.generateQI([AM_Ci.nsIPluginHost])
};

var PluginHostFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return gPluginHost.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(AM_Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{aa6f9fef-cbe2-4d55-a2fa-dcf5482068b9}"), "PluginHost",
                          "@mozilla.org/plugin/host;1", PluginHostFactory);

// This verifies that when the list of plugins changes the add-ons manager
// correctly updates
function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref("media.gmp-gmpopenh264.provider.enabled", false);

  startupManager();
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);

  run_test_1();
}

function end_test() {
  do_execute_soon(do_test_finished);
}

function sortAddons(addons) {
  addons.sort(function(a, b) {
    return a.name.localeCompare(b.name);
  });
}

// Basic check that the mock object works
function run_test_1() {
  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    sortAddons(addons);

    do_check_eq(addons.length, 2);

    do_check_eq(addons[0].name, "Flash");
    do_check_false(addons[0].userDisabled);
    do_check_eq(addons[1].name, "Java");
    do_check_false(addons[1].userDisabled);

    run_test_2();
  });
}

// No change to the list should not trigger any events or changes in the API
function run_test_2() {
  // Reorder the list a bit
  let tag = PLUGINS[0];
  PLUGINS[0] = PLUGINS[2];
  PLUGINS[2] = PLUGINS[1];
  PLUGINS[1] = tag;

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC, null);

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    sortAddons(addons);

    do_check_eq(addons.length, 2);

    do_check_eq(addons[0].name, "Flash");
    do_check_false(addons[0].userDisabled);
    do_check_eq(addons[1].name, "Java");
    do_check_false(addons[1].userDisabled);

    run_test_3();
  });
}

// Tests that a newly detected plugin shows up in the API and sends out events
function run_test_3() {
  let tag = new PluginTag("Quicktime", "A mock Quicktime plugin");
  PLUGINS.push(tag);
  let id = getIDHashForString(tag.name + tag.description);

  let test_params = {};
  test_params[id] = [
    ["onInstalling", false],
    "onInstalled"
  ];

  prepare_test(test_params, [
    "onExternalInstall"
  ]);

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC, null);

  ensure_test_completed();

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    sortAddons(addons);

    do_check_eq(addons.length, 3);

    do_check_eq(addons[0].name, "Flash");
    do_check_false(addons[0].userDisabled);
    do_check_eq(addons[1].name, "Java");
    do_check_false(addons[1].userDisabled);
    do_check_eq(addons[2].name, "Quicktime");
    do_check_false(addons[2].userDisabled);

    run_test_4();
  });
}

// Tests that a removed plugin disappears from in the API and sends out events
function run_test_4() {
  let tag = PLUGINS.splice(1, 1)[0];
  let id = getIDHashForString(tag.name + tag.description);

  let test_params = {};
  test_params[id] = [
    ["onUninstalling", false],
    "onUninstalled"
  ];

  prepare_test(test_params);

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC, null);

  ensure_test_completed();

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    sortAddons(addons);

    do_check_eq(addons.length, 2);

    do_check_eq(addons[0].name, "Flash");
    do_check_false(addons[0].userDisabled);
    do_check_eq(addons[1].name, "Quicktime");
    do_check_false(addons[1].userDisabled);

    run_test_5();
  });
}

// Removing part of the flash plugin should have no effect
function run_test_5() {
  PLUGINS.splice(0, 1);

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC, null);

  ensure_test_completed();

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    sortAddons(addons);

    do_check_eq(addons.length, 2);

    do_check_eq(addons[0].name, "Flash");
    do_check_false(addons[0].userDisabled);
    do_check_eq(addons[1].name, "Quicktime");
    do_check_false(addons[1].userDisabled);

    run_test_6();
  });
}

// Replacing flash should be detected
function run_test_6() {
  let oldTag = PLUGINS.splice(0, 1)[0];
  let newTag = new PluginTag("Flash 2", "A new crash-free Flash!");
  newTag.disabled = true;
  PLUGINS.push(newTag);

  let test_params = {};
  test_params[getIDHashForString(oldTag.name + oldTag.description)] = [
    ["onUninstalling", false],
    "onUninstalled"
  ];
  test_params[getIDHashForString(newTag.name + newTag.description)] = [
    ["onInstalling", false],
    "onInstalled"
  ];

  prepare_test(test_params, [
    "onExternalInstall"
  ]);

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC, null);

  ensure_test_completed();

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    sortAddons(addons);

    do_check_eq(addons.length, 2);

    do_check_eq(addons[0].name, "Flash 2");
    do_check_true(addons[0].userDisabled);
    do_check_eq(addons[1].name, "Quicktime");
    do_check_false(addons[1].userDisabled);

    run_test_7();
  });
}

// If new tags are detected and the disabled state changes then we should send
// out appropriate notifications
function run_test_7() {
  PLUGINS[0] = new PluginTag("Quicktime", "A mock Quicktime plugin");
  PLUGINS[0].disabled = true;
  PLUGINS[1] = new PluginTag("Flash 2", "A new crash-free Flash!");

  let test_params = {};
  test_params[getIDHashForString(PLUGINS[0].name + PLUGINS[0].description)] = [
    ["onDisabling", false],
    "onDisabled"
  ];
  test_params[getIDHashForString(PLUGINS[1].name + PLUGINS[1].description)] = [
    ["onEnabling", false],
    "onEnabled"
  ];

  prepare_test(test_params);

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC, null);

  ensure_test_completed();

  AddonManager.getAddonsByTypes(["plugin"], function(addons) {
    sortAddons(addons);

    do_check_eq(addons.length, 2);

    do_check_eq(addons[0].name, "Flash 2");
    do_check_false(addons[0].userDisabled);
    do_check_eq(addons[1].name, "Quicktime");
    do_check_true(addons[1].userDisabled);

    end_test();
  });
}
