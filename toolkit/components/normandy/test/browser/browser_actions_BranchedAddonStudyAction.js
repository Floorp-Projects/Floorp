"use strict";

const { BaseAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BaseAction.sys.mjs"
);
const { BranchedAddonStudyAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BranchedAddonStudyAction.sys.mjs"
);
const { Uptake } = ChromeUtils.importESModule(
  "resource://normandy/lib/Uptake.sys.mjs"
);

const { NormandyTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NormandyTestUtils.sys.mjs"
);
const { branchedAddonStudyFactory } = NormandyTestUtils.factories;

function branchedAddonStudyRecipeFactory(overrides = {}) {
  let args = {
    slug: "fake-slug",
    userFacingName: "Fake name",
    userFacingDescription: "fake description",
    isEnrollmentPaused: false,
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
    isEnrollmentPaused: false,
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([branchedAddonStudyFactory()]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "1.0" }),
  async function enrollTwiceFail({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const recipe = recipeFromStudy(study);
    mockNormandyApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        hash: study.extensionHash,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const updateSpy = sinon.spy(action, "update");
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(!enrollSpy.called, "enroll should not be called");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([]);
  }
);

// Test that if the add-on fails to install, the database is cleaned up and the
// error is correctly reported.
decorate_task(
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  AddonStudies.withStudies(),
  async function enrollDownloadFail({ mockNormandyApi, sendEventSpy }) {
    const recipe = branchedAddonStudyRecipeFactory({
      arguments: {
        branches: [{ slug: "missing", ratio: 1, extensionApiId: 404 }],
      },
    });
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
        xpi: "https://example.com/404.xpi",
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    const studies = await AddonStudies.getAll();
    Assert.deepEqual(studies, [], "the study should not be in the database");

    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  AddonStudies.withStudies(),
  async function enrollHashCheckFails({ mockNormandyApi, sendEventSpy }) {
    const recipe = branchedAddonStudyRecipeFactory();
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        hash: "badhash",
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    const studies = await AddonStudies.getAll();
    Assert.deepEqual(studies, [], "the study should not be in the database");

    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  AddonStudies.withStudies(),
  async function enrollFailsMetadataMismatch({
    mockNormandyApi,
    sendEventSpy,
  }) {
    const recipe = branchedAddonStudyRecipeFactory();
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
        version: "1.5",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    const studies = await AddonStudies.getAll();
    Assert.deepEqual(studies, [], "the study should not be in the database");

    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ version: "0.1", id: FIXTURE_ADDON_ID }),
  AddonStudies.withStudies(),
  async function conflictingEnrollment({
    mockNormandyApi,
    sendEventSpy,
    installedWebExtensionSafe: { addonId },
  }) {
    is(
      addonId,
      FIXTURE_ADDON_ID,
      "Generated, installed add-on should have the same ID as the fixture"
    );
    const recipe = branchedAddonStudyRecipeFactory({
      arguments: { slug: "conflicting" },
    });
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
        addonUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      }),
    };
    const action = new BranchedAddonStudyAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.version, "0.1", "The installed add-on should not be replaced");

    Assert.deepEqual(
      await AddonStudies.getAll(),
      [],
      "There should be no enrolled studies"
    );

    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      extensionHashAlgorithm: "sha256",
      addonVersion: "1.0",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "1.0" }),
  async function successfulUpdate({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const addonUrl = FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url;
    const recipe = recipeFromStudy(study, {
      arguments: {
        branches: [
          { slug: "a", extensionApiId: study.extensionApiId, ratio: 1 },
        ],
      },
    });
    const hash = FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash;
    mockNormandyApi.extensionDetails = {
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(!enrollSpy.called, "enroll should not be called");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: "test@example.com",
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "0.1" }),
  async function updateFailsAddonIdMismatch({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const recipe = recipeFromStudy(study);
    mockNormandyApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: FIXTURE_ADDON_ID,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: "test@example.com", version: "0.1" }),
  async function updateFailsAddonDoesNotExist({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const recipe = recipeFromStudy(study);
    mockNormandyApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "0.1" }),
  async function updateDownloadFailure({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const recipe = recipeFromStudy(study);
    mockNormandyApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: "https://example.com/404.xpi",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "0.1",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "0.1" }),
  async function updateFailsHashCheckFail({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const recipe = recipeFromStudy(study);
    mockNormandyApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        hash: "badhash",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: "01d",
      extensionHashAlgorithm: "sha256",
      addonVersion: "2.0",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "2.0" }),
  async function upgradeFailsNoDowngrades({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const recipe = recipeFromStudy(study);
    mockNormandyApi.extensionDetails = {
      [study.extensionApiId]: extensionDetailsFactory({
        id: study.extensionApiId,
        extension_id: study.addonId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
        version: "1.0",
      }),
    };
    const action = new BranchedAddonStudyAction();
    const updateSpy = sinon.spy(action, "update");
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      extensionHashAlgorithm: "sha256",
      addonVersion: "1.0",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionFromURL(
    FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url
  ),
  async function upgradeFailsMetadataMismatchVersion({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const recipe = recipeFromStudy(study);
    mockNormandyApi.extensionDetails = {
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  AddonStudies.withStudies(),
  async function unenrollNonexistent() {
    const action = new BranchedAddonStudyAction();
    await Assert.rejects(
      action.unenroll(42),
      /no study found/i,
      "unenroll should fail when no study exists"
    );
  }
);

// Test that unenrolling an inactive study fails
decorate_task(
  withStudiesEnabled(),
  ensureAddonCleanup(),
  AddonStudies.withStudies([branchedAddonStudyFactory({ active: false })]),
  withSendEventSpy(),
  async ({ addonStudies: [study], sendEventSpy }) => {
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      active: true,
      addonId: testStopId,
      studyEndDate: null,
    }),
  ]),
  withInstalledWebExtension({ id: testStopId }, { expectUninstall: true }),
  withSendEventSpy(),
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  async function unenrollTest({
    addonStudies: [study],
    installedWebExtension: { addonId },
    sendEventSpy,
    setExperimentInactiveStub,
  }) {
    let addon = await AddonManager.getAddonByID(addonId);
    ok(addon, "the add-on should be installed before unenrolling");

    const action = new BranchedAddonStudyAction();
    await action.unenroll(study.recipeId, "test-reason");

    const newStudy = AddonStudies.get(study.recipeId);
    is(!newStudy, false, "stop should mark the study as inactive");
    ok(newStudy.studyEndDate !== null, "the study should have an end date");

    addon = await AddonManager.getAddonByID(addonId);
    is(addon, null, "the add-on should be uninstalled after unenrolling");

    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      active: true,
      addonId: "missingAddon@example.com",
    }),
  ]),
  withSendEventSpy(),
  async function unenrollMissingAddonTest({
    addonStudies: [study],
    sendEventSpy,
  }) {
    const action = new BranchedAddonStudyAction();

    await action.unenroll(study.recipeId);

    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  withMockPreferences(),
  AddonStudies.withStudies(),
  async function testOptOut({
    mockNormandyApi,
    sendEventSpy,
    mockPreferences,
  }) {
    mockPreferences.set("app.shield.optoutstudies.enabled", false);
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const recipe = branchedAddonStudyRecipeFactory();
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.branches[0].extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.branches[0].extensionApiId,
      }),
    };
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
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
    sendEventSpy.assertEvents([]);
  }
);

