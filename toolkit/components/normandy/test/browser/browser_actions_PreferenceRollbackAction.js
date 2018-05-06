"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/actions/PreferenceRollbackAction.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceRollouts.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);

// Test that a simple recipe rollsback as expected
decorate_task(
  PreferenceRollouts.withTestMock,
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventStub,
  async function simple_rollback(setExperimentInactiveStub, sendEventStub) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref1", 2);
    Services.prefs.getDefaultBranch("").setCharPref("test.pref2", "rollout value");
    Services.prefs.getDefaultBranch("").setBoolPref("test.pref3", true);

    PreferenceRollouts.add({
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ACTIVE,
      preferences: [
        {preferenceName: "test.pref1", value: 2, previousValue: 1},
        {preferenceName: "test.pref2", value: "rollout value", previousValue: "builtin value"},
        {preferenceName: "test.pref3", value: true, previousValue: false},
      ],
    });

    const recipe = {id: 1, arguments: {rolloutSlug: "test-rollout"}};

    const action = new PreferenceRollbackAction();
    await action.runRecipe(recipe);
    await action.finalize();

    // rollout prefs are reset
    is(Services.prefs.getIntPref("test.pref1"), 1, "integer pref should be rolled back");
    is(Services.prefs.getCharPref("test.pref2"), "builtin value", "string pref should be rolled back");
    is(Services.prefs.getBoolPref("test.pref3"), false, "boolean pref should be rolled back");

    // start up prefs are unset
    is(Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref1"), Services.prefs.PREF_INVALID, "integer startup pref should be unset");
    is(Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref2"), Services.prefs.PREF_INVALID, "string startup pref should be unset");
    is(Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref3"), Services.prefs.PREF_INVALID, "boolean startup pref should be unset");

    // rollout in db was updated
    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [{
        slug: "test-rollout",
        state: PreferenceRollouts.STATE_ROLLED_BACK,
        preferences: [
          {preferenceName: "test.pref1", value: 2, previousValue: 1},
          {preferenceName: "test.pref2", value: "rollout value", previousValue: "builtin value"},
          {preferenceName: "test.pref3", value: true, previousValue: false},
        ],
      }],
      "Rollout should be updated in db"
    );

    // Telemetry is updated
    Assert.deepEqual(
      sendEventStub.args,
      [["unenroll", "preference_rollback", recipe.arguments.rolloutSlug, {reason: "rollback"}]],
      "an unenrollment event should be sent"
    );
    Assert.deepEqual(setExperimentInactiveStub.args, [["test-rollout"]], "the telemetry experiment should deactivated");

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref1");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref2");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref3");
  },
);

// Test that a graduated rollout can't be rolled back
decorate_task(
  PreferenceRollouts.withTestMock,
  withSendEventStub,
  async function cant_rollback_graduated(sendEventStub) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);
    await PreferenceRollouts.add({
      slug: "graduated-rollout",
      state: PreferenceRollouts.STATE_GRADUATED,
      preferences: [{preferenceName: "test.pref", value: 1, previousValue: 1}],
    });

    let recipe = {id: 1, arguments: {rolloutSlug: "graduated-rollout"}};

    const action = new PreferenceRollbackAction();
    await action.runRecipe(recipe);
    await action.finalize();

    is(Services.prefs.getIntPref("test.pref"), 1, "pref should not change");
    is(Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"), Services.prefs.PREF_INVALID, "no startup pref should be added");

    // rollout in the DB hasn't changed
    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [{
        slug: "graduated-rollout",
        state: PreferenceRollouts.STATE_GRADUATED,
        preferences: [{preferenceName: "test.pref", value: 1, previousValue: 1}],
      }],
      "Rollout should not change in db"
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["unenrollFailed", "preference_rollback", "graduated-rollout", {reason: "graduated"}]],
      "correct event was sent"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  },
);

// Test that a rollback without a matching rollout
decorate_task(
  PreferenceRollouts.withTestMock,
  withSendEventStub,
  withStub(Uptake, "reportRecipe"),
  async function rollback_without_rollout(sendEventStub, reportRecipeStub) {
    let recipe = {id: 1, arguments: {rolloutSlug: "missing-rollout"}};

    const action = new PreferenceRollbackAction();
    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(
      sendEventStub.args,
      [["unenrollFailed", "preference_rollback", "missing-rollout", {reason: "rollout missing"}]],
      "an unenrollFailure event should be sent",
    );
    // This is too common a case for an error, so it should be reported as success
    Assert.deepEqual(
      reportRecipeStub.args,
      [[recipe.id, Uptake.RECIPE_SUCCESS]],
      "recipe should be reported as succesful",
    );
  },
);

// Test that rolling back an already rolled back recipe doesn't do anything
decorate_task(
  PreferenceRollouts.withTestMock,
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventStub,
  async function rollback_already_rolled_back(setExperimentInactiveStub, sendEventStub) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);

    const recipe = {id: 1, arguments: {rolloutSlug: "test-rollout"}};
    const rollout = {
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ROLLED_BACK,
      preferences: [{preferenceName: "test.pref", value: 2, previousValue: 1}],
    };
    PreferenceRollouts.add(rollout);

    const action = new PreferenceRollbackAction();
    await action.runRecipe(recipe);
    await action.finalize();

    is(Services.prefs.getIntPref("test.pref"), 1, "pref shouldn't change");
    is(Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"), Services.prefs.PREF_INVALID, "startup pref should not be set");

    // rollout in db was updated
    Assert.deepEqual(await PreferenceRollouts.getAll(), [rollout], "Rollout shouldn't change in db");

    // Telemetry is updated
    Assert.deepEqual(sendEventStub.args, [], "no telemetry event should be sent");
    Assert.deepEqual(setExperimentInactiveStub.args, [], "telemetry experiments should not be updated");

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  },
);

// Test that a rollback doesn't affect user prefs
decorate_task(
  PreferenceRollouts.withTestMock,
  async function simple_rollback(setExperimentInactiveStub, sendEventStub) {
    Services.prefs.getDefaultBranch("").setCharPref("test.pref", "rollout value");
    Services.prefs.setCharPref("test.pref", "user value");

    PreferenceRollouts.add({
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ACTIVE,
      preferences: [
        {preferenceName: "test.pref", value: "rollout value", previousValue: "builtin value"},
      ],
    });

    const recipe = {id: 1, arguments: {rolloutSlug: "test-rollout"}};

    const action = new PreferenceRollbackAction();
    await action.runRecipe(recipe);
    await action.finalize();

    is(Services.prefs.getDefaultBranch("").getCharPref("test.pref"), "builtin value", "default branch should be reset");
    is(Services.prefs.getCharPref("test.pref"), "user value", "user branch should remain the same");

    // Cleanup
    Services.prefs.deleteBranch("test.pref");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  },
);
