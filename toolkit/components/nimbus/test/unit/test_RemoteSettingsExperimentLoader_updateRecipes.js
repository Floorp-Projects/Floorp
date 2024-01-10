"use strict";

const { ExperimentFakes, ExperimentTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { FirstStartup } = ChromeUtils.importESModule(
  "resource://gre/modules/FirstStartup.sys.mjs"
);
const { NimbusFeatures, _ExperimentFeature: ExperimentFeature } =
  ChromeUtils.importESModule("resource://nimbus/ExperimentAPI.sys.mjs");
const { EnrollmentsContext } = ChromeUtils.importESModule(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.sys.mjs"
);
const { PanelTestProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/PanelTestProvider.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { TelemetryEvents } = ChromeUtils.importESModule(
  "resource://normandy/lib/TelemetryEvents.sys.mjs"
);

add_setup(async function setup() {
  do_get_profile();
  Services.fog.initializeFOG();
});

add_task(async function test_updateRecipes_activeExperiments() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const recipe = ExperimentFakes.recipe("foo");
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;
  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo", {
    targeting: `"${recipe.slug}" in activeExperiments`,
  });
  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([PASS_FILTER_RECIPE]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActiveExperiments").returns([recipe]);

  await loader.init();

  ok(onRecipe.calledOnce, "Should match active experiments");

  await assertEmptyStore(manager.store);
});

add_task(async function test_updateRecipes_isFirstRun() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const recipe = ExperimentFakes.recipe("foo");
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;
  const PASS_FILTER_RECIPE = { ...recipe, targeting: "isFirstStartup" };
  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([PASS_FILTER_RECIPE]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActiveExperiments").returns([recipe]);

  // Pretend to be in the first startup
  FirstStartup._state = FirstStartup.IN_PROGRESS;
  await loader.init();

  Assert.ok(onRecipe.calledOnce, "Should match first run");

  await assertEmptyStore(manager.store);
});

add_task(async function test_updateRecipes_invalidFeatureId() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;

  const badRecipe = ExperimentFakes.recipe("foo", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "invalid-feature-id",
            value: { hello: "world" },
          },
        ],
      },
      {
        slug: "treatment",
        ratio: 1,
        features: [
          {
            featureId: "invalid-feature-id",
            value: { hello: "goodbye" },
          },
        ],
      },
    ],
  });

  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([badRecipe]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActiveExperiments").returns([]);

  await loader.init();
  ok(onRecipe.notCalled, "No recipes");

  await assertEmptyStore(manager.store);
});

