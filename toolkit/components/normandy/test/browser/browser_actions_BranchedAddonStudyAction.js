"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://testing-common/TestUtils.jsm", this);
ChromeUtils.import(
  "resource://normandy/actions/BranchedAddonStudyAction.jsm",
  this
);
ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const { branchedAddonStudyFactory } = NormandyTestUtils.factories;

function branchedAddonStudyRecipeFactory(overrides = {}) {
  let args = {
    slug: "fake-slug",
    userFacingName: "Fake name",
    userFacingDescription: "fake description",
    branches: [
      {
        slug: "a",
        ratio: 1,
        extensionApiId: 1,
      },
    ],
  };
  if (Object.hasOwnProperty.call(overrides, "arguments")) {
    args = Object.assign(args, overrides.arguments);
    delete overrides.arguments;
  }
  return recipeFactory(
    Object.assign(
      {
        action: "branched-addon-study",
        arguments: args,
      },
      overrides
    )
  );
}

function recipeFromStudy(study, overrides = {}) {
  let args = {
    slug: study.slug,
    userFacingName: study.userFacingName,
    branches: [
      {
        slug: "a",
        ratio: 1,
        extensionApiId: study.extensionApiId,
      },
    ],
  };

  if (Object.hasOwnProperty.call(overrides, "arguments")) {
    args = Object.assign(args, overrides.arguments);
    delete overrides.arguments;
  }

  return branchedAddonStudyRecipeFactory(
    Object.assign(
      {
        id: study.recipeId,
        arguments: args,
      },
      overrides
    )
  );
}

// Test that enroll is not called if recipe is already enrolled and update does nothing
// if recipe is unchanged
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([branchedAddonStudyFactory()]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "1.0" }),
  async function enrollTwiceFail(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const recipe = recipeFromStudy(study);
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        hash: study.extensionHash,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(!enrollSpy.called, "enroll should not be called");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([]);
  }
);

// Test that if the add-on fails to install, the database is cleaned up and the
// error is correctly reported.
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  AddonStudies.withStudies(),
  async function enrollDownloadFail(mockApi, sendEventStub) {
    const recipe = branchedAddonStudyRecipeFactory({
      arguments: {
        branches: [{ slug: "missing", ratio: 1, extensionApiId: 404 }],
      },
    });
    mockApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
        xpi: "https://example.com/404.xpi",
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");

    const studies = await AddonStudies.getAll();
    Assert.deepEqual(studies, [], "the study should not be in the database");

    sendEventStub.assertEvents([
      [
        "enrollFailed",
        "addon_study",
        recipe.arguments.name,
        {
          reason: "download-failure",
          detail: "ERROR_NETWORK_FAILURE",
          branch: "missing",
        },
      ],
    ]);
  }
);

// Ensure that the database is clean and error correctly reported if hash check fails
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  AddonStudies.withStudies(),
  async function enrollHashCheckFails(mockApi, sendEventStub) {
    const recipe = branchedAddonStudyRecipeFactory();
    mockApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        hash: "badhash",
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");

    const studies = await AddonStudies.getAll();
    Assert.deepEqual(studies, [], "the study should not be in the database");

    sendEventStub.assertEvents([
      [
        "enrollFailed",
        "addon_study",
        recipe.arguments.name,
        {
          reason: "download-failure",
          detail: "ERROR_INCORRECT_HASH",
          branch: "a",
        },
      ],
    ]);
  }
);

// Ensure that the database is clean and error correctly reported if there is a metadata mismatch
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  AddonStudies.withStudies(),
  async function enrollFailsMetadataMismatch(mockApi, sendEventStub) {
    const recipe = branchedAddonStudyRecipeFactory();
    mockApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
        version: "1.5",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");

    const studies = await AddonStudies.getAll();
    Assert.deepEqual(studies, [], "the study should not be in the database");

    sendEventStub.assertEvents([
      [
        "enrollFailed",
        "addon_study",
        recipe.arguments.name,
        {
          reason: "metadata-mismatch",
          branch: "a",
        },
      ],
    ]);
  }
);

