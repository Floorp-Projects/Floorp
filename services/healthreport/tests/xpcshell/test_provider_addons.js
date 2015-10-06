/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {utils: Cu, classes: Cc, interfaces: Ci} = Components;


Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");

// The hack, it burns. This could go away if extensions code exposed its
// test environment setup functions as a testing-only JSM. See similar
// code in Sync's head_helpers.js.
var gGlobalScope = this;
function loadAddonManager() {
  let ns = {};
  Cu.import("resource://gre/modules/Services.jsm", ns);
  let head = "../../../../toolkit/mozapps/extensions/test/xpcshell/head_addons.js";
  let file = do_get_file(head);
  let uri = ns.Services.io.newFileURI(file);
  ns.Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
}

function run_test() {
  loadAddonManager();
  run_next_test();
}

add_test(function test_constructor() {
  let provider = new AddonsProvider();

  run_next_test();
});

add_task(function test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new AddonsProvider();
  yield provider.init(storage);
  yield provider.shutdown();

  yield storage.close();
});

function monkeypatchAddons(provider, addons) {
  if (!Array.isArray(addons)) {
    throw new Error("Must define array of addon objects.");
  }

  Object.defineProperty(provider, "_createDataStructure", {
    value: function _createDataStructure() {
      return AddonsProvider.prototype._createDataStructure.call(provider, addons);
    },
  });
}