add_task(async function test_updateRecipes_invalidFeatureValue() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;

  const badRecipe = ExperimentFakes.recipe("foo", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "spotlight",
            value: {
              template: "spotlight",
            },
          },
        ],
      },
      {
        slug: "treatment",
        ratio: 1,
        features: [
          {
            featureId: "spotlight",
            value: {
              template: "spotlight",
            },
          },
        ],
      },
    ],
  });

  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([badRecipe]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActiveExperiments").returns([]);

  await loader.init();
  ok(onRecipe.notCalled, "No recipes");

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_invalidRecipe() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;

  const badRecipe = ExperimentFakes.recipe("foo");
  delete badRecipe.slug;

  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([badRecipe]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActiveExperiments").returns([]);

  await loader.init();
  ok(onRecipe.notCalled, "No recipes");

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_invalidRecipeAfterUpdate() {
  Services.fog.testResetFOG();

  const manager = ExperimentFakes.manager();
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;

  const recipe = ExperimentFakes.recipe("foo");
  const badRecipe = { ...recipe };
  delete badRecipe.branches;

  sinon.stub(loader, "setTimer");
  sinon.stub(manager, "onRecipe");
  sinon.stub(manager, "onFinalize");

  sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);
  sinon.stub(manager.store, "ready").resolves();
  sinon.spy(loader, "updateRecipes");

  await loader.init();

  ok(loader.updateRecipes.calledOnce, "should call .updateRecipes");
  equal(loader.manager.onRecipe.callCount, 1, "should call .onRecipe once");
  ok(
    loader.manager.onRecipe.calledWith(recipe, "rs-loader"),
    "should call .onRecipe with argument data"
  );
  equal(loader.manager.onFinalize.callCount, 1, "should call .onFinalize once");

  ok(
    onFinalizeCalled(loader.manager.onFinalize, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "should call .onFinalize with no mismatches or invalid recipes"
  );

  info("Replacing recipe with an invalid one");

  loader.remoteSettingsClient.get.resolves([badRecipe]);

  await loader.updateRecipes("timer");
  equal(
    loader.manager.onRecipe.callCount,
    1,
    "should not have called .onRecipe again"
  );
  equal(
    loader.manager.onFinalize.callCount,
    2,
    "should have called .onFinalize again"
  );

  ok(
    onFinalizeCalled(loader.manager.onFinalize.secondCall.args, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: ["foo"],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "should call .onFinalize with an invalid recipe"
  );

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_invalidBranchAfterUpdate() {
  const message = await PanelTestProvider.getMessages().then(msgs =>
    msgs.find(m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE")
  );

  const manager = ExperimentFakes.manager();
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;

  const recipe = ExperimentFakes.recipe("foo", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "spotlight",
            value: { ...message },
          },
        ],
      },
      {
        slug: "treatment",
        ratio: 1,
        features: [
          {
            featureId: "spotlight",
            value: { ...message },
          },
        ],
      },
    ],
  });

  const badRecipe = {
    ...recipe,
    branches: [
      { ...recipe.branches[0] },
      {
        ...recipe.branches[1],
        features: [
          {
            ...recipe.branches[1].features[0],
            value: { ...message },
          },
        ],
      },
    ],
  };
  delete badRecipe.branches[1].features[0].value.template;

  sinon.stub(loader, "setTimer");
  sinon.stub(manager, "onRecipe");
  sinon.stub(manager, "onFinalize");

  sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);
  sinon.stub(manager.store, "ready").resolves();
  sinon.spy(loader, "updateRecipes");

  await loader.init();

  ok(loader.updateRecipes.calledOnce, "should call .updateRecipes");
  equal(loader.manager.onRecipe.callCount, 1, "should call .onRecipe once");
  ok(
    loader.manager.onRecipe.calledWith(recipe, "rs-loader"),
    "should call .onRecipe with argument data"
  );
  equal(loader.manager.onFinalize.callCount, 1, "should call .onFinalize once");
  ok(
    onFinalizeCalled(loader.manager.onFinalize, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "should call .onFinalize with no mismatches or invalid recipes"
  );

  info("Replacing recipe with an invalid one");

  loader.remoteSettingsClient.get.resolves([badRecipe]);

  await loader.updateRecipes("timer");
  equal(
    loader.manager.onRecipe.callCount,
    1,
    "should not have called .onRecipe again"
  );
  equal(
    loader.manager.onFinalize.callCount,
    2,
    "should have called .onFinalize again"
  );

  ok(
    onFinalizeCalled(loader.manager.onFinalize.secondCall.args, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map([["foo", [badRecipe.branches[1].slug]]]),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "should call .onFinalize with an invalid branch"
  );

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_simpleFeatureInvalidAfterUpdate() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  const recipe = ExperimentFakes.recipe("foo");
  const badRecipe = ExperimentFakes.recipe("foo", {
    branches: [
      {
        ...recipe.branches[0],
        features: [
          {
            featureId: "testFeature",
            value: { testInt: "abc123", enabled: true },
          },
        ],
      },
      {
        ...recipe.branches[1],
        features: [
          {
            featureId: "testFeature",
            value: { testInt: 456, enabled: true },
          },
        ],
      },
    ],
  });

  const EXPECTED_SCHEMA = {
    $schema: "https://json-schema.org/draft/2019-09/schema",
    title: "testFeature",
    description: NimbusFeatures.testFeature.manifest.description,
    type: "object",
    properties: {
      testInt: {
        type: "integer",
      },
      enabled: {
        type: "boolean",
      },
      testSetString: {
        type: "string",
      },
    },
    additionalProperties: true,
  };

  sinon.spy(loader, "updateRecipes");
  sinon.spy(EnrollmentsContext.prototype, "_generateVariablesOnlySchema");
  sinon.stub(loader, "setTimer");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);

  sinon.stub(manager, "onFinalize");
  sinon.stub(manager, "onRecipe");
  sinon.stub(manager.store, "ready").resolves();

  await loader.init();
  ok(manager.onRecipe.calledOnce, "should call .updateRecipes");
  equal(loader.manager.onRecipe.callCount, 1, "should call .onRecipe once");
  ok(
    loader.manager.onRecipe.calledWith(recipe, "rs-loader"),
    "should call .onRecipe with argument data"
  );
  equal(loader.manager.onFinalize.callCount, 1, "should call .onFinalize once");
  ok(
    onFinalizeCalled(loader.manager.onFinalize, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "should call .onFinalize with nomismatches or invalid recipes"
  );

  ok(
    EnrollmentsContext.prototype._generateVariablesOnlySchema.calledOnce,
    "Should have generated a schema for testFeature"
  );

  Assert.deepEqual(
    EnrollmentsContext.prototype._generateVariablesOnlySchema.returnValues[0],
    EXPECTED_SCHEMA,
    "should have generated a schema with three fields"
  );

  info("Replacing recipe with an invalid one");

  loader.remoteSettingsClient.get.resolves([badRecipe]);

  await loader.updateRecipes("timer");
  equal(
    manager.onRecipe.callCount,
    1,
    "should not have called .onRecipe again"
  );
  equal(
    manager.onFinalize.callCount,
    2,
    "should have called .onFinalize again"
  );

  ok(
    onFinalizeCalled(loader.manager.onFinalize.secondCall.args, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map([["foo", [badRecipe.branches[0].slug]]]),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "should call .onFinalize with an invalid branch"
  );

  EnrollmentsContext.prototype._generateVariablesOnlySchema.restore();

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_validationTelemetry() {
  TelemetryEvents.init();

  Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    /* clear = */ true
  );

  const invalidRecipe = ExperimentFakes.recipe("invalid-recipe");
  delete invalidRecipe.channel;

  const invalidBranch = ExperimentFakes.recipe("invalid-branch");
  invalidBranch.branches[0].features[0].value.testInt = "hello";
  invalidBranch.branches[1].features[0].value.testInt = "world";

  const invalidFeature = ExperimentFakes.recipe("invalid-feature", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "unknown-feature",
            value: { foo: "bar" },
          },
          {
            featureId: "second-unknown-feature",
            value: { baz: "qux" },
          },
        ],
      },
    ],
  });

  const TEST_CASES = [
    {
      recipe: invalidRecipe,
      reason: "invalid-recipe",
      events: [{}],
      callCount: 1,
    },
    {
      recipe: invalidBranch,
      reason: "invalid-branch",
      events: invalidBranch.branches.map(branch => ({ branch: branch.slug })),
      callCount: 2,
    },
    {
      recipe: invalidFeature,
      reason: "invalid-feature",
      events: invalidFeature.branches[0].features.map(feature => ({
        feature: feature.featureId,
      })),
      callCount: 2,
    },
  ];

  const LEGACY_FILTER = {
    category: "normandy",
    method: "validationFailed",
    object: "nimbus_experiment",
  };

  for (const { recipe, reason, events, callCount } of TEST_CASES) {
    info(`Testing validation failed telemetry for reason = "${reason}" ...`);
    const loader = ExperimentFakes.rsLoader();
    const manager = loader.manager;

    sinon.stub(loader, "setTimer");
    sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);

    sinon.stub(manager, "onRecipe");
    sinon.stub(manager.store, "ready").resolves();
    sinon.stub(manager.store, "getAllActiveExperiments").returns([]);
    sinon.stub(manager.store, "getAllActiveRollouts").returns([]);

    const telemetrySpy = sinon.spy(manager, "sendValidationFailedTelemetry");

    await loader.init();

    Assert.equal(
      telemetrySpy.callCount,
      callCount,
      `Should call sendValidationFailedTelemetry ${callCount} times for reason ${reason}`
    );

    const gleanEvents = Glean.nimbusEvents.validationFailed
      .testGetValue()
      .map(event => {
        event = { ...event };
        // We do not care about the timestamp.
        delete event.timestamp;
        return event;
      });

    const expectedGleanEvents = events.map(event => ({
      category: "nimbus_events",
      name: "validation_failed",
      extra: {
        experiment: recipe.slug,
        reason,
        ...event,
      },
    }));

    Assert.deepEqual(
      gleanEvents,
      expectedGleanEvents,
      "Glean telemetry matches"
    );

    const expectedLegacyEvents = events.map(event => ({
      ...LEGACY_FILTER,
      value: recipe.slug,
      extra: {
        reason,
        ...event,
      },
      LEGACY_FILTER,
    }));

    TelemetryTestUtils.assertEvents(expectedLegacyEvents, LEGACY_FILTER, {
      clear: true,
    });

    Services.fog.testResetFOG();

    await assertEmptyStore(manager.store, { cleanup: true });
  }
});

