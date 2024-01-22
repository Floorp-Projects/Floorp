/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests Search Engine IDs are created correctly.
 */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

const CONFIG = [
  {
    webExtension: {
      id: "engine@search.mozilla.org",
      name: "Test search engine",
      search_url: "https://www.google.com/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
        {
          name: "channel",
          condition: "purpose",
          purpose: "contextmenu",
          value: "rcs",
        },
        {
          name: "channel",
          condition: "purpose",
          purpose: "keyword",
          value: "fflb",
        },
      ],
      suggest_url:
        "https://suggestqueries.google.com/complete/search?output=firefox&client=firefox&hl={moz:locale}&q={searchTerms}",
    },
    appliesTo: [
      {
        included: { everywhere: true },
        default: "yes",
      },
    ],
  },
];

add_setup(async function () {
  useHttpServer("opensearch");
  await SearchTestUtils.useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_add_on_engine_id() {
  let addOnEngine = Services.search.defaultEngine;

  Assert.equal(
    addOnEngine.name,
    "Test search engine",
    "Should have installed the Test search engine as default."
  );
  Assert.ok(addOnEngine.id, "The Addon Search Engine should have an id.");
  Assert.equal(
    addOnEngine.id,
    "engine@search.mozilla.orgdefault",
    "The Addon Search Engine id should be the webextension id + the locale."
  );
});

add_task(async function test_user_engine_id() {
  let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ADDED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  await Services.search.addUserEngine(
    "user",
    "https://example.com/user?q={searchTerms}",
    "u"
  );

  await promiseEngineAdded;
  let userEngine = Services.search.getEngineByName("user");

  Assert.ok(userEngine, "Should have installed the User Search Engine.");
  Assert.ok(userEngine.id, "The User Search Engine should have an id.");
  Assert.equal(
    userEngine.id.length,
    36,
    "The User Search Engine id should be a 36 character uuid."
  );
});

add_task(async function test_open_search_engine_id() {
  let openSearchEngine = await SearchTestUtils.promiseNewSearchEngine({
    url: gDataUrl + "simple.xml",
  });

  Assert.ok(openSearchEngine, "Should have installed the Open Search Engine.");
  Assert.ok(openSearchEngine.id, "The Open Search Engine should have an id.");
  Assert.equal(
    openSearchEngine.id.length,
    36,
    "The Open Search Engine id should be a 36 character uuid."
  );
});

add_task(async function test_enterprise_policy_engine_id() {
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

  let policyEngine = Services.search.getEngineByName("policy");

  Assert.ok(policyEngine, "Should have installed the Policy Engine.");
  Assert.ok(policyEngine.id, "The Policy Engine should have an id.");
  Assert.equal(
    policyEngine.id,
    "policy-policy",
    "The Policy Engine id should be 'policy-' + 'the name of the policy engine'."
  );
});

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
