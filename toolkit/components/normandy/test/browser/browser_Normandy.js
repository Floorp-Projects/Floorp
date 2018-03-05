"use strict";

ChromeUtils.import("resource://normandy/Normandy.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);
ChromeUtils.import("resource://normandy/lib/RecipeRunner.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);
ChromeUtils.import("resource://normandy-content/AboutPages.jsm", this);

const experimentPref1 = "test.initExperimentPrefs1";
const experimentPref2 = "test.initExperimentPrefs2";
const experimentPref3 = "test.initExperimentPrefs3";
const experimentPref4 = "test.initExperimentPrefs4";

function withStubInits(testFunction) {
  return decorate(
    withStub(AboutPages, "init"),
    withStub(AddonStudies, "init"),
    withStub(PreferenceExperiments, "init"),
    withStub(RecipeRunner, "init"),
    withStub(TelemetryEvents, "init"),
    testFunction
  );
}

decorate_task(
  withPrefEnv({
    set: [
      [`app.normandy.startupExperimentPrefs.${experimentPref1}`, true],
      [`app.normandy.startupExperimentPrefs.${experimentPref2}`, 2],
      [`app.normandy.startupExperimentPrefs.${experimentPref3}`, "string"],
    ],
  }),
  async function testInitExperimentPrefs() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    for (const pref of [experimentPref1, experimentPref2, experimentPref3]) {
      is(
        defaultBranch.getPrefType(pref),
        defaultBranch.PREF_INVALID,
        `Pref ${pref} don't exist before being initialized.`,
      );
    }

    Normandy.initExperimentPrefs();

    ok(
      defaultBranch.getBoolPref(experimentPref1),
      `Pref ${experimentPref1} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getIntPref(experimentPref2),
      2,
      `Pref ${experimentPref2} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getCharPref(experimentPref3),
      "string",
      `Pref ${experimentPref3} has a default value after being initialized.`,
    );

    for (const pref of [experimentPref1, experimentPref2, experimentPref3]) {
      ok(
        !defaultBranch.prefHasUserValue(pref),
        `Pref ${pref} doesn't have a user value after being initialized.`,
      );
      Services.prefs.clearUserPref(pref);
      defaultBranch.deleteBranch(pref);
    }
  },
);

decorate_task(
  withPrefEnv({
    set: [
      ["app.normandy.startupExperimentPrefs.test.existingPref", "experiment"],
    ],
  }),
  async function testInitExperimentPrefsExisting() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    defaultBranch.setCharPref("test.existingPref", "default");
    Normandy.initExperimentPrefs();
    is(
      defaultBranch.getCharPref("test.existingPref"),
      "experiment",
      "initExperimentPrefs overwrites the default values of existing preferences.",
    );
  },
);

decorate_task(
  withPrefEnv({
    set: [
      ["app.normandy.startupExperimentPrefs.test.mismatchPref", "experiment"],
    ],
  }),
  async function testInitExperimentPrefsMismatch() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    defaultBranch.setIntPref("test.mismatchPref", 2);
    Normandy.initExperimentPrefs();
    is(
      defaultBranch.getPrefType("test.mismatchPref"),
      Services.prefs.PREF_INT,
      "initExperimentPrefs skips prefs that don't match the existing default value's type.",
    );
  },
);

decorate_task(
  withStub(Normandy, "finishInit"),
  async function testStartupDelayed(finishInitStub) {
    Normandy.init();
    ok(
      !finishInitStub.called,
      "When initialized, do not call finishInit immediately.",
    );

    Normandy.observe(null, "sessionstore-windows-restored");
    ok(
      finishInitStub.called,
      "Once the sessionstore-windows-restored event is observed, finishInit should be called.",
    );
  },
);

