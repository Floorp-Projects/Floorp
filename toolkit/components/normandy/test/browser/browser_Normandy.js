"use strict";

const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { Normandy } = ChromeUtils.import("resource://normandy/Normandy.jsm");
const { AddonRollouts } = ChromeUtils.import(
  "resource://normandy/lib/AddonRollouts.jsm"
);
const { PreferenceExperiments } = ChromeUtils.import(
  "resource://normandy/lib/PreferenceExperiments.jsm"
);
const { PreferenceRollouts } = ChromeUtils.import(
  "resource://normandy/lib/PreferenceRollouts.jsm"
);
const { RecipeRunner } = ChromeUtils.import(
  "resource://normandy/lib/RecipeRunner.jsm"
);
const {
  NormandyTestUtils: { factories },
} = ChromeUtils.import("resource://testing-common/NormandyTestUtils.jsm");

const experimentPref1 = "test.initExperimentPrefs1";
const experimentPref2 = "test.initExperimentPrefs2";
const experimentPref3 = "test.initExperimentPrefs3";
const experimentPref4 = "test.initExperimentPrefs4";

function withStubInits() {
  return function(testFunction) {
    return decorate(
      withStub(AddonRollouts, "init"),
      withStub(AddonStudies, "init"),
      withStub(PreferenceRollouts, "init"),
      withStub(PreferenceExperiments, "init"),
      withStub(RecipeRunner, "init"),
      withStub(TelemetryEvents, "init"),
      () => testFunction()
    );
  };
}

decorate_task(
  withPrefEnv({
    set: [
      [`app.normandy.startupExperimentPrefs.${experimentPref1}`, true],
      [`app.normandy.startupExperimentPrefs.${experimentPref2}`, 2],
      [`app.normandy.startupExperimentPrefs.${experimentPref3}`, "string"],
    ],
  }),
  async function testApplyStartupPrefs() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    for (const pref of [experimentPref1, experimentPref2, experimentPref3]) {
      is(
        defaultBranch.getPrefType(pref),
        defaultBranch.PREF_INVALID,
        `Pref ${pref} don't exist before being initialized.`
      );
    }

    let oldValues = Normandy.applyStartupPrefs(
      "app.normandy.startupExperimentPrefs."
    );

    Assert.deepEqual(
      oldValues,
      {
        [experimentPref1]: null,
        [experimentPref2]: null,
        [experimentPref3]: null,
      },
      "the correct set of old values should be reported"
    );

    ok(
      defaultBranch.getBoolPref(experimentPref1),
      `Pref ${experimentPref1} has a default value after being initialized.`
    );
    is(
      defaultBranch.getIntPref(experimentPref2),
      2,
      `Pref ${experimentPref2} has a default value after being initialized.`
    );
    is(
      defaultBranch.getCharPref(experimentPref3),
      "string",
      `Pref ${experimentPref3} has a default value after being initialized.`
    );

    for (const pref of [experimentPref1, experimentPref2, experimentPref3]) {
      ok(
        !defaultBranch.prefHasUserValue(pref),
        `Pref ${pref} doesn't have a user value after being initialized.`
      );
      Services.prefs.clearUserPref(pref);
      defaultBranch.deleteBranch(pref);
    }
  }
);

decorate_task(
  withPrefEnv({
    set: [
      ["app.normandy.startupExperimentPrefs.test.existingPref", "experiment"],
    ],
  }),
  async function testApplyStartupPrefsExisting() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    defaultBranch.setCharPref("test.existingPref", "default");
    Normandy.applyStartupPrefs("app.normandy.startupExperimentPrefs.");
    is(
      defaultBranch.getCharPref("test.existingPref"),
      "experiment",
      "applyStartupPrefs overwrites the default values of existing preferences."
    );
  }
);

