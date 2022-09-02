/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

function getRecipe(slug) {
  return ExperimentFakes.recipe(slug, {
    bucketConfig: {
      start: 0,
      // Make sure the experiment enrolls
      count: 10000,
      total: 10000,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
    targeting: "!(experiment.slug in activeExperiments)",
  });
}

add_task(async function test_double_feature_enrollment() {
  let sandbox = sinon.createSandbox();
  let sendFailureTelemetryStub = sandbox.stub(
    ExperimentManager,
    "sendFailureTelemetry"
  );
  await ExperimentAPI.ready();

  Assert.ok(ExperimentManager.store.getAllActive().length === 0, "Clean state");

  let recipe1 = getRecipe("foo" + Math.random());
  let recipe2 = getRecipe("foo" + Math.random());

  let enrollPromise1 = ExperimentFakes.waitForExperimentUpdate(ExperimentAPI, {
    slug: recipe1.slug,
  });

  ExperimentManager.enroll(recipe1, "test_double_feature_enrollment");
  await enrollPromise1;
  ExperimentManager.enroll(recipe2, "test_double_feature_enrollment");

  Assert.equal(
    ExperimentManager.store.getAllActive().length,
    1,
    "1 active experiment"
  );

  await BrowserTestUtils.waitForCondition(
    () => sendFailureTelemetryStub.callCount === 1,
    "Expected to fail one of the recipes"
  );

  Assert.equal(
    sendFailureTelemetryStub.firstCall.args[0],
    "enrollFailed",
    "Check expected event"
  );
  Assert.ok(
    sendFailureTelemetryStub.firstCall.args[1] === recipe1.slug ||
      sendFailureTelemetryStub.firstCall.args[1] === recipe2.slug,
    "Failed one of the two recipes"
  );
  Assert.equal(
    sendFailureTelemetryStub.firstCall.args[2],
    "feature-conflict",
    "Check expected reason"
  );

  await ExperimentFakes.cleanupAll([recipe1.slug]);
  sandbox.restore();
});
