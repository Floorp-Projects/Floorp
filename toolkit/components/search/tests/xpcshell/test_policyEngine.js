/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that Enterprise Policy Engines can be installed correctly.
 */

"use strict";

add_task(async function setup() {
  Services.fog.initializeFOG();
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_enterprise_policy_engine() {
  let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ADDED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  await Services.search.addPolicyEngine({
    name: "policy",
    description: "Test policy engine",
    iconURL: "data:image/gif;base64,R0lGODl",
    keyword: "p",
    search_url: "https://example.com?q={searchTerms}",
    suggest_url: "https://example.com/suggest/?q={searchTerms}",
  });
  await promiseEngineAdded;

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
      loadPath: "[other]addEngineWithDetails:set-via-policy",
      submissionUrl: "blank:",
      verified: "verified",
    },
  });
});
