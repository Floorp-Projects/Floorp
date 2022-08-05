"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { FirstStartup } = ChromeUtils.import(
  "resource://gre/modules/FirstStartup.jsm"
);
const { NimbusFeatures } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { TelemetryEvents } = ChromeUtils.import(
  "resource://normandy/lib/TelemetryEvents.jsm"
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
  sandbox.stub(manager.store, "getAllActive").returns([recipe]);

  await loader.init();

  ok(onRecipe.calledOnce, "Should match active experiments");
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
  sandbox.stub(manager.store, "getAllActive").returns([recipe]);

  // Pretend to be in the first startup
  FirstStartup._state = FirstStartup.IN_PROGRESS;
  await loader.init();

  Assert.ok(onRecipe.calledOnce, "Should match first run");
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
  sandbox.stub(manager.store, "getAllActive").returns([]);

  await loader.init();
  ok(onRecipe.notCalled, "No recipes");
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
              id: "test-spotlight-invalid-1",
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
              id: "test-spotlight-invalid-2",
            },
          },
        ],
      },
    ],
  });

  const onRecipe = sandbox.stub(manager, "onRecipe");
  sinon.stub(loader.remoteSettingsClient, "get").resolves([badRecipe]);
  sandbox.stub(manager.store, "ready").resolves();
  sandbox.stub(manager.store, "getAllActive").returns([]);

  await loader.init();
  ok(onRecipe.notCalled, "No recipes");
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
  sandbox.stub(manager.store, "getAllActive").returns([]);

  await loader.init();
  ok(onRecipe.notCalled, "No recipes");
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
    loader.manager.onFinalize.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
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
    loader.manager.onFinalize.secondCall.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: ["foo"],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      validationEnabled: true,
    }),
    "should call .onFinalize with an invalid recipe"
  );
});

add_task(async function test_updateRecipes_invalidBranchAfterUpdate() {
  const message = await PanelTestProvider.getMessages().then(msgs =>
    msgs.find(m => m.id === "SPOTLIGHT_MESSAGE_93")
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
    loader.manager.onFinalize.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
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
    loader.manager.onFinalize.secondCall.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map([["foo", [badRecipe.branches[0].slug]]]),
      invalidFeatures: new Map(),
      validationEnabled: true,
    }),
    "should call .onFinalize with an invalid branch"
  );
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
    },
    additionalProperties: false,
  };

  sinon.spy(loader, "updateRecipes");
  sinon.spy(loader, "_generateVariablesOnlySchema");
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
    loader.manager.onFinalize.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      validationEnabled: true,
    }),
    "should call .onFinalize with nomismatches or invalid recipes"
  );

  ok(
    loader._generateVariablesOnlySchema.calledOnce,
    "Should have generated a schema for testFeature"
  );

  Assert.deepEqual(
    loader._generateVariablesOnlySchema.returnValues[0],
    EXPECTED_SCHEMA,
    "should have generated a schema with two fields"
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
    loader.manager.onFinalize.secondCall.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map([["foo", [badRecipe.branches[0].slug]]]),
      invalidFeatures: new Map(),
      validationEnabled: true,
    }),
    "should call .onFinalize with an invalid branch"
  );
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
    sinon.stub(manager.store, "getAllActive").returns([]);
    sinon.stub(manager.store, "getAllRollouts").returns([]);

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
    sinon.stub(manager.store, "getAllActive").returns([]);
    sinon.stub(manager.store, "getAllRollouts").returns([]);

    const finalizeStub = sinon.stub(manager, "onFinalize");
    const telemetrySpy = sinon.spy(manager, "sendValidationFailedTelemetry");

    await loader.init();

    Assert.equal(
      telemetrySpy.callCount,
      0,
      "Should not send validation failed telemetry"
    );
    Assert.ok(
      finalizeStub.calledWith("rs-loader", {
        recipeMismatches: [],
        invalidRecipes: [],
        invalidBranches: new Map(),
        invalidFeatures: new Map(),
        validationEnabled: false,
      }),
      "should call .onFinalize with no validation issues"
    );
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
    manager.onFinalize.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
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
    manager.onFinalize.calledWith("rs-loader", {
      recipeMismatches: [],
      invalidRecipes: [],
      invalidBranches: new Map(),
      invalidFeatures: new Map(),
      validationEnabled: true,
    }),
    "Should call .onFinalize with no validation issues"
  );

  Services.prefs.clearUserPref("nimbus.appId");
});