// Test that the action does not enroll paused recipes
decorate_task(
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  AddonStudies.withStudies(),
  async function testEnrollmentPaused({ mockNormandyApi, sendEventSpy }) {
    const action = new BranchedAddonStudyAction();
    const enrollSpy = sinon.spy(action, "enroll");
    const updateSpy = sinon.spy(action, "update");
    const recipe = branchedAddonStudyRecipeFactory({
      arguments: { isEnrollmentPaused: true },
    });
    const extensionDetails = extensionDetailsFactory({
      id: recipe.arguments.extensionApiId,
    });
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetails,
    };
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    const addon = await AddonManager.getAddonByID(
      extensionDetails.extension_id
    );
    is(addon, null, "the add-on should not have been installed");
    await action.finalize();
    ok(!updateSpy.called, "update should not be called");
    ok(enrollSpy.called, "enroll should be called");
    sendEventSpy.assertEvents([]);
  }
);

// Test that the action updates paused recipes
decorate_task(
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([
    branchedAddonStudyFactory({
      addonId: FIXTURE_ADDON_ID,
      extensionHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      extensionHashAlgorithm: "sha256",
      addonVersion: "1.0",
    }),
  ]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe({ id: FIXTURE_ADDON_ID, version: "1.0" }),
  async function testUpdateEnrollmentPaused({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
  }) {
    const addonUrl = FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url;
    const recipe = recipeFromStudy(study, {
      arguments: { isEnrollmentPaused: true },
    });
    mockNormandyApi.extensionDetails = {
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");
    ok(!enrollSpy.called, "enroll should not be called");
    ok(updateSpy.called, "update should be called");
    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  AddonStudies.withStudies([branchedAddonStudyFactory()]),
  async function unenroll({ addonStudies: [study] }) {
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  AddonStudies.withStudies(),
  async function ({
    branch,
    mockNormandyApi,
    sendEventSpy,
    setExperimentActiveStub,
  }) {
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
    mockNormandyApi.extensionDetails = {
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
    const extensionDetails = mockNormandyApi.extensionDetails[extensionApiId];

    const action = new BranchedAddonStudyAction();
    const chooseBranchStub = sinon.stub(action, "chooseBranch");
    chooseBranchStub.callsFake(async ({ branches }) =>
      branches.find(b => b.slug === branch)
    );
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    const study = await AddonStudies.get(recipe.id);
    sendEventSpy.assertEvents([
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
        temporaryErrorDeadline: null,
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

add_task(args => successEnrollBranchedTest({ ...args, branch: "a" }));
add_task(args => successEnrollBranchedTest({ ...args, branch: "b" }));

// If the enrolled branch no longer exists, unenroll
decorate_task(
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  AddonStudies.withStudies([branchedAddonStudyFactory()]),
  withSendEventSpy(),
  withInstalledWebExtensionSafe(
    { id: FIXTURE_ADDON_ID, version: "1.0" },
    { expectUninstall: true }
  ),
  async function unenrollIfBranchDisappears({
    mockNormandyApi,
    addonStudies: [study],
    sendEventSpy,
    installedWebExtensionSafe: { addonId },
  }) {
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
    mockNormandyApi.extensionDetails = {
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    ok(!enrollSpy.called, "Enroll should not be called");
    ok(updateSpy.called, "Update should be called");
    ok(unenrollSpy.called, "Unenroll should be called");

    sendEventSpy.assertEvents([
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
  withStudiesEnabled(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  AddonStudies.withStudies(),
  async function noAddonBranches({ sendEventSpy }) {
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    let study = await AddonStudies.get(recipe.id);
    sendEventSpy.assertEvents([
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
        temporaryErrorDeadline: null,
      },
      "the correct study data should be stored"
    );
    ok(study.studyStartDate, "studyStartDate should have a value");

    // Now unenroll
    action = new BranchedAddonStudyAction();
    await action.finalize();
    is(action.lastError, null, "lastError should be null");

    sendEventSpy.assertEvents([
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
        temporaryErrorDeadline: null,
      },
      "the correct study data should be stored"
    );
    ok(study.studyStartDate, "studyStartDate should have a value");
    ok(study.studyEndDate, "studyEndDate should have a value");
  }
);

// Check that the appropriate set of suitabilities are considered temporary errors
decorate_task(
  withStudiesEnabled(),
  async function test_temporary_errors_set_deadline() {
    let suitabilities = [
      {
        suitability: BaseAction.suitability.SIGNATURE_ERROR,
        isTemporaryError: true,
      },
      {
        suitability: BaseAction.suitability.CAPABILITIES_MISMATCH,
        isTemporaryError: false,
      },
      {
        suitability: BaseAction.suitability.FILTER_MATCH,
        isTemporaryError: false,
      },
      {
        suitability: BaseAction.suitability.FILTER_MISMATCH,
        isTemporaryError: false,
      },
      {
        suitability: BaseAction.suitability.FILTER_ERROR,
        isTemporaryError: true,
      },
      {
        suitability: BaseAction.suitability.ARGUMENTS_INVALID,
        isTemporaryError: false,
      },
    ];

    Assert.deepEqual(
      suitabilities.map(({ suitability }) => suitability).sort(),
      Array.from(Object.values(BaseAction.suitability)).sort(),
      "This test covers all suitabilities"
    );

    // The action should set a deadline 1 week from now. To avoid intermittent
    // failures, give this a generous bound of 2 hours on either side.
    let now = Date.now();
    let hour = 60 * 60 * 1000;
    let expectedDeadline = now + 7 * 24 * hour;
    let minDeadline = new Date(expectedDeadline - 2 * hour);
    let maxDeadline = new Date(expectedDeadline + 2 * hour);

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const { suitability, isTemporaryError } of suitabilities) {
      const decorator = AddonStudies.withStudies([
        branchedAddonStudyFactory({
          slug: `test-for-suitability-${suitability}`,
        }),
      ]);
      await decorator(async ({ addonStudies: [study] }) => {
        let action = new BranchedAddonStudyAction();
        let recipe = recipeFromStudy(study);
        await action.processRecipe(recipe, suitability);
        let modifiedStudy = await AddonStudies.get(recipe.id);

        if (isTemporaryError) {
          ok(
            // The constructor of this object is a Date, but is not the same as
            // the Date that we have in our global scope, because it got sent
            // through IndexedDB. Check the name of the constructor instead.
            modifiedStudy.temporaryErrorDeadline.constructor.name == "Date",
            `A temporary failure deadline should be set as a date for suitability ${suitability}`
          );
          let deadline = modifiedStudy.temporaryErrorDeadline;
          ok(
            deadline >= minDeadline && deadline <= maxDeadline,
            `The temporary failure deadline should be in the expected range for ` +
              `suitability ${suitability} (got ${deadline}, expected between ${minDeadline} and ${maxDeadline})`
          );
        } else {
          ok(
            !modifiedStudy.temporaryErrorDeadline,
            `No temporary failure deadline should be set for suitability ${suitability}`
          );
        }
      })();
    }
  }
);

// Check that if there is an existing deadline, temporary errors don't overwrite it
decorate_task(
  withStudiesEnabled(),
  async function test_temporary_errors_dont_overwrite_deadline() {
    let temporaryFailureSuitabilities = [
      BaseAction.suitability.SIGNATURE_ERROR,
      BaseAction.suitability.FILTER_ERROR,
    ];

    // A deadline two hours in the future won't be hit during the test.
    let now = Date.now();
    let hour = 2 * 60 * 60 * 1000;
    let unhitDeadline = new Date(now + hour);

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of temporaryFailureSuitabilities) {
      const decorator = AddonStudies.withStudies([
        branchedAddonStudyFactory({
          slug: `test-for-suitability-${suitability}`,
          active: true,
          temporaryErrorDeadline: unhitDeadline,
        }),
      ]);
      await decorator(async ({ addonStudies: [study] }) => {
        let action = new BranchedAddonStudyAction();
        let recipe = recipeFromStudy(study);
        await action.processRecipe(recipe, suitability);
        let modifiedStudy = await AddonStudies.get(recipe.id);
        is(
          modifiedStudy.temporaryErrorDeadline.toJSON(),
          unhitDeadline.toJSON(),
          `The temporary failure deadline should not be cleared for suitability ${suitability}`
        );
      })();
    }
  }
);

// Check that if the deadline is past, temporary errors end the study.
decorate_task(
  withStudiesEnabled(),
  async function test_temporary_errors_hit_deadline() {
    let temporaryFailureSuitabilities = [
      BaseAction.suitability.SIGNATURE_ERROR,
      BaseAction.suitability.FILTER_ERROR,
    ];

    // Set a deadline of two hours in the past, so that the deadline is triggered.
    let now = Date.now();
    let hour = 60 * 60 * 1000;
    let hitDeadline = new Date(now - 2 * hour);

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of temporaryFailureSuitabilities) {
      const decorator = AddonStudies.withStudies([
        branchedAddonStudyFactory({
          slug: `test-for-suitability-${suitability}`,
          active: true,
          temporaryErrorDeadline: hitDeadline,
        }),
      ]);
      await decorator(async ({ addonStudies: [study] }) => {
        let action = new BranchedAddonStudyAction();
        let recipe = recipeFromStudy(study);
        await action.processRecipe(recipe, suitability);
        let modifiedStudy = await AddonStudies.get(recipe.id);
        ok(
          !modifiedStudy.active,
          `The study should end for suitability ${suitability}`
        );
      })();
    }
  }
);

// Check that non-temporary-error suitabilities clear the temporary deadline
decorate_task(
  withStudiesEnabled(),
  async function test_non_temporary_error_clears_temporary_error_deadline() {
    let suitabilitiesThatShouldClearDeadline = [
      BaseAction.suitability.CAPABILITIES_MISMATCH,
      BaseAction.suitability.FILTER_MATCH,
      BaseAction.suitability.FILTER_MISMATCH,
      BaseAction.suitability.ARGUMENTS_INVALID,
    ];

    // Use a deadline in the past to demonstrate that even if the deadline has
    // passed, only a temporary error suitability ends the study.
    let now = Date.now();
    let hour = 60 * 60 * 1000;
    let hitDeadline = new Date(now - 2 * hour);

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of suitabilitiesThatShouldClearDeadline) {
      const decorator = AddonStudies.withStudies([
        branchedAddonStudyFactory({
          slug: `test-for-suitability-${suitability}`.toLocaleLowerCase(),
          active: true,
          temporaryErrorDeadline: hitDeadline,
        }),
      ]);
      await decorator(async ({ addonStudies: [study] }) => {
        let action = new BranchedAddonStudyAction();
        let recipe = recipeFromStudy(study);
        await action.processRecipe(recipe, suitability);
        let modifiedStudy = await AddonStudies.get(recipe.id);
        ok(
          !modifiedStudy.temporaryErrorDeadline,
          `The temporary failure deadline should be cleared for suitabilitiy ${suitability}`
        );
      })();
    }
  }
);

// Check that invalid deadlines are reset
decorate_task(
  withStudiesEnabled(),
  async function test_non_temporary_error_clears_temporary_error_deadline() {
    let temporaryFailureSuitabilities = [
      BaseAction.suitability.SIGNATURE_ERROR,
      BaseAction.suitability.FILTER_ERROR,
    ];

    // The action should set a deadline 1 week from now. To avoid intermittent
    // failures, give this a generous bound of 2 hours on either side.
    let invalidDeadline = new Date("not a valid date");
    let now = Date.now();
    let hour = 60 * 60 * 1000;
    let expectedDeadline = now + 7 * 24 * hour;
    let minDeadline = new Date(expectedDeadline - 2 * hour);
    let maxDeadline = new Date(expectedDeadline + 2 * hour);

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of temporaryFailureSuitabilities) {
      const decorator = AddonStudies.withStudies([
        branchedAddonStudyFactory({
          slug: `test-for-suitability-${suitability}`.toLocaleLowerCase(),
          active: true,
          temporaryErrorDeadline: invalidDeadline,
        }),
      ]);
      await decorator(async ({ addonStudies: [study] }) => {
        let action = new BranchedAddonStudyAction();
        let recipe = recipeFromStudy(study);
        await action.processRecipe(recipe, suitability);
        is(action.lastError, null, "No errors should be reported");
        let modifiedStudy = await AddonStudies.get(recipe.id);
        ok(
          modifiedStudy.temporaryErrorDeadline != invalidDeadline,
          `The temporary failure deadline should be reset for suitabilitiy ${suitability}`
        );
        let deadline = new Date(modifiedStudy.temporaryErrorDeadline);
        ok(
          deadline >= minDeadline && deadline <= maxDeadline,
          `The temporary failure deadline should be reset to a valid deadline for ${suitability}`
        );
      })();
    }
  }
);

// Check that an already unenrolled study doesn't try to unenroll again if
// the recipe doesn't apply the client anymore.
decorate_task(
  withStudiesEnabled(),
  async function test_unenroll_when_already_expired() {
    // Use a deadline that is already past
    const now = new Date();
    const hour = 1000 * 60 * 60;
    const temporaryErrorDeadline = new Date(now - hour * 2).toJSON();

    const suitabilitiesToCheck = Object.values(BaseAction.suitability);

    const subtest = decorate(
      AddonStudies.withStudies([
        branchedAddonStudyFactory({
          active: false,
          temporaryErrorDeadline,
        }),
      ]),

      async ({ addonStudies: [study], suitability }) => {
        const recipe = recipeFromStudy(study);
        const action = new BranchedAddonStudyAction();
        const unenrollSpy = sinon.spy(action.unenroll);
        await action.processRecipe(recipe, suitability);
        Assert.deepEqual(
          unenrollSpy.args,
          [],
          `Stop should not be called for ${suitability}`
        );
      }
    );

    for (const suitability of suitabilitiesToCheck) {
      await subtest({ suitability });
    }
  }
);

// If no recipes are received, it should be considered a temporary error
decorate_task(
  withStudiesEnabled(),
  AddonStudies.withStudies([branchedAddonStudyFactory({ active: true })]),
  withSpy(BranchedAddonStudyAction.prototype, "unenroll"),
  withStub(BranchedAddonStudyAction.prototype, "_considerTemporaryError"),
  async function testNoRecipes({
    unenrollSpy,
    _considerTemporaryErrorStub,
    addonStudies: [study],
  }) {
    let action = new BranchedAddonStudyAction();
    await action.finalize({ noRecipes: true });

    Assert.deepEqual(unenrollSpy.args, [], "Unenroll should not be called");
    Assert.deepEqual(
      _considerTemporaryErrorStub.args,
      [[{ study, reason: "no-recipes" }]],
      "The experiment should accumulate a temporary error"
    );
  }
);

// If recipes are received, but the flag that none were received is set, an error should be thrown
decorate_task(
  withStudiesEnabled(),
  AddonStudies.withStudies([branchedAddonStudyFactory({ active: true })]),
  async function testNoRecipes({ addonStudies: [study] }) {
    let action = new BranchedAddonStudyAction();
    let recipe = recipeFromStudy(study);
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MISMATCH);
    await action.finalize({ noRecipes: true });
    ok(
      action.lastError instanceof BranchedAddonStudyAction.BadNoRecipesArg,
      "An error should be logged since some recipes were received"
    );
  }
);
