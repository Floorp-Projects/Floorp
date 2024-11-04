/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNR: "resource://gre/modules/ExtensionDNR.sys.mjs",
  ExtensionDNRStore: "resource://gre/modules/ExtensionDNRStore.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_dnr.js")).spec,
  this
);
Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("head_dnr_static_rules.js")).spec,
  this
);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("response from server");
});

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);

  setupTelemetryForTests();

  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_load_static_rules() {
  const ruleInRuleset1 = getDNRRule({
    action: { type: "allow" },
    condition: { resourceTypes: ["main_frame"] },
  });
  const ruleset1Data = [ruleInRuleset1];
  const ruleset2Data = [
    getDNRRule({
      action: { type: "block" },
      condition: { resourceTypes: ["main_frame", "script"] },
    }),
  ];

  const rule_resources = [
    {
      id: "ruleset_1",
      enabled: true,
      path: "ruleset_1.json",
    },
    {
      id: "ruleset_2",
      enabled: true,
      path: "ruleset_2.json",
    },
    {
      id: "ruleset_3",
      enabled: false,
      path: "ruleset_3.json",
    },
  ];
  const files = {
    // Missing ruleset_3.json on purpose.
    "ruleset_1.json": JSON.stringify([
      {
        ...ruleInRuleset1,
        // Unrecognized props should be allowed.  We add the here instead of
        // above because unrecognized properties are discarded after
        // normalization, and the assertions do not expect any unrecognized
        // props in the resf of the test.
        condition: {
          ...ruleInRuleset1.condition,
          nested_unexpected_prop: true,
        },
        unexpected_prop: true,
      },
    ]),
    "ruleset_2.json": JSON.stringify(ruleset2Data),
  };

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({ rule_resources, files })
  );

  await extension.startup();

  const extUUID = extension.uuid;

  await extension.awaitMessage("bgpage:ready");

  const dnrStore = ExtensionDNRStore._getStoreForTesting();

  info("Verify DNRStore data for the test extension");
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1", "ruleset_2"]);

  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, ruleset1Data),
    ruleset_2: getSchemaNormalizedRules(extension, ruleset2Data),
  });

  info("Verify matched rules using testMatchOutcome");
  const testRequestMainFrame = {
    url: "https://example.com/some-dummy-url",
    type: "main_frame",
  };
  const testRequestScript = {
    url: "https://example.com/some-dummy-url.js",
    type: "script",
  };

  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: testRequestMainFrame,
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset_1" }],
      },
    },
    "Expect ruleset_1 to be matched on the main-frame test request"
  );
  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: testRequestScript,
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset_2" }],
      },
    },
    "Expect ruleset_2 to be matched on the script test request"
  );

  info("Verify DNRStore data persisted on disk for the test extension");
  // The data will not be stored on disk until something is being changed
  // from what was already available in the manifest and so in this
  // test we save manually (a test for the updateEnabledRulesets will
  // take care of asserting that the data has been stored automatically
  // on disk when it is meant to).
  await dnrStore.save(extension.extension);

  const { storeFile } = dnrStore.getFilePaths(extUUID);

  ok(await IOUtils.exists(storeFile), `DNR storeFile ${storeFile} found`);

  // force deleting the data stored in memory to confirm if it being loaded again from
  // the files stored on disk.
  dnrStore._data.delete(extUUID);
  dnrStore._dataPromises.delete(extUUID);

  info("Verify the expected DNRStore data persisted on disk is loaded back");
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs"
  );
  const addon = await AddonManager.getAddonByID(extension.id);
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
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1", "ruleset_2"]);

  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, ruleset1Data),
    ruleset_2: getSchemaNormalizedRules(extension, ruleset2Data),
  });

  info("Verify matched rules using testMatchOutcome");
  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: testRequestMainFrame,
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset_1" }],
      },
    },
    "Expect ruleset_1 to be matched on the main-frame test request"
  );

  info("Verify enabled static rules updated on addon updates");
  await extension.upgrade(
    getDNRExtension({
      version: "2.0",
      rule_resources: [
        {
          id: "ruleset_1",
          enabled: false,
          path: "ruleset_1.json",
        },
        {
          id: "ruleset_2",
          enabled: true,
          path: "ruleset_2.json",
        },
      ],
      files: {
        "ruleset_2.json": JSON.stringify(ruleset2Data),
      },
    })
  );
  await extension.awaitMessage("bgpage:ready");
  await assertDNRGetEnabledRulesets(extension, ["ruleset_2"]);
  await assertDNRStoreData(dnrStore, extension, {
    ruleset_2: getSchemaNormalizedRules(extension, ruleset2Data),
  });

  info("Verify matched rules using testMatchOutcome");
  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: testRequestMainFrame,
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset_2" }],
      },
    },
    "Expect ruleset_2 to be matched on the main-frame test request"
  );

  info(
    "Verify enabled static rules updated on addon updates even if version in the manifest did not change"
  );
  await extension.upgrade(
    getDNRExtension({
      rule_resources: [
        {
          id: "ruleset_1",
          enabled: true,
          path: "ruleset_1.json",
        },
        {
          id: "ruleset_2",
          enabled: false,
          path: "ruleset_2.json",
        },
      ],
      files: {
        "ruleset_1.json": JSON.stringify(ruleset1Data),
      },
    })
  );
  await extension.awaitMessage("bgpage:ready");
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1"]);
  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, ruleset1Data),
  });

  info("Verify matched rules using testMatchOutcome");
  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: testRequestMainFrame,
      expected: {
        matchedRules: [{ ruleId: 1, rulesetId: "ruleset_1" }],
      },
    },
    "Expect ruleset_2 to be matched on the main-script test request"
  );

  info(
    "Verify updated addon version with no static rules but declarativeNetRequest permission granted"
  );
  await extension.upgrade(
    getDNRExtension({
      version: "3.0",
      rule_resources: undefined,
      files: {},
    })
  );
  await extension.awaitMessage("bgpage:ready");
  await assertDNRGetEnabledRulesets(extension, []);
  await assertDNRStoreData(dnrStore, extension, {});

  info("Verify matched rules using testMatchOutcome");
  await assertDNRTestMatchOutcome(
    {
      extension,
      testRequest: testRequestScript,
      expected: {
        matchedRules: [],
      },
    },
    "Expect no match on the script test request on test extension without no static rules"
  );

  info("Verify store file removed on addon uninstall");
  await extension.unload();

  ok(
    !dnrStore._dataPromises.has(extUUID),
    "DNR store read data promise cleared after the extension has been unloaded"
  );
  ok(
    !dnrStore._data.has(extUUID),
    "DNR store data cleared from memory after the extension has been unloaded"
  );

  ok(
    !(await IOUtils.exists(storeFile)),
    `DNR storeFile ${storeFile} removed on addon uninstalled`
  );
});

