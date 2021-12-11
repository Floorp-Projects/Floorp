"use strict";

const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const { PreferenceRolloutAction } = ChromeUtils.import(
  "resource://normandy/actions/PreferenceRolloutAction.jsm"
);
const { PreferenceRollouts } = ChromeUtils.import(
  "resource://normandy/lib/PreferenceRollouts.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);

// Test that a simple recipe enrolls as expected
decorate_task(
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withSendEventSpy(),
  PreferenceRollouts.withTestMock(),
  async function simple_recipe_enrollment({
    setExperimentActiveStub,
    sendEventSpy,
  }) {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [
          { preferenceName: "test.pref1", value: 1 },
          { preferenceName: "test.pref2", value: true },
          { preferenceName: "test.pref3", value: "it works" },
        ],
      },
    };

    const action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    // rollout prefs are set
    is(
      Services.prefs.getIntPref("test.pref1"),
      1,
      "integer pref should be set"
    );
    is(
      Services.prefs.getBoolPref("test.pref2"),
      true,
      "boolean pref should be set"
    );
    is(
      Services.prefs.getCharPref("test.pref3"),
      "it works",
      "string pref should be set"
    );

    // start up prefs are set
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref1"),
      1,
      "integer startup pref should be set"
    );
    is(
      Services.prefs.getBoolPref("app.normandy.startupRolloutPrefs.test.pref2"),
      true,
      "boolean startup pref should be set"
    );
    is(
      Services.prefs.getCharPref("app.normandy.startupRolloutPrefs.test.pref3"),
      "it works",
      "string startup pref should be set"
    );

    // rollout was stored
    let rollouts = await PreferenceRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          slug: "test-rollout",
          state: PreferenceRollouts.STATE_ACTIVE,
          preferences: [
            { preferenceName: "test.pref1", value: 1, previousValue: null },
            { preferenceName: "test.pref2", value: true, previousValue: null },
            {
              preferenceName: "test.pref3",
              value: "it works",
              previousValue: null,
            },
          ],
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "Rollout should have a UUID enrollmentId"
    );

    sendEventSpy.assertEvents([
      [
        "enroll",
        "preference_rollout",
        recipe.arguments.slug,
        { enrollmentId: rollouts[0].enrollmentId },
      ],
    ]);
    ok(
      setExperimentActiveStub.calledWithExactly("test-rollout", "active", {
        type: "normandy-prefrollout",
        enrollmentId: rollouts[0].enrollmentId,
      }),
      "a telemetry experiment should be activated"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref1");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref2");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref3");
  }
);

// Test that an enrollment's values can change, be removed, and be added
decorate_task(
  withSendEventSpy(),
  PreferenceRollouts.withTestMock(),
  async function update_enrollment({ sendEventSpy }) {
    // first enrollment
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [
          { preferenceName: "test.pref1", value: 1 },
          { preferenceName: "test.pref2", value: 1 },
        ],
      },
    };

    let action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    const defaultBranch = Services.prefs.getDefaultBranch("");
    is(defaultBranch.getIntPref("test.pref1"), 1, "pref1 should be set");
    is(defaultBranch.getIntPref("test.pref2"), 1, "pref2 should be set");
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref1"),
      1,
      "startup pref1 should be set"
    );
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref2"),
      1,
      "startup pref2 should be set"
    );

    // update existing enrollment
    recipe.arguments.preferences = [
      // pref1 is removed
      // pref2's value is updated
      { preferenceName: "test.pref2", value: 2 },
      // pref3 is added
      { preferenceName: "test.pref3", value: 2 },
    ];
    action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    /* Todo because of bug 1502410 and bug 1505941 */
    todo_is(
      Services.prefs.getPrefType("test.pref1"),
      Services.prefs.PREF_INVALID,
      "pref1 should be removed"
    );
    is(Services.prefs.getIntPref("test.pref2"), 2, "pref2 should be updated");
    is(Services.prefs.getIntPref("test.pref3"), 2, "pref3 should be added");

    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref1"),
      Services.prefs.PREF_INVALID,
      "startup pref1 should be removed"
    );
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref2"),
      2,
      "startup pref2 should be updated"
    );
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref3"),
      2,
      "startup pref3 should be added"
    );

    // rollout in the DB has been updated
    const rollouts = await PreferenceRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          slug: "test-rollout",
          state: PreferenceRollouts.STATE_ACTIVE,
          preferences: [
            { preferenceName: "test.pref2", value: 2, previousValue: null },
            { preferenceName: "test.pref3", value: 2, previousValue: null },
          ],
        },
      ],
      "Rollout should be updated in db"
    );

    sendEventSpy.assertEvents([
      [
        "enroll",
        "preference_rollout",
        "test-rollout",
        { enrollmentId: rollouts[0].enrollmentId },
      ],
      [
        "update",
        "preference_rollout",
        "test-rollout",
        { previousState: "active", enrollmentId: rollouts[0].enrollmentId },
      ],
    ]);

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref1");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref2");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref3");
  }
);

