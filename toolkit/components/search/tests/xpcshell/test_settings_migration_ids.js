/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test migration of user, enterprise policy and OpenSearch engines
 * from when engines were referenced by name rather than id.
 *
 * Add-ons and default engine ids are tested in test_settings.js.
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

/**
 * Loads the settings file and ensures it has not already been migrated.
 *
 * @param {string} settingsFile The settings file to load
 */
async function loadSettingsFile(settingsFile) {
  let settingsTemplate = await readJSONFile(do_get_file(settingsFile));

  Assert.less(
    settingsTemplate.version,
    7,
    "Should be a version older than when indexing engines by id was introduced"
  );
  for (let engine of settingsTemplate.engines) {
    Assert.ok(!("id" in engine));
  }

  await promiseSaveSettingsData(settingsTemplate);
}

/**
 * Test reading from search.json.mozlz4
 */
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
  // so we wait for that to complete before starting the test.
  await Services.search.init();
});

/**
 * Tests that an installed engine matches the expected data.
 *
 * @param {object} expectedData The expected data for the engine
 */
async function assertInstalledEngineMatches(expectedData) {
  let engine = await Services.search.getEngineByName(expectedData.name);

  Assert.ok(engine, `Should have found the ${expectedData.type} engine`);
  if (expectedData.idLength) {
    Assert.equal(
      engine.id.length,
      expectedData.idLength,
      "Should have been given an id"
    );
  } else {
    Assert.equal(engine.id, expectedData.id, "Should have the expected id");
  }
  Assert.equal(engine.alias, expectedData.alias, "Should have kept the alias");
}

add_task(async function test_migration_from_pre_ids() {
  await loadSettingsFile("data/search-legacy-no-ids.json");

  const settingsFileWritten = promiseAfterSettings();

  await Services.search.wrappedJSObject.reset();
  await Services.search.init();

  await settingsFileWritten;

  await assertInstalledEngineMatches({
    type: "OpenSearch",
    name: "Bugzilla@Mozilla",
    idLength: 36,
    alias: "bugzillaAlias",
  });
  await assertInstalledEngineMatches({
    type: "Enterprise Policy",
    name: "Policy",
    id: "policy-Policy",
    alias: "PolicyAlias",
  });
  await assertInstalledEngineMatches({
    type: "User",
    name: "User",
    idLength: 36,
    alias: "UserAlias",
  });
});