// As an optimization, the hasEnabledStaticRules flag in StartupCache is used
// to avoid unnecessarily trying to read and parse DNR rules from disk when an
// extension does knowingly not have any.
//
// 1. When rule reading was skipped, necessary internal state was not correctly
//    initialized, and consequently updateStaticRules() would reject.
// 2. The hasDynamicRules/hasEnabledStaticRules flags were not cleared upon
//    update, and consequently the flag stayed in the initial state (false),
//    and the previously stored rules did not apply after a browser restart.
//
// See also test_register_dynamic_rules_after_restart in
// test_ext_dnr_dynamic_rules.js for the similar test with dynamic rules.
add_task(async function test_enable_disabled_static_rules_after_restart() {
  // Through this test, we confirm that the underlying "expensive" rule data
  // storage is accessed when needed, and skipped when no relevant rules had
  // been detected at the previous session. Caching too much or too little in
  // StartupCache will trigger test failures in assertStoreReadsSinceLastCall.
  const sandboxStoreSpies = sinon.createSandbox();
  const dnrStore = ExtensionDNRStore._getStoreForTesting();
  const spyReadDNRStore = sandboxStoreSpies.spy(dnrStore, "_readData");

  function assertStoreReadsSinceLastCall(expectedCount, description) {
    equal(spyReadDNRStore.callCount, expectedCount, description);
    spyReadDNRStore.resetHistory();
  }

  const rule_resources = [
    {
      id: "ruleset_initially_disabled",
      enabled: false,
      path: "ruleset_initially_disabled.json",
    },
  ];

  const files = {
    "ruleset_initially_disabled.json": JSON.stringify([getDNRRule()]),
  };

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({ rule_resources, files, id: "dnr@initially-disabled" })
  );
  await extension.startup();
  await extension.awaitMessage("bgpage:ready");

  assertStoreReadsSinceLastCall(1, "Read once at initial startup");
  await AddonTestUtils.promiseRestartManager();
  // Note: ordinarily, event pages do not wake up after a browser restart,
  // unless a relevant event such as runtime.onStartup is triggered. But as
  // noted in bug 1822735, "event pages without any event listeners" will be
  // awakened after a restart, so we can expect the bgpage:ready message here:
  await extension.awaitMessage("bgpage:ready");
  assertStoreReadsSinceLastCall(
    0,
    "Read skipped due to hasEnabledStaticRules=false"
  );

  // Regression test 1: before bug 1921353 was fixed, the test got stuck here
  // because the updateStaticRules() call after a restart unexpectedly failed
  // with "can't access property "disabledStaticRuleIds", data is undefined".
  extension.sendMessage("updateStaticRules", {
    rulesetId: "ruleset_initially_disabled",
    enableRuleIds: [1234], // Does not exist, does not matter.
  });
  info("Trying to call declarativeNetRequest.updateStaticRules()...");
  Assert.deepEqual(
    await extension.awaitMessage("updateStaticRules:done"),
    [undefined],
    "updateStaticRules() succeeded without error"
  );

  // Now transition from zero rulesets to one (zero_to_one_rule).
  extension.sendMessage("updateEnabledRulesets", {
    enableRulesetIds: ["ruleset_initially_disabled"],
  });
  info("Trying to enable ruleset_initially_disabled...");
  await extension.awaitMessage("updateEnabledRulesets:done");
  await assertDNRGetEnabledRulesets(extension, ["ruleset_initially_disabled"]);

  assertStoreReadsSinceLastCall(0, "No further reads before restart");
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitMessage("bgpage:ready");

  // Regression test 2: before bug 1921353 was fixed, even with a fix to the
  // previous bug, the static rules would not be read at startup due to the
  // wrong cached hasEnabledStaticRules flag in StartupCache.
  // Verify that the static rules are still enabled.
  assertStoreReadsSinceLastCall(1, "Read due to hasEnabledStaticRules=true");
  await assertDNRGetEnabledRulesets(extension, ["ruleset_initially_disabled"]);

  // For full coverage, also verify that when all static rules are disabled,
  // that the initialization is skipped as expected.

  extension.sendMessage("updateEnabledRulesets", {
    disableRulesetIds: ["ruleset_initially_disabled"],
  });
  info("Trying to disable ruleset_initially_disabled...");
  await extension.awaitMessage("updateEnabledRulesets:done");
  await assertDNRGetEnabledRulesets(extension, []);

  await AddonTestUtils.promiseRestartManager();
  await extension.awaitMessage("bgpage:ready");
  assertStoreReadsSinceLastCall(0, "Read skipped because rules were disabled");
  await assertDNRGetEnabledRulesets(extension, []);
  // declarativeNetRequest.getEnabledStaticRulesets() queries in-memory state,
  // so we do not expect another read from disk.
  assertStoreReadsSinceLastCall(0, "Read still skipped despite API call");

  await extension.unload();

  sandboxStoreSpies.restore();
});