// Test that in the case of a study add-on conflicting with a non-study add-on, the study does not enroll
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  withInstalledWebExtensionSafe({ version: "0.1", id: FIXTURE_ADDON_ID }),
  AddonStudies.withStudies(),
  async function conflictingEnrollment(
    mockApi,
    sendEventStub,
    [installedAddonId, installedAddonFile]
  ) {
    is(
      installedAddonId,
      FIXTURE_ADDON_ID,
      "Generated, installed add-on should have the same ID as the fixture"
    );
    const recipe = branchedAddonStudyRecipeFactory({
      arguments: { slug: "conflicting" },
    });
    mockApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
        addonUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.version, "0.1", "The installed add-on should not be replaced");

    Assert.deepEqual(
      await AddonStudies.getAll(),
      [],
      "There should be no enrolled studies"
    );

    sendEventStub.assertEvents([
      [
        "enrollFailed",
        "addon_study",
        recipe.arguments.slug,
        { reason: "conflicting-addon-id" },
      ],
    ]);
  }
);

// Test a successful update
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      extensionHashAlgorithm: "sha256",
      addonVersion: "1.0",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "1.0" }),
  async function successfulUpdate(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const addonUrl = FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url;
    const recipe = recipeFromStudy(study, {
      arguments: {
        branches: [
          { slug: "a", extensionApiId: study.extensionApiId, ratio: 1 },
        ],
      },
    });
    const hash = FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash;
    mockApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
        extension_id: FIXTURE_ADDON_ID,
        xpi: addonUrl,
        hash,
        version: "2.0",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(!enrollSpy.called, "enroll should not be called");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "update",
        "addon_study",
        recipe.arguments.name,
        {
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "2.0",
          branch: "a",
        },
      ],
    ]);

    const updatedStudy = await AddonStudies.get(recipe.id);
    Assert.deepEqual(
      updatedStudy,
      {
        ...study,
        addonVersion: "2.0",
        addonUrl,
        extensionApiId: recipe.arguments.branches[0].extensionApiId,
        extensionHash: hash,
      },
      "study data should be updated"
    );

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon.version === "2.0", "add-on should be updated");
  }
);

// Test update fails when addon ID does not match
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: "test@example.com",
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "0.1" }),
  async function updateFailsAddonIdMismatch(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const recipe = recipeFromStudy(study);
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: FIXTURE_ADDON_ID,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "updateFailed",
        "addon_study",
        recipe.arguments.name,
        {
          reason: "addon-id-mismatch",
          branch: "a",
        },
      ],
    ]);

    const updatedStudy = await AddonStudies.get(recipe.id);
    Assert.deepEqual(updatedStudy, study, "study data should be unchanged");

    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon.version === "0.1", "add-on should be unchanged");
  }
);

// Test update fails when original addon does not exist
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: "test@example.com", version: "0.1" }),
  async function updateFailsAddonDoesNotExist(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const recipe = recipeFromStudy(study);
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "updateFailed",
        "addon_study",
        recipe.arguments.name,
        {
          reason: "addon-does-not-exist",
          branch: "a",
        },
      ],
    ]);

    const updatedStudy = await AddonStudies.get(recipe.id);
    Assert.deepEqual(updatedStudy, study, "study data should be unchanged");

    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(!addon, "new add-on should not be installed");

    addon = await AddonManager.getAddonByID("test@example.com");
    ok(addon, "old add-on should still be installed");
  }
);

// Test update fails when download fails
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "0.1" }),
  async function updateDownloadFailure(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const recipe = recipeFromStudy(study);
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: "https://example.com/404.xpi",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "updateFailed",
        "addon_study",
        recipe.arguments.name,
        {
          branch: "a",
          reason: "download-failure",
          detail: "ERROR_NETWORK_FAILURE",
        },
      ],
    ]);

    const updatedStudy = await AddonStudies.get(recipe.id);
    Assert.deepEqual(updatedStudy, study, "study data should be unchanged");

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon.version === "0.1", "add-on should be unchanged");
  }
);

// Test update fails when hash check fails
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "0.1" }),
  async function updateFailsHashCheckFail(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const recipe = recipeFromStudy(study);
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        hash: "badhash",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "updateFailed",
        "addon_study",
        recipe.arguments.name,
        {
          branch: "a",
          reason: "download-failure",
          detail: "ERROR_INCORRECT_HASH",
        },
      ],
    ]);

    const updatedStudy = await AddonStudies.get(recipe.id);
    Assert.deepEqual(updatedStudy, study, "study data should be unchanged");

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon.version === "0.1", "add-on should be unchanged");
  }
);

