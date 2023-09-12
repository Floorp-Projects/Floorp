/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test migration load path for user, enterprise policy and add-on
 * engines.
 */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

const enterprisePolicy = {
  policies: {
    SearchEngines: {
      Add: [
        {
          Name: "Policy",
          Encoding: "windows-1252",
          URLTemplate: "http://example.com/?q={searchTerms}",
        },
      ],
    },
  },
};

add_setup(async function () {
  // This initializes the policy engine for xpcshell tests
  let policies = Cc["@mozilla.org/enterprisepolicies;1"].getService(
    Ci.nsIObserver
  );
  policies.observe(null, "policies-startup", null);

  await SearchTestUtils.useTestEngines("data1");
  await AddonTestUtils.promiseStartupManager();
  await EnterprisePolicyTesting.setupPolicyEngineWithJson(enterprisePolicy);
  // Setting the enterprise policy starts the search service initialising,
  // so we wait for that to complete before starting the test, we can
  // then also add an extra add-on engine.
  await Services.search.init();
  let settingsFileWritten = promiseAfterSettings();
  await SearchTestUtils.installSearchExtension();
  await settingsFileWritten;
});

/**
 * Loads the settings file and ensures it has not already been migrated.
 */
add_task(async function test_load_and_check_settings() {
  let settingsTemplate = await readJSONFile(
    do_get_file("data/search-legacy-old-loadPaths.json")
  );

  Assert.less(
    settingsTemplate.version,
    8,
    "Should be a version older than when indexing engines by id was introduced"
  );
  let engine = settingsTemplate.engines.find(e => e.id == "policy-Policy");
  Assert.equal(
    engine._loadPath,
    "[other]addEngineWithDetails:set-via-policy",
    "Should have a old style load path for the policy engine"
  );
  engine = settingsTemplate.engines.find(
    e => e.id == "bbc163e7-7b1a-47aa-a32c-c59062de2754"
  );
  Assert.equal(
    engine._loadPath,
    "[other]addEngineWithDetails:set-via-user",
    "Should have a old style load path for the user engine"
  );
  engine = settingsTemplate.engines.find(
    e => e.id == "example@tests.mozilla.orgdefault"
  );
  Assert.equal(
    engine._loadPath,
    "[other]addEngineWithDetails:example@tests.mozilla.org",
    "Should have a old style load path for the add-on engine"
  );

  await promiseSaveSettingsData(settingsTemplate);
});

/**
 * Tests that an installed engine matches the expected data.
 *
 * @param {object} expectedData The expected data for the engine
 */
async function assertInstalledEngineMatches(expectedData) {
  let engine = await Services.search.getEngineByName(expectedData.name);

  Assert.ok(engine, `Should have found the ${expectedData.type} engine`);
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    expectedData.loadPath,
    "Should have migrated the loadPath"
  );
}

add_task(async function test_migration_from_pre_ids() {
  const settingsFileWritten = promiseAfterSettings();

  await Services.search.wrappedJSObject.reset();
  await Services.search.init();

  await settingsFileWritten;

  await assertInstalledEngineMatches({
    type: "Policy",
    name: "Policy",
    loadPath: "[policy]",
  });
  await assertInstalledEngineMatches({
    type: "User",
    name: "User",
    loadPath: "[user]",
  });
  await assertInstalledEngineMatches({
    type: "Add-on",
    name: "Example",
    loadPath: "[addon]example@tests.mozilla.org",
  });
});
