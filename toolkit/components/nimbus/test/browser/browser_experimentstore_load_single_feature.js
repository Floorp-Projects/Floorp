/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentStore } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentStore.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { NimbusFeatures, ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);

const SINGLE_FEATURE_RECIPE = {
  ...ExperimentFakes.experiment(),
  branch: {
    feature: {
      featureId: "urlbar",
      value: {
        valueThatWillDefinitelyShowUp: 42,
        quickSuggestNonSponsoredIndex: 2021,
      },
    },
    ratio: 1,
    slug: "control",
  },
  featureIds: ["urlbar"],
  slug: "browser_experimentstore_load_single_feature",
  userFacingDescription: "Smarter suggestions in the AwesomeBar",
  userFacingName: "Firefox Suggest - History vs Offline",
};

function getPath() {
  const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
  // NOTE: If this test is failing because you have updated this path in `ExperimentStore`,
  // users will lose their old experiment data. You should do something to migrate that data.
  return PathUtils.join(profileDir, "ExperimentStoreData.json");
}

add_task(async function test_load_from_disk_event() {
  Services.prefs.setStringPref("messaging-system.log", "all");
  const stub = sinon.stub();
  const previousSession = new JSONFile({ path: getPath() });
  await previousSession.load();
  previousSession.data[SINGLE_FEATURE_RECIPE.slug] = SINGLE_FEATURE_RECIPE;
  previousSession.saveSoon();
  await previousSession.finalize();

  // Create a store and expect to load data from previous session
  const store = new ExperimentStore();

  let apiStoreStub = sinon.stub(ExperimentAPI, "_store").get(() => store);

  store._onFeatureUpdate("urlbar", stub);

  await store.init();
  await store.ready();

  await TestUtils.waitForCondition(() => stub.called, "Stub was called");
  Assert.ok(
    store.get(SINGLE_FEATURE_RECIPE.slug)?.slug,
    "Experiment is loaded from disk"
  );
  Assert.ok(stub.firstCall.args[1], "feature-experiment-loaded");
  Assert.equal(
    NimbusFeatures.urlbar.getAllVariables().valueThatWillDefinitelyShowUp,
    SINGLE_FEATURE_RECIPE.branch.feature.value.valueThatWillDefinitelyShowUp,
    "Should match getAllVariables"
  );
  Assert.equal(
    NimbusFeatures.urlbar.getVariable("quickSuggestNonSponsoredIndex"),
    SINGLE_FEATURE_RECIPE.branch.feature.value.quickSuggestNonSponsoredIndex,
    "Should match getVariable"
  );

  registerCleanupFunction(async () => {
    // Remove the experiment from disk
    const fileStore = new JSONFile({ path: getPath() });
    await fileStore.load();
    fileStore.data = {};
    fileStore.saveSoon();
    await fileStore.finalize();
    apiStoreStub.restore();
  });
});