// During startup, preferences that are changed for experiments should
// be record by calling PreferenceExperiments.recordOriginalValues.
decorate_task(
  withPrefEnv({
    set: [
      [`app.normandy.startupExperimentPrefs.${experimentPref1}`, true],
      [`app.normandy.startupExperimentPrefs.${experimentPref2}`, 2],
      [`app.normandy.startupExperimentPrefs.${experimentPref3}`, "string"],
      [`app.normandy.startupExperimentPrefs.${experimentPref4}`, "another string"],
    ],
  }),
  withStub(PreferenceExperiments, "recordOriginalValues"),
  async function testInitExperimentPrefs(recordOriginalValuesStub) {
    const defaultBranch = Services.prefs.getDefaultBranch("");

    defaultBranch.setBoolPref(experimentPref1, false);
    defaultBranch.setIntPref(experimentPref2, 1);
    defaultBranch.setCharPref(experimentPref3, "original string");
    // experimentPref4 is left unset

    Normandy.initExperimentPrefs();
    await Normandy.finishInit();

    Assert.deepEqual(
      recordOriginalValuesStub.getCall(0).args,
      [{
        [experimentPref1]: false,
        [experimentPref2]: 1,
        [experimentPref3]: "original string",
        [experimentPref4]: null,  // null because it was not initially set.
      }],
      "finishInit should record original values of the prefs initExperimentPrefs changed",
    );

    for (const pref of [experimentPref1, experimentPref2, experimentPref3, experimentPref4]) {
      Services.prefs.clearUserPref(pref);
      defaultBranch.deleteBranch(pref);
    }
  },
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
  async function testInitExperimentPrefsNoDefaultValue() {
    Normandy.initExperimentPrefs();
    ok(true, "initExperimentPrefs should not throw for non-existant prefs");
  },
);

decorate_task(
  withStubInits,
  async function testStartup() {
    const initObserved = TestUtils.topicObserved("shield-init-complete");
    await Normandy.finishInit();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    await initObserved;
  }
);

decorate_task(
  withStubInits,
  async function testStartupPrefInitFail() {
    PreferenceExperiments.init.returns(Promise.reject(new Error("oh no")));

    await Normandy.finishInit();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupAboutPagesInitFail() {
    AboutPages.init.returns(Promise.reject(new Error("oh no")));

    await Normandy.finishInit();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupAddonStudiesInitFail() {
    AddonStudies.init.returns(Promise.reject(new Error("oh no")));

    await Normandy.finishInit();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);

decorate_task(
  withStubInits,
  async function testStartupTelemetryEventsInitFail() {
    TelemetryEvents.init.throws();

    await Normandy.finishInit();
    ok(AboutPages.init.called, "startup calls AboutPages.init");
    ok(AddonStudies.init.called, "startup calls AddonStudies.init");
    ok(PreferenceExperiments.init.called, "startup calls PreferenceExperiments.init");
    ok(RecipeRunner.init.called, "startup calls RecipeRunner.init");
    ok(TelemetryEvents.init.called, "startup calls TelemetryEvents.init");
  }
);

decorate_task(
  withMockPreferences,
  async function testPrefMigration(mockPreferences) {
    const legacyPref = "extensions.shield-recipe-client.test";
    const migratedPref = "app.normandy.test";
    mockPreferences.set(legacyPref, 1);

    ok(
      Services.prefs.prefHasUserValue(legacyPref),
      "Legacy pref should have a user value before running migration",
    );
    ok(
      !Services.prefs.prefHasUserValue(migratedPref),
      "Migrated pref should not have a user value before running migration",
    );

    Normandy.migrateShieldPrefs();

    ok(
      !Services.prefs.prefHasUserValue(legacyPref),
      "Legacy pref should not have a user value after running migration",
    );
    ok(
      Services.prefs.prefHasUserValue(migratedPref),
      "Migrated pref should have a user value after running migration",
    );
    is(Services.prefs.getIntPref(migratedPref), 1, "Value should have been migrated");

    Services.prefs.clearUserPref(migratedPref);
  },
);
