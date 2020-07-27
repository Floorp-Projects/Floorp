"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/actions/AddonRollbackAction.jsm", this);
ChromeUtils.import("resource://normandy/actions/AddonRolloutAction.jsm", this);
ChromeUtils.import("resource://normandy/actions/BaseAction.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonRollouts.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);
ChromeUtils.import("resource://testing-common/NormandyTestUtils.jsm", this);

// Test that a simple recipe unenrolls as expected
decorate_task(
  AddonRollouts.withTestMock,
  ensureAddonCleanup,
  withMockNormandyApi,
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventStub,
  async function simple_recipe_unenrollment(
    mockApi,
    setExperimentInactiveStub,
    sendEventStub
  ) {
    const rolloutRecipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockApi.extensionDetails = {
      [rolloutRecipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: rolloutRecipe.arguments.extensionApiId,
      }),
    };

    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    const rolloutAction = new AddonRolloutAction();
    await rolloutAction.processRecipe(
      rolloutRecipe,
      BaseAction.suitability.FILTER_MATCH
    );
    is(rolloutAction.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    const rollbackRecipe = {
      id: 2,
      arguments: {
        rolloutSlug: "test-rollout",
      },
    };

    const rollbackAction = new AddonRollbackAction();
    ok(
      await AddonRollouts.has(rolloutRecipe.arguments.slug),
      "Rollout should have been added"
    );
    await rollbackAction.processRecipe(
      rollbackRecipe,
      BaseAction.suitability.FILTER_MATCH
    );

    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(!addon, "add-on is uninstalled");

    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: rolloutRecipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ROLLED_BACK,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "1.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollback should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "enrollmentId should be a UUID"
    );

    sendEventStub.assertEvents([
      ["enroll", "addon_rollout", rollbackRecipe.arguments.rolloutSlug],
      ["unenroll", "addon_rollback", rollbackRecipe.arguments.rolloutSlug],
    ]);

    Assert.deepEqual(
      setExperimentInactiveStub.args,
      [["test-rollout"]],
      "the telemetry experiment should deactivated"
    );
  }
);

// Add-on already uninstalled
decorate_task(
  AddonRollouts.withTestMock,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  async function addon_already_uninstalled(mockApi, sendEventStub) {
    const rolloutRecipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockApi.extensionDetails = {
      [rolloutRecipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: rolloutRecipe.arguments.extensionApiId,
      }),
    };

    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    const rolloutAction = new AddonRolloutAction();
    await rolloutAction.processRecipe(
      rolloutRecipe,
      BaseAction.suitability.FILTER_MATCH
    );
    is(rolloutAction.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    const rollbackRecipe = {
      id: 2,
      arguments: {
        rolloutSlug: "test-rollout",
      },
    };

    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    await addon.uninstall();

    const rollbackAction = new AddonRollbackAction();
    await rollbackAction.processRecipe(
      rollbackRecipe,
      BaseAction.suitability.FILTER_MATCH
    );

    addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    ok(!addon, "add-on is uninstalled");

    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: rolloutRecipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ROLLED_BACK,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "1.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollback should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "enrollment ID should be a UUID"
    );

    sendEventStub.assertEvents([
      ["enroll", "addon_rollout", rollbackRecipe.arguments.rolloutSlug],
      ["unenroll", "addon_rollback", rollbackRecipe.arguments.rolloutSlug],
    ]);
  }
);

// Already rolled back, do nothing
decorate_task(
  AddonRollouts.withTestMock,
  ensureAddonCleanup,
  withMockNormandyApi,
  withSendEventStub,
  async function already_rolled_back(mockApi, sendEventStub) {
    const rollout = {
      recipeId: 1,
      slug: "test-rollout",
      state: AddonRollouts.STATE_ROLLED_BACK,
      extensionApiId: 1,
      addonId: FIXTURE_ADDON_ID,
      addonVersion: "1.0",
      xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
      xpiHashAlgorithm: "sha256",
    };
    AddonRollouts.add(rollout);

    const action = new AddonRollbackAction();
    await action.processRecipe(
      {
        id: 2,
        arguments: {
          rolloutSlug: "test-rollout",
        },
      },
      BaseAction.suitability.FILTER_MATCH
    );

    Assert.deepEqual(
      await AddonRollouts.getAll(),
      [
        {
          recipeId: 1,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ROLLED_BACK,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "1.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
          xpiHashAlgorithm: "sha256",
        },
      ],
      "Rollback should be stored in db"
    );

    sendEventStub.assertEvents([]);
  }
);
