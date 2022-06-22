/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that User Engines can be installed correctly.
 */

"use strict";

add_task(async function setup() {
  Services.fog.initializeFOG();
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_user_engine() {
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

  let engine = Services.search.getEngineByName("user");
  Assert.ok(engine, "Should have installed the engine.");

  Assert.equal(engine.name, "user", "Should have the correct name");
  Assert.equal(engine.description, null, "Should not have a description");
  Assert.deepEqual(engine.aliases, ["u"], "Should have the correct alias");

  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/user?q=foo",
    "Should have the correct search url"
  );

  submission = engine.getSubmission("foo", SearchUtils.URL_TYPE.SUGGEST_JSON);
  Assert.equal(submission, null, "Should not have a suggest url");

  Services.search.defaultEngine = engine;

  await assertGleanDefaultEngine({
    normal: {
      engineId: "other-user",
      displayName: "user",
      loadPath: "[other]addEngineWithDetails:set-via-user",
      submissionUrl: "blank:",
      verified: "verified",
    },
  });
});
