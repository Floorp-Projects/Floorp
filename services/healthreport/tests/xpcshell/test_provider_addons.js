/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;


Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");


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
  let addons = [
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
    },
  ];

  monkeypatchAddons(provider, addons);

  yield provider.collectConstantData();

  let active = provider.getMeasurement("active", 1);
  let data = yield active.getValues();

  do_check_eq(data.days.size, 0);
  do_check_eq(data.singular.size, 1);
  do_check_true(data.singular.has("addons"));

  let json = data.singular.get("addons")[1];
  let value = JSON.parse(json);
  do_check_eq(typeof(value), "object");
  do_check_eq(Object.keys(value).length, 3);
  do_check_true("addon0" in value);
  do_check_true("addon1" in value);
  do_check_true("addon3" in value);

  let serializer = active.serializer(active.SERIALIZE_JSON);
  let serialized = serializer.singular(data.singular);
  do_check_eq(typeof(serialized), "object");
  do_check_eq(Object.keys(serialized).length, 4);   // Our three keys, plus _v.
  do_check_true("addon0" in serialized);
  do_check_true("addon1" in serialized);
  do_check_true("addon3" in serialized);
  do_check_eq(serialized._v, 1);

  let counts = provider.getMeasurement("counts", 2);
  data = yield counts.getValues();
  do_check_eq(data.days.size, 1);
  do_check_eq(data.singular.size, 0);
  do_check_true(data.days.hasDay(now));

  value = data.days.getDay(now);
  do_check_eq(value.size, 4);
  do_check_eq(value.get("extension"), 1);
  do_check_eq(value.get("plugin"), 1);
  do_check_eq(value.get("theme"), 1);
  do_check_eq(value.get("service"), 1);

  yield provider.shutdown();
  yield storage.close();
});