add_task(async function test_updateRecipes_validationDisabled() {
  Services.prefs.setBoolPref("nimbus.validation.enabled", false);

  const invalidRecipe = ExperimentFakes.recipe("invalid-recipe");
  delete invalidRecipe.channel;

  const invalidBranch = ExperimentFakes.recipe("invalid-branch");
  invalidBranch.branches[0].features[0].value.testInt = "hello";
  invalidBranch.branches[1].features[0].value.testInt = "world";

  const invalidFeature = ExperimentFakes.recipe("invalid-feature", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "unknown-feature",
            value: { foo: "bar" },
          },
          {
            featureId: "second-unknown-feature",
            value: { baz: "qux" },
          },
        ],
      },
    ],
  });

  for (const recipe of [invalidRecipe, invalidBranch, invalidFeature]) {
    const loader = ExperimentFakes.rsLoader();
    const manager = loader.manager;

    sinon.stub(loader, "setTimer");
    sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);

    sinon.stub(manager, "onRecipe");
    sinon.stub(manager.store, "ready").resolves();
    sinon.stub(manager.store, "getAllActiveExperiments").returns([]);
    sinon.stub(manager.store, "getAllActiveRollouts").returns([]);

    const finalizeStub = sinon.stub(manager, "onFinalize");
    const telemetrySpy = sinon.spy(manager, "sendValidationFailedTelemetry");

    await loader.init();

    Assert.equal(
      telemetrySpy.callCount,
      0,
      "Should not send validation failed telemetry"
    );
    Assert.ok(
      onFinalizeCalled(finalizeStub, "rs-loader", {
        recipeMismatches: [],
        invalidRecipes: [],
        invalidBranches: new Map(),
        invalidFeatures: new Map(),
        missingLocale: [],
        missingL10nIds: new Map(),
        locale: Services.locale.appLocaleAsBCP47,
        validationEnabled: false,
      }),
      "should call .onFinalize with no validation issues"
    );

    await assertEmptyStore(manager.store, { cleanup: true });
  }

  Services.prefs.clearUserPref("nimbus.validation.enabled");
});

