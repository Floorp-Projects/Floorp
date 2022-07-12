/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentStore } = ChromeUtils.import(
  "resource://nimbus/lib/ExperimentStore.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { ExperimentFeatures } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);

function getPath() {
  const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
  // NOTE: If this test is failing because you have updated this path in `ExperimentStore`,
  // users will lose their old experiment data. You should do something to migrate that data.
  return PathUtils.join(profileDir, "ExperimentStoreData.json");
}

// Ensure that data persisted to disk is succesfully loaded by the store.
// We write data to the expected location in the user profile and
// instantiate an ExperimentStore that should then see the value.
add_task(async function test_loadFromFile() {
  const previousSession = new JSONFile({ path: getPath() });
  await previousSession.load();
  previousSession.data.test = {
    slug: "test",
    active: true,
    lastSeen: Date.now(),
  };
  previousSession.saveSoon();
  await previousSession.finalize();

  // Create a store and expect to load data from previous session
  const store = new ExperimentStore();

  await store.init();
  await store.ready();

  Assert.equal(
    previousSession.path,
    store._store.path,
    "Should have the same path"
  );

  Assert.ok(
    store.get("test"),
    "This should pass if the correct store path loaded successfully"
  );
});

add_task(async function test_load_from_disk_event() {
  const experiment = ExperimentFakes.experiment("foo", {
    branch: {
      slug: "variant",
      features: [{ featureId: "green" }],
    },
    lastSeen: Date.now(),
  });
  const stub = sinon.stub();
  const previousSession = new JSONFile({ path: getPath() });
  await previousSession.load();
  previousSession.data.foo = experiment;
  previousSession.saveSoon();
  await previousSession.finalize();

  // Create a store and expect to load data from previous session
  const store = new ExperimentStore();

  store._onFeatureUpdate("green", stub);

  await store.init();
  await store.ready();

  Assert.equal(
    previousSession.path,
    store._store.path,
    "Should have the same path as previousSession."
  );

  await TestUtils.waitForCondition(() => stub.called, "Stub was called");

  Assert.ok(stub.firstCall.args[1], "feature-experiment-loaded");
});
