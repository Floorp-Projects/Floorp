/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNR: "resource://gre/modules/ExtensionDNR.sys.mjs",
  ExtensionDNRLimits: "resource://gre/modules/ExtensionDNRLimits.sys.mjs",
  ExtensionDNRStore: "resource://gre/modules/ExtensionDNRStore.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_dnr.js")).spec,
  this
);

const { promiseStartupManager, promiseRestartManager } = AddonTestUtils;

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);

  await promiseStartupManager();
});

// This function is serialized and called in the context of the test extension's
// background page. dnrTestUtils is passed to the background function.
function makeDnrTestUtils() {
  const dnrTestUtils = {};
  const dnr = browser.declarativeNetRequest;

  function serializeForLog(data) {
    // JSON-stringify, but drop null values (replacing them with undefined
    // causes JSON.stringify to drop them), so that optional keys with the null
    // values are hidden.
    let str = JSON.stringify(data, rep => rep ?? undefined);
    return str;
  }

  async function testInvalidRule(rule, expectedError, isSchemaError) {
    if (isSchemaError) {
      // Schema validation error = thrown error instead of a rejection.
      browser.test.assertThrows(
        () => dnr.updateDynamicRules({ addRules: [rule] }),
        expectedError,
        `Rule should be invalid (schema-validated): ${serializeForLog(rule)}`
      );
    } else {
      await browser.test.assertRejects(
        dnr.updateDynamicRules({ addRules: [rule] }),
        expectedError,
        `Rule should be invalid: ${serializeForLog(rule)}`
      );
    }
  }

  Object.assign(dnrTestUtils, {
    testInvalidRule,
    serializeForLog,
  });
  return dnrTestUtils;
}

async function runAsDNRExtension({
  background,
  unloadTestAtEnd = true,
  awaitFinish = false,
}) {
  const testExtensionParams = {
    background: `(${background})((${makeDnrTestUtils})())`,
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
      browser_specific_settings: {
        gecko: { id: "test-dynamic-rules@test-extension" },
      },
    },
  };
  const extension = ExtensionTestUtils.loadExtension(testExtensionParams);
  await extension.startup();
  if (awaitFinish) {
    await extension.awaitFinish();
  }
  if (unloadTestAtEnd) {
    await extension.unload();
  }
  return { extension, testExtensionParams };
}

function callTestMessageHandler(extension, testMessage, ...args) {
  extension.sendMessage(testMessage, ...args);
  return extension.awaitMessage(`${testMessage}:done`);
}

