"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://normandy/actions/AddonStudyAction.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

const FIXTURE_ADDON_ID = "normandydriver@example.com";
const FIXTURE_ADDON_URL = "http://example.com/browser/toolkit/components/normandy/test/browser/fixtures/normandy.xpi";

function addonStudyRecipeFactory(overrides = {}) {
  let args = {
    name: "Fake name",
    description: "fake description",
    addonUrl: "https://example.com/study.xpi",
  };
  if (Object.hasOwnProperty.call(overrides, "arguments")) {
    args = Object.assign(args, overrides.arguments);
    delete overrides.arguments;
  }
  return recipeFactory(Object.assign({ action: "addon-study", arguments: args }, overrides));
}

/**
 * Test decorator that checks that the test cleans up all add-ons installed
 * during the test. Likely needs to be the first decorator used.
 */
function ensureAddonCleanup(testFunction) {
  return async function wrappedTestFunction(...args) {
    const beforeAddons = new Set(await AddonManager.getAllAddons());

    try {
      await testFunction(...args);
    } finally {
      const afterAddons = new Set(await AddonManager.getAllAddons());
      Assert.deepEqual(beforeAddons, afterAddons, "The add-ons should be same before and after the test");
    }
  };
}

// Test that enroll is not called if recipe is already enrolled
decorate_task(
  ensureAddonCleanup,
  AddonStudies.withStudies([addonStudyFactory()]),
  withSendEventStub,
  async function enrollTwiceFail([study], sendEventStub) {
    const recipe = recipeFactory({
      id: study.recipeId,
      type: "addon-study",
      arguments: {
        name: study.name,
        description: study.description,
        addonUrl: study.addonUrl,
      },
    });
    const action = new AddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    await action.runRecipe(recipe);
    Assert.deepEqual(enrollSpy.args, [], "enroll should not be called");
    Assert.deepEqual(sendEventStub.args, [], "no events should be sent");
  },
);

// Test that if the add-on fails to install, the database is cleaned up and the
// error is correctly reported.
decorate_task(
  ensureAddonCleanup,
  withSendEventStub,
  AddonStudies.withStudies([]),
  async function enrollFailInstall(sendEventStub) {
    const recipe = addonStudyRecipeFactory({ arguments: { addonUrl: "https://example.com/404.xpi" }});
    const action = new AddonStudyAction();
    await action.enroll(recipe);

    const studies = await AddonStudies.getAll();
    Assert.deepEqual(studies, [], "the study should not be in the database");

    Assert.deepEqual(
      sendEventStub.args,
      [["enrollFailed", "addon_study", recipe.arguments.name, {reason: "download-failure"}]],
      "An enrollFailed event should be sent",
    );
  }
);

// Test that in the case of a study add-on conflicting with a non-study add-on, the study does not enroll
decorate_task(
  ensureAddonCleanup,
  AddonStudies.withStudies([]),
  withSendEventStub,
  withInstalledWebExtension({ version: "0.1", id: FIXTURE_ADDON_ID }),
  async function conflictingEnrollment(studies, sendEventStub, [installedAddonId, installedAddonFile]) {
    is(installedAddonId, FIXTURE_ADDON_ID, "Generated, installed add-on should have the same ID as the fixture");
    const addonUrl = FIXTURE_ADDON_URL;
    const recipe = addonStudyRecipeFactory({ arguments: { name: "conflicting", addonUrl } });
    const action = new AddonStudyAction();
    await action.runRecipe(recipe);

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.version, "0.1", "The installed add-on should not be replaced");

    Assert.deepEqual(await AddonStudies.getAll(), [], "There should be no enrolled studies");

    Assert.deepEqual(
      sendEventStub.args,
      [["enrollFailed", "addon_study", recipe.arguments.name, { reason: "conflicting-addon-id" }]],
      "A enrollFailed event should be sent",
    );
  },
);

// Test a successful enrollment
decorate_task(
  ensureAddonCleanup,
  withSendEventStub,
  AddonStudies.withStudies(),
  async function successfulEnroll(sendEventStub, studies) {
    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(FIXTURE_ADDON_ID);
    const addonUrl = FIXTURE_ADDON_URL;

    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon, null, "Before enroll, the add-on is not installed");

    const recipe = addonStudyRecipeFactory({ arguments: { name: "success", addonUrl } });
    const action = new AddonStudyAction();
    await action.runRecipe(recipe);

    await webExtStartupPromise;
    addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon, "After start is called, the add-on is installed");

    const study = await AddonStudies.get(recipe.id);
    Assert.deepEqual(
      study,
      {
        recipeId: recipe.id,
        name: recipe.arguments.name,
        description: recipe.arguments.description,
        addonId: FIXTURE_ADDON_ID,
        addonVersion: "1.0",
        addonUrl,
        active: true,
        studyStartDate: study.studyStartDate,
      },
      "study data should be stored",
    );
    ok(study.studyStartDate, "a start date should be assigned");
    is(study.studyEndDate, null, "an end date should not be assigned");

    Assert.deepEqual(
      sendEventStub.args,
      [["enroll", "addon_study", recipe.arguments.name, { addonId: FIXTURE_ADDON_ID, addonVersion: "1.0" }]],
      "an enrollment event should be sent",
    );

    // cleanup
    await addon.uninstall();
  },
);