add_task(async function test_load_from_corrupted_data() {
  const ruleset1Data = [
    getDNRRule({
      action: { type: "allow" },
      condition: { resourceTypes: ["main_frame"] },
    }),
  ];

  const rule_resources = [
    {
      id: "ruleset_1",
      enabled: true,
      path: "ruleset_1.json",
    },
  ];

  const files = {
    "ruleset_1.json": JSON.stringify(ruleset1Data),
  };

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({ rule_resources, files })
  );

  await extension.startup();

  const extUUID = extension.uuid;

  await extension.awaitMessage("bgpage:ready");

  const dnrStore = ExtensionDNRStore._getStoreForTesting();

  info("Verify DNRStore data for the test extension");
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1"]);

  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, ruleset1Data),
  });

  info("Verify DNRStore data after loading corrupted store data");
  await dnrStore.save(extension.extension);

  const { storeFile } = dnrStore.getFilePaths(extUUID);
  ok(await IOUtils.exists(storeFile), `DNR storeFile ${storeFile} found`);

  const nonCorruptedData = await IOUtils.readJSON(storeFile, {
    decompress: true,
  });

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

    // Make sure we remove a previous corrupt file in case there is one from a previous run.
    await IOUtils.remove(expectedCorruptFile, { ignoreAbsent: true });

    await asyncWriteStoreFile();

    await extension.addon.enable();
    await extension.awaitMessage("bgpage:ready");

    info("Verify DNRStore data for the test extension");
    await assertDNRGetEnabledRulesets(extension, ["ruleset_1"]);

    await assertDNRStoreData(dnrStore, extension, {
      ruleset_1: getSchemaNormalizedRules(extension, ruleset1Data),
    });

    await TestUtils.waitForCondition(
      () => IOUtils.exists(`${expectedCorruptFile}`),
      `Wait for the "${expectedCorruptFile}" file to have been created`
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
          schemaVersion: nonCorruptedData.schemaVersion,
          extVersion: extension.extension.version,
          staticRulesets: "Not an array",
        }),
        { compress: true }
      ),
    expectedCorruptFile: `${storeFile}-3.corrupt`,
  });

  await extension.unload();
});