// Test that a graduated rollout can be ungraduated
decorate_task(
  withSendEventSpy(),
  PreferenceRollouts.withTestMock(),
  async function ungraduate_enrollment({ sendEventSpy }) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);
    await PreferenceRollouts.add({
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_GRADUATED,
      preferences: [
        { preferenceName: "test.pref", value: 1, previousValue: 1 },
      ],
      enrollmentId: "test-enrollment-id",
    });

    let recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [{ preferenceName: "test.pref", value: 2 }],
      },
    };

    const action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(Services.prefs.getIntPref("test.pref"), 2, "pref should be updated");
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref"),
      2,
      "startup pref should be set"
    );

    // rollout in the DB has been ungraduated
    const rollouts = await PreferenceRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          slug: "test-rollout",
          state: PreferenceRollouts.STATE_ACTIVE,
          preferences: [
            { preferenceName: "test.pref", value: 2, previousValue: 1 },
          ],
        },
      ],
      "Rollout should be updated in db"
    );

    sendEventSpy.assertEvents([
      [
        "update",
        "preference_rollout",
        "test-rollout",
        { previousState: "graduated", enrollmentId: "test-enrollment-id" },
      ],
    ]);

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);

// Test when recipes conflict, only one is applied
decorate_task(
  withSendEventSpy(),
  PreferenceRollouts.withTestMock(),
  async function conflicting_recipes({ sendEventSpy }) {
    // create two recipes that each share a pref and have a unique pref.
    const recipe1 = {
      id: 1,
      arguments: {
        slug: "test-rollout-1",
        preferences: [
          { preferenceName: "test.pref1", value: 1 },
          { preferenceName: "test.pref2", value: 1 },
        ],
      },
    };
    const recipe2 = {
      id: 2,
      arguments: {
        slug: "test-rollout-2",
        preferences: [
          { preferenceName: "test.pref1", value: 2 },
          { preferenceName: "test.pref3", value: 2 },
        ],
      },
    };

    // running both in the same session
    let action = new PreferenceRolloutAction();
    await action.processRecipe(recipe1, BaseAction.suitability.FILTER_MATCH);
    await action.processRecipe(recipe2, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    // running recipe2 in a separate session shouldn't change things
    action = new PreferenceRolloutAction();
    await action.processRecipe(recipe2, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(
      Services.prefs.getIntPref("test.pref1"),
      1,
      "pref1 is set to recipe1's value"
    );
    is(
      Services.prefs.getIntPref("test.pref2"),
      1,
      "pref2 is set to recipe1's value"
    );
    is(
      Services.prefs.getPrefType("test.pref3"),
      Services.prefs.PREF_INVALID,
      "pref3 is not set"
    );

    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref1"),
      1,
      "startup pref1 is set to recipe1's value"
    );
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref2"),
      1,
      "startup pref2 is set to recipe1's value"
    );
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref3"),
      Services.prefs.PREF_INVALID,
      "startup pref3 is not set"
    );

    // only successful rollout was stored
    const rollouts = await PreferenceRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          slug: "test-rollout-1",
          state: PreferenceRollouts.STATE_ACTIVE,
          preferences: [
            { preferenceName: "test.pref1", value: 1, previousValue: null },
            { preferenceName: "test.pref2", value: 1, previousValue: null },
          ],
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Only recipe1's rollout should be stored in db"
    );

    sendEventSpy.assertEvents([
      ["enroll", "preference_rollout", recipe1.arguments.slug],
      [
        "enrollFailed",
        "preference_rollout",
        recipe2.arguments.slug,
        { reason: "conflict", preference: "test.pref1" },
      ],
      [
        "enrollFailed",
        "preference_rollout",
        recipe2.arguments.slug,
        { reason: "conflict", preference: "test.pref1" },
      ],
    ]);

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref1");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref2");
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref3");
  }
);

// Test when the wrong value type is given, the recipe is not applied
decorate_task(
  withSendEventSpy(),
  PreferenceRollouts.withTestMock(),
  async function wrong_preference_value({ sendEventSpy }) {
    Services.prefs.getDefaultBranch("").setCharPref("test.pref", "not an int");
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [{ preferenceName: "test.pref", value: 1 }],
      },
    };

    const action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(
      Services.prefs.getCharPref("test.pref"),
      "not an int",
      "the pref should not be modified"
    );
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"),
      Services.prefs.PREF_INVALID,
      "startup pref is not set"
    );

    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [],
      "no rollout is stored in the db"
    );
    sendEventSpy.assertEvents([
      [
        "enrollFailed",
        "preference_rollout",
        recipe.arguments.slug,
        { reason: "invalid type", preference: "test.pref" },
      ],
    ]);

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);