add_task(async function test_updateRecipes_appId() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  const recipe = ExperimentFakes.recipe("background-task-recipe", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "backgroundTaskMessage",
            value: {},
          },
        ],
      },
    ],
  });

  sinon.stub(loader, "setTimer");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);

  sinon.stub(manager, "onRecipe");
  sinon.stub(manager, "onFinalize");
  sinon.stub(manager.store, "ready").resolves();

  info("Testing updateRecipes() with the default application ID");
  await loader.init();

  Assert.equal(manager.onRecipe.callCount, 0, ".onRecipe was never called");
  Assert.ok(
    onFinalizeCalled(manager.onFinalize, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "Should call .onFinalize with no validation issues"
  );

  info("Testing updateRecipes() with a custom application ID");

  Services.prefs.setStringPref(
    "nimbus.appId",
    "firefox-desktop-background-task"
  );

  await loader.updateRecipes();
  Assert.ok(
    manager.onRecipe.calledWith(recipe, "rs-loader"),
    `.onRecipe called with ${recipe.slug}`
  );

  Assert.ok(
    onFinalizeCalled(manager.onFinalize, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "Should call .onFinalize with no validation issues"
  );

  Services.prefs.clearUserPref("nimbus.appId");

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_withPropNotInManifest() {
  // Need to randomize the slug so subsequent test runs don't skip enrollment
  // due to a conflicting slug
  const PASS_FILTER_RECIPE = ExperimentFakes.recipe("foo" + Math.random(), {
    arguments: {},
    branches: [
      {
        features: [
          {
            enabled: true,
            featureId: "testFeature",
            value: {
              enabled: true,
              testInt: 5,
              testSetString: "foo",
              additionalPropNotInManifest: 7,
            },
          },
        ],
        ratio: 1,
        slug: "treatment-2",
      },
    ],
    channel: "nightly",
    schemaVersion: "1.9.0",
    targeting: "true",
  });

  const loader = ExperimentFakes.rsLoader();
  sinon.stub(loader.remoteSettingsClient, "get").resolves([PASS_FILTER_RECIPE]);
  sinon.stub(loader.manager, "onRecipe").resolves();
  sinon.stub(loader.manager, "onFinalize");

  await loader.init();

  ok(
    loader.manager.onRecipe.calledWith(PASS_FILTER_RECIPE, "rs-loader"),
    "should call .onRecipe with this recipe"
  );
  equal(loader.manager.onRecipe.callCount, 1, "should only call onRecipe once");

  await assertEmptyStore(loader.manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_recipeAppId() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  const recipe = ExperimentFakes.recipe("mobile-experiment", {
    appId: "org.mozilla.firefox",
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "mobile-feature",
            value: {
              enabled: true,
            },
          },
        ],
      },
    ],
  });

  sinon.stub(loader, "setTimer");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);

  sinon.stub(manager, "onRecipe");
  sinon.stub(manager, "onFinalize");
  sinon.stub(manager.store, "ready").resolves();

  await loader.init();

  Assert.equal(manager.onRecipe.callCount, 0, ".onRecipe was never called");
  Assert.ok(
    onFinalizeCalled(manager.onFinalize, "rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      missingLocale: [],
      missingL10nIds: new Map(),
      locale: Services.locale.appLocaleAsBCP47,
      validationEnabled: true,
    }),
    "Should call .onFinalize with no validation issues"
  );

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_featureValidationOptOut() {
  const invalidTestRecipe = ExperimentFakes.recipe("invalid-recipe", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "testFeature",
            value: {
              enabled: "true",
              testInt: false,
            },
          },
        ],
      },
    ],
  });

  const message = await PanelTestProvider.getMessages().then(msgs =>
    msgs.find(m => m.id === "MULTISTAGE_SPOTLIGHT_MESSAGE")
  );
  delete message.template;

  const invalidMsgRecipe = ExperimentFakes.recipe("invalid-recipe", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "spotlight",
            value: message,
          },
        ],
      },
    ],
  });

  for (const invalidRecipe of [invalidTestRecipe, invalidMsgRecipe]) {
    const optOutRecipe = {
      ...invalidMsgRecipe,
      slug: "optout-recipe",
      featureValidationOptOut: true,
    };

    const loader = ExperimentFakes.rsLoader();
    const manager = loader.manager;

    sinon.stub(loader, "setTimer");
    sinon
      .stub(loader.remoteSettingsClient, "get")
      .resolves([invalidRecipe, optOutRecipe]);

    sinon.stub(manager, "onRecipe");
    sinon.stub(manager, "onFinalize");
    sinon.stub(manager.store, "ready").resolves();
    sinon.stub(manager.store, "getAllActiveExperiments").returns([]);
    sinon.stub(manager.store, "getAllActiveRollouts").returns([]);

    await loader.init();
    ok(
      manager.onRecipe.calledOnceWith(optOutRecipe, "rs-loader"),
      "should call .onRecipe for the opt-out recipe"
    );

    ok(
      manager.onFinalize.calledOnce &&
        onFinalizeCalled(manager.onFinalize, "rs-loader", {
          recipeMismatches: [],
          invalidRecipes: [],
          invalidBranches: new Map([[invalidRecipe.slug, ["control"]]]),
          invalidFeatures: new Map(),
          missingLocale: [],
          missingL10nIds: new Map(),
          locale: Services.locale.appLocaleAsBCP47,
          validationEnabled: true,
        }),
      "should call .onFinalize with only one invalid recipe"
    );

    await assertEmptyStore(manager.store, { cleanup: true });
  }
});

