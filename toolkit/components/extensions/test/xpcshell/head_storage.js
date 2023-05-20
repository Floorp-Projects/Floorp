/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* import-globals-from head.js */

const STORAGE_SYNC_PREF = "webextensions.storage.sync.enabled";

// Test implementations and utility functions that are used against multiple
// storage areas (eg, a test which is run against browser.storage.local and
// browser.storage.sync, or a test against browser.storage.sync but needs to
// be run against both the kinto and rust implementations.)

/**
 * Utility function to ensure that all supported APIs for getting are
 * tested.
 *
 * @param {string} areaName
 *        either "local" or "sync" according to what we want to test
 * @param {string} prop
 *        "key" to look up using the storage API
 * @param {object} value
 *        "value" to compare against
 */
async function checkGetImpl(areaName, prop, value) {
  let storage = browser.storage[areaName];

  let data = await storage.get();
  browser.test.assertEq(
    value,
    data[prop],
    `unspecified getter worked for ${prop} in ${areaName}`
  );

  data = await storage.get(null);
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

function test_config_flag_needed() {
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
      background,
    });

    await extension.startup();
    await extension.awaitFinish("flag needed");
    await extension.unload();
  }

  return runWithPrefs([[STORAGE_SYNC_PREF, false]], testFn);
}

async function test_storage_after_reload(areaName, { expectPersistency }) {
  // Just some random extension ID that we can re-use
  const extensionId = "my-extension-id@1";

  function loadExtension() {
    async function background(areaName) {
      browser.test.sendMessage(
        "initialItems",
        await browser.storage[areaName].get(null)
      );
      await browser.storage[areaName].set({ a: "b" });
      browser.test.notifyPass("set-works");
    }

    return ExtensionTestUtils.loadExtension({
      manifest: {
        browser_specific_settings: { gecko: { id: extensionId } },
        permissions: ["storage"],
      },
      background: `(${background})("${areaName}")`,
    });
  }

  let extension1 = loadExtension();
  await extension1.startup();

  Assert.deepEqual(
    await extension1.awaitMessage("initialItems"),
    {},
    "No stored items at first"
  );

  await extension1.awaitFinish("set-works");
  await extension1.unload();

  let extension2 = loadExtension();
  await extension2.startup();

  Assert.deepEqual(
    await extension2.awaitMessage("initialItems"),
    expectPersistency ? { a: "b" } : {},
    `Expect ${areaName} stored items ${
      expectPersistency ? "to" : "not"
    } be available after restart`
  );

  await extension2.awaitFinish("set-works");
  await extension2.unload();
}