// Test update fails on downgrade when study version is greater than extension version
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "2.0",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "2.0" }),
  async function upgradeFailsNoDowngrades(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const recipe = recipeFromStudy(study);
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        version: "1.0",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "updateFailed",
        "addon_study",
        recipe.arguments.name,
        {
          reason: "no-downgrade",
          branch: "a",
        },
      ],
    ]);

    const updatedStudy = await AddonStudies.get(recipe.id);
    Assert.deepEqual(updatedStudy, study, "study data should be unchanged");

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon.version === "2.0", "add-on should be unchanged");
  }
);

// Test update fails when there is a version mismatch with metadata
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      extensionHashAlgorithm: "sha256",
      addonVersion: "1.0",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionFromURL(
    FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url
  ),
  async function upgradeFailsMetadataMismatchVersion(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const recipe = recipeFromStudy(study);
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url,
        version: "3.0",
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "updateFailed",
        "addon_study",
        recipe.arguments.name,
        {
          branch: "a",
          reason: "metadata-mismatch",
        },
      ],
    ]);

    const updatedStudy = await AddonStudies.get(recipe.id);
    Assert.deepEqual(updatedStudy, study, "study data should be unchanged");

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon.version === "1.0", "add-on should be unchanged");

    let addonSourceURI = addon.getResourceURI();
    if (addonSourceURI instanceof Ci.nsIJARURI) {
      addonSourceURI = addonSourceURI.JARFile;
    }
    const xpiFile = addonSourceURI.QueryInterface(Ci.nsIFileURL).file;
    const installedHash = CryptoUtils.getFileHash(
      xpiFile,
      study.extensionHashAlgorithm
    );
    ok(installedHash === study.extensionHash, "add-on should be unchanged");
  }
);

// Test that unenrolling fails if the study doesn't exist
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  AddonStudies.withStudies(),
  async function unenrollNonexistent(studies) {
    const action = new BranchedAddonStudyAction();
    await Assert.rejects(
      action.unenroll(42),
      /no study found/i,
      "unenroll should fail when no study exists"
    );
  }
);

// Test that unenrolling an inactive experiment fails
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  AddonStudies.withStudies([branchedAddonStudyFactory({ active: false })]),
  withSendEventStub,
  async ([study], sendEventStub) => {
    const action = new BranchedAddonStudyAction();
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
  withStudiesEnabled,
  ensureAddonCleanup,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      active: true,
      addonId: testStopId,
      studyEndDate: null,
    }),
  ]),
  withInstalledWebExtension({ id: testStopId }, /* expectUninstall: */ true),
  withSendEventStub,
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  async function unenrollTest(
    [study],
    [addonId, addonFile],
    sendEventStub,
    setExperimentInactiveStub
  ) {
    let addon = await AddonManager.getAddonByID(addonId);
    ok(addon, "the add-on should be installed before unenrolling");

    const action = new BranchedAddonStudyAction();
    await action.unenroll(study.recipeId, "test-reason");

    const newStudy = AddonStudies.get(study.recipeId);
    is(!newStudy, false, "stop should mark the study as inactive");
    ok(newStudy.studyEndDate !== null, "the study should have an end date");

    addon = await AddonManager.getAddonByID(addonId);
    is(addon, null, "the add-on should be uninstalled after unenrolling");

    sendEventStub.assertEvents([
      [
        "unenroll",
        "addon_study",
        study.name,
        {
          addonId,
          addonVersion: study.addonVersion,
          reason: "test-reason",
        },
      ],
    ]);

    Assert.deepEqual(
      setExperimentInactiveStub.args,
      [[study.slug]],
      "setExperimentInactive should be called"
    );
  }
);