add_task(async function test_updateRecipes_invalidFeature_mismatch() {
  info(
    "Testing that we do not submit validation telemetry when the targeting does not match"
  );
  const recipe = ExperimentFakes.recipe("recipe", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "bogus",
            value: {
              bogus: "bogus",
            },
          },
        ],
      },
    ],
    targeting: "false",
  });

  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  sinon.stub(loader, "setTimer");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([recipe]);

  sinon.stub(manager, "onRecipe");
  sinon.stub(manager, "onFinalize");
  sinon.stub(manager.store, "ready").resolves();
  sinon.stub(manager.store, "getAllActiveExperiments").returns([]);
  sinon.stub(manager.store, "getAllActiveRollouts").returns([]);

  const telemetrySpy = sinon.stub(manager, "sendValidationFailedTelemetry");
  const targetingSpy = sinon.spy(
    EnrollmentsContext.prototype,
    "checkTargeting"
  );

  await loader.init();
  ok(targetingSpy.calledOnce, "Should have checked targeting for recipe");
  ok(
    !(await targetingSpy.returnValues[0]),
    "Targeting should not have matched"
  );
  ok(manager.onRecipe.notCalled, "should not call .onRecipe for the recipe");
  ok(
    telemetrySpy.notCalled,
    "Should not have submitted validation failed telemetry"
  );

  targetingSpy.restore();

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_updateRecipes_rollout_bucketing() {
  TelemetryEvents.init();
  Services.fog.testResetFOG();
  Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    /* clear = */ true
  );

  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  const experiment = ExperimentFakes.recipe("experiment", {
    branches: [
      {
        slug: "control",
        ratio: 1,
        features: [
          {
            featureId: "testFeature",
            value: {},
          },
        ],
      },
    ],
    bucketConfig: {
      namespace: "nimbus-test-utils",
      randomizationUnit: "normandy_id",
      start: 0,
      count: 1000,
      total: 1000,
    },
  });
  const rollout = ExperimentFakes.recipe("rollout", {
    isRollout: true,
    branches: [
      {
        slug: "rollout",
        ratio: 1,
        features: [
          {
            featureId: "testFeature",
            value: {},
          },
        ],
      },
    ],
    bucketConfig: {
      namespace: "nimbus-test-utils",
      randomizationUnit: "normandy_id",
      start: 0,
      count: 1000,
      total: 1000,
    },
  });

  await loader.init();
  await manager.onStartup();
  await manager.store.ready();

  sinon
    .stub(loader.remoteSettingsClient, "get")
    .resolves([experiment, rollout]);

  await loader.updateRecipes();

  Assert.equal(
    manager.store.getExperimentForFeature("testFeature")?.slug,
    experiment.slug,
    "Should enroll in experiment"
  );
  Assert.equal(
    manager.store.getRolloutForFeature("testFeature")?.slug,
    rollout.slug,
    "Should enroll in rollout"
  );

  experiment.bucketConfig.count = 0;
  rollout.bucketConfig.count = 0;

  await loader.updateRecipes();

  Assert.equal(
    manager.store.getExperimentForFeature("testFeature")?.slug,
    experiment.slug,
    "Should stay enrolled in experiment -- experiments cannot be resized"
  );
  Assert.ok(
    !manager.store.getRolloutForFeature("testFeature"),
    "Should unenroll from rollout"
  );

  const unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(
    unenrollmentEvents.length,
    1,
    "Should be one unenrollment event"
  );
  Assert.equal(
    unenrollmentEvents[0].extra.experiment,
    rollout.slug,
    "Experiment slug should match"
  );
  Assert.equal(
    unenrollmentEvents[0].extra.reason,
    "bucketing",
    "Reason should match"
  );

  TelemetryTestUtils.assertEvents(
    [
      {
        value: rollout.slug,
        extra: {
          reason: "bucketing",
        },
      },
    ],
    {
      category: "normandy",
      method: "unenroll",
      object: "nimbus_experiment",
    }
  );

  manager.unenroll(experiment.slug);
  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_reenroll_rollout_resized() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  await loader.init();
  await manager.onStartup();
  await manager.store.ready();

  const rollout = ExperimentFakes.recipe("rollout", {
    isRollout: true,
  });
  rollout.bucketConfig = {
    ...rollout.bucketConfig,
    start: 0,
    count: 1000,
    total: 1000,
  };

  sinon.stub(loader.remoteSettingsClient, "get").resolves([rollout]);

  await loader.updateRecipes();
  Assert.equal(
    manager.store.getRolloutForFeature("testFeature")?.slug,
    rollout.slug,
    "Should enroll in rollout"
  );

  rollout.bucketConfig.count = 0;
  await loader.updateRecipes();

  Assert.ok(
    !manager.store.getRolloutForFeature("testFeature"),
    "Should unenroll from rollout"
  );

  const enrollment = manager.store.get(rollout.slug);
  Assert.equal(enrollment.unenrollReason, "bucketing");

  rollout.bucketConfig.count = 1000;
  await loader.updateRecipes();

  Assert.equal(
    manager.store.getRolloutForFeature("testFeature")?.slug,
    rollout.slug,
    "Should re-enroll in rollout"
  );

  const newEnrollment = manager.store.get(rollout.slug);
  Assert.ok(
    !Object.is(enrollment, newEnrollment),
    "Should have new enrollment object"
  );
  Assert.ok(
    !("unenrollReason" in newEnrollment),
    "New enrollment should not have unenroll reason"
  );

  manager.unenroll(rollout.slug);
  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_experiment_reenroll() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  await loader.init();
  await manager.onStartup();
  await manager.store.ready();

  const experiment = ExperimentFakes.recipe("experiment");
  experiment.bucketConfig = {
    ...experiment.bucketConfig,
    start: 0,
    count: 1000,
    total: 1000,
  };

  await manager.enroll(experiment, "test");
  Assert.equal(
    manager.store.getExperimentForFeature("testFeature")?.slug,
    experiment.slug,
    "Should enroll in experiment"
  );

  manager.unenroll(experiment.slug);
  Assert.ok(
    !manager.store.getExperimentForFeature("testFeature"),
    "Should unenroll from experiment"
  );

  sinon.stub(loader.remoteSettingsClient, "get").resolves([experiment]);

  await loader.updateRecipes();
  Assert.ok(
    !manager.store.getExperimentForFeature("testFeature"),
    "Should not re-enroll in experiment"
  );

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_rollout_reenroll_optout() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  await loader.init();
  await manager.onStartup();
  await manager.store.ready();

  const rollout = ExperimentFakes.recipe("experiment", { isRollout: true });
  rollout.bucketConfig = {
    ...rollout.bucketConfig,
    start: 0,
    count: 1000,
    total: 1000,
  };

  sinon.stub(loader.remoteSettingsClient, "get").resolves([rollout]);
  await loader.updateRecipes();

  Assert.ok(
    manager.store.getRolloutForFeature("testFeature"),
    "Should enroll in rollout"
  );

  manager.unenroll(rollout.slug, "individual-opt-out");

  await loader.updateRecipes();

  Assert.ok(
    !manager.store.getRolloutForFeature("testFeature"),
    "Should not re-enroll in rollout"
  );

  await assertEmptyStore(manager.store, { cleanup: true });
});

