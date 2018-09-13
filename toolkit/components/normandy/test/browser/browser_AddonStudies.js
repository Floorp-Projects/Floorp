"use strict";

ChromeUtils.import("resource://gre/modules/IndexedDB.jsm", this);
ChromeUtils.import("resource://gre/modules/AddonManager.jsm", this);
ChromeUtils.import("resource://testing-common/TestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);

// Initialize test utils
AddonTestUtils.initMochitest(this);

let _startArgsFactoryId = 1;
function startArgsFactory(args) {
  return Object.assign({
    recipeId: _startArgsFactoryId++,
    name: "Test",
    description: "Test",
    addonUrl: "http://test/addon.xpi",
  }, args);
}

decorate_task(
  AddonStudies.withStudies(),
  async function testGetMissing() {
    is(
      await AddonStudies.get("does-not-exist"),
      null,
      "get returns null when the requested study does not exist"
    );
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({name: "test-study"}),
  ]),
  async function testGet([study]) {
    const storedStudy = await AddonStudies.get(study.recipeId);
    Assert.deepEqual(study, storedStudy, "get retrieved a study from storage.");
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory(),
    studyFactory(),
  ]),
  async function testGetAll(studies) {
    const storedStudies = await AddonStudies.getAll();
    Assert.deepEqual(
      new Set(storedStudies),
      new Set(studies),
      "getAll returns every stored study.",
    );
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({name: "test-study"}),
  ]),
  async function testHas([study]) {
    let hasStudy = await AddonStudies.has(study.recipeId);
    ok(hasStudy, "has returns true for a study that exists in storage.");

    hasStudy = await AddonStudies.has("does-not-exist");
    ok(!hasStudy, "has returns false for a study that doesn't exist in storage.");
  }
);

decorate_task(
  AddonStudies.withStudies(),
  async function testCloseDatabase() {
    await AddonStudies.close();
    const openSpy = sinon.spy(IndexedDB, "open");
    sinon.assert.notCalled(openSpy);

    // Using studies at all should open the database, but only once.
    await AddonStudies.has("foo");
    await AddonStudies.get("foo");
    sinon.assert.calledOnce(openSpy);

    // close can be called multiple times
    await AddonStudies.close();
    await AddonStudies.close();

    // After being closed, new operations cause the database to be opened again
    await AddonStudies.has("test-study");
    sinon.assert.calledTwice(openSpy);

    openSpy.restore();
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({name: "test-study1"}),
    studyFactory({name: "test-study2"}),
  ]),
  async function testClear([study1, study2]) {
    const hasAll = (
      (await AddonStudies.has(study1.recipeId)) &&
      (await AddonStudies.has(study2.recipeId))
    );
    ok(hasAll, "Before calling clear, both studies are in storage.");

    await AddonStudies.clear();
    const hasAny = (
      (await AddonStudies.has(study1.recipeId)) ||
      (await AddonStudies.has(study2.recipeId))
    );
    ok(!hasAny, "After calling clear, all studies are removed from storage.");
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({active: true, addonId: "does.not.exist@example.com", studyEndDate: null}),
    studyFactory({active: true, addonId: "installed@example.com"}),
    studyFactory({active: false, addonId: "already.gone@example.com", studyEndDate: new Date(2012, 1)}),
  ]),
  withSendEventStub,
  withInstalledWebExtension({id: "installed@example.com"}),
  async function testInit([activeUninstalledStudy, activeInstalledStudy, inactiveStudy], sendEventStub) {
    await AddonStudies.init();

    const newActiveStudy = await AddonStudies.get(activeUninstalledStudy.recipeId);
    ok(!newActiveStudy.active, "init marks studies as inactive if their add-on is not installed.");
    ok(
      newActiveStudy.studyEndDate,
      "init sets the study end date if a study's add-on is not installed."
    );
    Assert.deepEqual(
      sendEventStub.getCall(0).args,
      ["unenroll", "addon_study", activeUninstalledStudy.name, {
        addonId: activeUninstalledStudy.addonId,
        addonVersion: activeUninstalledStudy.addonVersion,
        reason: "uninstalled-sideload",
      }],
      "AddonStudies.init() should send the correct telemetry event"
    );

    const newInactiveStudy = await AddonStudies.get(inactiveStudy.recipeId);
    is(
      newInactiveStudy.studyEndDate.getFullYear(),
      2012,
      "init does not modify inactive studies."
    );

    const newActiveInstalledStudy = await AddonStudies.get(activeInstalledStudy.recipeId);
    Assert.deepEqual(
      activeInstalledStudy,
      newActiveInstalledStudy,
      "init does not modify studies whose add-on is still installed."
    );

    // Only activeUninstalledStudy should have generated any events
    ok(sendEventStub.calledOnce, "no extra events should be generated");
  }
);

decorate_task(
  AddonStudies.withStudies([
    studyFactory({active: true, addonId: "installed@example.com", studyEndDate: null}),
  ]),
  withInstalledWebExtension({id: "installed@example.com"}, /* expectUninstall: */ true),
  async function testInit([study], [id, addonFile]) {
    const addon = await AddonManager.getAddonByID(id);
    await addon.uninstall();
    await TestUtils.topicObserved("shield-study-ended");

    const newStudy = await AddonStudies.get(study.recipeId);
    ok(!newStudy.active, "Studies are marked as inactive when their add-on is uninstalled.");
    ok(
      newStudy.studyEndDate,
      "The study end date is set when the add-on for the study is uninstalled."
    );
  }
);