function test_sync_reloading_extensions_works() {
  return runWithPrefs([[STORAGE_SYNC_PREF, true]], async () => {
    ok(
      Services.prefs.getBoolPref(STORAGE_SYNC_PREF, false),
      "The `${STORAGE_SYNC_PREF}` should be set to true"
    );

    await test_storage_after_reload("sync", { expectPersistency: true });
  });
}

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

    // Regression test for https://bugzilla.mozilla.org/show_bug.cgi?id=1645598
    async function testNonExistingKeys(storage, storageAreaDesc) {
      let data = await storage.get({ test6: 6 });
      browser.test.assertEq(
        `{"test6":6}`,
        JSON.stringify(data),
        `Use default value when not stored for ${storageAreaDesc}`
      );

      data = await storage.get({ test6: null });
      browser.test.assertEq(
        `{"test6":null}`,
        JSON.stringify(data),
        `Use default value, even if null for ${storageAreaDesc}`
      );

      data = await storage.get("test6");
      browser.test.assertEq(
        `{}`,
        JSON.stringify(data),
        `Empty result if key is not found for ${storageAreaDesc}`
      );

      data = await storage.get(["test6", "test7"]);
      browser.test.assertEq(
        `{}`,
        JSON.stringify(data),
        `Empty result if list of keys is not found for ${storageAreaDesc}`
      );
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
      await testNonExistingKeys(storage, `${areaName} before clearing`);
      await storage.clear();
      await testNonExistingKeys(storage, `${areaName} after clearing`);
      await globalChanges;
      clearGlobalChanges();
    }

    function CustomObj() {
      this.testKey1 = "testValue1";
    }

    CustomObj.prototype.toString = function () {
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

        if (areaName === "local" || areaName === "session") {
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
          // storage.local and .session don't use toJSON().
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
      } else if (msg === "test-session") {
        promise = runTests("session");
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

function test_storage_sync_requires_real_id() {
  async function testFn() {
    async function background() {
      const EXCEPTION_MESSAGE =
        "The storage API will not work with a temporary addon ID. " +
        "Please add an explicit addon ID to your manifest. " +
        "For more information see https://mzl.la/3lPk1aE.";

      await browser.test.assertRejects(
        browser.storage.sync.set({ foo: "bar" }),
        EXCEPTION_MESSAGE
      );

      browser.test.notifyPass("exception correct");
    }

    let extensionData = {
      background,
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
}

// Test for storage areas which don't support getBytesInUse() nor QUOTA
// constants.
async function check_storage_area_no_bytes_in_use(area) {
  let impl = browser.storage[area];

  browser.test.assertEq(
    typeof impl.getBytesInUse,
    "undefined",
    "getBytesInUse API method should not be available"
  );
  browser.test.sendMessage("test-complete");
}

async function test_background_storage_area_no_bytes_in_use(area) {
  const EXT_ID = "test-gbiu@mozilla.org";

  const extensionDef = {
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: { gecko: { id: EXT_ID } },
    },
    background: `(${check_storage_area_no_bytes_in_use})("${area}")`,
  };

  const extension = ExtensionTestUtils.loadExtension(extensionDef);

  await extension.startup();
  await extension.awaitMessage("test-complete");
  await extension.unload();
}

async function test_contentscript_storage_area_no_bytes_in_use(area) {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  function contentScript(checkImpl) {
    browser.test.onMessage.addListener(msg => {
      if (msg === "test-local") {
        checkImpl("local");
      } else if (msg === "test-sync") {
        checkImpl("sync");
      } else if (msg === "test-session") {
        checkImpl("session");
      } else {
        browser.test.fail(`Unexpected test message received: ${msg}`);
        browser.test.sendMessage("test-complete");
      }
    });
    browser.test.sendMessage("ready");
  }

  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],

      permissions: ["storage"],
    },

    files: {
      "content_script.js": `(${contentScript})(${check_storage_area_no_bytes_in_use})`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage(`test-${area}`);
  await extension.awaitMessage("test-complete");

  await extension.unload();
  await contentPage.close();
}

// Test for storage areas which do support getBytesInUse() (but which may or may
// not support enforcement of the quota)
async function check_storage_area_with_bytes_in_use(area, expectQuota) {
  let impl = browser.storage[area];

  // QUOTA_* constants aren't currently exposed - see bug 1396810.
  // However, the quotas are still enforced, so test them here.
  // (Note that an implication of this is that we can't test area other than
  // 'sync', because its limits are different - so for completeness...)
  browser.test.assertEq(
    area,
    "sync",
    "Running test on storage.sync API as expected"
  );
  const QUOTA_BYTES_PER_ITEM = 8192;
  const MAX_ITEMS = 512;

  // bytes is counted as "length of key as a string, length of value as
  // JSON" - ie, quotes not counted in the key, but are in the value.
  let value = "x".repeat(QUOTA_BYTES_PER_ITEM - 3);

  await impl.set({ x: value }); // Shouldn't reject on either kinto or rust-based storage.sync.
  browser.test.assertEq(await impl.getBytesInUse(null), QUOTA_BYTES_PER_ITEM);
  // kinto does implement getBytesInUse() but doesn't enforce a quota.
  if (expectQuota) {
    await browser.test.assertRejects(
      impl.set({ x: value + "x" }),
      /QuotaExceededError/,
      "Got a rejection with the expected error message"
    );
    // MAX_ITEMS
    await impl.clear();
    let ob = {};
    for (let i = 0; i < MAX_ITEMS; i++) {
      ob[`key-${i}`] = "x";
    }
    await impl.set(ob); // should work.
    await browser.test.assertRejects(
      impl.set({ straw: "camel's back" }), // exceeds MAX_ITEMS
      /QuotaExceededError/,
      "Got a rejection with the expected error message"
    );
    // QUOTA_BYTES is being already tested for the underlying StorageSyncService
    // so we don't duplicate those tests here.
  } else {
    // Exceeding quota should work on the previous kinto-based storage.sync implementation
    await impl.set({ x: value + "x" }); // exceeds quota but should work.
    browser.test.assertEq(
      await impl.getBytesInUse(null),
      QUOTA_BYTES_PER_ITEM + 1,
      "Got the expected result from getBytesInUse"
    );
  }
  browser.test.sendMessage("test-complete");
}

async function test_background_storage_area_with_bytes_in_use(
  area,
  expectQuota
) {
  const EXT_ID = "test-gbiu@mozilla.org";

  const extensionDef = {
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: { gecko: { id: EXT_ID } },
    },
    background: `(${check_storage_area_with_bytes_in_use})("${area}", ${expectQuota})`,
  };

  const extension = ExtensionTestUtils.loadExtension(extensionDef);

  await extension.startup();
  await extension.awaitMessage("test-complete");
  await extension.unload();
}

async function test_contentscript_storage_area_with_bytes_in_use(
  area,
  expectQuota
) {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  function contentScript(checkImpl) {
    browser.test.onMessage.addListener(([area, expectQuota]) => {
      if (
        !["local", "sync"].includes(area) ||
        typeof expectQuota !== "boolean"
      ) {
        browser.test.fail(`Unexpected test message: [${area}, ${expectQuota}]`);
        // Let the test to fail immediately instead of wait for a timeout failure.
        browser.test.sendMessage("test-complete");
        return;
      }
      checkImpl(area, expectQuota);
    });
    browser.test.sendMessage("ready");
  }

  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],

      permissions: ["storage"],
    },

    files: {
      "content_script.js": `(${contentScript})(${check_storage_area_with_bytes_in_use})`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage([area, expectQuota]);
  await extension.awaitMessage("test-complete");

  await extension.unload();
  await contentPage.close();
}