add_task(async function test_active_and_past_experiment_targeting() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  await loader.init();
  await manager.onStartup();
  await manager.store.ready();

  const cleanupFeatures = ExperimentTestUtils.addTestFeatures(
    new ExperimentFeature("feature-a", {
      isEarlyStartup: false,
      variables: {},
    }),
    new ExperimentFeature("feature-b", {
      isEarlyStartup: false,
      variables: {},
    }),
    new ExperimentFeature("feature-c", { isEarlyStartup: false, variables: {} })
  );

  const experimentA = ExperimentFakes.recipe("experiment-a", {
    branches: [
      {
        ...ExperimentFakes.recipe.branches[0],
        features: [{ featureId: "feature-a", value: {} }],
      },
    ],
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
  });
  const experimentB = ExperimentFakes.recipe("experiment-b", {
    branches: [
      {
        ...ExperimentFakes.recipe.branches[0],
        features: [{ featureId: "feature-b", value: {} }],
      },
    ],
    bucketConfig: experimentA.bucketConfig,
    targeting: "'experiment-a' in activeExperiments",
  });
  const experimentC = ExperimentFakes.recipe("experiment-c", {
    branches: [
      {
        ...ExperimentFakes.recipe.branches[0],
        features: [{ featureId: "feature-c", value: {} }],
      },
    ],
    bucketConfig: experimentA.bucketConfig,
    targeting: "'experiment-a' in previousExperiments",
  });

  const rolloutA = ExperimentFakes.recipe("rollout-a", {
    branches: [
      {
        ...ExperimentFakes.recipe.branches[0],
        features: [{ featureId: "feature-a", value: {} }],
      },
    ],
    bucketConfig: experimentA.bucketConfig,
    isRollout: true,
  });
  const rolloutB = ExperimentFakes.recipe("rollout-b", {
    branches: [
      {
        ...ExperimentFakes.recipe.branches[0],
        features: [{ featureId: "feature-b", value: {} }],
      },
    ],
    bucketConfig: experimentA.bucketConfig,
    targeting: "'rollout-a' in activeRollouts",
    isRollout: true,
  });
  const rolloutC = ExperimentFakes.recipe("rollout-c", {
    branches: [
      {
        ...ExperimentFakes.recipe.branches[0],
        features: [{ featureId: "feature-c", value: {} }],
      },
    ],
    bucketConfig: experimentA.bucketConfig,
    targeting: "'rollout-a' in previousRollouts",
    isRollout: true,
  });

  sinon
    .stub(loader.remoteSettingsClient, "get")
    .resolves([experimentA, rolloutA]);

  // Enroll in A.
  await loader.updateRecipes();
  Assert.equal(
    manager.store.getExperimentForFeature("feature-a")?.slug,
    "experiment-a"
  );
  Assert.ok(!manager.store.getExperimentForFeature("feature-b"));
  Assert.ok(!manager.store.getExperimentForFeature("feature-c"));
  Assert.equal(
    manager.store.getRolloutForFeature("feature-a")?.slug,
    "rollout-a"
  );
  Assert.ok(!manager.store.getRolloutForFeature("feature-b"));
  Assert.ok(!manager.store.getRolloutForFeature("feature-c"));

  loader.remoteSettingsClient.get.resolves([
    experimentA,
    experimentB,
    experimentC,
    rolloutA,
    rolloutB,
    rolloutC,
  ]);

  // B will enroll becuase A is enrolled.
  await loader.updateRecipes();
  Assert.equal(
    manager.store.getExperimentForFeature("feature-a")?.slug,
    "experiment-a"
  );
  Assert.equal(
    manager.store.getExperimentForFeature("feature-b")?.slug,
    "experiment-b"
  );
  Assert.ok(!manager.store.getExperimentForFeature("feature-c"));
  Assert.equal(
    manager.store.getRolloutForFeature("feature-a")?.slug,
    "rollout-a"
  );
  Assert.equal(
    manager.store.getRolloutForFeature("feature-b")?.slug,
    "rollout-b"
  );
  Assert.ok(!manager.store.getRolloutForFeature("feature-c"));

  // Remove experiment A and rollout A to cause them to unenroll. A will still
  // be enrolled while B and C are evaluating targeting, so their enrollment
  // won't change.
  loader.remoteSettingsClient.get.resolves([
    experimentB,
    experimentC,
    rolloutB,
    rolloutC,
  ]);
  await loader.updateRecipes();
  Assert.ok(!manager.store.getExperimentForFeature("feature-a"));
  Assert.equal(
    manager.store.getExperimentForFeature("feature-b")?.slug,
    "experiment-b"
  );
  Assert.ok(!manager.store.getExperimentForFeature("feature-c"));
  Assert.ok(!manager.store.getRolloutForFeature("feature-a"));
  Assert.equal(
    manager.store.getRolloutForFeature("feature-b")?.slug,
    "rollout-b"
  );
  Assert.ok(!manager.store.getRolloutForFeature("feature-c"));

  // Now A will be marked as unenrolled while evaluating B and C's targeting, so
  // their enrollment will change.
  await loader.updateRecipes();
  Assert.ok(!manager.store.getExperimentForFeature("feature-a"));
  Assert.ok(!manager.store.getExperimentForFeature("feature-b"));
  Assert.equal(
    manager.store.getExperimentForFeature("feature-c")?.slug,
    "experiment-c"
  );
  Assert.ok(!manager.store.getRolloutForFeature("feature-a"));
  Assert.ok(!manager.store.getRolloutForFeature("feature-b"));
  Assert.equal(
    manager.store.getRolloutForFeature("feature-c")?.slug,
    "rollout-c"
  );

  manager.unenroll("experiment-c");
  manager.unenroll("rollout-c");

  await assertEmptyStore(manager.store, { cleanup: true });
  cleanupFeatures();
});

