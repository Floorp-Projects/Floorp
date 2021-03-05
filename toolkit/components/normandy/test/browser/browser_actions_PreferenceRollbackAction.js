"use strict";

const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const { PreferenceRollbackAction } = ChromeUtils.import(
  "resource://normandy/actions/PreferenceRollbackAction.jsm"
);
const { Uptake } = ChromeUtils.import("resource://normandy/lib/Uptake.jsm");
const { PreferenceRollouts } = ChromeUtils.import(
  "resource://normandy/lib/PreferenceRollouts.jsm"
);

// Test that a simple recipe rollsback as expected
decorate_task(
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventSpy,
  PreferenceRollouts.withTestMock(),
  async function simple_rollback(setExperimentInactiveStub, sendEventStub) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref1", 2);
    Services.prefs
      .getDefaultBranch("")
      .setCharPref("test.pref2", "rollout value");
    Services.prefs.getDefaultBranch("").setBoolPref("test.pref3", true);

    await PreferenceRollouts.add({
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ACTIVE,
      preferences: [
        { preferenceName: "test.pref1", value: 2, previousValue: 1 },
        {
          preferenceName: "test.pref2",
          value: "rollout value",
          previousValue: "builtin value",
        },
        { preferenceName: "test.pref3", value: true, previousValue: false },
      ],
      enrollmentId: "test-enrollment-id",
    });

    const recipe = { id: 1, arguments: { rolloutSlug: "test-rollout" } };

    const action = new PreferenceRollbackAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    // rollout prefs are reset
    is(
      Services.prefs.getIntPref("test.pref1"),
      1,
      "integer pref should be rolled back"
    );
    is(
      Services.prefs.getCharPref("test.pref2"),
      "builtin value",
      "string pref should be rolled back"
    );
    is(
      Services.prefs.getBoolPref("test.pref3"),
      false,
      "boolean pref should be rolled back"
    );

    // start up prefs are unset
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref1"),
      Services.prefs.PREF_INVALID,
      "integer startup pref should be unset"
    );
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref2"),
      Services.prefs.PREF_INVALID,
      "string startup pref should be unset"
    );
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref3"),
      Services.prefs.PREF_INVALID,
      "boolean startup pref should be unset"
    );

    // rollout in db was updated
    const rollouts = await PreferenceRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          slug: "test-rollout",
          state: PreferenceRollouts.STATE_ROLLED_BACK,
          preferences: [
            { preferenceName: "test.pref1", value: 2, previousValue: 1 },
            {
              preferenceName: "test.pref2",
              value: "rollout value",
              previousValue: "builtin value",
            },
            { preferenceName: "test.pref3", value: true, previousValue: false },
          ],
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be updated in db"
    );

    // Telemetry is updated
    sendEventStub.assertEvents([
      [
        "unenroll",
        "preference_rollback",
        recipe.arguments.rolloutSlug,
        { reason: "rollback" },
      ],
    ]);
    Assert.deepEqual(
      setExperimentInactiveStub.args,
      [["test-rollout"]],
      "the telemetry experiment should deactivated"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref1");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref2");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref3");
  }
);

// Test that a graduated rollout can't be rolled back
decorate_task(
  withSendEventSpy,
  PreferenceRollouts.withTestMock(),
  async function cant_rollback_graduated(sendEventStub) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);
    await PreferenceRollouts.add({
      slug: "graduated-rollout",
      state: PreferenceRollouts.STATE_GRADUATED,
      preferences: [
        { preferenceName: "test.pref", value: 1, previousValue: 1 },
      ],
      enrollmentId: "test-enrollment-id",
    });

    let recipe = { id: 1, arguments: { rolloutSlug: "graduated-rollout" } };

    const action = new PreferenceRollbackAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(Services.prefs.getIntPref("test.pref"), 1, "pref should not change");
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"),
      Services.prefs.PREF_INVALID,
      "no startup pref should be added"
    );

    // rollout in the DB hasn't changed
    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [
        {
          slug: "graduated-rollout",
          state: PreferenceRollouts.STATE_GRADUATED,
          preferences: [
            { preferenceName: "test.pref", value: 1, previousValue: 1 },
          ],
          enrollmentId: "test-enrollment-id",
        },
      ],
      "Rollout should not change in db"
    );

    sendEventStub.assertEvents([
      [
        "unenrollFailed",
        "preference_rollback",
        "graduated-rollout",
        { reason: "graduated", enrollmentId: "test-enrollment-id" },
      ],
    ]);

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);

