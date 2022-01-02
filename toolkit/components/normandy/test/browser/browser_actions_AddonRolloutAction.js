"use strict";

const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
const { AddonRolloutAction } = ChromeUtils.import(
  "resource://normandy/actions/AddonRolloutAction.jsm"
);
const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const { AddonRollouts } = ChromeUtils.import(
  "resource://normandy/lib/AddonRollouts.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);

// Test that a simple recipe enrolls as expected
decorate_task(
  AddonRollouts.withTestMock(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withSendEventSpy(),
  async function simple_recipe_enrollment({
    mockNormandyApi,
    setExperimentActiveStub,
    sendEventSpy,
  }) {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
      }),
    };

    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    const action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    // addon was installed
    const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should be installed");

    // rollout was stored
    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: recipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ACTIVE,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "1.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "enrollmentId should be a UUID"
    );

    sendEventSpy.assertEvents([
      ["enroll", "addon_rollout", recipe.arguments.slug],
    ]);
    ok(
      setExperimentActiveStub.calledWithExactly("test-rollout", "active", {
        type: "normandy-addonrollout",
      }),
      "a telemetry experiment should be activated"
    );

    // cleanup installed addon
    await addon.uninstall();
  }
);

// Test that a rollout can update the addon
decorate_task(
  AddonRollouts.withTestMock(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  async function update_rollout({ mockNormandyApi, sendEventSpy }) {
    // first enrollment
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
      }),
      2: extensionDetailsFactory({
        id: 2,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url,
        version: "2.0",
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash,
      }),
    };

    let webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    let action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    // addon was installed
    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should be installed");
    is(addon.version, "1.0", "addon should be the correct version");

    // update existing enrollment
    recipe.arguments.extensionApiId = 2;
    webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );
    action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should still be installed");
    is(addon.version, "2.0", "addon should be the correct version");

    // rollout in the DB has been updated
    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: recipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ACTIVE,
          extensionApiId: 2,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "2.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "enrollmentId should be a UUID"
    );

    sendEventSpy.assertEvents([
      ["enroll", "addon_rollout", "test-rollout"],
      ["update", "addon_rollout", "test-rollout"],
    ]);

    // Cleanup
    await addon.uninstall();
  }
);

// Re-running a recipe does nothing
decorate_task(
  AddonRollouts.withTestMock(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  async function rerun_recipe({ mockNormandyApi, sendEventSpy }) {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
      }),
    };

    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    let action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    // addon was installed
    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should be installed");
    is(addon.version, "1.0", "addon should be the correct version");

    // re-run the same recipe
    action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should still be installed");
    is(addon.version, "1.0", "addon should be the correct version");

    // rollout in the DB has not been updated
    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: recipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ACTIVE,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "1.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "Enrollment ID should be a UUID"
    );

    sendEventSpy.assertEvents([["enroll", "addon_rollout", "test-rollout"]]);

    // Cleanup
    await addon.uninstall();
  }
);

// Conflicting rollouts
decorate_task(
  AddonRollouts.withTestMock(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  async function conflicting_rollout({ mockNormandyApi, sendEventSpy }) {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
      }),
    };

    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    let action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    // addon was installed
    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should be installed");
    is(addon.version, "1.0", "addon should be the correct version");

    // update existing enrollment
    action = new AddonRolloutAction();
    await action.processRecipe(
      {
        ...recipe,
        id: 2,
        arguments: {
          ...recipe.arguments,
          slug: "test-conflict",
        },
      },
      BaseAction.suitability.FILTER_MATCH
    );
    is(action.lastError, null, "lastError should be null");

    addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should still be installed");
    is(addon.version, "1.0", "addon should be the correct version");

    // rollout in the DB has not been updated
    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: recipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ACTIVE,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "1.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be stored in db"
    );
    ok(NormandyTestUtils.isUuid(rollouts[0].enrollmentId));

    sendEventSpy.assertEvents([
      [
        "enroll",
        "addon_rollout",
        "test-rollout",
        { addonId: FIXTURE_ADDON_ID, enrollmentId: rollouts[0].enrollmentId },
      ],
      [
        "enrollFailed",
        "addon_rollout",
        "test-conflict",
        { enrollmentId: rollouts[0].enrollmentId, reason: "conflict" },
      ],
    ]);

    // Cleanup
    await addon.uninstall();
  }
);

