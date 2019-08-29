"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);
ChromeUtils.import("resource://normandy/lib/ShieldPreferences.jsm", this);

const OPT_OUT_STUDIES_ENABLED_PREF = "app.shield.optoutstudies.enabled";

const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const {
  addonStudyFactory,
  preferenceStudyFactory,
} = NormandyTestUtils.factories;

ShieldPreferences.init();

decorate_task(
  withMockPreferences,
  AddonStudies.withStudies([
    addonStudyFactory({ active: true }),
    addonStudyFactory({ active: true }),
  ]),
  async function testDisableStudiesWhenOptOutDisabled(
    mockPreferences,
    [study1, study2]
  ) {
    mockPreferences.set(OPT_OUT_STUDIES_ENABLED_PREF, true);
    const observers = [
      studyEndObserved(study1.recipeId),
      studyEndObserved(study2.recipeId),
    ];
    Services.prefs.setBoolPref(OPT_OUT_STUDIES_ENABLED_PREF, false);
    await Promise.all(observers);

    const newStudy1 = await AddonStudies.get(study1.recipeId);
    const newStudy2 = await AddonStudies.get(study2.recipeId);
    ok(
      !newStudy1.active && !newStudy2.active,
      "Setting the opt-out pref to false stops all active opt-out studies."
    );
  }
);

decorate_task(
  withMockPreferences,
  PreferenceExperiments.withMockExperiments([
    preferenceStudyFactory({ active: true }),
    preferenceStudyFactory({ active: true }),
  ]),
  withStub(PreferenceExperiments, "stop"),
  async function testDisableExperimentsWhenOptOutDisabled(
    mockPreferences,
    [study1, study2],
    stopStub
  ) {
    mockPreferences.set(OPT_OUT_STUDIES_ENABLED_PREF, true);
    let stopArgs = [];
    let stoppedBoth = new Promise(resolve => {
      let calls = 0;
      stopStub.callsFake(function() {
        stopArgs.push(Array.from(arguments));
        calls++;
        if (calls == 2) {
          resolve();
        }
      });
    });
    Services.prefs.setBoolPref(OPT_OUT_STUDIES_ENABLED_PREF, false);
    await stoppedBoth;

    Assert.deepEqual(stopArgs, [
      [study1.slug, { reason: "general-opt-out" }],
      [study2.slug, { reason: "general-opt-out" }],
    ]);
  }
);