add_task(async function test_enrollment_targeting() {
  const loader = ExperimentFakes.rsLoader();
  const manager = loader.manager;

  await loader.init();
  await manager.onStartup();
  await manager.store.ready();

  const cleanupFeatures = ExperimentTestUtils.addTestFeatures(
    new ExperimentFeature("feature-a", {
      isEarlyStartup: false,
      variables: {},
    }),
    new ExperimentFeature("feature-b", {
      isEarlyStartup: false,
      variables: {},
    }),
    new ExperimentFeature("feature-c", {
      isEarlyStartup: false,
      variables: {},
    }),
    new ExperimentFeature("feature-d", {
      isEarlyStartup: false,
      variables: {},
    })
  );

  function recipe(
    name,
    featureId,
    { targeting = "true", isRollout = false } = {}
  ) {
    return ExperimentFakes.recipe(name, {
      branches: [
        {
          ...ExperimentFakes.recipe.branches[0],
          features: [{ featureId, value: {} }],
        },
      ],
      bucketConfig: {
        ...ExperimentFakes.recipe.bucketConfig,
        count: 1000,
      },
      targeting,
      isRollout,
    });
  }

  const experimentA = recipe("experiment-a", "feature-a", {
    targeting: "!('rollout-c' in enrollments)",
  });
  const experimentB = recipe("experiment-b", "feature-b", {
    targeting: "'rollout-a' in enrollments",
  });
  const experimentC = recipe("experiment-c", "feature-c");

  const rolloutA = recipe("rollout-a", "feature-a", {
    targeting: "!('experiment-c' in enrollments)",
    isRollout: true,
  });
  const rolloutB = recipe("rollout-b", "feature-b", {
    targeting: "'experiment-a' in enrollments",
    isRollout: true,
  });
  const rolloutC = recipe("rollout-c", "feature-c", { isRollout: true });

  async function check(current, past, unenrolled) {
    await loader.updateRecipes();

    for (const slug of current) {
      const enrollment = manager.store.get(slug);
      Assert.equal(
        enrollment?.active,
        true,
        `Enrollment exists for ${slug} and is active`
      );
    }

    for (const slug of past) {
      const enrollment = manager.store.get(slug);
      Assert.equal(
        enrollment?.active,
        false,
        `Enrollment exists for ${slug} and is inactive`
      );
    }

    for (const slug of unenrolled) {
      Assert.ok(
        !manager.store.get(slug),
        `Enrollment does not exist for ${slug}`
      );
    }
  }

  sinon
    .stub(loader.remoteSettingsClient, "get")
    .resolves([experimentB, rolloutB]);
  await check(
    [],
    [],
    [
      "experiment-a",
      "experiment-b",
      "experiment-c",
      "rollout-a",
      "rollout-b",
      "rollout-c",
    ]
  );

  // Order matters -- B will be checked before A.
  loader.remoteSettingsClient.get.resolves([
    experimentB,
    rolloutB,
    experimentA,
    rolloutA,
  ]);
  await check(
    ["experiment-a", "rollout-a"],
    [],
    ["experiment-b", "experiment-c", "rollout-b", "rollout-c"]
  );

  // B will see A enrolled.
  loader.remoteSettingsClient.get.resolves([
    experimentB,
    rolloutB,
    experimentA,
    rolloutA,
  ]);
  await check(
    ["experiment-a", "experiment-b", "rollout-a", "rollout-b"],
    [],
    ["experiment-c", "rollout-c"]
  );

  // Order matters -- A will be checked before C.
  loader.remoteSettingsClient.get.resolves([
    experimentB,
    rolloutB,
    experimentA,
    rolloutA,
    experimentC,
    rolloutC,
  ]);
  await check(
    [
      "experiment-a",
      "experiment-b",
      "experiment-c",
      "rollout-a",
      "rollout-b",
      "rollout-c",
    ],
    [],
    []
  );

  // A will see C has enrolled and unenroll. B will stay enrolled.
  await check(
    ["experiment-b", "experiment-c", "rollout-b", "rollout-c"],
    ["experiment-a", "rollout-a"],
    []
  );

  // A being unenrolled does not affect B. Rollout A will not re-enroll due to targeting.
  await check(
    ["experiment-b", "experiment-c", "rollout-b", "rollout-c"],
    ["experiment-a", "rollout-a"],
    []
  );

  for (const slug of [
    "experiment-b",
    "experiment-c",
    "rollout-b",
    "rollout-c",
  ]) {
    manager.unenroll(slug);
  }

  await assertEmptyStore(manager.store, { cleanup: true });
  cleanupFeatures();
});