add_task(async function test_ruleset_validation() {
  const invalidRulesetIdCases = [
    {
      description: "empty ruleset id",
      rule_resources: [
        {
          // Invalid empty ruleset id.
          id: "",
          path: "ruleset_0.json",
          enabled: true,
        },
      ],
      expected: [
        // Validation error emitted from the manifest schema validation.
        {
          message: /rule_resources\.0\.id: String "" must match/,
        },
      ],
    },
    {
      description: "invalid ruleset id starting with '_'",
      rule_resources: [
        {
          // Invalid empty ruleset id.
          id: "_invalid_ruleset_id",
          path: "ruleset_0.json",
          enabled: true,
        },
      ],
      expected: [
        // Validation error emitted from the manifest schema validation.
        {
          message:
            /rule_resources\.0\.id: String "_invalid_ruleset_id" must match/,
        },
      ],
    },
    {
      description: "duplicated ruleset ids",
      rule_resources: [
        {
          id: "ruleset_2",
          path: "ruleset_2.json",
          enabled: true,
        },
        {
          // Duplicated ruleset id.
          id: "ruleset_2",
          path: "duplicated_ruleset_2.json",
          enabled: true,
        },
        {
          id: "ruleset_3",
          path: "ruleset_3.json",
          enabled: true,
        },
        {
          // Other duplicated ruleset id.
          id: "ruleset_3",
          path: "duplicated_ruleset_3.json",
          enabled: true,
        },
      ],
      // NOTE: this is currently a warning logged from onManifestEntry, and so it would actually
      // fail in test harness due to the manifest warning, because it is too late at that point
      // the addon is technically already starting at that point.
      expectInstallFailed: false,
      expected: [
        {
          message:
            /declarative_net_request: Static ruleset ids should be unique.*: "ruleset_2" at index 1, "ruleset_3" at index 3/,
        },
      ],
    },
    {
      description: "missing mandatory path",
      rule_resources: [
        {
          // Missing mandatory path.
          id: "ruleset_3",
          enabled: true,
        },
      ],
      expected: [
        {
          message: /rule_resources\.0: Property "path" is required/,
        },
      ],
    },
    {
      description: "missing mandatory id",
      rule_resources: [
        {
          // Missing mandatory id.
          enabled: true,
          path: "missing_ruleset_id.json",
        },
      ],
      expected: [
        {
          message: /rule_resources\.0: Property "id" is required/,
        },
      ],
    },
    {
      description: "duplicated ruleset path",
      rule_resources: [
        {
          id: "ruleset_2",
          path: "ruleset_2.json",
          enabled: true,
        },
        {
          // Duplicate path.
          id: "ruleset_3",
          path: "ruleset_2.json",
          enabled: true,
        },
      ],
      // NOTE: we couldn't get on agreement about making this a manifest validation error, apparently Chrome doesn't validate it and doesn't
      // even report any warning, and so it is logged only as an informative warning but without triggering an install failure.
      expectInstallFailed: false,
      expected: [
        {
          message:
            /declarative_net_request: Static rulesets paths are not unique.*: ".*ruleset_2.json" at index 1/,
        },
      ],
    },
    {
      description: "missing mandatory enabled",
      rule_resources: [
        {
          id: "ruleset_without_enabled",
          path: "ruleset.json",
        },
      ],
      expected: [
        {
          message: /rule_resources\.0: Property "enabled" is required/,
        },
      ],
    },
    {
      description: "allows and warns additional properties",
      declarative_net_request: {
        unexpected_prop: true,
        rule_resources: [
          {
            id: "ruleset1",
            path: "ruleset1.json",
            enabled: false,
            unexpected_prop: true,
          },
        ],
      },
      expectInstallFailed: false,
      expected: [
        {
          message:
            /declarative_net_request.unexpected_prop: An unexpected property was found/,
        },
        {
          message:
            /rule_resources.0.unexpected_prop: An unexpected property was found/,
        },
      ],
    },
    {
      description: "invalid ruleset JSON - unexpected comments",
      rule_resources: [
        {
          id: "invalid_ruleset_with_comments",
          path: "invalid_ruleset_with_comments.json",
          enabled: true,
        },
      ],
      files: {
        "invalid_ruleset_with_comments.json":
          "/* an unexpected inline comment */\n[]",
      },
      expectInstallFailed: false,
      expected: [
        {
          message:
            /Reading declarative_net_request .*invalid_ruleset_with_comments\.json: JSON.parse: unexpected character/,
        },
      ],
    },
    {
      description: "invalid ruleset JSON - empty string",
      rule_resources: [
        {
          id: "invalid_ruleset_emptystring",
          path: "invalid_ruleset_emptystring.json",
          enabled: true,
        },
      ],
      files: {
        "invalid_ruleset_emptystring.json": JSON.stringify(""),
      },
      expectInstallFailed: false,
      expected: [
        {
          message:
            /Reading declarative_net_request .*invalid_ruleset_emptystring\.json: rules file must contain an Array/,
        },
      ],
    },
    {
      description: "invalid ruleset JSON - object",
      rule_resources: [
        {
          id: "invalid_ruleset_object",
          path: "invalid_ruleset_object.json",
          enabled: true,
        },
      ],
      files: {
        "invalid_ruleset_object.json": JSON.stringify({}),
      },
      expectInstallFailed: false,
      expected: [
        {
          message:
            /Reading declarative_net_request .*invalid_ruleset_object\.json: rules file must contain an Array/,
        },
      ],
    },
    {
      description: "invalid ruleset JSON - null",
      rule_resources: [
        {
          id: "invalid_ruleset_null",
          path: "invalid_ruleset_null.json",
          enabled: true,
        },
      ],
      files: {
        "invalid_ruleset_null.json": JSON.stringify(null),
      },
      expectInstallFailed: false,
      expected: [
        {
          message:
            /Reading declarative_net_request .*invalid_ruleset_null\.json: rules file must contain an Array/,
        },
      ],
    },
  ];

  for (const {
    description,
    declarative_net_request,
    rule_resources,
    files,
    expected,
    expectInstallFailed = true,
  } of invalidRulesetIdCases) {
    info(`Test manifest validation: ${description}`);
    let extension = ExtensionTestUtils.loadExtension(
      getDNRExtension({ rule_resources, declarative_net_request, files })
    );

    const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
      ExtensionTestUtils.failOnSchemaWarnings(false);
      if (expectInstallFailed) {
        await Assert.rejects(
          extension.startup(),
          /Install failed/,
          "Expected install to fail"
        );
      } else {
        await extension.startup();
        await extension.awaitMessage("bgpage:ready");
        await extension.unload();
      }
      ExtensionTestUtils.failOnSchemaWarnings(true);
    });

    AddonTestUtils.checkMessages(messages, { expected });
  }
});