// A couple of common tests for checking content scripts.
async function testStorageContentScript(checkGet) {
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
      browser.test.assertEq("value1", data["test-prop1"], "prop1 correct (a)");
      browser.test.assertEq("value2", data["test-prop2"], "prop2 correct (a)");
      browser.test.assertEq("default", data["other"], "other correct");

      data = await storage.get(["test-prop1", "test-prop2", "other"]);
      browser.test.assertEq("value1", data["test-prop1"], "prop1 correct (b)");
      browser.test.assertEq("value2", data["test-prop2"], "prop2 correct (b)");
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
      browser.test.assertEq(data["test-prop1"], "value1", "prop1 correct (c)");
      browser.test.assertEq(data["test-prop2"], "value2", "prop2 correct (c)");

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
          arr: [1, 2],
          date: new Date(0),
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
      }

      browser.test.assertEq("hello", obj.str, "string part correct");
      browser.test.assertEq(true, obj.bool, "bool part correct");
      browser.test.assertEq(null, obj.null, "null part correct");
      browser.test.assertEq(undefined, obj.undef, "undefined part correct");
      browser.test.assertEq(undefined, obj.window, "window part correct");
      browser.test.assertEq("object", typeof obj.obj, "object part correct");
      browser.test.assertTrue(Array.isArray(obj.arr), "array part present");
      browser.test.assertEq(1, obj.arr[0], "arr[0] part correct");
      browser.test.assertEq(2, obj.arr[1], "arr[1] part correct");
      browser.test.assertEq(2, obj.arr.length, "arr.length part correct");
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
    } else if (msg === "test-session") {
      promise = runTests("session");
    }
    promise.then(() => browser.test.sendMessage("test-finished"));
  });

  browser.test.sendMessage("ready");
}

async function test_contentscript_storage(storageType) {
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
          run_at: "document_idle",
        },
      ],

      permissions: ["storage"],
    },

    files: {
      "content_script.js": `(${testStorageContentScript})(${checkGetImpl})`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage(`test-${storageType}`);
  await extension.awaitMessage("test-finished");

  await extension.unload();
  await contentPage.close();
}

