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
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

let gSandbox;

add_setup(() => {
  gSandbox = sinon.createSandbox();

  registerCleanupFunction(() => {
    gSandbox.restore();
  });
});

/**
 * Test Nimbus integration - enable.
 */
add_task(async function test_NimbusIntegration_enable() {
  gSandbox.spy(ContentRelevancyManager, "notify");

  await ExperimentAPI.ready();
  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "contentRelevancy",
    value: {
      enabled: true,
      minInputUrls: 1,
      maxInputUrls: 3,
      // Set the timer interval to 0 will trigger the timer right away.
      timerInterval: 0,
      ingestEnabled: false,
    },
  });

  await TestUtils.waitForCondition(
    () => ContentRelevancyManager.shouldEnable,
    "Should enable it via Nimbus"
  );

  await TestUtils.waitForCondition(
    () => ContentRelevancyManager.notify.called,
    "The timer callback should be called"
  );

  doExperimentCleanup();
  gSandbox.restore();
});

/**
 * Test Nimbus integration - disable.
 */
add_task(async function test_NimbusIntegration_disable() {
  gSandbox.spy(ContentRelevancyManager, "notify");

  await ExperimentAPI.ready();
  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "contentRelevancy",
    value: {
      enabled: false,
      minInputUrls: 1,
      maxInputUrls: 3,
      // Set the timer interval to 0 will trigger the timer right away.
      timerInterval: 0,
      ingestEnabled: false,
    },
  });

  await TestUtils.waitForCondition(
    () => !ContentRelevancyManager.shouldEnable,
    "Should disable it via Nimbus"
  );

  await TestUtils.waitForCondition(
    () => ContentRelevancyManager.notify.notCalled,
    "The timer callback should not be called"
  );

  doExperimentCleanup();
  gSandbox.restore();
});