add_task(async function test_dynamic_rule_registration() {
  await runAsDNRExtension({
    background: async () => {
      const dnr = browser.declarativeNetRequest;

      await dnr.updateDynamicRules({
        addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
      });

      const url = "https://example.com/some-dummy-url";
      const type = "font";
      browser.test.assertDeepEq(
        { matchedRules: [{ ruleId: 1, rulesetId: "_dynamic" }] },
        await dnr.testMatchOutcome({ url, type }),
        "Dynamic rule matched after registration"
      );

      await dnr.updateDynamicRules({
        removeRuleIds: [
          1,
          1234567890, // Invalid rules should be ignored.
        ],
        addRules: [{ id: 2, condition: {}, action: { type: "block" } }],
      });
      browser.test.assertDeepEq(
        { matchedRules: [{ ruleId: 2, rulesetId: "_dynamic" }] },
        await dnr.testMatchOutcome({ url, type }),
        "Dynamic rule matched after update"
      );

      await dnr.updateDynamicRules({ removeRuleIds: [2] });
      browser.test.assertDeepEq(
        { matchedRules: [] },
        await dnr.testMatchOutcome({ url, type }),
        "Dynamic rule not matched after unregistration"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_dynamic_rules_count_limits() {
  await runAsDNRExtension({
    unloadTestAtEnd: true,
    awaitFinish: true,
    background: async () => {
      const dnr = browser.declarativeNetRequest;
      const [dyamicRules, sessionRules] = await Promise.all([
        dnr.getDynamicRules(),
        dnr.getSessionRules(),
      ]);

      browser.test.assertDeepEq(
        { session: [], dynamic: [] },
        { session: sessionRules, dynamic: dyamicRules },
        "Expect no session and no dynamic rules"
      );

      const { MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES } = dnr;
      const DUMMY_RULE = {
        action: { type: "block" },
        condition: { resourceTypes: ["main_frame"] },
      };
      const rules = [];
      for (let i = 0; i < MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES; i++) {
        rules.push({ ...DUMMY_RULE, id: i + 1 });
      }

      await browser.test.assertRejects(
        dnr.updateDynamicRules({
          addRules: [
            ...rules,
            { ...DUMMY_RULE, id: MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES + 1 },
          ],
        }),
        `Number of rules in ruleset "_dynamic" exceeds MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES.`,
        "Got the expected rejection of exceeding the number of dynamic rules allowed"
      );

      await dnr.updateDynamicRules({
        addRules: rules,
      });
      browser.test.assertEq(
        5000,
        (await dnr.getDynamicRules()).length,
        "Got the expected number of dynamic rules stored"
      );

      await dnr.updateDynamicRules({
        removeRuleIds: rules.map(r => r.id),
      });

      browser.test.assertEq(
        0,
        (await dnr.getDynamicRules()).length,
        "All dynamic rules should have been removed"
      );

      browser.test.log(
        "Verify rules count limits with multiple async API calls"
      );

      const [
        updateDynamicRulesSingle,
        updateDynamicRulesTooMany,
      ] = await Promise.allSettled([
        dnr.updateDynamicRules({
          addRules: [
            { ...DUMMY_RULE, id: MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES + 1 },
          ],
        }),
        dnr.updateDynamicRules({ addRules: rules }),
      ]);

      browser.test.assertDeepEq(
        updateDynamicRulesSingle,
        { status: "fulfilled", value: undefined },
        "Expect the first updateDynamicRules call to be successful"
      );

      await browser.test.assertRejects(
        updateDynamicRulesTooMany?.status === "rejected"
          ? Promise.reject(updateDynamicRulesTooMany.reason)
          : Promise.resolve(),
        `Number of rules in ruleset "_dynamic" exceeds MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES.`,
        "Got the expected rejection on the second call exceeding the number of dynamic rules allowed"
      );

      browser.test.assertDeepEq(
        (await dnr.getDynamicRules()).map(rule => rule.id),
        [MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES + 1],
        "Got the expected dynamic rules"
      );

      await dnr.updateDynamicRules({
        removeRuleIds: [MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES + 1],
      });

      const [
        updateSessionResult,
        updateDynamicResult,
      ] = await Promise.allSettled([
        dnr.updateSessionRules({ addRules: rules }),
        dnr.updateDynamicRules({ addRules: rules }),
      ]);

      browser.test.assertDeepEq(
        updateDynamicResult,
        { status: "fulfilled", value: undefined },
        "Expect the number of dynamic rules to be still allowed, despite the session rule added"
      );

      // NOTE: In this test we do not exceed the quota of session rules. The
      // updateSessionRules call here is to verify that the quota of session and
      // dynamic rules are separate. The limits for session rules are tested
      // by session_rules_total_rule_limit in test_ext_dnr_session_rules.js.
      browser.test.assertDeepEq(
        updateSessionResult,
        { status: "fulfilled", value: undefined },
        "Got expected success from the updateSessionRules request"
      );

      browser.test.assertDeepEq(
        { sessionRulesCount: 5000, dynamicRulesCount: 5000 },
        {
          sessionRulesCount: (await dnr.getSessionRules()).length,
          dynamicRulesCount: (await dnr.getDynamicRules()).length,
        },
        "Got expected session and dynamic rules counts"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_stored_dynamic_rules_exceeding_limits() {
  const { extension } = await runAsDNRExtension({
    unloadTestAtEnd: false,
    awaitFinish: false,
    background: async () => {
      const dnr = browser.declarativeNetRequest;

      browser.test.onMessage.addListener(async (msg, ...args) => {
        switch (msg) {
          case "createDynamicRules": {
            const [{ updateRuleOptions }] = args;
            await dnr.updateDynamicRules(updateRuleOptions);
            break;
          }
          case "assertGetDynamicRulesCount": {
            const [{ expectedRulesCount }] = args;
            browser.test.assertEq(
              expectedRulesCount,
              (await dnr.getDynamicRules()).length,
              "getDynamicRules() resolves to the expected number of dynamic rules"
            );
            break;
          }
          default:
            browser.test.fail(
              `Got unexpected unhandled test message: "${msg}"`
            );
            break;
        }
        browser.test.sendMessage(`${msg}:done`);
      });
      browser.test.sendMessage("bgpage:ready");
    },
  });

  const initialRules = [getDNRRule({ id: 1 })];
  await extension.awaitMessage("bgpage:ready");
  await callTestMessageHandler(extension, "createDynamicRules", {
    updateRuleOptions: { addRules: initialRules },
  });
  await callTestMessageHandler(extension, "assertGetDynamicRulesCount", {
    expectedRulesCount: 1,
  });

  const extUUID = extension.uuid;
  const dnrStore = ExtensionDNRStore._getStoreForTesting();
  await dnrStore._savePromises.get(extUUID);
  const { storeFile } = dnrStore.getFilePaths(extUUID);

  await extension.addon.disable();

  ok(
    !dnrStore._dataPromises.has(extUUID),
    "DNR store read data promise cleared after the extension has been disabled"
  );
  ok(
    !dnrStore._data.has(extUUID),
    "DNR store data cleared from memory after the extension has been disabled"
  );

  ok(await IOUtils.exists(storeFile), `DNR storeFile ${storeFile} found`);
  const dnrDataFromFile = await IOUtils.readJSON(storeFile, {
    decompress: true,
  });

  const { MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES } = ExtensionDNRLimits;

  const expectedDynamicRules = [];
  const unexpectedDynamicRules = [];

  for (let i = 0; i < MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES + 5; i++) {
    const rule = getDNRRule({ id: i + 1 });
    if (i < MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES) {
      expectedDynamicRules.push(rule);
    } else {
      unexpectedDynamicRules.push(rule);
    }
  }

  const tooManyDynamicRules = [
    ...expectedDynamicRules,
    ...unexpectedDynamicRules,
  ];

  const dnrDataNew = {
    schemaVersion: dnrDataFromFile.schemaVersion,
    extVersion: extension.extension.version,
    staticRulesets: [],
    dynamicRuleset: getSchemaNormalizedRules(extension, tooManyDynamicRules),
  };

  await IOUtils.writeJSON(storeFile, dnrDataNew, { compress: true });

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    await extension.addon.enable();
    await extension.awaitMessage("bgpage:ready");
  });

  await callTestMessageHandler(extension, "assertGetDynamicRulesCount", {
    expectedRulesCount: 0,
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      {
        message: new RegExp(
          `Ignoring dynamic ruleset in extension "${extension.id}" because: Number of rules in ruleset "_dynamic" exceeds MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES`
        ),
      },
    ],
  });

  await extension.unload();
});

add_task(async function test_save_and_load_dynamic_rules() {
  let { extension, testExtensionParams } = await runAsDNRExtension({
    unloadTestAtEnd: false,
    awaitFinish: false,
    background: async dnrTestUtils => {
      const dnr = browser.declarativeNetRequest;

      browser.test.onMessage.addListener(async (msg, ...args) => {
        switch (msg) {
          case "assertGetDynamicRules": {
            const [{ expectedRules }] = args;
            browser.test.assertDeepEq(
              expectedRules,
              await dnr.getDynamicRules(),
              "getDynamicRules() resolves to the expected dynamic rules"
            );
            break;
          }
          case "testUpdateDynamicRules": {
            const [{ updateRulesRequests, expectedRules }] = args;
            const promiseResults = await Promise.allSettled(
              updateRulesRequests.map(updateRuleOptions =>
                dnr.updateDynamicRules(updateRuleOptions)
              )
            );

            // All calls should have been resolved successfully.
            for (const [i, request] of updateRulesRequests.entries()) {
              browser.test.assertDeepEq(
                { status: "fulfilled", value: undefined },
                promiseResults[i],
                `Expect resolved updateDynamicRules request for ${dnrTestUtils.serializeForLog(
                  request
                )}`
              );
            }

            browser.test.assertDeepEq(
              expectedRules,
              await dnr.getDynamicRules(),
              "getDynamicRules resolves to the expected updated dynamic rules"
            );
            break;
          }
          case "testInvalidDynamicAddRule": {
            const [
              { rule, expectedError, isSchemaError, isErrorRegExp },
            ] = args;
            await dnrTestUtils.testInvalidRule(
              rule,
              expectedError,
              isSchemaError,
              isErrorRegExp
            );
            break;
          }
          default:
            browser.test.fail(
              `Got unexpected unhandled test message: "${msg}"`
            );
            break;
        }

        browser.test.sendMessage(`${msg}:done`);
      });

      browser.test.sendMessage("bgpage:ready");
    },
  });

  await extension.awaitMessage("bgpage:ready");
  await callTestMessageHandler(extension, "assertGetDynamicRules", {
    expectedRules: [],
  });

  const rules = [
    getDNRRule({
      id: 1,
      action: { type: "allow" },
      condition: { resourceTypes: ["main_frame"] },
    }),
    getDNRRule({
      id: 2,
      action: { type: "block" },
      condition: { resourceTypes: ["main_frame", "script"] },
    }),
  ];

  info("Verify updateDynamicRules adding new valid rules");
  // Send two concurrent API requests, the first one adds 3 rules and the second
  // one removing a rule defined in the first call, the result of the combined
  // API calls is expected to only store 2 dynamic rules in the DNR store.
  await callTestMessageHandler(extension, "testUpdateDynamicRules", {
    updateRulesRequests: [
      { addRules: [...rules, getDNRRule({ id: 3 })] },
      { removeRuleIds: [3] },
    ],
    expectedRules: getSchemaNormalizedRules(extension, rules),
  });

  const extUUID = extension.uuid;
  const dnrStore = ExtensionDNRStore._getStoreForTesting();
  await dnrStore._savePromises.get(extUUID);
  const { storeFile } = dnrStore.getFilePaths(extUUID);
  const dnrDataFromFile = await IOUtils.readJSON(storeFile, {
    decompress: true,
  });

  Assert.deepEqual(
    dnrDataFromFile.dynamicRuleset,
    getSchemaNormalizedRules(extension, rules),
    "Got the expected rules stored on disk"
  );

  info("Verify updateDynamicRules rejects on new invalid rules");
  await callTestMessageHandler(extension, "testInvalidDynamicAddRule", {
    rule: rules[0],
    expectedError: "Duplicate rule ID: 1",
    isSchemaError: false,
  });

  await callTestMessageHandler(extension, "testInvalidDynamicAddRule", {
    rule: getDNRRule({ action: { type: "invalid-action" } }),
    expectedError: /addRules.0.action.type: Invalid enumeration value "invalid-action"/,
    isSchemaError: true,
  });

  info("Expect dynamic rules to not have been changed");
  await callTestMessageHandler(extension, "assertGetDynamicRules", {
    expectedRules: getSchemaNormalizedRules(extension, rules),
  });

  Assert.deepEqual(
    dnrStore._data.get(extUUID).dynamicRuleset,
    getSchemaNormalizedRules(extension, rules),
    "Got the expected dynamic rules in the DNR store"
  );

  info("Verify dynamic rules loaded back from disk on addon restart");
  ok(await IOUtils.exists(storeFile), `DNR storeFile ${storeFile} found`);

  // force deleting the data stored in memory to confirm if it being loaded again from
  // the files stored on disk.
  dnrStore._data.delete(extUUID);
  dnrStore._dataPromises.delete(extUUID);

  const { addon } = extension;
  await addon.disable();

  ok(
    !dnrStore._dataPromises.has(extUUID),
    "DNR store read data promise cleared after the extension has been disabled"
  );
  ok(
    !dnrStore._data.has(extUUID),
    "DNR store data cleared from memory after the extension has been disabled"
  );

  await addon.enable();
  await extension.awaitMessage("bgpage:ready");

  info("Expect dynamic rules to have been loaded back");
  await callTestMessageHandler(extension, "assertGetDynamicRules", {
    expectedRules: getSchemaNormalizedRules(extension, rules),
  });

  Assert.deepEqual(
    dnrStore._data.get(extUUID).dynamicRuleset,
    getSchemaNormalizedRules(extension, rules),
    "Got the expected dynamic rules loaded back from the DNR store after addon restart"
  );

  info("Verify dynamic rules loaded back as expected on AOM restart");
  dnrStore._data.delete(extUUID);
  dnrStore._dataPromises.delete(extUUID);

  // NOTE: promiseRestartManager will not be enough to make sure the
  // DNR store data for the test extension is going to be loaded from
  // the DNR startup cache file.
  // See test_ext_dnr_startup_cache.js for a test case that more completely
  // simulates ExtensionDNRStore initialization on browser restart.
  await promiseRestartManager();

  await extension.awaitStartup();
  await extension.awaitMessage("bgpage:ready");

  await callTestMessageHandler(extension, "assertGetDynamicRules", {
    expectedRules: getSchemaNormalizedRules(extension, rules),
  });

  Assert.deepEqual(
    dnrStore._data.get(extUUID).dynamicRuleset,
    getSchemaNormalizedRules(extension, rules),
    "Got the expected dynamic rules loaded back from the DNR store after AOM restart"
  );

  info(
    "Verify updateDynamicRules adding new valid rules and remove one of the existing"
  );
  // Expect the first rule to be removed and a new one being added.
  const newRule3 = getDNRRule({
    id: 3,
    action: { type: "allow" },
    condition: { resourceTypes: ["main_frame"] },
  });
  const updatedRules = [rules[1], newRule3];

  await callTestMessageHandler(extension, "testUpdateDynamicRules", {
    updateRulesRequests: [{ addRules: [newRule3], removeRuleIds: [1] }],
    expectedRules: getSchemaNormalizedRules(extension, updatedRules),
  });

  info("Verify dynamic rules preserved across addon updates");

  const staticRules = [
    getDNRRule({
      id: 4,
      action: { type: "block" },
      condition: { resourceTypes: ["xmlhttprequest"] },
    }),
  ];
  await extension.upgrade({
    ...testExtensionParams,
    manifest: {
      ...testExtensionParams.manifest,
      version: "2.0",
      declarative_net_request: {
        rule_resources: [
          {
            id: "ruleset_1",
            enabled: true,
            path: "ruleset_1.json",
          },
        ],
      },
    },
    files: { "ruleset_1.json": JSON.stringify(staticRules) },
  });
  await extension.awaitMessage("bgpage:ready");

  await callTestMessageHandler(extension, "assertGetDynamicRules", {
    expectedRules: getSchemaNormalizedRules(extension, updatedRules),
  });

  info(
    "Verify static rules included in the new addon version have been loaded"
  );

  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, staticRules),
  });

  info("Verify rules after extension downgrade");
  await extension.upgrade({
    ...testExtensionParams,
    manifest: {
      ...testExtensionParams.manifest,
      version: "1.0",
    },
  });
  await extension.awaitMessage("bgpage:ready");

  info("Verify stored dynamic rules are unchanged");

  await callTestMessageHandler(extension, "assertGetDynamicRules", {
    expectedRules: getSchemaNormalizedRules(extension, updatedRules),
  });

  info(
    "Verify static rules included in the new addon version are cleared on downgrade to previous version"
  );
  await assertDNRStoreData(dnrStore, extension, {});

  info("Verify rules after extension upgrade to one without DNR permissions");
  await extension.upgrade({
    ...testExtensionParams,
    manifest: {
      ...testExtensionParams.manifest,
      permissions: [],
      version: "1.1",
    },
    background: async () => {
      browser.test.assertEq(
        browser.declarativeNetRequest,
        undefined,
        "Expect DNR API namespace to not be available"
      );
      browser.test.sendMessage("bgpage:ready");
    },
  });
  await extension.awaitMessage("bgpage:ready");
  ok(
    !dnrStore._dataPromises.has(extension.uuid),
    "Expect dnrStore to not have any promise for the extension DNR data being loaded"
  );
  ok(
    !ExtensionDNR.getRuleManager(
      extension.extension,
      false /* createIfMissing */
    ),
    "Expect no ruleManager found for the extenson"
  );

  info(
    "Verify rules are loaded back after upgrading again to one with DNR permissions"
  );
  await extension.upgrade({
    ...testExtensionParams,
    manifest: {
      ...testExtensionParams.manifest,
      version: "1.2",
    },
  });
  await extension.awaitMessage("bgpage:ready");

  await callTestMessageHandler(extension, "assertGetDynamicRules", {
    expectedRules: getSchemaNormalizedRules(extension, updatedRules),
  });

  let ruleManager = ExtensionDNR.getRuleManager(
    extension.extension,
    /* createIfMissing= */ false
  );
  Assert.ok(ruleManager, "Rule manager exists before unload");
  Assert.deepEqual(
    ruleManager.getDynamicRules(),
    getSchemaNormalizedRules(extension, updatedRules),
    "Found the expected dynamic rules in the Rule manager"
  );
  await extension.addon.disable();
  Assert.ok(
    !ExtensionDNR.getRuleManager(
      extension.extension,
      /* createIfMissing= */ false
    ),
    "Rule manager erased after unload"
  );

  await extension.addon.enable();
  await extension.awaitMessage("bgpage:ready");

  info("Verify dynamic rules updates after corrupted storage");

  async function testLoadedRulesAfterDataCorruption({
    name,
    asyncWriteStoreFile,
    expectedCorruptFile,
  }) {
    info(`Tempering DNR store data: ${name}`);

    await extension.addon.disable();

    ok(
      !dnrStore._dataPromises.has(extUUID),
      "DNR store read data promise cleared after the extension has been disabled"
    );
    ok(
      !dnrStore._data.has(extUUID),
      "DNR store data cleared from memory after the extension has been disabled"
    );

    await asyncWriteStoreFile();

    await extension.addon.enable();
    await extension.awaitMessage("bgpage:ready");

    await TestUtils.waitForCondition(
      () => IOUtils.exists(`${expectedCorruptFile}`),
      `Wait for the "${expectedCorruptFile}" file to have been created`
    );

    ok(
      !(await IOUtils.exists(storeFile)),
      "Corrupted store file expected to be removed"
    );

    await callTestMessageHandler(extension, "assertGetDynamicRules", {
      expectedRules: [],
    });

    const newRules = [getDNRRule({ id: 3 })];
    const expectedRules = getSchemaNormalizedRules(extension, newRules);
    await callTestMessageHandler(extension, "testUpdateDynamicRules", {
      updateRulesRequests: [{ addRules: newRules }],
      expectedRules,
    });

    await TestUtils.waitForCondition(
      () => IOUtils.exists(storeFile),
      `Wait for the "${storeFile}" file to have been created`
    );

    const newData = await IOUtils.readJSON(storeFile, { decompress: true });
    Assert.deepEqual(
      newData.dynamicRuleset,
      expectedRules,
      "Expect the new rules to have been stored on disk"
    );
  }

  await testLoadedRulesAfterDataCorruption({
    name: "invalid lz4 header",
    asyncWriteStoreFile: () =>
      IOUtils.writeUTF8(storeFile, "not an lz4 compressed file", {
        compress: false,
      }),
    expectedCorruptFile: `${storeFile}.corrupt`,
  });

  await testLoadedRulesAfterDataCorruption({
    name: "invalid json data",
    asyncWriteStoreFile: () =>
      IOUtils.writeUTF8(storeFile, "invalid json data", { compress: true }),
    expectedCorruptFile: `${storeFile}-1.corrupt`,
  });

  await testLoadedRulesAfterDataCorruption({
    name: "empty json data",
    asyncWriteStoreFile: () =>
      IOUtils.writeUTF8(storeFile, "{}", { compress: true }),
    expectedCorruptFile: `${storeFile}-2.corrupt`,
  });

  await testLoadedRulesAfterDataCorruption({
    name: "invalid staticRulesets property type",
    asyncWriteStoreFile: () =>
      IOUtils.writeUTF8(
        storeFile,
        JSON.stringify({
          schemaVersion: dnrDataFromFile.schemaVersion,
          extVersion: extension.extension.version,
          staticRulesets: "Not an array",
        }),
        { compress: true }
      ),
    expectedCorruptFile: `${storeFile}-3.corrupt`,
  });

  await testLoadedRulesAfterDataCorruption({
    name: "invalid dynamicRuleset property type",
    asyncWriteStoreFile: () =>
      IOUtils.writeUTF8(
        storeFile,
        JSON.stringify({
          schemaVersion: dnrDataFromFile.schemaVersion,
          extVersion: extension.extension.version,
          staticRulesets: [],
          dynamicRuleset: "Not an array",
        }),
        { compress: true }
      ),
    expectedCorruptFile: `${storeFile}-4.corrupt`,
  });

  await extension.unload();
});

add_task(async function test_tabId_conditions_invalid_in_dynamic_rules() {
  await runAsDNRExtension({
    unloadTestAtEnd: true,
    awaitFinish: true,
    background: async dnrTestUtils => {
      await dnrTestUtils.testInvalidRule(
        { id: 1, action: { type: "block" }, condition: { tabIds: [1] } },
        "tabIds and excludedTabIds can only be specified in session rules"
      );
      await dnrTestUtils.testInvalidRule(
        {
          id: 1,
          action: { type: "block" },
          condition: { excludedTabIds: [1] },
        },
        "tabIds and excludedTabIds can only be specified in session rules"
      );
      browser.test.assertDeepEq(
        [],
        await browser.declarativeNetRequest.getDynamicRules(),
        "Expect the invalid rules to not be enabled"
      );
      browser.test.notifyPass();
    },
  });
});
