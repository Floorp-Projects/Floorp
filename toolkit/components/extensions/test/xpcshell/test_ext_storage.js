/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const STORAGE_SYNC_PREF = "webextensions.storage.sync.enabled";
Cu.import("resource://gre/modules/Preferences.jsm");

/**
 * Utility function to ensure that all supported APIs for getting are
 * tested.
 *
 * @param {string} areaName
 *        either "local" or "sync" according to what we want to test
 * @param {string} prop
 *        "key" to look up using the storage API
 * @param {Object} value
 *        "value" to compare against
 * @returns {Promise}
 */
function checkGet(areaName, prop, value) {
  let storage = browser.storage[areaName];
  return storage.get(null).then(data => {
    browser.test.assertEq(value, data[prop], `null getter worked for ${prop} in ${areaName}`);
    return storage.get(prop);
  }).then(data => {
    browser.test.assertEq(value, data[prop], `string getter worked for ${prop} in ${areaName}`);
    return storage.get([prop]);
  }).then(data => {
    browser.test.assertEq(value, data[prop], `array getter worked for ${prop} in ${areaName}`);
    return storage.get({[prop]: undefined});
  }).then(data => {
    browser.test.assertEq(value, data[prop], `object getter worked for ${prop} in ${areaName}`);
  });
}

add_task(function* test_local_cache_invalidation() {
  function background(checkGet) {
    browser.test.onMessage.addListener(msg => {
      if (msg === "set-initial") {
        browser.storage.local.set({"test-prop1": "value1", "test-prop2": "value2"}).then(() => {
          browser.test.sendMessage("set-initial-done");
        });
      } else if (msg === "check") {
        checkGet("local", "test-prop1", "value1").then(() => {
          return checkGet("local", "test-prop2", "value2");
        }).then(() => {
          browser.test.sendMessage("check-done");
        });
      }
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
    },
    background: `(${background})(${checkGet})`,
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");

  extension.sendMessage("set-initial");
  yield extension.awaitMessage("set-initial-done");

  Services.obs.notifyObservers(null, "extension-invalidate-storage-cache", "");

  extension.sendMessage("check");
  yield extension.awaitMessage("check-done");

  yield extension.unload();
});

add_task(function* test_config_flag_needed() {
  function background() {
    let promises = [];
    let apiTests = [
      {method: "get", args: ["foo"]},
      {method: "set", args: [{foo: "bar"}]},
      {method: "remove", args: ["foo"]},
      {method: "clear", args: []},
    ];
    apiTests.forEach(testDef => {
      const test = browser.storage.sync[testDef.method](...testDef.args).then(() => {
        browser.test.fail("didn't fail with extension.storage.sync.enabled = false");
        return Promise.reject();
      }).catch(error => {
        browser.test.assertEq("Please set webextensions.storage.sync.enabled to " +
                              "true in about:config", error.message,
                              `storage.sync.${testDef.method} is behind a flag`);
        return Promise.resolve();
      });
      promises.push(test);
    });

    Promise.all(promises).then(() => browser.test.notifyPass("flag needed"));
  }

  ok(!Preferences.get(STORAGE_SYNC_PREF));
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
    },
    background: `(${background})(${checkGet})`,
  });

  yield extension.startup();
  yield extension.awaitFinish("flag needed");
  yield extension.unload();
});

add_task(function* test_reloading_extensions_works() {
  // Just some random extension ID that we can re-use
  const extensionId = "my-extension-id@1";

  function loadExtension() {
    function background() {
      browser.storage.sync.set({"a": "b"}).then(() => {
        browser.test.notifyPass("set-works");
      });
    }

    return ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["storage"],
      },
      background: `(${background})()`,
    }, extensionId);
  }

  Preferences.set(STORAGE_SYNC_PREF, true);

  let extension1 = loadExtension();

  yield extension1.startup();
  yield extension1.awaitFinish("set-works");
  yield extension1.unload();

  let extension2 = loadExtension();

  yield extension2.startup();
  yield extension2.awaitFinish("set-works");
  yield extension2.unload();

  Preferences.reset(STORAGE_SYNC_PREF);
});