decorate_task(
  withPrefEnv({
    set: [
      ["app.normandy.startupExperimentPrefs.test.mismatchPref", "experiment"],
    ],
  }),
  async function testApplyStartupPrefsMismatch() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    defaultBranch.setIntPref("test.mismatchPref", 2);
    Normandy.applyStartupPrefs("app.normandy.startupExperimentPrefs.");
    is(
      defaultBranch.getPrefType("test.mismatchPref"),
      Services.prefs.PREF_INT,
      "applyStartupPrefs skips prefs that don't match the existing default value's type."
    );
  }
);

decorate_task(
  withStub(Normandy, "finishInit"),
  async function testStartupDelayed(finishInitStub) {
    await Normandy.init();
    ok(
      !finishInitStub.called,
      "When initialized, do not call finishInit immediately."
    );

    Normandy.observe(null, "sessionstore-windows-restored");
    ok(
      finishInitStub.called,
      "Once the sessionstore-windows-restored event is observed, finishInit should be called."
    );
  }
);

// During startup, preferences that are changed for experiments should
// be record by calling PreferenceExperiments.recordOriginalValues.
decorate_task(
  withStub(PreferenceExperiments, "recordOriginalValues"),
  withStub(PreferenceRollouts, "recordOriginalValues"),
  async function testApplyStartupPrefs(
    experimentsRecordOriginalValuesStub,
    rolloutsRecordOriginalValueStub
  ) {
    const defaultBranch = Services.prefs.getDefaultBranch("");

    defaultBranch.setBoolPref(experimentPref1, false);
    defaultBranch.setIntPref(experimentPref2, 1);
    defaultBranch.setCharPref(experimentPref3, "original string");
    // experimentPref4 is left unset

    Normandy.applyStartupPrefs("app.normandy.startupExperimentPrefs.");
    Normandy.studyPrefsChanged = { "test.study-pref": 1 };
    Normandy.rolloutPrefsChanged = { "test.rollout-pref": 1 };
    await Normandy.finishInit();

    Assert.deepEqual(
      experimentsRecordOriginalValuesStub.args,
      [[{ "test.study-pref": 1 }]],
      "finishInit should record original values of the study prefs"
    );
    Assert.deepEqual(
      rolloutsRecordOriginalValueStub.args,
      [[{ "test.rollout-pref": 1 }]],
      "finishInit should record original values of the study prefs"
    );

    // cleanup
    defaultBranch.deleteBranch(experimentPref1);
    defaultBranch.deleteBranch(experimentPref2);
    defaultBranch.deleteBranch(experimentPref3);
  }
);

// Test that startup prefs are handled correctly when there is a value on the user branch but not the default branch.
decorate_task(
  withPrefEnv({
    set: [
      ["app.normandy.startupExperimentPrefs.testing.does-not-exist", "foo"],
      ["testing.does-not-exist", "foo"],
    ],
  }),
  withStub(PreferenceExperiments, "recordOriginalValues"),
  async function testApplyStartupPrefsNoDefaultValue() {
    Normandy.applyStartupPrefs("app.normandy.startupExperimentPrefs");
    ok(
      true,
      "initExperimentPrefs should not throw for prefs that doesn't exist on the default branch"
    );
  }
);

decorate_task(withStubInits(), async function testStartup() {
  const initObserved = TestUtils.topicObserved("shield-init-complete");
  await Normandy.finishInit();
  ok(AddonStudies.init.called, "startup calls AddonStudies.init");
  ok(
    PreferenceExperiments.init.called,
    "startup calls PreferenceExperiments.init"
  );
  ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
  await initObserved;
});

decorate_task(withStubInits(), async function testStartupPrefInitFail() {
  PreferenceExperiments.init.rejects();

  await Normandy.finishInit();
  ok(AddonStudies.init.called, "startup calls AddonStudies.init");
  ok(AddonRollouts.init.called, "startup calls AddonRollouts.init");
  ok(
    PreferenceExperiments.init.called,
    "startup calls PreferenceExperiments.init"
  );
  ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
  ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  ok(PreferenceRollouts.init.called, "startup calls PreferenceRollouts.init");
});

