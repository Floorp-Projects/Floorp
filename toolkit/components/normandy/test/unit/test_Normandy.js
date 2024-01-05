/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Normandy } = ChromeUtils.importESModule(
  "resource://normandy/Normandy.sys.mjs"
);
const { NormandyMigrations } = ChromeUtils.importESModule(
  "resource://normandy/NormandyMigrations.sys.mjs"
);
ChromeUtils.defineESModuleGetters(this, {
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

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

    let initPromise = Normandy.init({ runAsync: false });

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

// Normandy's initialization function should register the observer for UI
// startup before it's first await.
decorate_task(
  NormandyTestUtils.withStub(Normandy, "finishInit"),
  NormandyTestUtils.withStub(NormandyMigrations, "applyAll"),
  async function test_normandy_init_applies_startup_prefs_synchronously({
    applyAllStub,
  }) {
    let originalDeferred = Normandy.uiAvailableNotificationObserved;
    let mockUiAvailableDeferred = Promise.withResolvers();
    Normandy.uiAvailableNotificationObserved = mockUiAvailableDeferred;

    let applyAllDeferred = Promise.withResolvers();
    applyAllStub.returns(applyAllStub);

    let promiseResolvedCount = 0;
    mockUiAvailableDeferred.promise.then(() => promiseResolvedCount++);

    let initPromise = Normandy.init();

    Assert.equal(promiseResolvedCount, 0);
    Normandy.observe(null, "sessionstore-windows-restored");
    await TestUtils.waitForCondition(() => promiseResolvedCount === 1);

    applyAllDeferred.resolve();

    await initPromise;
    await Normandy.uninit();
    Normandy.uiAvailableNotificationObserved = originalDeferred;
  }
);