do_register_cleanup(() => {
  Preferences.reset(STORAGE_SYNC_PREF);
});

add_task(function* test_backgroundScript() {
  function backgroundScript(checkGet) {
    let globalChanges, gResolve;
    function clearGlobalChanges() {
      globalChanges = new Promise(resolve => { gResolve = resolve; });
    }
    clearGlobalChanges();
    let expectedAreaName;

    browser.storage.onChanged.addListener((changes, areaName) => {
      browser.test.assertEq(expectedAreaName, areaName,
        "Expected area name received by listener");
      gResolve(changes);
    });

    function checkChanges(areaName, changes, message) {
      function checkSub(obj1, obj2) {
        for (let prop in obj1) {
          browser.test.assertTrue(obj1[prop] !== undefined,
                                  `checkChanges ${areaName} ${prop} is missing (${message})`);
          browser.test.assertTrue(obj2[prop] !== undefined,
                                  `checkChanges ${areaName} ${prop} is missing (${message})`);
          browser.test.assertEq(obj1[prop].oldValue, obj2[prop].oldValue,
                                `checkChanges ${areaName} ${prop} old (${message})`);
          browser.test.assertEq(obj1[prop].newValue, obj2[prop].newValue,
                                `checkChanges ${areaName} ${prop} new (${message})`);
        }
      }

      return globalChanges.then(recentChanges => {
        checkSub(changes, recentChanges);
        checkSub(recentChanges, changes);
        clearGlobalChanges();
      });
    }

    /* eslint-disable dot-notation */
    function runTests(areaName) {
      expectedAreaName = areaName;
      let storage = browser.storage[areaName];
      // Set some data and then test getters.
      return storage.set({"test-prop1": "value1", "test-prop2": "value2"}).then(() => {
        return checkChanges(areaName,
          {"test-prop1": {newValue: "value1"}, "test-prop2": {newValue: "value2"}},
          "set (a)");
      }).then(() => {
        return checkGet(areaName, "test-prop1", "value1");
      }).then(() => {
        return checkGet(areaName, "test-prop2", "value2");
      }).then(() => {
        return storage.get({"test-prop1": undefined, "test-prop2": undefined, "other": "default"});
      }).then(data => {
        browser.test.assertEq("value1", data["test-prop1"], "prop1 correct (a)");
        browser.test.assertEq("value2", data["test-prop2"], "prop2 correct (a)");
        browser.test.assertEq("default", data["other"], "other correct");
        return storage.get(["test-prop1", "test-prop2", "other"]);
      }).then(data => {
        browser.test.assertEq("value1", data["test-prop1"], "prop1 correct (b)");
        browser.test.assertEq("value2", data["test-prop2"], "prop2 correct (b)");
        browser.test.assertFalse("other" in data, "other correct");

        // Remove data in various ways.
      }).then(() => {
        return storage.remove("test-prop1");
      }).then(() => {
        return checkChanges(areaName, {"test-prop1": {oldValue: "value1"}}, "remove string");
      }).then(() => {
        return storage.get(["test-prop1", "test-prop2"]);
      }).then(data => {
        browser.test.assertFalse("test-prop1" in data, "prop1 absent (remove string)");
        browser.test.assertTrue("test-prop2" in data, "prop2 present (remove string)");

        return storage.set({"test-prop1": "value1"});
      }).then(() => {
        return checkChanges(areaName, {"test-prop1": {newValue: "value1"}}, "set (c)");
      }).then(() => {
        return storage.get(["test-prop1", "test-prop2"]);
      }).then(data => {
        browser.test.assertEq(data["test-prop1"], "value1", "prop1 correct (c)");
        browser.test.assertEq(data["test-prop2"], "value2", "prop2 correct (c)");
      }).then(() => {
        return storage.remove(["test-prop1", "test-prop2"]);
      }).then(() => {
        return checkChanges(areaName,
          {"test-prop1": {oldValue: "value1"}, "test-prop2": {oldValue: "value2"}},
          "remove array");
      }).then(() => {
        return storage.get(["test-prop1", "test-prop2"]);
      }).then(data => {
        browser.test.assertFalse("test-prop1" in data, "prop1 absent (remove array)");
        browser.test.assertFalse("test-prop2" in data, "prop2 absent (remove array)");

        // test storage.clear
      }).then(() => {
        return storage.set({"test-prop1": "value1", "test-prop2": "value2"});
      }).then(() => {
        // Make sure that set() handler happened before we clear the
        // promise again.
        return globalChanges;
      }).then(() => {
        clearGlobalChanges();
        return storage.clear();
      }).then(() => {
        return checkChanges(areaName,
          {"test-prop1": {oldValue: "value1"}, "test-prop2": {oldValue: "value2"}},
          "clear");
      }).then(() => {
        return storage.get(["test-prop1", "test-prop2"]);
      }).then(data => {
        browser.test.assertFalse("test-prop1" in data, "prop1 absent (clear)");
        browser.test.assertFalse("test-prop2" in data, "prop2 absent (clear)");

        // Make sure we can store complex JSON data.
      }).then(() => {
        // known previous values
        return storage.set({"test-prop1": "value1", "test-prop2": "value2"});
      }).then(() => {
        // Make sure the set() handler landed.
        return globalChanges;
      }).then(() => {
        clearGlobalChanges();
        return storage.set({
          "test-prop1": {
            str: "hello",
            bool: true,
            null: null,
            undef: undefined,
            obj: {},
            arr: [1, 2],
            date: new Date(0),
            regexp: /regexp/,
            func: function func() {},
            window,
          },
        });
      }).then(() => {
        return storage.set({"test-prop2": function func() {}});
      }).then(() => globalChanges).then(recentChanges => {
        browser.test.assertEq("value1", recentChanges["test-prop1"].oldValue, "oldValue correct");
        browser.test.assertEq("object", typeof(recentChanges["test-prop1"].newValue), "newValue is obj");
        clearGlobalChanges();
        return storage.get({"test-prop1": undefined, "test-prop2": undefined});
      }).then(data => {
        let obj = data["test-prop1"];

        browser.test.assertEq("hello", obj.str, "string part correct");
        browser.test.assertEq(true, obj.bool, "bool part correct");
        browser.test.assertEq(null, obj.null, "null part correct");
        browser.test.assertEq(undefined, obj.undef, "undefined part correct");
        browser.test.assertEq(undefined, obj.func, "function part correct");
        browser.test.assertEq(undefined, obj.window, "window part correct");
        browser.test.assertEq("1970-01-01T00:00:00.000Z", obj.date, "date part correct");
        browser.test.assertEq("/regexp/", obj.regexp, "regexp part correct");
        browser.test.assertEq("object", typeof(obj.obj), "object part correct");
        browser.test.assertTrue(Array.isArray(obj.arr), "array part present");
        browser.test.assertEq(1, obj.arr[0], "arr[0] part correct");
        browser.test.assertEq(2, obj.arr[1], "arr[1] part correct");
        browser.test.assertEq(2, obj.arr.length, "arr.length part correct");

        obj = data["test-prop2"];

        browser.test.assertEq("[object Object]", {}.toString.call(obj), "function serialized as a plain object");
        browser.test.assertEq(0, Object.keys(obj).length, "function serialized as an empty object");
      }).catch(e => {
        browser.test.fail(`Error: ${e} :: ${e.stack}`);
        browser.test.notifyFail("storage");
      });
    }

    browser.test.onMessage.addListener(msg => {
      let promise;
      if (msg === "test-local") {
        promise = runTests("local");
      } else if (msg === "test-sync") {
        promise = runTests("sync");
      }
      promise.then(() => browser.test.sendMessage("test-finished"));
    });

    browser.test.sendMessage("ready");
  }

  let extensionData = {
    background: `(${backgroundScript})(${checkGet})`,
    manifest: {
      permissions: ["storage"],
    },
  };

  Preferences.set(STORAGE_SYNC_PREF, true);

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();
  yield extension.awaitMessage("ready");

  extension.sendMessage("test-local");
  yield extension.awaitMessage("test-finished");

  extension.sendMessage("test-sync");
  yield extension.awaitMessage("test-finished");

  Preferences.reset(STORAGE_SYNC_PREF);
  yield extension.unload();
});