add_task(function test_collect() {
  let storage = yield Metrics.Storage("collect");
  let provider = new AddonsProvider();
  yield provider.init(storage);

  let now = new Date();

  // FUTURE install add-on via AddonManager and don't use monkeypatching.
  let testAddons = [
    {
      id: "addon0",
      userDisabled: false,
      appDisabled: false,
      version: "1",
      type: "extension",
      scope: 1,
      foreignInstall: false,
      hasBinaryComponents: false,
      installDate: now,
      updateDate: now,
    },
    // This plugin entry should get ignored.
    {
      id: "addon1",
      userDisabled: false,
      appDisabled: false,
      version: "2",
      type: "plugin",
      scope: 1,
      foreignInstall: false,
      hasBinaryComponents: false,
      installDate: now,
      updateDate: now,
    },
    // Is counted but full details are omitted because it is a theme.
    {
      id: "addon2",
      userDisabled: false,
      appDisabled: false,
      version: "3",
      type: "theme",
      scope: 1,
      foreignInstall: false,
      hasBinaryComponents: false,
      installDate: now,
      updateDate: now,
    },
    {
      id: "addon3",
      userDisabled: false,
      appDisabled: false,
      version: "4",
      type: "service",
      scope: 1,
      foreignInstall: false,
      hasBinaryComponents: false,
      installDate: now,
      updateDate: now,
      description: "addon3 description"
    },
    {
      // Should be excluded from the report completely
      id: "pluginfake",
      type: "plugin",
      userDisabled: false,
      appDisabled: false,
    },
    {
      // Should be in gm-plugins
      id: "gmp-testgmp",
      type: "plugin",
      userDisabled: false,
      version: "7.2",
      isGMPlugin: true,
    },
  ];

  monkeypatchAddons(provider, testAddons);

  let testPlugins = {
    "Test Plug-in":
    {
      "version": "1.0.0.0",
      "description": "Plug-in for testing purposes.™ (हिन्दी 中文 العربية)",
      "blocklisted": false,
      "disabled": false,
      "clicktoplay": false,
      "mimeTypes":[
        "application/x-test"
      ],
    },
    "Second Test Plug-in":
    {
      "version": "1.0.0.0",
      "description": "Second plug-in for testing purposes.",
      "blocklisted": false,
      "disabled": false,
      "clicktoplay": false,
      "mimeTypes":[
        "application/x-second-test"
      ],
    },
    "Java Test Plug-in":
    {
      "version": "1.0.0.0",
      "description": "Dummy Java plug-in for testing purposes.",
      "blocklisted": false,
      "disabled": false,
      "clicktoplay": false,
      "mimeTypes":[
        "application/x-java-test"
      ],
    },
    "Third Test Plug-in":
    {
      "version": "1.0.0.0",
      "description": "Third plug-in for testing purposes.",
      "blocklisted": false,
      "disabled": false,
      "clicktoplay": false,
      "mimeTypes":[
        "application/x-third-test"
      ],
    },
    "Flash Test Plug-in":
    {
      "version": "1.0.0.0",
      "description": "Flash plug-in for testing purposes.",
      "blocklisted": false,
      "disabled": false,
      "clicktoplay": false,
      "mimeTypes":[
        "application/x-shockwave-flash-test"
      ],
    },
  };

  let pluginTags = Cc["@mozilla.org/plugin/host;1"]
                    .getService(Ci.nsIPluginHost)
                    .getPluginTags({});

  for (let tag of pluginTags) {
    if (tag.name in testPlugins) {
      let p = testPlugins[tag.name];
      p.id = tag.filename+":"+tag.name+":"+p.version+":"+p.description;
    }
  }

  yield provider.collectConstantData();

  // Test addons measurement.

  let addons = provider.getMeasurement("addons", 2);
  let data = yield addons.getValues();

  do_check_eq(data.days.size, 0);
  do_check_eq(data.singular.size, 1);
  do_check_true(data.singular.has("addons"));

  let json = data.singular.get("addons")[1];
  let value = JSON.parse(json);
  do_check_eq(typeof(value), "object");
  do_check_eq(Object.keys(value).length, 2);
  do_check_true("addon0" in value);
  do_check_true(!("addon1" in value));
  do_check_true(!("addon2" in value));
  do_check_true("addon3" in value);
  do_check_true(!("pluginfake" in value));
  do_check_true(!("gmp-testgmp" in value));

  let serializer = addons.serializer(addons.SERIALIZE_JSON);
  let serialized = serializer.singular(data.singular);
  do_check_eq(typeof(serialized), "object");
  do_check_eq(Object.keys(serialized).length, 3); // Our entries, plus _v.
  do_check_true("addon0" in serialized);
  do_check_true("addon3" in serialized);
  do_check_eq(serialized._v, 2);

  // Test plugins measurement.

  let plugins = provider.getMeasurement("plugins", 1);
  data = yield plugins.getValues();

  do_check_eq(data.days.size, 0);
  do_check_eq(data.singular.size, 1);
  do_check_true(data.singular.has("plugins"));

  json = data.singular.get("plugins")[1];
  value = JSON.parse(json);
  do_check_eq(typeof(value), "object");
  do_check_eq(Object.keys(value).length, pluginTags.length);

  do_check_true(testPlugins["Test Plug-in"].id in value);
  do_check_true(testPlugins["Second Test Plug-in"].id in value);
  do_check_true(testPlugins["Java Test Plug-in"].id in value);

  for (let id in value) {
    let item = value[id];
    let testData = testPlugins[item.name];
    for (let prop in testData) {
      if (prop == "mimeTypes" || prop == "id") {
        continue;
      }
      do_check_eq(testData[prop], item[prop]);
    }

    for (let mime of testData.mimeTypes) {
      do_check_true(item.mimeTypes.indexOf(mime) != -1);
    }
  }

  serializer = plugins.serializer(plugins.SERIALIZE_JSON);
  serialized = serializer.singular(data.singular);
  do_check_eq(typeof(serialized), "object");
  do_check_eq(Object.keys(serialized).length, pluginTags.length+1); // Our entries, plus _v.
  for (let name in testPlugins) {
    // Special case for bug 1165981. There is a test plugin that
    // exists to make sure we don't load it on certain platforms.
    // We skip the check for that plugin here, as it will work on some
    // platforms but not others.
    if (name == "Third Test Plug-in") {
      continue;
    }
    do_check_true(testPlugins[name].id in serialized);
  }
  do_check_eq(serialized._v, 1);

  // Test GMP plugins measurement.

  let gmPlugins = provider.getMeasurement("gm-plugins", 1);
  data = yield gmPlugins.getValues();

  do_check_eq(data.days.size, 0);
  do_check_eq(data.singular.size, 1);
  do_check_true(data.singular.has("gm-plugins"));

  json = data.singular.get("gm-plugins")[1];
  value = JSON.parse(json);
  do_print("value: " + json);
  do_check_eq(typeof(value), "object");
  do_check_eq(Object.keys(value).length, 1);

  do_check_eq(value["gmp-testgmp"].version, "7.2");
  do_check_eq(value["gmp-testgmp"].userDisabled, false);

  serializer = gmPlugins.serializer(plugins.SERIALIZE_JSON);
  serialized = serializer.singular(data.singular);
  do_check_eq(typeof(serialized), "object");
  do_check_eq(serialized["gmp-testgmp"].version, "7.2");
  do_check_eq(serialized._v, 1);

  // Test counts measurement.

  let counts = provider.getMeasurement("counts", 2);
  data = yield counts.getValues();
  do_check_eq(data.days.size, 1);
  do_check_eq(data.singular.size, 0);
  do_check_true(data.days.hasDay(now));

  value = data.days.getDay(now);
  do_check_eq(value.size, 4);
  do_check_eq(value.get("extension"), 1);
  do_check_eq(value.get("plugin"), pluginTags.length);
  do_check_eq(value.get("theme"), 1);
  do_check_eq(value.get("service"), 1);

  yield provider.shutdown();
  yield storage.close();
});