add_task(async function test_update_experiments_ordered_by_published_date() {
  const manager = ExperimentFakes.manager();
  const sandbox = sinon.createSandbox();
  const loader = ExperimentFakes.rsLoader();
  loader.manager = manager;
  const RECIPE_NO_PUBLISHED_DATE_1 = ExperimentFakes.recipe("foo");
  const RECIPE_NO_PUBLISHED_DATE_2 = ExperimentFakes.recipe("bar");
  const RECIPE_PUBLISHED_DATE_1 = ExperimentFakes.recipe("baz", {
    publishedDate: `2024-01-05T12:00:00Z`,
  });
  const RECIPE_PUBLISHED_DATE_2 = ExperimentFakes.recipe("qux", {
    publishedDate: `2024-01-03T12:00:00Z`,
  });
  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon
    .stub(loader.remoteSettingsClient, "get")
    .resolves([
      RECIPE_NO_PUBLISHED_DATE_1,
      RECIPE_PUBLISHED_DATE_1,
      RECIPE_PUBLISHED_DATE_2,
      RECIPE_NO_PUBLISHED_DATE_2,
    ]);
  sandbox.stub(manager.store, "ready").resolves();

  await loader.init();

  ok(onRecipe.getCall(0).calledWithMatch({ slug: "foo" }, "rs-loader"));
  ok(onRecipe.getCall(1).calledWithMatch({ slug: "bar" }, "rs-loader"));
  ok(onRecipe.getCall(2).calledWithMatch({ slug: "qux" }, "rs-loader"));
  ok(onRecipe.getCall(3).calledWithMatch({ slug: "baz" }, "rs-loader"));

  await assertEmptyStore(manager.store);
});
