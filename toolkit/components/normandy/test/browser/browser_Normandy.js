"use strict";

ChromeUtils.import("resource://normandy/Normandy.jsm", this);
ChromeUtils.import("resource://normandy/lib/ShieldRecipeClient.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);

const initPref1 = "test.initShieldPrefs1";
const initPref2 = "test.initShieldPrefs2";
const initPref3 = "test.initShieldPrefs3";

const experimentPref1 = "test.initExperimentPrefs1";
const experimentPref2 = "test.initExperimentPrefs2";
const experimentPref3 = "test.initExperimentPrefs3";
const experimentPref4 = "test.initExperimentPrefs4";

decorate_task(
  async function testInitShieldPrefs() {
    const defaultBranch = Services.prefs.getDefaultBranch("");

    const prefDefaults = {
      [initPref1]: true,
      [initPref2]: 2,
      [initPref3]: "string",
    };

    for (const pref of Object.keys(prefDefaults)) {
      is(
        defaultBranch.getPrefType(pref),
        defaultBranch.PREF_INVALID,
        `Pref ${pref} don't exist before being initialized.`,
      );
    }

    Normandy.initShieldPrefs(prefDefaults);

    ok(
      defaultBranch.getBoolPref(initPref1),
      `Pref ${initPref1} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getIntPref(initPref2),
      2,
      `Pref ${initPref2} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getCharPref(initPref3),
      "string",
      `Pref ${initPref3} has a default value after being initialized.`,
    );

    for (const pref of Object.keys(prefDefaults)) {
      ok(
        !defaultBranch.prefHasUserValue(pref),
        `Pref ${pref} doesn't have a user value after being initialized.`,
      );
    }

    defaultBranch.deleteBranch("test.");
  },
);

decorate_task(
  async function testInitShieldPrefsError() {
    Assert.throws(
      () => Normandy.initShieldPrefs({"test.prefTypeError": new Date()}),
      "initShieldPrefs throws when given an invalid type for the pref value.",
    );
  },
);

decorate_task(
  withPrefEnv({
    set: [
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref1}`, true],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref2}`, 2],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref3}`, "string"],
    ],
    clear: [[experimentPref1], [experimentPref2], [experimentPref3]],
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
    }
  },
);

decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.startupExperimentPrefs.test.existingPref", "experiment"],
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
      ["extensions.shield-recipe-client.startupExperimentPrefs.test.mismatchPref", "experiment"],
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
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref1}`, true],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref2}`, 2],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref3}`, "string"],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref4}`, "another string"],
    ],
    clear: [
      [experimentPref1],
      [experimentPref2],
      [experimentPref3],
      [experimentPref4],
      ["extensions.shield-recipe-client.startupExperimentPrefs.existingPref"],
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
  },
);

// Test that startup prefs are handled correctly when there is a value on the user branch but not the default branch.
decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.startupExperimentPrefs.testing.does-not-exist", "foo"],
      ["testing.does-not-exist", "foo"],
    ],
  }),
  withStub(PreferenceExperiments, "recordOriginalValues"),
  async function testInitExperimentPrefsNoDefaultValue() {
    Normandy.initExperimentPrefs();
    ok(true, "initExperimentPrefs should not throw for non-existant prefs");
  },
);