add_task(async function test_updateEnabledRuleset_id_validation() {
  const rule_resources = [
    {
      id: "ruleset_1",
      enabled: true,
      path: "ruleset_1.json",
    },
    {
      id: "ruleset_2",
      enabled: false,
      path: "ruleset_2.json",
    },
  ];

  const ruleset1Data = [
    getDNRRule({
      action: { type: "allow" },
      condition: { resourceTypes: ["main_frame"] },
    }),
  ];
  const ruleset2Data = [
    getDNRRule({
      action: { type: "block" },
      condition: { resourceTypes: ["main_frame", "script"] },
    }),
  ];

  const files = {
    "ruleset_1.json": JSON.stringify(ruleset1Data),
    "ruleset_2.json": JSON.stringify(ruleset2Data),
  };

  let extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({ rule_resources, files })
  );

  await extension.startup();
  await extension.awaitMessage("bgpage:ready");

  await assertDNRGetEnabledRulesets(extension, ["ruleset_1"]);

  const dnrStore = ExtensionDNRStore._getStoreForTesting();
  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, ruleset1Data),
  });

  const invalidStaticRulesetIds = [
    // The following two are reserved for session and dynamic rules.
    "_session",
    "_dynamic",
    "ruleset_non_existing",
  ];

  for (const invalidRSId of invalidStaticRulesetIds) {
    extension.sendMessage(
      "updateEnabledRulesets",
      // Only in rulesets to be disabled.
      { disableRulesetIds: [invalidRSId] },
      // Only in rulesets to be enabled.
      { enableRulesetIds: [invalidRSId] },
      // In both rulesets to be enabled and disabled.
      { disableRulesetIds: [invalidRSId], enableRulesetIds: [invalidRSId] },
      // Along with existing rulesets (and expected the existing rulesets
      // to stay unchanged due to the invalid ruleset ids.)
      {
        disableRulesetIds: [invalidRSId, "ruleset_1"],
        enableRulesetIds: [invalidRSId, "ruleset_2"],
      }
    );
    const [
      resInDisable,
      resInEnable,
      resInEnableAndDisable,
      resInSameRequestAsValid,
    ] = await extension.awaitMessage("updateEnabledRulesets:done");
    await Assert.rejects(
      Promise.reject(resInDisable?.rejectedWithErrorMessage),
      new RegExp(`Invalid ruleset id: "${invalidRSId}"`),
      `Got the expected rejection on invalid ruleset id "${invalidRSId}" in disableRulesetIds`
    );
    await Assert.rejects(
      Promise.reject(resInEnable?.rejectedWithErrorMessage),
      new RegExp(`Invalid ruleset id: "${invalidRSId}"`),
      `Got the expected rejection on invalid ruleset id "${invalidRSId}" in enableRulesetIds`
    );
    await Assert.rejects(
      Promise.reject(resInEnableAndDisable?.rejectedWithErrorMessage),
      new RegExp(`Invalid ruleset id: "${invalidRSId}"`),
      `Got the expected rejection on invalid ruleset id "${invalidRSId}" in both enable/disableRulesetIds`
    );
    await Assert.rejects(
      Promise.reject(resInSameRequestAsValid?.rejectedWithErrorMessage),
      new RegExp(`Invalid ruleset id: "${invalidRSId}"`),
      `Got the expected rejection on invalid ruleset id "${invalidRSId}" along with valid ruleset ids`
    );
  }

  // Confirm that the expected rulesets didn't change neither.
  await assertDNRGetEnabledRulesets(extension, ["ruleset_1"]);
  await assertDNRStoreData(dnrStore, extension, {
    ruleset_1: getSchemaNormalizedRules(extension, ruleset1Data),
  });

  // - List the same ruleset ids more than ones is expected to work and
  //   to be resulting in the same set of rules being enabled
  // - Disabling and Enabling the same ruleset id should result in the
  //   ruleset being enabled.
  await extension.sendMessage("updateEnabledRulesets", {
    disableRulesetIds: [
      "ruleset_1",
      "ruleset_1",
      "ruleset_2",
      "ruleset_2",
      "ruleset_2",
    ],
    enableRulesetIds: ["ruleset_2", "ruleset_2"],
  });
  Assert.deepEqual(
    await extension.awaitMessage("updateEnabledRulesets:done"),
    [undefined],
    "Expect the updateEnabledRulesets to result successfully"
  );

  await assertDNRGetEnabledRulesets(extension, ["ruleset_2"]);
  await assertDNRStoreData(dnrStore, extension, {
    ruleset_2: getSchemaNormalizedRules(extension, ruleset2Data),
  });

  await extension.unload();
});

