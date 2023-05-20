"use strict";

const { EnrollmentsContext, RemoteSettingsExperimentLoader } =
  ChromeUtils.importESModule(
    "resource://nimbus/lib/RemoteSettingsExperimentLoader.sys.mjs"
  );
const { ExperimentManager } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentManager.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });

  CONTEXT = new EnrollmentsContext(RemoteSettingsExperimentLoader.manager);
});

let CONTEXT;

const FAKE_CONTEXT = {
  experiment: ExperimentFakes.recipe("fake-test-experiment"),
  source: "browser_experiment_evaluate_jexl",
};

add_task(async function test_throws_if_no_experiment_in_context() {
  await Assert.rejects(
    CONTEXT.evaluateJexl("true", {
      customThing: 1,
      source: "test_throws_if_no_experiment_in_context",
    }),
    /Expected an .experiment/,
    "should throw if experiment is not passed to the custom context"
  );
});

add_task(async function test_evaluate_jexl() {
  Assert.deepEqual(
    await CONTEXT.evaluateJexl(`["hello"]`, FAKE_CONTEXT),
    ["hello"],
    "should return the evaluated result of a jexl expression"
  );
});

add_task(async function test_evaluate_custom_context() {
  const result = await CONTEXT.evaluateJexl("experiment.slug", FAKE_CONTEXT);
  Assert.equal(
    result,
    "fake-test-experiment",
    "should have the custom .experiment context"
  );
});

add_task(async function test_evaluate_active_experiments_isFirstStartup() {
  const result = await CONTEXT.evaluateJexl("isFirstStartup", FAKE_CONTEXT);
  Assert.equal(
    typeof result,
    "boolean",
    "should have a .isFirstStartup property from ExperimentManager "
  );
});

add_task(async function test_evaluate_active_experiments_activeExperiments() {
  // Add an experiment to active experiments
  const slug = "foo" + Math.random();
  // Init the store before we use it
  await ExperimentManager.onStartup();

  let recipe = ExperimentFakes.recipe(slug);
  recipe.branches[0].slug = "mochitest-active-foo";
  delete recipe.branches[1];

  let { enrollmentPromise, doExperimentCleanup } =
    ExperimentFakes.enrollmentHelper(recipe);

  await enrollmentPromise;

  Assert.equal(
    await CONTEXT.evaluateJexl(`"${slug}" in activeExperiments`, FAKE_CONTEXT),
    true,
    "should find an active experiment"
  );

  Assert.equal(
    await CONTEXT.evaluateJexl(
      `"does-not-exist-fake" in activeExperiments`,
      FAKE_CONTEXT
    ),
    false,
    "should not find an experiment that doesn't exist"
  );

  await doExperimentCleanup();
});