// Test that a rollback without a matching rollout does not send telemetry
decorate_task(
  withSendEventSpy,
  withStub(Uptake, "reportRecipe"),
  PreferenceRollouts.withTestMock(),
  async function rollback_without_rollout(sendEventStub, reportRecipeStub) {
    let recipe = { id: 1, arguments: { rolloutSlug: "missing-rollout" } };

    const action = new PreferenceRollbackAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    sendEventStub.assertEvents([]);
    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe, Uptake.RECIPE_SUCCESS]],
      "recipe should be reported as succesful"
    );
  }
);

// Test that rolling back an already rolled back recipe doesn't do anything
decorate_task(
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventSpy,
  PreferenceRollouts.withTestMock(),
  async function rollback_already_rolled_back(
    setExperimentInactiveStub,
    sendEventStub
  ) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);

    const recipe = { id: 1, arguments: { rolloutSlug: "test-rollout" } };
    const rollout = {
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ROLLED_BACK,
      preferences: [
        { preferenceName: "test.pref", value: 2, previousValue: 1 },
      ],
      enrollmentId: "test-rollout-id",
    };
    await PreferenceRollouts.add(rollout);

    const action = new PreferenceRollbackAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(Services.prefs.getIntPref("test.pref"), 1, "pref shouldn't change");
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"),
      Services.prefs.PREF_INVALID,
      "startup pref should not be set"
    );

    // rollout in db was updated
    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [rollout],
      "Rollout shouldn't change in db"
    );

    // Telemetry is updated
    sendEventStub.assertEvents([]);
    Assert.deepEqual(
      setExperimentInactiveStub.args,
      [],
      "telemetry experiments should not be updated"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);

// Test that a rollback doesn't affect user prefs
decorate_task(PreferenceRollouts.withTestMock(), async function simple_rollback(
  setExperimentInactiveStub,
  sendEventStub
) {
  Services.prefs.getDefaultBranch("").setCharPref("test.pref", "rollout value");
  Services.prefs.setCharPref("test.pref", "user value");

  await PreferenceRollouts.add({
    slug: "test-rollout",
    state: PreferenceRollouts.STATE_ACTIVE,
    preferences: [
      {
        preferenceName: "test.pref",
        value: "rollout value",
        previousValue: "builtin value",
      },
    ],
    enrollmentId: "test-enrollment-id",
  });

  const recipe = { id: 1, arguments: { rolloutSlug: "test-rollout" } };

  const action = new PreferenceRollbackAction();
  await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
  await action.finalize();
  is(action.lastError, null, "lastError should be null");

  is(
    Services.prefs.getDefaultBranch("").getCharPref("test.pref"),
    "builtin value",
    "default branch should be reset"
  );
  is(
    Services.prefs.getCharPref("test.pref"),
    "user value",
    "user branch should remain the same"
  );

  // Cleanup
  Services.prefs.deleteBranch("test.pref");
  Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
});

// Test that a rollouts in the graduation set can't be rolled back
decorate_task(
  withSendEventSpy,
  PreferenceRollouts.withTestMock({
    graduationSet: new Set(["graduated-rollout"]),
  }),
  async function cant_rollback_graduation_set(sendEventStub) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);

    let recipe = { id: 1, arguments: { rolloutSlug: "graduated-rollout" } };

    const action = new PreferenceRollbackAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(Services.prefs.getIntPref("test.pref"), 1, "pref should not change");
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"),
      Services.prefs.PREF_INVALID,
      "no startup pref should be added"
    );

    // No entry in the DB
    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [],
      "Rollout should be in the db"
    );

    sendEventStub.assertEvents([
      [
        "unenrollFailed",
        "preference_rollback",
        "graduated-rollout",
        {
          reason: "in-graduation-set",
          enrollmentId: TelemetryEvents.NO_ENROLLMENT_ID,
        },
      ],
    ]);

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);