// If the add-on for a study isn't installed, a warning should be logged, but the action is still successful
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      active: true,
      addonId: "missingAddon@example.com",
    }),
  ]),
  withSendEventStub,
  async function unenrollMissingAddonTest([study], sendEventStub) {
    const action = new BranchedAddonStudyAction();

    await action.unenroll(study.recipeId);

    sendEventStub.assertEvents([
      [
        "unenroll",
        "addon_study",
        study.name,
        {
          addonId: study.addonId,
          addonVersion: study.addonVersion,
          reason: "unknown",
        },
      ],
    ]);

    SimpleTest.endMonitorConsole();
  }
);

// Test that the action respects the study opt-out
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  withMockPreferences,
  AddonStudies.withStudies(),
  async function testOptOut(mockApi, sendEventStub, mockPreferences) {
    mockPreferences.set("app.shield.optoutstudies.enabled", false);
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const recipe = branchedAddonStudyRecipeFactory();
    mockApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
      }),
    };
    await action.runRecipe(recipe);
    is(
      action.state,
      BranchedAddonStudyAction.STATE_DISABLED,
      "the action should be disabled"
    );
    await action.finalize();
    is(
      action.state,
      BranchedAddonStudyAction.STATE_FINALIZED,
      "the action should be finalized"
    );
    is(action.lastError, null, "lastError should be null");
    Assert.deepEqual(enrollSpy.args, [], "enroll should not be called");
    sendEventStub.assertEvents([]);
  }
);

// Test that the action does not enroll paused recipes
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  AddonStudies.withStudies(),
  async function testEnrollmentPaused(mockApi, sendEventStub) {
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const updateSpy = sinon.spy(action, "update");
    const recipe = branchedAddonStudyRecipeFactory({
      arguments: { isEnrollmentPaused: true },
    });
    const extensionDetails = extensionDetailsFactory({
      id: recipe.arguments.extensionApiId,
    });
    mockApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetails,
    };
    await action.runRecipe(recipe);
    const addon = await AddonManager.getAddonByID(
      extensionDetails.extension_id
    );
    is(addon, null, "the add-on should not have been installed");
    await action.finalize();
    ok(!updateSpy.called, "update should not be called");
    ok(enrollSpy.called, "enroll should be called");
    sendEventStub.assertEvents([]);
  }
);

// Test that the action updates paused recipes
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      extensionHashAlgorithm: "sha256",
      addonVersion: "1.0",
    }),
  ]),
  withSendEventStub,
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "1.0" }),
  async function testUpdateEnrollmentPaused(
    mockApi,
    [study],
    sendEventStub,
    installedAddon
  ) {
    const addonUrl = FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url;
    const recipe = recipeFromStudy(study, {
      arguments: { isEnrollmentPaused: true },
    });
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: addonUrl,
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash,
        version: "2.0",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");
    ok(!enrollSpy.called, "enroll should not be called");
    ok(updateSpy.called, "update should be called");
    sendEventStub.assertEvents([
      [
        "update",
        "addon_study",
        recipe.arguments.name,
        {
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "2.0",
        },
      ],
    ]);

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(addon.version === "2.0", "add-on should be updated");
  }
);

// Test that unenroll called if the study is no longer sent from the server
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  AddonStudies.withStudies([branchedAddonStudyFactory()]),
  async function unenroll([study]) {
    const action = new BranchedAddonStudyAction();
    const unenrollSpy = sinon.stub(action, "unenroll");
    await action.finalize();
    is(action.lastError, null, "lastError should be null");
    Assert.deepEqual(
      unenrollSpy.args,
      [[study.recipeId, "recipe-not-seen"]],
      "unenroll should be called"
    );
  }
);

