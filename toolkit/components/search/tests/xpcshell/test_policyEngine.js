/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that Enterprise Policy Engines can be installed correctly.
 */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

SearchSettings.SETTINGS_INVALIDATION_DELAY = 100;

/**
 * Loads a new enterprise policy, and re-initialise the search service
 * with the new policy. Also waits for the search service to write the settings
 * file to disk.
 *
 * @param {object} policy
 *   The enterprise policy to use.
 */
async function setupPolicyEngineWithJson(policy) {
  Services.search.wrappedJSObject.reset();

  await EnterprisePolicyTesting.setupPolicyEngineWithJson(policy);

  let settingsWritten = SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
  await Services.search.init();
  await settingsWritten;
}

add_setup(async function () {
  // This initializes the policy engine for xpcshell tests
  let policies = Cc["@mozilla.org/enterprisepolicies;1"].getService(
    Ci.nsIObserver
  );
  policies.observe(null, "policies-startup", null);

  Services.fog.initializeFOG();
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines();

  SearchUtils.GENERAL_SEARCH_ENGINE_IDS = new Set([
    "engine-resourceicon@search.mozilla.org",
  ]);
});

add_task(async function test_enterprise_policy_engine() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "policy",
            Description: "Test policy engine",
            IconURL: "data:image/gif;base64,R0lGODl",
            Alias: "p",
            URLTemplate: "https://example.com?q={searchTerms}",
            SuggestURLTemplate: "https://example.com/suggest/?q={searchTerms}",
          },
        ],
      },
    },
  });

  let engine = Services.search.getEngineByName("policy");
  Assert.ok(engine, "Should have installed the engine.");

  Assert.equal(engine.name, "policy", "Should have the correct name");
  Assert.equal(
    engine.description,
    "Test policy engine",
    "Should have a description"
  );
  Assert.deepEqual(engine.aliases, ["p"], "Should have the correct alias");

  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/?q=foo",
    "Should have the correct search url"
  );

  submission = engine.getSubmission("foo", SearchUtils.URL_TYPE.SUGGEST_JSON);
  Assert.equal(
    submission.uri.spec,
    "https://example.com/suggest/?q=foo",
    "Should have the correct suggest url"
  );

  Services.search.defaultEngine = engine;

  await assertGleanDefaultEngine({
    normal: {
      engineId: "other-policy",
      displayName: "policy",
      loadPath: "[policy]",
      submissionUrl: "blank:",
      verified: "verified",
    },
  });
});

add_task(async function test_enterprise_policy_engine_hidden_persisted() {
  // Set the engine alias, and wait for the settings to be written.
  let settingsWritten = SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
  let engine = Services.search.getEngineByName("policy");
  engine.hidden = "p1";
  engine.alias = "p1";
  await settingsWritten;

  // This will reset and re-initialise the search service.
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "policy",
            Description: "Test policy engine",
            IconURL: "data:image/gif;base64,R0lGODl",
            Alias: "p",
            URLTemplate: "https://example.com?q={searchTerms}",
            SuggestURLTemplate: "https://example.com/suggest/?q={searchTerms}",
          },
        ],
      },
    },
  });

  engine = Services.search.getEngineByName("policy");
  Assert.equal(engine.alias, "p1", "Should have retained the engine alias");
  Assert.ok(engine.hidden, "Should have kept the engine hidden");
});

add_task(async function test_enterprise_policy_engine_remove() {
  // This will reset and re-initialise the search service.
  await setupPolicyEngineWithJson({
    policies: {},
  });

  Assert.ok(
    !Services.search.getEngineByName("policy"),
    "Should not have the policy engine installed"
  );

  let settings = await promiseSettingsData();
  Assert.ok(
    !settings.engines.find(e => e.name == "p1"),
    "Should not have the engine settings stored"
  );
});

add_task(async function test_enterprise_policy_hidden_default() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Remove: ["Test search engine"],
      },
    },
  });

  Services.search.resetToAppDefaultEngine();

  Assert.equal(Services.search.defaultEngine.name, "engine-resourceicon");
});

add_task(async function test_enterprise_policy_default() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Default: "engine-pref",
      },
    },
  });

  Services.search.resetToAppDefaultEngine();

  Assert.equal(Services.search.defaultEngine.name, "engine-pref");
});
