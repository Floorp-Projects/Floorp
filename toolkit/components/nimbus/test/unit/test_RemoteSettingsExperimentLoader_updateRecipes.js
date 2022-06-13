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
            enabled: true,
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
            enabled: true,
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
      invalidBranches: [],
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
      invalidBranches: [],
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
      invalidBranches: [],
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
      invalidBranches: ["foo"],
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
    required: ["enabled", "testInt"],
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
      invalidBranches: [],
    }),
    "should call .onFinalize with nomismatches or invalid recipes"
  );

  ok(
    loader._generateVariablesOnlySchema.calledOnce,
    "Should have generated a schema for testFeature"
  );

  {
    const returned = loader._generateVariablesOnlySchema.returnValues[0];

    // Ensure required is kept in sorted order, otherwise Assert.deepEqual will fail.
    returned.required.sort();
    EXPECTED_SCHEMA.required.sort();

    Assert.deepEqual(
      returned,
      EXPECTED_SCHEMA,
      "should have generated a schema with two fields"
    );
  }

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
      invalidBranches: ["foo"],
    }),
    "should call .onFinalize with an invalid branch"
  );
});
