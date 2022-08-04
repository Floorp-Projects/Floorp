/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentAPI, NimbusFeatures } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);

const SINGLE_FEATURE_RECIPE = {
  appId: "firefox-desktop",
  appName: "firefox_desktop",
  arguments: {},
  branches: [
    {
      feature: {
        featureId: "urlbar",
        isEarlyStartup: true,
        value: {
          enabled: true,
          quickSuggestEnabled: false,
          quickSuggestNonSponsoredIndex: -1,
          quickSuggestShouldShowOnboardingDialog: true,
          quickSuggestShowOnboardingDialogAfterNRestarts: 2,
          quickSuggestSponsoredIndex: -1,
        },
      },
      ratio: 1,
      slug: "control",
    },
    {
      feature: {
        featureId: "urlbar",
        isEarlyStartup: true,
        value: {
          enabled: true,
          quickSuggestEnabled: true,
          quickSuggestNonSponsoredIndex: -1,
          quickSuggestShouldShowOnboardingDialog: false,
          quickSuggestShowOnboardingDialogAfterNRestarts: 2,
          quickSuggestSponsoredIndex: -1,
        },
      },
      ratio: 1,
      slug: "treatment",
    },
  ],
  bucketConfig: {
    count: 10000,
    namespace: "urlbar-9",
    randomizationUnit: "normandy_id",
    start: 0,
    total: 10000,
  },
  channel: "release",
  endDate: null,
  featureIds: ["urlbar"],
  id: "firefox-suggest-history-vs-offline",
  isEnrollmentPaused: false,
  outcomes: [],
  probeSets: [],
  proposedDuration: 28,
  proposedEnrollment: 7,
  referenceBranch: "control",
  schemaVersion: "1.5.0",
  slug: "firefox-suggest-history-vs-offline",
  startDate: "2021-07-21",
  targeting: "true",
  userFacingDescription: "Smarter suggestions in the AwesomeBar",
  userFacingName: "Firefox Suggest - History vs Offline",
};

const SYNC_DATA_PREF_BRANCH = "nimbus.syncdatastore.";

add_task(async function test_TODO() {
  let {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(SINGLE_FEATURE_RECIPE);
  let sandbox = sinon.createSandbox();
  let stub = sandbox.stub(ExperimentAPI, "recordExposureEvent");

  await enrollmentPromise;

  Assert.ok(
    ExperimentAPI.getExperiment({ featureId: "urlbar" }),
    "Should enroll in single feature experiment"
  );

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DATA_PREF_BRANCH}urlbar`),
    "Should store early startup feature for sync access"
  );
  Assert.equal(
    Services.prefs.getIntPref(
      `${SYNC_DATA_PREF_BRANCH}urlbar.quickSuggestSponsoredIndex`
    ),
    -1,
    "Should store early startup variable for sync access"
  );

  Assert.equal(
    NimbusFeatures.urlbar.getVariable(
      "quickSuggestShowOnboardingDialogAfterNRestarts"
    ),
    2,
    "Should return value"
  );

  NimbusFeatures.urlbar.recordExposureEvent();

  Assert.ok(stub.calledOnce, "Should be called once by urlbar");
  Assert.equal(
    stub.firstCall.args[0].experimentSlug,
    "firefox-suggest-history-vs-offline",
    "Should have expected slug"
  );
  Assert.equal(
    stub.firstCall.args[0].featureId,
    "urlbar",
    "Should have expected featureId"
  );

  await doExperimentCleanup();
  sandbox.restore();
  NimbusFeatures.urlbar._didSendExposureEvent = false;
});