add_task(async function test_tabId_conditions_invalid_in_static_rules() {
  const ruleset1_with_tabId_condition = [
    getDNRRule({ id: 1, condition: { tabIds: [1] } }),
    getDNRRule({ id: 3, condition: { urlFilter: "valid-ruleset1-rule" } }),
  ];

  const ruleset2_with_excludeTabId_condition = [
    getDNRRule({ id: 2, condition: { excludedTabIds: [1] } }),
    getDNRRule({ id: 3, condition: { urlFilter: "valid-ruleset2-rule" } }),
  ];

  const rule_resources = [
    {
      id: "ruleset1_with_tabId_condition",
      enabled: true,
      path: "ruleset1.json",
    },
    {
      id: "ruleset2_with_excludeTabId_condition",
      enabled: true,
      path: "ruleset2.json",
    },
  ];

  const files = {
    "ruleset1.json": JSON.stringify(ruleset1_with_tabId_condition),
    "ruleset2.json": JSON.stringify(ruleset2_with_excludeTabId_condition),
  };

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      id: "tabId-invalid-in-session-rules@mochitest",
      rule_resources,
      files,
    })
  );

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    ExtensionTestUtils.failOnSchemaWarnings(false);
    await extension.startup();
    ExtensionTestUtils.failOnSchemaWarnings(true);
    await extension.awaitMessage("bgpage:ready");
    await assertDNRGetEnabledRulesets(extension, [
      "ruleset1_with_tabId_condition",
      "ruleset2_with_excludeTabId_condition",
    ]);
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      {
        message:
          /"ruleset1_with_tabId_condition": tabIds and excludedTabIds can only be specified in session rules/,
      },
      {
        message:
          /"ruleset2_with_excludeTabId_condition": tabIds and excludedTabIds can only be specified in session rules/,
      },
    ],
  });

  info("Expect the invalid rule to not be enabled");
  const dnrStore = ExtensionDNRStore._getStoreForTesting();
  // Expect the two valid rules to have been loaded as expected.
  await assertDNRStoreData(dnrStore, extension, {
    ruleset1_with_tabId_condition: getSchemaNormalizedRules(extension, [
      ruleset1_with_tabId_condition[1],
    ]),
    ruleset2_with_excludeTabId_condition: getSchemaNormalizedRules(extension, [
      ruleset2_with_excludeTabId_condition[1],
    ]),
  });

  await extension.unload();
});