decorate_task(
  withStubInits(),
  async function testStartupAddonStudiesInitFail() {
    AddonStudies.init.rejects();

    await Normandy.finishInit();
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(AddonRollouts.init.called, "startup calls AddonRollouts.init");
    ok(
      PreferenceExperiments.init.called,
      "startup calls PreferenceExperiments.init"
    );
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
    ok(PreferenceRollouts.init.called, "startup calls PreferenceRollouts.init");
  }
);

decorate_task(
  withStubInits(),
  async function testStartupTelemetryEventsInitFail() {
    TelemetryEvents.init.throws();

    await Normandy.finishInit();
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(AddonRollouts.init.called, "startup calls AddonRollouts.init");
    ok(
      PreferenceExperiments.init.called,
      "startup calls PreferenceExperiments.init"
    );
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
    ok(PreferenceRollouts.init.called, "startup calls PreferenceRollouts.init");
  }
);

decorate_task(
  withStubInits(),
  async function testStartupPreferenceRolloutsInitFail() {
    PreferenceRollouts.init.throws();

    await Normandy.finishInit();
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(AddonRollouts.init.called, "startup calls AddonRollouts.init");
    ok(
      PreferenceExperiments.init.called,
      "startup calls PreferenceExperiments.init"
    );
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
    ok(PreferenceRollouts.init.called, "startup calls PreferenceRollouts.init");
  }
);

// Test that disabling telemetry removes all stored enrollment IDs
decorate_task(
  PreferenceExperiments.withMockExperiments([
    factories.preferenceStudyFactory({
      enrollmentId: "test-enrollment-id",
    }),
  ]),
  AddonStudies.withStudies([
    factories.addonStudyFactory({ slug: "test-study" }),
  ]),
  PreferenceRollouts.withTestMock(),
  AddonRollouts.withTestMock(),
  async function disablingTelemetryClearsEnrollmentIds(
    [prefExperiment],
    [addonStudy]
  ) {
    const prefRollout = {
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ACTIVE,
      preferences: [],
      enrollmentId: "test-enrollment-id",
    };
    await PreferenceRollouts.add(prefRollout);
    const addonRollout = {
      slug: "test-rollout",
      state: AddonRollouts.STATE_ACTIVE,
      extension: {},
      enrollmentId: "test-enrollment-id",
    };
    await AddonRollouts.add(addonRollout);

    // pre-check
    ok(
      (await PreferenceExperiments.get(prefExperiment.slug)).enrollmentId,
      "pref experiment should have an enrollment id"
    );
    ok(
      (await AddonStudies.get(addonStudy.recipeId)).enrollmentId,
      "addon study should have an enrollment id"
    );
    ok(
      (await PreferenceRollouts.get(prefRollout.slug)).enrollmentId,
      "pref rollout should have an enrollment id"
    );
    ok(
      (await AddonRollouts.get(addonRollout.slug)).enrollmentId,
      "addon rollout should have an enrollment id"
    );

    // trigger telemetry being disabled
    await Normandy.observe(
      null,
      TelemetryUtils.TELEMETRY_UPLOAD_DISABLED_TOPIC,
      null
    );

    // no enrollment IDs anymore
    is(
      (await PreferenceExperiments.get(prefExperiment.slug)).enrollmentId,
      TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      "pref experiment should not have an enrollment id"
    );
    is(
      (await AddonStudies.get(addonStudy.recipeId)).enrollmentId,
      TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      "addon study should not have an enrollment id"
    );
    is(
      (await PreferenceRollouts.get(prefRollout.slug)).enrollmentId,
      TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      "pref rollout should not have an enrollment id"
    );
    is(
      (await AddonRollouts.get(addonRollout.slug)).enrollmentId,
      TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      "addon rollout should not have an enrollment id"
    );
  }
);