// Test that unenrolling fails if the study doesn't exist
decorate_task(
  ensureAddonCleanup,
  AddonStudies.withStudies(),
  async function unenrollNonexistent(studies) {
    const action = new AddonStudyAction();
    await Assert.rejects(
      action.unenroll(42),
      /no study found/i,
      "unenroll should fail when no study exists"
    );
  }
);

// Test that unenrolling an inactive experiment fails
decorate_task(
  ensureAddonCleanup,
  AddonStudies.withStudies([
    addonStudyFactory({active: false}),
  ]),
  withSendEventStub,
  async ([study], sendEventStub) => {
    const action = new AddonStudyAction();
    await Assert.rejects(
      action.unenroll(study.recipeId),
      /cannot stop study.*already inactive/i,
      "unenroll should fail when the requested study is inactive"
    );
  }
);

// test a successful unenrollment
const testStopId = "testStop@example.com";
decorate_task(
  ensureAddonCleanup,
  AddonStudies.withStudies([
    addonStudyFactory({active: true, addonId: testStopId, studyEndDate: null}),
  ]),
  withInstalledWebExtension({id: testStopId}, /* expectUninstall: */ true),
  withSendEventStub,
  async function unenrollTest([study], [addonId, addonFile], sendEventStub) {
    let addon = await AddonManager.getAddonByID(addonId);
    ok(addon, "the add-on should be installed before unenrolling");

    const action = new AddonStudyAction();
    await action.unenroll(study.recipeId, "test-reason");

    const newStudy = AddonStudies.get(study.recipeId);
    is(!newStudy, false, "stop should mark the study as inactive");
    ok(newStudy.studyEndDate !== null, "the study should have an end date");

    addon = await AddonManager.getAddonByID(addonId);
    is(addon, null, "the add-on should be uninstalled after unenrolling");

    Assert.deepEqual(
      sendEventStub.args,
      [["unenroll", "addon_study", study.name, {
        addonId,
        addonVersion: study.addonVersion,
        reason: "test-reason",
      }]],
      "an unenroll event should be sent",
    );
  },
);

// If the add-on for a study isn't installed, a warning should be logged, but the action is still successful
decorate_task(
  ensureAddonCleanup,
  AddonStudies.withStudies([
    addonStudyFactory({active: true, addonId: "missingAddon@example.com", studyEndDate: null}),
  ]),
  withSendEventStub,
  async function unenrollTest([study], sendEventStub) {
    const action = new AddonStudyAction();

    SimpleTest.waitForExplicitFinish();
    SimpleTest.monitorConsole(() => SimpleTest.finish(), [{message: /could not uninstall addon/i}]);
    await action.unenroll(study.recipeId);

    Assert.deepEqual(
      sendEventStub.args,
      [["unenroll", "addon_study", study.name, {
        addonId: study.addonId,
        addonVersion: study.addonVersion,
        reason: "unknown",
      }]],
      "an unenroll event should be sent",
    );

    SimpleTest.endMonitorConsole();
  },
);

// Test that the action respects the study opt-out
decorate_task(
  ensureAddonCleanup,
  withSendEventStub,
  withMockPreferences,
  AddonStudies.withStudies([]),
  async function testOptOut(sendEventStub, mockPreferences) {
    mockPreferences.set("app.shield.optoutstudies.enabled", false);
    const action = new AddonStudyAction();
    is(action.state, AddonStudyAction.STATE_DISABLED, "the action should be disabled");
    const enrollSpy = sinon.spy(action, "enroll");
    const recipe = addonStudyRecipeFactory();
    await action.runRecipe(recipe);
    await action.finalize();
    is(action.state, AddonStudyAction.STATE_FINALIZED, "the action should be finalized");
    Assert.deepEqual(enrollSpy.args, [], "enroll should not be called");
    Assert.deepEqual(sendEventStub.args, [], "no events should be sent");
  },
);

// Test that the action does not execute paused recipes
decorate_task(
  ensureAddonCleanup,
  withSendEventStub,
  AddonStudies.withStudies([]),
  async function testOptOut(sendEventStub) {
    const action = new AddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const recipe = addonStudyRecipeFactory({arguments: {isEnrollmentPaused: true}});
    await action.runRecipe(recipe);
    await action.finalize();
    Assert.deepEqual(enrollSpy.args, [], "enroll should not be called");
    Assert.deepEqual(sendEventStub.args, [], "no events should be sent");
  },
);

// Test that enroll is not called if recipe is already enrolled
decorate_task(
  ensureAddonCleanup,
  AddonStudies.withStudies([addonStudyFactory()]),
  async function enrollTwiceFail([study]) {
    const action = new AddonStudyAction();
    const unenrollSpy = sinon.stub(action, "unenroll");
    await action.finalize();
    Assert.deepEqual(unenrollSpy.args, [[study.recipeId, "recipe-not-seen"]], "unenroll should be called");
  },
);