async function test_storage_empty_events(areaName) {
  async function background(areaName) {
    let eventCount = 0;

    browser.storage[areaName].onChanged.addListener(changes => {
      browser.test.sendMessage("onChanged", [++eventCount, changes]);
    });

    browser.test.onMessage.addListener(async (method, arg) => {
      let result = await browser.storage[areaName][method](arg);
      browser.test.sendMessage("result", result);
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["storage"] },
    background: `(${background})("${areaName}")`,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  async function callStorageMethod(method, arg) {
    info(`call storage.${areaName}.${method}(${JSON.stringify(arg) ?? ""})`);
    extension.sendMessage(method, arg);
    await extension.awaitMessage("result");
  }

  async function expectEvent(expectCount, expectChanges) {
    equal(
      JSON.stringify([expectCount, expectChanges]),
      JSON.stringify(await extension.awaitMessage("onChanged")),
      "Correct onChanged events count and data in the last changes notified."
    );
  }

  await callStorageMethod("set", { alpha: 1 });
  await expectEvent(1, { alpha: { newValue: 1 } });

  await callStorageMethod("set", {});
  // Setting nothing doesn't trigger onChanged event.

  await callStorageMethod("set", { beta: 12 });
  await expectEvent(2, { beta: { newValue: 12 } });

  await callStorageMethod("remove", "alpha");
  await expectEvent(3, { alpha: { oldValue: 1 } });

  await callStorageMethod("remove", "alpha");
  // Trying to remove alpha again doesn't trigger onChanged.

  await callStorageMethod("clear");
  await expectEvent(4, { beta: { oldValue: 12 } });

  await callStorageMethod("clear");
  // Clear again wothout onChanged. Test will fail on unexpected event/message.

  await extension.unload();
}

async function test_storage_change_event_page(areaName) {
  async function testOnChanged(targetIsStorageArea) {
    function backgroundTestStorageTopNamespace(areaName) {
      browser.storage.onChanged.addListener((changes, area) => {
        browser.test.assertEq(area, areaName, "Expected areaName");
        browser.test.assertEq(
          JSON.stringify(changes),
          `{"storageKey":{"newValue":"newStorageValue"}}`,
          "Expected changes"
        );
        browser.test.sendMessage("onChanged_was_fired");
      });
    }
    function backgroundTestStorageAreaNamespace(areaName) {
      browser.storage[areaName].onChanged.addListener((changes, ...args) => {
        browser.test.assertEq(args.length, 0, "no more args after changes");
        browser.test.assertEq(
          JSON.stringify(changes),
          `{"storageKey":{"newValue":"newStorageValue"}}`,
          `Expected changes via ${areaName}.onChanged event`
        );
        browser.test.sendMessage("onChanged_was_fired");
      });
    }
    let background, onChangedName;
    if (targetIsStorageArea) {
      // Test storage.local.onChanged / storage.sync.onChanged.
      background = backgroundTestStorageAreaNamespace;
      onChangedName = `${areaName}.onChanged`;
    } else {
      background = backgroundTestStorageTopNamespace;
      onChangedName = "onChanged";
    }
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["storage"],
        background: { persistent: false },
      },
      background: `(${background})("${areaName}")`,
      files: {
        "trigger-change.html": `
          <!DOCTYPE html><meta charset="utf-8">
          <script src="trigger-change.js"></script>
        `,
        "trigger-change.js": async () => {
          let areaName = location.search.slice(1);
          await browser.storage[areaName].set({
            storageKey: "newStorageValue",
          });
          browser.test.sendMessage("tried_to_trigger_change");
        },
      },
    });
    await extension.startup();
    assertPersistentListeners(extension, "storage", onChangedName, {
      primed: false,
    });

    await extension.terminateBackground();
    assertPersistentListeners(extension, "storage", onChangedName, {
      primed: true,
    });

    // Now trigger the event
    let contentPage = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}/trigger-change.html?${areaName}`
    );
    await extension.awaitMessage("tried_to_trigger_change");
    await contentPage.close();
    await extension.awaitMessage("onChanged_was_fired");

    assertPersistentListeners(extension, "storage", onChangedName, {
      primed: false,
    });
    await extension.unload();
  }

  async function testFn() {
    // Test browser.storage.onChanged.addListener
    await testOnChanged(/* targetIsStorageArea */ false);
    // Test browser.storage.local.onChanged.addListener
    // and browser.storage.sync.onChanged.addListener, depending on areaName.
    await testOnChanged(/* targetIsStorageArea */ true);
  }

  return runWithPrefs([["extensions.eventPages.enabled", true]], testFn);
}
