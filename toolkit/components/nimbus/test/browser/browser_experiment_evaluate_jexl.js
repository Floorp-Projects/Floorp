"use strict";

const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);
const { ExperimentManager } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentManager.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

const FAKE_CONTEXT = {
  experiment: ExperimentFakes.recipe("fake-test-experiment"),
};

add_task(async function test_throws_if_no_experiment_in_context() {
  await Assert.rejects(
    RemoteSettingsExperimentLoader.evaluateJexl("true", { customThing: 1 }),
    /Expected an .experiment or .activeRemoteDefaults/,
    "should throw if experiment is not passed to the custom context"
  );
});

add_task(async function test_evaluate_jexl() {
  Assert.deepEqual(
    await RemoteSettingsExperimentLoader.evaluateJexl(
      `["hello"]`,
      FAKE_CONTEXT
    ),
    ["hello"],
    "should return the evaluated result of a jexl expression"
  );
});

add_task(async function test_evaluate_custom_context() {
  const result = await RemoteSettingsExperimentLoader.evaluateJexl(
    "experiment.slug",
    FAKE_CONTEXT
  );
  Assert.equal(
    result,
    "fake-test-experiment",
    "should have the custom .experiment context"
  );
});

add_task(async function test_evaluate_active_experiments_isFirstStartup() {
  const result = await RemoteSettingsExperimentLoader.evaluateJexl(
    "isFirstStartup",
    FAKE_CONTEXT
  );
  Assert.equal(
    typeof result,
    "boolean",
    "should have a .isFirstStartup property from ExperimentManager "
  );
});

add_task(async function test_evaluate_active_experiments_activeExperiments() {
  // Add an experiment to active experiments
  const slug = "foo" + Date.now();
  // Init the store before we use it
  await ExperimentManager.onStartup();
  ExperimentManager.store.addExperiment(ExperimentFakes.experiment(slug));
  registerCleanupFunction(() => {
    ExperimentManager.store._deleteForTests(slug);
  });

  Assert.equal(
    await RemoteSettingsExperimentLoader.evaluateJexl(
      `"${slug}" in activeExperiments`,
      FAKE_CONTEXT
    ),
    true,
    "should find an active experiment"
  );

  Assert.equal(
    await RemoteSettingsExperimentLoader.evaluateJexl(
      `"does-not-exist-fake" in activeExperiments`,
      FAKE_CONTEXT
    ),
    false,
    "should not find an experiment that doesn't exist"
  );
});