add_task(async function test_dnr_all_rules_disabled_allowed() {
  const ruleset1 = [
    getDNRRule({ id: 3, condition: { urlFilter: "valid-ruleset1-rule" } }),
  ];

  const rule_resources = [
    {
      id: "ruleset1",
      enabled: true,
      path: "ruleset1.json",
    },
  ];

  const files = {
    "ruleset1.json": JSON.stringify(ruleset1),
  };

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      id: "all-static-rulesets-disabled-allowed@mochitest",
      rule_resources,
      files,
    })
  );

  await extension.startup();
  await extension.awaitMessage("bgpage:ready");

  await assertDNRGetEnabledRulesets(extension, ["ruleset1"]);

  const dnrStore = ExtensionDNRStore._getStoreForTesting();
  await assertDNRStoreData(dnrStore, extension, {
    ruleset1: getSchemaNormalizedRules(extension, ruleset1),
  });

  info("Disable static ruleset1");
  extension.sendMessage("updateEnabledRulesets", {
    disableRulesetIds: ["ruleset1"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");
  await assertDNRGetEnabledRulesets(extension, []);
  await assertDNRStoreData(dnrStore, extension, {});

  info("Verify that static ruleset1 is still disable after browser restart");

  // NOTE: promiseRestartManager will not be enough to make sure the
  // DNR store data for the test extension is going to be loaded from
  // the DNR startup cache file.
  // See test_ext_dnr_startup_cache.js for a test case that more completely
  // simulates ExtensionDNRStore initialization on browser restart.
  await AddonTestUtils.promiseRestartManager();

  await extension.awaitStartup;
  await ExtensionDNR.ensureInitialized(extension.extension);
  await extension.awaitMessage("bgpage:ready");

  await assertDNRGetEnabledRulesets(extension, []);
  await assertDNRStoreData(dnrStore, extension, {});

  await extension.unload();
});

add_task(async function test_static_rules_telemetry() {
  resetTelemetryData();

  const ruleset1 = [
    getDNRRule({
      id: 1,
      action: { type: "block" },
      condition: {
        resourceTypes: ["xmlhttprequest"],
        requestDomains: ["example.com"],
      },
    }),
  ];
  const ruleset2 = [
    getDNRRule({
      id: 1,
      action: { type: "block" },
      condition: {
        resourceTypes: ["xmlhttprequest"],
        requestDomains: ["example.org"],
      },
    }),
    getDNRRule({
      id: 2,
      action: { type: "block" },
      condition: {
        resourceTypes: ["xmlhttprequest"],
        requestDomains: ["example2.org"],
      },
    }),
  ];

  const rule_resources = [
    {
      id: "ruleset1",
      enabled: false,
      path: "ruleset1.json",
    },
    {
      id: "ruleset2",
      enabled: false,
      path: "ruleset2.json",
    },
  ];

  const files = {
    "ruleset1.json": JSON.stringify(ruleset1),
    "ruleset2.json": JSON.stringify(ruleset2),
  };

  const extension = ExtensionTestUtils.loadExtension(
    getDNRExtension({
      id: "tabId-invalid-in-session-rules@mochitest",
      rule_resources,
      files,
    })
  );

  assertDNRTelemetryMetricsNoSamples(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
      },
      {
        metric: "evaluateRulesTime",
        mirroredName: "WEBEXT_DNR_EVALUATE_RULES_MS",
        mirroredType: "histogram",
      },
    ],
    "before test extension have been loaded"
  );

  await extension.startup();
  await extension.awaitMessage("bgpage:ready");

  await assertDNRGetEnabledRulesets(extension, []);

  assertDNRTelemetryMetricsNoSamples(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
      },
    ],
    "after test extension loaded with all static rulesets disabled"
  );

  info("Enable static ruleset1");
  extension.sendMessage("updateEnabledRulesets", {
    enableRulesetIds: ["ruleset1"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");

  await assertDNRGetEnabledRulesets(extension, ["ruleset1"]);

  // Expect one sample after enabling ruleset1.
  let expectedValidateRulesTimeSamples = 1;
  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: expectedValidateRulesTimeSamples,
      },
    ],
    "after enabling static rulesets1"
  );

  info("Enable static ruleset2");
  extension.sendMessage("updateEnabledRulesets", {
    enableRulesetIds: ["ruleset2"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");

  await assertDNRGetEnabledRulesets(extension, ["ruleset1", "ruleset2"]);

  // Expect one new sample after enabling ruleset2.
  expectedValidateRulesTimeSamples += 1;
  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: expectedValidateRulesTimeSamples,
      },
    ],
    "after enabling static rulesets2"
  );

  await extension.addon.disable();

  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: expectedValidateRulesTimeSamples,
      },
    ],
    "no new samples expected after disabling test extension"
  );

  await extension.addon.enable();
  await extension.awaitMessage("bgpage:ready");
  await ExtensionDNR.ensureInitialized(extension.extension);

  // Expect 2 new samples after re-enabling the addon with
  // the 2 rulesets enabled being loaded from the DNR store file.
  expectedValidateRulesTimeSamples += 2;
  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: expectedValidateRulesTimeSamples,
      },
    ],
    "after re-enabling test extension"
  );

  info("Disable static ruleset1");

  extension.sendMessage("updateEnabledRulesets", {
    disableRulesetIds: ["ruleset1"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");

  await assertDNRGetEnabledRulesets(extension, ["ruleset2"]);

  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "validateRulesTime",
        mirroredName: "WEBEXT_DNR_VALIDATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: expectedValidateRulesTimeSamples,
      },
    ],
    "no new validation should be hit after disabling ruleset1"
  );

  info("Verify telemetry recorded on rules evaluation");
  extension.sendMessage("updateEnabledRulesets", {
    enableRulesetIds: ["ruleset1"],
    disableRulesetIds: ["ruleset2"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");
  await assertDNRGetEnabledRulesets(extension, ["ruleset1"]);

  assertDNRTelemetryMetricsNoSamples(
    [
      {
        metric: "evaluateRulesTime",
        mirroredName: "WEBEXT_DNR_EVALUATE_RULES_MS",
        mirroredType: "histogram",
      },
      {
        metric: "evaluateRulesCountMax",
        mirroredName: "extensions.apis.dnr.evaluate_rules_count_max",
        mirroredType: "scalar",
      },
    ],
    "before any request have been intercepted"
  );

  Assert.equal(
    await fetch("http://example.com/").then(res => res.text()),
    "response from server",
    "DNR should not block system requests"
  );

  assertDNRTelemetryMetricsNoSamples(
    [
      {
        metric: "evaluateRulesTime",
        mirroredName: "WEBEXT_DNR_EVALUATE_RULES_MS",
        mirroredType: "histogram",
      },
      {
        metric: "evaluateRulesCountMax",
        mirroredName: "extensions.apis.dnr.evaluate_rules_count_max",
        mirroredType: "scalar",
      },
    ],
    "after restricted request have been intercepted (but no rules evaluated)"
  );

  const page = await ExtensionTestUtils.loadContentPage("http://example.com");
  const callPageFetch = async () => {
    Assert.equal(
      await page.spawn([], () => {
        return this.content.fetch("http://example.com/").then(
          res => res.text(),
          err => err.message
        );
      }),
      "NetworkError when attempting to fetch resource.",
      "DNR should have blocked test request to example.com"
    );
  };

  // Expect one sample recorded on evaluating rules for the
  // top level navigation.
  let expectedEvaluateRulesTimeSamples = 1;
  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "evaluateRulesTime",
        mirroredName: "WEBEXT_DNR_EVALUATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: expectedEvaluateRulesTimeSamples,
      },
    ],
    "evaluateRulesTime should be collected after evaluated rulesets"
  );
  // Expect same number of rules included in the single ruleset
  // currently enabled.
  let expectedEvaluateRulesCountMax = ruleset1.length;
  assertDNRTelemetryMetricsGetValueEq(
    [
      {
        metric: "evaluateRulesCountMax",
        mirroredName: "extensions.apis.dnr.evaluate_rules_count_max",
        mirroredType: "scalar",
        expectedGetValue: expectedEvaluateRulesCountMax,
      },
    ],
    "evaluateRulesCountMax should be collected after evaluated rulesets1"
  );

  await callPageFetch();

  // Expect one new sample reported on evaluating rules for the
  // first fetch request originated from the test page.
  expectedEvaluateRulesTimeSamples += 1;
  assertDNRTelemetryMetricsSamplesCount(
    [
      {
        metric: "evaluateRulesTime",
        mirroredName: "WEBEXT_DNR_EVALUATE_RULES_MS",
        mirroredType: "histogram",
        expectedSamplesCount: expectedEvaluateRulesTimeSamples,
      },
    ],
    "evaluateRulesTime should be collected after evaluated rulesets"
  );

  extension.sendMessage("updateEnabledRulesets", {
    enableRulesetIds: ["ruleset2"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");
  await assertDNRGetEnabledRulesets(extension, ["ruleset1", "ruleset2"]);

  await callPageFetch();

  // Expect 3 rules with both rulesets enabled
  // (1 from ruleset1 and 2 more from ruleset2).
  expectedEvaluateRulesCountMax += ruleset2.length;
  assertDNRTelemetryMetricsGetValueEq(
    [
      {
        metric: "evaluateRulesCountMax",
        mirroredName: "extensions.apis.dnr.evaluate_rules_count_max",
        mirroredType: "scalar",
        expectedGetValue: expectedEvaluateRulesCountMax,
      },
    ],
    "evaluateRulesCountMax should have been increased after enabling ruleset2"
  );

  extension.sendMessage("updateEnabledRulesets", {
    disableRulesetIds: ["ruleset2"],
  });
  await extension.awaitMessage("updateEnabledRulesets:done");
  await assertDNRGetEnabledRulesets(extension, ["ruleset1"]);

  await callPageFetch();

  assertDNRTelemetryMetricsGetValueEq(
    [
      {
        metric: "evaluateRulesCountMax",
        mirroredName: "extensions.apis.dnr.evaluate_rules_count_max",
        mirroredType: "scalar",
        expectedGetValue: expectedEvaluateRulesCountMax,
      },
    ],
    "evaluateRulesCountMax should have not been decreased after disabling ruleset2"
  );

  await page.close();

  await extension.unload();
});
