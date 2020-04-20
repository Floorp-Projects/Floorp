/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionStorageIDB",
  "resource://gre/modules/ExtensionStorageIDB.jsm"
);

const STORAGE_SYNC_PREF = "webextensions.storage.sync.enabled";

AddonTestUtils.init(this);

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

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
 */
async function checkGetImpl(areaName, prop, value) {
  let storage = browser.storage[areaName];

  let data = await storage.get(null);
  browser.test.assertEq(
    value,
    data[prop],
    `null getter worked for ${prop} in ${areaName}`
  );

  data = await storage.get(prop);
  browser.test.assertEq(
    value,
    data[prop],
    `string getter worked for ${prop} in ${areaName}`
  );
  browser.test.assertEq(
    Object.keys(data).length,
    1,
    `string getter should return an object with a single property`
  );

  data = await storage.get([prop]);
  browser.test.assertEq(
    value,
    data[prop],
    `array getter worked for ${prop} in ${areaName}`
  );
  browser.test.assertEq(
    Object.keys(data).length,
    1,
    `array getter with a single key should return an object with a single property`
  );

  data = await storage.get({ [prop]: undefined });
  browser.test.assertEq(
    value,
    data[prop],
    `object getter worked for ${prop} in ${areaName}`
  );
  browser.test.assertEq(
    Object.keys(data).length,
    1,
    `object getter with a single key should return an object with a single property`
  );
}

add_task(async function test_local_cache_invalidation() {
  function background(checkGet) {
    browser.test.onMessage.addListener(async msg => {
      if (msg === "set-initial") {
        await browser.storage.local.set({
          "test-prop1": "value1",
          "test-prop2": "value2",
        });
        browser.test.sendMessage("set-initial-done");
      } else if (msg === "check") {
        await checkGet("local", "test-prop1", "value1");
        await checkGet("local", "test-prop2", "value2");
        browser.test.sendMessage("check-done");
      }
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
    },
    background: `(${background})(${checkGetImpl})`,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage("set-initial");
  await extension.awaitMessage("set-initial-done");

  Services.obs.notifyObservers(null, "extension-invalidate-storage-cache");

  extension.sendMessage("check");
  await extension.awaitMessage("check-done");

  await extension.unload();
});

add_task(async function test_single_initialization() {
  // Grab access to this via the backstage pass to check if we're calling openConnection too often.
  const { FirefoxAdapter } = ChromeUtils.import(
    "resource://gre/modules/ExtensionStorageSync.jsm",
    null
  );
  const origOpenConnection = FirefoxAdapter.openConnection;
  let callCount = 0;
  FirefoxAdapter.openConnection = function(...args) {
    ++callCount;
    return origOpenConnection.apply(this, args);
  };
  function background() {
    let promises = ["foo", "bar", "baz", "quux"].map(key =>
      browser.storage.sync.get(key)
    );
    Promise.all(promises).then(() =>
      browser.test.notifyPass("initialize once")
    );
  }
  try {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["storage"],
      },
      background: `(${background})()`,
    });

    await extension.startup();
    await extension.awaitFinish("initialize once");
    await extension.unload();
    equal(
      callCount,
      1,
      "Initialized FirefoxAdapter connection and Kinto exactly once"
    );
  } finally {
    FirefoxAdapter.openConnection = origOpenConnection;
  }
});

add_task(function test_config_flag_needed() {
  async function testFn() {
    function background() {
      let promises = [];
      let apiTests = [
        { method: "get", args: ["foo"] },
        { method: "set", args: [{ foo: "bar" }] },
        { method: "remove", args: ["foo"] },
        { method: "clear", args: [] },
      ];
      apiTests.forEach(testDef => {
        promises.push(
          browser.test.assertRejects(
            browser.storage.sync[testDef.method](...testDef.args),
            "Please set webextensions.storage.sync.enabled to true in about:config",
            `storage.sync.${testDef.method} is behind a flag`
          )
        );
      });

      Promise.all(promises).then(() => browser.test.notifyPass("flag needed"));
    }

    ok(
      !Services.prefs.getBoolPref(STORAGE_SYNC_PREF, false),
      "The `${STORAGE_SYNC_PREF}` should be set to false"
    );

    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["storage"],
      },
      background: `(${background})(${checkGetImpl})`,
    });

    await extension.startup();
    await extension.awaitFinish("flag needed");
    await extension.unload();
  }

  return runWithPrefs([[STORAGE_SYNC_PREF, false]], testFn);
});

