/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { ContentRelevancyManager } = ChromeUtils.importESModule(
  "resource://gre/modules/ContentRelevancyManager.sys.mjs"
);

/**
 * Test classification metrics - succeed.
 */
add_task(async function test_classify_succeed() {
  Services.fog.testResetFOG();
  await ExperimentAPI.ready();
  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "contentRelevancy",
    value: {
      enabled: true,
      minInputUrls: 0,
      maxInputUrls: 3,
      timerInterval: 3600,
    },
  });

  await TestUtils.waitForCondition(
    () => ContentRelevancyManager.shouldEnable,
    "Should enable it via Nimbus"
  );

  Assert.equal(undefined, Glean.relevancyClassify.succeed.testGetValue());
  Assert.equal(undefined, Glean.relevancyClassify.duration.testGetValue());

  await ContentRelevancyManager._test_doClassification();

  Assert.deepEqual(
    {
      input_size: 0,
      input_classified_size: 0,
      input_inconclusive_size: 0,
      output_interest_size: 0,
      interest_top_1_hits: 0,
      interest_top_2_hits: 0,
      interest_top_3_hits: 0,
    },
    Glean.relevancyClassify.succeed.testGetValue()[0].extra,
    "Should record the succeed event"
  );
  Assert.ok(
    Glean.relevancyClassify.duration.testGetValue().sum > 0,
    "Should record the duration"
  );

  await doExperimentCleanup();
});

/**
 * Test classification metrics - fail - insufficient-input.
 */
add_task(async function test_classify_fail_case1() {
  Services.fog.testResetFOG();
  await ExperimentAPI.ready();
  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "contentRelevancy",
    value: {
      enabled: true,
      // Require at least 1 input to trigger the failure.
      minInputUrls: 1,
      maxInputUrls: 3,
      timerInterval: 3600,
    },
  });

  await TestUtils.waitForCondition(
    () => ContentRelevancyManager.shouldEnable,
    "Should enable it via Nimbus"
  );

  Assert.equal(undefined, Glean.relevancyClassify.fail.testGetValue());
  Assert.equal(undefined, Glean.relevancyClassify.duration.testGetValue());

  await ContentRelevancyManager._test_doClassification();

  Assert.deepEqual(
    {
      reason: "insufficient-input",
    },
    Glean.relevancyClassify.fail.testGetValue()[0].extra,
    "Should record the fail event"
  );
  Assert.equal(
    undefined,
    Glean.relevancyClassify.duration.testGetValue(),
    "Should not record the duration"
  );

  await doExperimentCleanup();
});

/**
 * Test classification metrics - fail - store-not-ready.
 */
add_task(async function test_classify_fail_case2() {
  Services.fog.testResetFOG();
  await ExperimentAPI.ready();
  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "contentRelevancy",
    value: {
      // Disable the feature so the `store` won't be ready.
      enabled: false,
      minInputUrls: 0,
      maxInputUrls: 3,
      timerInterval: 3600,
    },
  });

  await TestUtils.waitForCondition(
    () => !ContentRelevancyManager.shouldEnable,
    "Should be disabled it via Nimbus"
  );

  Assert.equal(undefined, Glean.relevancyClassify.fail.testGetValue());
  Assert.equal(undefined, Glean.relevancyClassify.duration.testGetValue());

  await ContentRelevancyManager._test_doClassification();

  Assert.deepEqual(
    {
      reason: "store-not-ready",
    },
    Glean.relevancyClassify.fail.testGetValue()[0].extra,
    "Should record the fail event"
  );
  Assert.equal(
    undefined,
    Glean.relevancyClassify.duration.testGetValue(),
    "Should not record the duration"
  );

  await doExperimentCleanup();
});