// A test function that will be parameterized over the argument "branch" below.
// Mocks the branch selector, and then tests that the user correctly gets enrolled in that branch.
const successEnrollBranchedTest = decorate(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  AddonStudies.withStudies(),
  async function(branch, mockApi, sendEventStub, setExperimentActiveStub) {
    ok(branch == "a" || branch == "b", "Branch should be either a or b");
    const initialAddonIds = (await AddonManager.getAllAddons()).map(
      addon => addon.id
    );
    const addonId = `normandydriver-${branch}@example.com`;
    const otherBranchAddonId = `normandydriver-${branch == "a" ? "b" : "a"}`;
    is(
      await AddonManager.getAddonByID(addonId),
      null,
      "The add-on should not be installed at the beginning of the test"
    );
    is(
      await AddonManager.getAddonByID(otherBranchAddonId),
      null,
      "The other branch's add-on should not be installed at the beginning of the test"
    );

    const recipe = branchedAddonStudyRecipeFactory({
      arguments: {
        slug: "success",
        branches: [
          { slug: "a", ratio: 1, extensionApiId: 1 },
          { slug: "b", ratio: 1, extensionApiId: 2 },
        ],
      },
    });
    mockApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: {
        id: recipe.arguments.branches[0].extensionApiId,
        name: "Normandy Fixture A",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        extension_id: "normandydriver-a@example.com",
        version: "1.0",
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
        hash_algorithm: "sha256",
      },
      [recipe.arguments.branches[1].extensionApiId]: {
        id: recipe.arguments.branches[1].extensionApiId,
        name: "Normandy Fixture B",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-b-1.0"].url,
        extension_id: "normandydriver-b@example.com",
        version: "1.0",
        hash: FIXTURE_ADDON_DETAILS["normandydriver-b-1.0"].hash,
        hash_algorithm: "sha256",
      },
    };
    const extensionApiId =
      recipe.arguments.branches[branch == "a" ? 0 : 1].extensionApiId;
    const extensionDetails = mockApi.extensionDetails[extensionApiId];

    const action = new BranchedAddonStudyAction();
    const chooseBranchStub = sinon.stub(action, "chooseBranch");
    chooseBranchStub.callsFake(async ({ branches }) =>
      branches.find(b => b.slug === branch)
    );
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");

    sendEventStub.assertEvents([
      [
        "enroll",
        "addon_study",
        recipe.arguments.slug,
        {
          addonId,
          addonVersion: "1.0",
          branch,
        },
      ],
    ]);

    Assert.deepEqual(
      setExperimentActiveStub.args,
      [[recipe.arguments.slug, branch, { type: "normandy-addonstudy" }]],
      "setExperimentActive should be called"
    );

    const addon = await AddonManager.getAddonByID(addonId);
    ok(addon, "The chosen branch's add-on should be installed");
    is(
      await AddonManager.getAddonByID(otherBranchAddonId),
      null,
      "The other branch's add-on should not be installed"
    );

    const study = await AddonStudies.get(recipe.id);
    Assert.deepEqual(
      study,
      {
        recipeId: recipe.id,
        slug: recipe.arguments.slug,
        userFacingName: recipe.arguments.userFacingName,
        userFacingDescription: recipe.arguments.userFacingDescription,
        addonId,
        addonVersion: "1.0",
        addonUrl: FIXTURE_ADDON_DETAILS[`normandydriver-${branch}-1.0`].url,
        active: true,
        branch,
        studyStartDate: study.studyStartDate, // This is checked below
        studyEndDate: null,
        extensionApiId: extensionDetails.id,
        extensionHash: extensionDetails.hash,
        extensionHashAlgorithm: extensionDetails.hash_algorithm,
      },
      "the correct study data should be stored"
    );

    // cleanup
    await safeUninstallAddon(addon);
    Assert.deepEqual(
      (await AddonManager.getAllAddons()).map(addon => addon.id),
      initialAddonIds,
      "all test add-ons are removed"
    );
  }
);

add_task(() => successEnrollBranchedTest("a"));
add_task(() => successEnrollBranchedTest("b"));

// If the enrolled branch no longer exists, unenroll
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  AddonStudies.withStudies([branchedAddonStudyFactory()]),
  withSendEventStub,
  withInstalledWebExtensionSafe(
    { id: FIXTURE_ADDON_ID, version: "1.0" },
    /* expectUninstall: */ true
  ),
  async function unenrollIfBranchDisappears(
    mockApi,
    [study],
    sendEventStub,
    [addonId, addonFile]
  ) {
    const recipe = recipeFromStudy(study, {
      arguments: {
        branches: [
          {
            slug: "b", // different from enrolled study
            ratio: 1,
            extensionApiId: study.extensionApiId,
          },
        ],
      },
    });
    mockApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        hash: study.extensionHash,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const unenrollSpy = sinon.spy(action, "unenroll");
    const updateSpy = sinon.spy(action, "update");
    await action.runRecipe(recipe);
    is(action.lastError, null, "lastError should be null");

    ok(!enrollSpy.called, "Enroll should not be called");
    ok(updateSpy.called, "Update should be called");
    ok(unenrollSpy.called, "Unenroll should be called");

    sendEventStub.assertEvents([
      [
        "unenroll",
        "addon_study",
        study.name,
        {
          addonId,
          addonVersion: study.addonVersion,
          reason: "branch-removed",
          branch: "a", // the original study branch
        },
      ],
    ]);

    is(
      await AddonManager.getAddonByID(addonId),
      null,
      "the add-on should be uninstalled"
    );

    const storedStudy = await AddonStudies.get(recipe.id);
    ok(!storedStudy.active, "Study should be inactive");
    ok(storedStudy.branch == "a", "Study's branch should not change");
    ok(storedStudy.studyEndDate, "Study's end date should be set");
  }
);

