/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Normandy } = ChromeUtils.import("resource://normandy/Normandy.jsm");
const { NormandyMigrations } = ChromeUtils.import(
  "resource://normandy/NormandyMigrations.jsm"
);

/* import-globals-from utils.js */
load("utils.js");

NormandyTestUtils.init({ add_task });
const { decorate_task } = NormandyTestUtils;

// Normandy's initialization function should set the start preferences before
// its first `await`.
decorate_task(
  NormandyTestUtils.withStub(Normandy, "finishInit"),
  NormandyTestUtils.withStub(NormandyMigrations, "applyAll"),
  NormandyTestUtils.withMockPreferences(),
  async function test_normandy_init_applies_startup_prefs_synchronously({
    mockPreferences,
  }) {
    const experimentPref = "test.experiment";
    const rolloutPref = "test.rollout";
    const experimentStartupPref = `app.normandy.startupExperimentPrefs.${experimentPref}`;
    const rolloutStartupPref = `app.normandy.startupRolloutPrefs.${rolloutPref}`;

    mockPreferences.preserve(experimentPref, "default");
    mockPreferences.preserve(rolloutPref, "default");
    mockPreferences.set(experimentStartupPref, "experiment");
    mockPreferences.set(rolloutStartupPref, "rollout");

    Assert.equal(
      Services.prefs.getCharPref(experimentPref, "default"),
      "default"
    );
    Assert.equal(Services.prefs.getCharPref(rolloutPref, "default"), "default");

    let initPromise = Normandy.init();

    // note: There are no awaits before these asserts, so only the part of
    // Normandy's initialization before its first await can run.
    Assert.equal(
      Services.prefs.getCharPref(experimentPref, "default"),
      "experiment"
    );
    Assert.equal(Services.prefs.getCharPref(rolloutPref, "default"), "rollout");

    await initPromise;
    await Normandy.uninit();
  }
);