add_task(function test_reloading_extensions_works() {
  async function testFn() {
    // Just some random extension ID that we can re-use
    const extensionId = "my-extension-id@1";

    function loadExtension() {
      function background() {
        browser.storage.sync.set({ a: "b" }).then(() => {
          browser.test.notifyPass("set-works");
        });
      }

      return ExtensionTestUtils.loadExtension(
        {
          manifest: {
            permissions: ["storage"],
          },
          background: `(${background})()`,
        },
        extensionId
      );
    }

    ok(
      Services.prefs.getBoolPref(STORAGE_SYNC_PREF, false),
      "The `${STORAGE_SYNC_PREF}` should be set to true"
    );

    let extension1 = loadExtension();

    await extension1.startup();
    await extension1.awaitFinish("set-works");
    await extension1.unload();

    let extension2 = loadExtension();

    await extension2.startup();
    await extension2.awaitFinish("set-works");
    await extension2.unload();
  }

  return runWithPrefs([[STORAGE_SYNC_PREF, true]], testFn);
});

async function test_background_page_storage(testAreaName) {
  async function backgroundScript(checkGet) {
    let globalChanges, gResolve;
    function clearGlobalChanges() {
      globalChanges = new Promise(resolve => {
        gResolve = resolve;
      });
    }
    clearGlobalChanges();
    let expectedAreaName;

    browser.storage.onChanged.addListener((changes, areaName) => {
      browser.test.assertEq(
        expectedAreaName,
        areaName,
        "Expected area name received by listener"
      );
      gResolve(changes);
    });

    async function checkChanges(areaName, changes, message) {
      function checkSub(obj1, obj2) {
        for (let prop in obj1) {
          browser.test.assertTrue(
            obj1[prop] !== undefined,
            `checkChanges ${areaName} ${prop} is missing (${message})`
          );
          browser.test.assertTrue(
            obj2[prop] !== undefined,
            `checkChanges ${areaName} ${prop} is missing (${message})`
          );
          browser.test.assertEq(
            obj1[prop].oldValue,
            obj2[prop].oldValue,
            `checkChanges ${areaName} ${prop} old (${message})`
          );
          browser.test.assertEq(
            obj1[prop].newValue,
            obj2[prop].newValue,
            `checkChanges ${areaName} ${prop} new (${message})`
          );
        }
      }

      const recentChanges = await globalChanges;
      checkSub(changes, recentChanges);
      checkSub(recentChanges, changes);
      clearGlobalChanges();
    }

    async function testFalseyValues(areaName) {
      let storage = browser.storage[areaName];
      const dataInitial = {
        "test-falsey-value-bool": false,
        "test-falsey-value-string": "",
        "test-falsey-value-number": 0,
      };
      const dataUpdate = {
        "test-falsey-value-bool": true,
        "test-falsey-value-string": "non-empty-string",
        "test-falsey-value-number": 10,
      };

      // Compute the expected changes.
      const onSetInitial = {
        "test-falsey-value-bool": { newValue: false },
        "test-falsey-value-string": { newValue: "" },
        "test-falsey-value-number": { newValue: 0 },
      };
      const onRemovedFalsey = {
        "test-falsey-value-bool": { oldValue: false },
        "test-falsey-value-string": { oldValue: "" },
        "test-falsey-value-number": { oldValue: 0 },
      };
      const onUpdatedFalsey = {
        "test-falsey-value-bool": { newValue: true, oldValue: false },
        "test-falsey-value-string": {
          newValue: "non-empty-string",
          oldValue: "",
        },
        "test-falsey-value-number": { newValue: 10, oldValue: 0 },
      };
      const keys = Object.keys(dataInitial);

      // Test on removing falsey values.
      await storage.set(dataInitial);
      await checkChanges(areaName, onSetInitial, "set falsey values");
      await storage.remove(keys);
      await checkChanges(areaName, onRemovedFalsey, "remove falsey value");

      // Test on updating falsey values.
      await storage.set(dataInitial);
      await checkChanges(areaName, onSetInitial, "set falsey values");
      await storage.set(dataUpdate);
      await checkChanges(areaName, onUpdatedFalsey, "set non-falsey values");

      // Clear the storage state.
      await storage.clear();
      await globalChanges;
      clearGlobalChanges();
    }

    function CustomObj() {
      this.testKey1 = "testValue1";
    }

    CustomObj.prototype.toString = function() {
      return '{"testKey2":"testValue2"}';
    };

    CustomObj.prototype.toJSON = function customObjToJSON() {
      return { testKey1: "testValue3" };
    };

    /* eslint-disable dot-notation */
    async function runTests(areaName) {
      expectedAreaName = areaName;
      let storage = browser.storage[areaName];
      // Set some data and then test getters.
      try {
        await storage.set({ "test-prop1": "value1", "test-prop2": "value2" });
        await checkChanges(
          areaName,
          {
            "test-prop1": { newValue: "value1" },
            "test-prop2": { newValue: "value2" },
          },
          "set (a)"
        );

        await checkGet(areaName, "test-prop1", "value1");
        await checkGet(areaName, "test-prop2", "value2");

        let data = await storage.get({
          "test-prop1": undefined,
          "test-prop2": undefined,
          other: "default",
        });
        browser.test.assertEq(
          "value1",
          data["test-prop1"],
          "prop1 correct (a)"
        );
        browser.test.assertEq(
          "value2",
          data["test-prop2"],
          "prop2 correct (a)"
        );
        browser.test.assertEq("default", data["other"], "other correct");

        data = await storage.get(["test-prop1", "test-prop2", "other"]);
        browser.test.assertEq(
          "value1",
          data["test-prop1"],
          "prop1 correct (b)"
        );
        browser.test.assertEq(
          "value2",
          data["test-prop2"],
          "prop2 correct (b)"
        );
        browser.test.assertFalse("other" in data, "other correct");

        // Remove data in various ways.
        await storage.remove("test-prop1");
        await checkChanges(
          areaName,
          { "test-prop1": { oldValue: "value1" } },
          "remove string"
        );

        data = await storage.get(["test-prop1", "test-prop2"]);
        browser.test.assertFalse(
          "test-prop1" in data,
          "prop1 absent (remove string)"
        );
        browser.test.assertTrue(
          "test-prop2" in data,
          "prop2 present (remove string)"
        );

        await storage.set({ "test-prop1": "value1" });
        await checkChanges(
          areaName,
          { "test-prop1": { newValue: "value1" } },
          "set (c)"
        );

        data = await storage.get(["test-prop1", "test-prop2"]);
        browser.test.assertEq(
          data["test-prop1"],
          "value1",
          "prop1 correct (c)"
        );
        browser.test.assertEq(
          data["test-prop2"],
          "value2",
          "prop2 correct (c)"
        );

        await storage.remove(["test-prop1", "test-prop2"]);
        await checkChanges(
          areaName,
          {
            "test-prop1": { oldValue: "value1" },
            "test-prop2": { oldValue: "value2" },
          },
          "remove array"
        );

        data = await storage.get(["test-prop1", "test-prop2"]);
        browser.test.assertFalse(
          "test-prop1" in data,
          "prop1 absent (remove array)"
        );
        browser.test.assertFalse(
          "test-prop2" in data,
          "prop2 absent (remove array)"
        );

        await testFalseyValues(areaName);

        // test storage.clear
        await storage.set({ "test-prop1": "value1", "test-prop2": "value2" });
        // Make sure that set() handler happened before we clear the
        // promise again.
        await globalChanges;

        clearGlobalChanges();
        await storage.clear();

        await checkChanges(
          areaName,
          {
            "test-prop1": { oldValue: "value1" },
            "test-prop2": { oldValue: "value2" },
          },
          "clear"
        );
        data = await storage.get(["test-prop1", "test-prop2"]);
        browser.test.assertFalse("test-prop1" in data, "prop1 absent (clear)");
        browser.test.assertFalse("test-prop2" in data, "prop2 absent (clear)");

        // Make sure we can store complex JSON data.
        // known previous values
        await storage.set({ "test-prop1": "value1", "test-prop2": "value2" });

        // Make sure the set() handler landed.
        await globalChanges;

        let date = new Date(0);

        clearGlobalChanges();
        await storage.set({
          "test-prop1": {
            str: "hello",
            bool: true,
            null: null,
            undef: undefined,
            obj: {},
            nestedObj: {
              testKey: {},
            },
            intKeyObj: {
              4: "testValue1",
              3: "testValue2",
              99: "testValue3",
            },
            floatKeyObj: {
              1.4: "testValue1",
              5.5: "testValue2",
            },
            customObj: new CustomObj(),
            arr: [1, 2],
            nestedArr: [1, [2, 3]],
            date,
            regexp: /regexp/,
          },
        });

        await browser.test.assertRejects(
          storage.set({
            window,
          }),
          /DataCloneError|cyclic object value/
        );

        await browser.test.assertRejects(
          storage.set({ "test-prop2": function func() {} }),
          /DataCloneError/
        );

        const recentChanges = await globalChanges;

        browser.test.assertEq(
          "value1",
          recentChanges["test-prop1"].oldValue,
          "oldValue correct"
        );
        browser.test.assertEq(
          "object",
          typeof recentChanges["test-prop1"].newValue,
          "newValue is obj"
        );
        clearGlobalChanges();

        data = await storage.get({
          "test-prop1": undefined,
          "test-prop2": undefined,
        });
        let obj = data["test-prop1"];

        browser.test.assertEq(
          "object",
          typeof obj.customObj,
          "custom object part correct"
        );
        browser.test.assertEq(
          1,
          Object.keys(obj.customObj).length,
          "customObj keys correct"
        );

        if (areaName === "local") {
          browser.test.assertEq(
            String(date),
            String(obj.date),
            "date part correct"
          );
          browser.test.assertEq(
            "/regexp/",
            obj.regexp.toString(),
            "regexp part correct"
          );
          // storage.local doesn't call toJSON
          browser.test.assertEq(
            "testValue1",
            obj.customObj.testKey1,
            "customObj keys correct"
          );
        } else {
          browser.test.assertEq(
            "1970-01-01T00:00:00.000Z",
            String(obj.date),
            "date part correct"
          );

          browser.test.assertEq(
            "object",
            typeof obj.regexp,
            "regexp part is an object"
          );
          browser.test.assertEq(
            0,
            Object.keys(obj.regexp).length,
            "regexp part is an empty object"
          );
          // storage.sync does call toJSON
          browser.test.assertEq(
            "testValue3",
            obj.customObj.testKey1,
            "customObj keys correct"
          );
        }

        browser.test.assertEq("hello", obj.str, "string part correct");
        browser.test.assertEq(true, obj.bool, "bool part correct");
        browser.test.assertEq(null, obj.null, "null part correct");
        browser.test.assertEq(undefined, obj.undef, "undefined part correct");
        browser.test.assertEq(undefined, obj.window, "window part correct");
        browser.test.assertEq("object", typeof obj.obj, "object part correct");
        browser.test.assertEq(
          "object",
          typeof obj.nestedObj,
          "nested object part correct"
        );
        browser.test.assertEq(
          "object",
          typeof obj.nestedObj.testKey,
          "nestedObj.testKey part correct"
        );
        browser.test.assertEq(
          "object",
          typeof obj.intKeyObj,
          "int key object part correct"
        );
        browser.test.assertEq(
          "testValue1",
          obj.intKeyObj[4],
          "intKeyObj[4] part correct"
        );
        browser.test.assertEq(
          "testValue2",
          obj.intKeyObj[3],
          "intKeyObj[3] part correct"
        );
        browser.test.assertEq(
          "testValue3",
          obj.intKeyObj[99],
          "intKeyObj[99] part correct"
        );
        browser.test.assertEq(
          "object",
          typeof obj.floatKeyObj,
          "float key object part correct"
        );
        browser.test.assertEq(
          "testValue1",
          obj.floatKeyObj[1.4],
          "floatKeyObj[1.4] part correct"
        );
        browser.test.assertEq(
          "testValue2",
          obj.floatKeyObj[5.5],
          "floatKeyObj[5.5] part correct"
        );

        browser.test.assertTrue(Array.isArray(obj.arr), "array part present");
        browser.test.assertEq(1, obj.arr[0], "arr[0] part correct");
        browser.test.assertEq(2, obj.arr[1], "arr[1] part correct");
        browser.test.assertEq(2, obj.arr.length, "arr.length part correct");
        browser.test.assertTrue(
          Array.isArray(obj.nestedArr),
          "nested array part present"
        );
        browser.test.assertEq(
          2,
          obj.nestedArr.length,
          "nestedArr.length part correct"
        );
        browser.test.assertEq(1, obj.nestedArr[0], "nestedArr[0] part correct");
        browser.test.assertTrue(
          Array.isArray(obj.nestedArr[1]),
          "nestedArr[1] part present"
        );
        browser.test.assertEq(
          2,
          obj.nestedArr[1].length,
          "nestedArr[1].length part correct"
        );
        browser.test.assertEq(
          2,
          obj.nestedArr[1][0],
          "nestedArr[1][0] part correct"
        );
        browser.test.assertEq(
          3,
          obj.nestedArr[1][1],
          "nestedArr[1][1] part correct"
        );
      } catch (e) {
        browser.test.fail(`Error: ${e} :: ${e.stack}`);
        browser.test.notifyFail("storage");
      }
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
    background: `(${backgroundScript})(${checkGetImpl})`,
    manifest: {
      permissions: ["storage"],
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage(`test-${testAreaName}`);
  await extension.awaitMessage("test-finished");

  await extension.unload();
}

add_task(function test_storage_local_file_backend() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, false]], () =>
    test_background_page_storage("local")
  );
});

add_task(function test_storage_local_idb_backend() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, true]], () =>
    test_background_page_storage("local")
  );
});

add_task(function test_storage_sync() {
  return runWithPrefs([[STORAGE_SYNC_PREF, true]], () =>
    test_background_page_storage("sync")
  );
});

add_task(function test_storage_sync_requires_real_id() {
  async function testFn() {
    async function backgroundScript() {
      const EXCEPTION_MESSAGE =
        "The storage API is not available with a temporary addon ID. " +
        "Please add an explicit addon ID to your manifest. " +
        "For more information see https://bugzil.la/1323228.";

      await browser.test.assertRejects(
        browser.storage.sync.set({ foo: "bar" }),
        EXCEPTION_MESSAGE
      );

      browser.test.notifyPass("exception correct");
    }

    let extensionData = {
      background: `(${backgroundScript})(${checkGetImpl})`,
      manifest: {
        permissions: ["storage"],
      },
      useAddonManager: "temporary",
    };

    let extension = ExtensionTestUtils.loadExtension(extensionData);
    await extension.startup();
    await extension.awaitFinish("exception correct");

    await extension.unload();
  }

  return runWithPrefs([[STORAGE_SYNC_PREF, true]], testFn);
});