// Test that branches without an add-on can be enrolled and unenrolled succesfully.
decorate_task(
  withStudiesEnabled,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  AddonStudies.withStudies(),
  async function noAddonBranches(mockApi, sendEventStub) {
    const initialAddonIds = (await AddonManager.getAllAddons()).map(
      addon => addon.id
    );

    const recipe = branchedAddonStudyRecipeFactory({
      arguments: {
        slug: "no-op-branch",
        branches: [{ slug: "a", ratio: 1, extensionApiId: null }],
      },
    });

    let action = new BranchedAddonStudyAction();
    await action.runRecipe(recipe);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    sendEventStub.assertEvents([
      [
        "enroll",
        "addon_study",
        recipe.arguments.name,
        {
          addonId: AddonStudies.NO_ADDON_MARKER,
          addonVersion: AddonStudies.NO_ADDON_MARKER,
          branch: "a",
        },
      ],
    ]);

    Assert.deepEqual(
      (await AddonManager.getAllAddons()).map(addon => addon.id),
      initialAddonIds,
      "No add-on should be installed for the study"
    );

    let study = await AddonStudies.get(recipe.id);
    Assert.deepEqual(
      study,
      {
        recipeId: recipe.id,
        slug: recipe.arguments.slug,
        userFacingName: recipe.arguments.userFacingName,
        userFacingDescription: recipe.arguments.userFacingDescription,
        addonId: null,
        addonVersion: null,
        addonUrl: null,
        active: true,
        branch: "a",
        studyStartDate: study.studyStartDate, // This is checked below
        studyEndDate: null,
        extensionApiId: null,
        extensionHash: null,
        extensionHashAlgorithm: null,
      },
      "the correct study data should be stored"
    );
    ok(study.studyStartDate, "studyStartDate should have a value");

    // Now unenroll
    action = new BranchedAddonStudyAction();
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    sendEventStub.assertEvents([
      // The event from before
      [
        "enroll",
        "addon_study",
        recipe.arguments.name,
        {
          addonId: AddonStudies.NO_ADDON_MARKER,
          addonVersion: AddonStudies.NO_ADDON_MARKER,
          branch: "a",
        },
      ],
      // And a new unenroll event
      [
        "unenroll",
        "addon_study",
        recipe.arguments.name,
        {
          addonId: AddonStudies.NO_ADDON_MARKER,
          addonVersion: AddonStudies.NO_ADDON_MARKER,
          branch: "a",
        },
      ],
    ]);

    Assert.deepEqual(
      (await AddonManager.getAllAddons()).map(addon => addon.id),
      initialAddonIds,
      "The set of add-ons should not change"
    );

    study = await AddonStudies.get(recipe.id);
    Assert.deepEqual(
      study,
      {
        recipeId: recipe.id,
        slug: recipe.arguments.slug,
        userFacingName: recipe.arguments.userFacingName,
        userFacingDescription: recipe.arguments.userFacingDescription,
        addonId: null,
        addonVersion: null,
        addonUrl: null,
        active: false,
        branch: "a",
        studyStartDate: study.studyStartDate, // This is checked below
        studyEndDate: study.studyEndDate, // This is checked below
        extensionApiId: null,
        extensionHash: null,
        extensionHashAlgorithm: null,
      },
      "the correct study data should be stored"
    );
    ok(study.studyStartDate, "studyStartDate should have a value");
    ok(study.studyEndDate, "studyEndDate should have a value");
  }
);