// Test that even when applying a rollout, user prefs are preserved
decorate_task(
  PreferenceRollouts.withTestMock(),
  async function preserves_user_prefs() {
    Services.prefs
      .getDefaultBranch("")
      .setCharPref("test.pref", "builtin value");
    Services.prefs.setCharPref("test.pref", "user value");
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [{ preferenceName: "test.pref", value: "rollout value" }],
      },
    };

    const action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(
      Services.prefs.getCharPref("test.pref"),
      "user value",
      "user branch value should be preserved"
    );
    is(
      Services.prefs.getDefaultBranch("").getCharPref("test.pref"),
      "rollout value",
      "default branch value should change"
    );

    const rollouts = await PreferenceRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          slug: "test-rollout",
          state: PreferenceRollouts.STATE_ACTIVE,
          preferences: [
            {
              preferenceName: "test.pref",
              value: "rollout value",
              previousValue: "builtin value",
            },
          ],
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "the rollout is added to the db with the correct previous value"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
    Services.prefs.deleteBranch("test.pref");
  }
);

// Enrollment works for prefs with only a user branch value, and no default value.
decorate_task(
  PreferenceRollouts.withTestMock(),
  async function simple_recipe_enrollment() {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [{ preferenceName: "test.pref", value: 1 }],
      },
    };

    // Set a pref on the user branch only
    Services.prefs.setIntPref("test.pref", 2);

    const action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(
      Services.prefs.getIntPref("test.pref"),
      2,
      "original user branch value still visible"
    );
    is(
      Services.prefs.getDefaultBranch("").getIntPref("test.pref"),
      1,
      "default branch was set"
    );
    is(
      Services.prefs.getIntPref("app.normandy.startupRolloutPrefs.test.pref"),
      1,
      "startup pref is est"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);

// When running a rollout a second time on a pref that doesn't have an existing
// value, the previous value is handled correctly.
decorate_task(
  PreferenceRollouts.withTestMock(),
  withSendEventSpy(),
  async function({ sendEventSpy }) {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [{ preferenceName: "test.pref", value: 1 }],
      },
    };

    // run once
    let action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    // run a second time
    action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    const rollouts = await PreferenceRollouts.getAll();

    Assert.deepEqual(
      rollouts,
      [
        {
          slug: "test-rollout",
          state: PreferenceRollouts.STATE_ACTIVE,
          preferences: [
            { preferenceName: "test.pref", value: 1, previousValue: null },
          ],
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "the DB should have the correct value stored for previousValue"
    );

    sendEventSpy.assertEvents([
      [
        "enroll",
        "preference_rollout",
        "test-rollout",
        { enrollmentId: rollouts[0].enrollmentId },
      ],
    ]);
  }
);

// New rollouts that are no-ops should send errors
decorate_task(
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withSendEventSpy(),
  PreferenceRollouts.withTestMock(),
  async function no_op_new_recipe({ setExperimentActiveStub, sendEventSpy }) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);

    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [{ preferenceName: "test.pref", value: 1 }],
      },
    };

    const action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(Services.prefs.getIntPref("test.pref"), 1, "pref should not change");

    // start up pref isn't set
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"),
      Services.prefs.PREF_INVALID,
      "startup pref1 should not be set"
    );

    // rollout was not stored
    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [],
      "Rollout should not be stored in db"
    );

    sendEventSpy.assertEvents([
      [
        "enrollFailed",
        "preference_rollout",
        recipe.arguments.slug,
        { reason: "would-be-no-op" },
      ],
    ]);
    Assert.deepEqual(
      setExperimentActiveStub.args,
      [],
      "a telemetry experiment should not be activated"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);

// New rollouts in the graduation set should silently do nothing
decorate_task(
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withSendEventSpy(),
  PreferenceRollouts.withTestMock({ graduationSet: new Set(["test-rollout"]) }),
  async function graduationSetNewRecipe({
    setExperimentActiveStub,
    sendEventSpy,
  }) {
    Services.prefs.getDefaultBranch("").setIntPref("test.pref", 1);

    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        preferences: [{ preferenceName: "test.pref", value: 1 }],
      },
    };

    const action = new PreferenceRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    is(Services.prefs.getIntPref("test.pref"), 1, "pref should not change");

    // start up pref isn't set
    is(
      Services.prefs.getPrefType("app.normandy.startupRolloutPrefs.test.pref"),
      Services.prefs.PREF_INVALID,
      "startup pref1 should not be set"
    );

    // rollout was not stored
    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [],
      "Rollout should not be stored in db"
    );

    sendEventSpy.assertEvents([]);
    Assert.deepEqual(
      setExperimentActiveStub.args,
      [],
      "a telemetry experiment should not be activated"
    );

    // Cleanup
    Services.prefs.getDefaultBranch("").deleteBranch("test.pref");
  }
);