// Add-on ID changed
decorate_task(
  AddonRollouts.withTestMock(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  async function enroll_failed_addon_id_changed({
    mockNormandyApi,
    sendEventSpy,
  }) {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
      }),
      2: extensionDetailsFactory({
        id: 2,
        extension_id: "normandydriver-b@example.com",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-b-1.0"].url,
        version: "1.0",
        hash: FIXTURE_ADDON_DETAILS["normandydriver-b-1.0"].hash,
      }),
    };

    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    let action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    // addon was installed
    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should be installed");
    is(addon.version, "1.0", "addon should be the correct version");

    // update existing enrollment
    recipe.arguments.extensionApiId = 2;
    action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should still be installed");
    is(addon.version, "1.0", "addon should be the correct version");

    // rollout in the DB has not been updated
    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: recipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ACTIVE,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "1.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "enrollment ID should be a UUID"
    );

    sendEventSpy.assertEvents([
      ["enroll", "addon_rollout", "test-rollout"],
      [
        "updateFailed",
        "addon_rollout",
        "test-rollout",
        { reason: "addon-id-changed" },
      ],
    ]);

    // Cleanup
    await addon.uninstall();
  }
);

// Add-on upgrade required
decorate_task(
  AddonRollouts.withTestMock(),
  ensureAddonCleanup(),
  withMockNormandyApi(),
  withSendEventSpy(),
  async function enroll_failed_upgrade_required({
    mockNormandyApi,
    sendEventSpy,
  }) {
    const recipe = {
      id: 1,
      arguments: {
        slug: "test-rollout",
        extensionApiId: 1,
      },
    };
    mockNormandyApi.extensionDetails = {
      [recipe.arguments.extensionApiId]: extensionDetailsFactory({
        id: recipe.arguments.extensionApiId,
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url,
        version: "2.0",
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash,
      }),
      2: extensionDetailsFactory({
        id: 2,
      }),
    };

    const webExtStartupPromise = AddonTestUtils.promiseWebExtensionStartup(
      FIXTURE_ADDON_ID
    );

    let action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    await webExtStartupPromise;

    // addon was installed
    let addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should be installed");
    is(addon.version, "2.0", "addon should be the correct version");

    // update existing enrollment
    recipe.arguments.extensionApiId = 2;
    action = new AddonRolloutAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "lastError should be null");

    addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
    is(addon.id, FIXTURE_ADDON_ID, "addon should still be installed");
    is(addon.version, "2.0", "addon should be the correct version");

    // rollout in the DB has not been updated
    const rollouts = await AddonRollouts.getAll();
    Assert.deepEqual(
      rollouts,
      [
        {
          recipeId: recipe.id,
          slug: "test-rollout",
          state: AddonRollouts.STATE_ACTIVE,
          extensionApiId: 1,
          addonId: FIXTURE_ADDON_ID,
          addonVersion: "2.0",
          xpiUrl: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].url,
          xpiHash: FIXTURE_ADDON_DETAILS["normandydriver-a-2.0"].hash,
          xpiHashAlgorithm: "sha256",
          enrollmentId: rollouts[0].enrollmentId,
        },
      ],
      "Rollout should be stored in db"
    );
    ok(
      NormandyTestUtils.isUuid(rollouts[0].enrollmentId),
      "enrollment ID should be a UUID"
    );

    sendEventSpy.assertEvents([
      ["enroll", "addon_rollout", "test-rollout"],
      [
        "updateFailed",
        "addon_rollout",
        "test-rollout",
        { reason: "upgrade-required" },
      ],
    ]);

    // Cleanup
    await addon.uninstall();
  }
);
