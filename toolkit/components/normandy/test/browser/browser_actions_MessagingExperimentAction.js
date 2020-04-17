"use strict";

const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const { Uptake } = ChromeUtils.import("resource://normandy/lib/Uptake.jsm");
const { MessagingExperimentAction } = ChromeUtils.import(
  "resource://normandy/actions/MessagingExperimentAction.jsm"
);

const { _ExperimentManager, ExperimentManager } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentManager.jsm"
);

decorate_task(
  withStudiesEnabled,
  withStub(Uptake, "reportRecipe"),
  async function arguments_are_validated(reportRecipe) {
    const action = new MessagingExperimentAction();

    is(
      action.manager,
      ExperimentManager,
      "should set .manager to ExperimentManager singleton"
    );
    // Override this for the purposes of the test
    action.manager = new _ExperimentManager();
    await action.manager.onStartup();
    const onRecipeStub = sinon.spy(action.manager, "onRecipe");

    const recipe = {
      id: 1,
      arguments: {
        slug: "foo",
        branches: [
          {
            slug: "control",
            ratio: 1,
            groups: ["green"],
            value: { title: "hello" },
          },
          {
            slug: "variant",
            ratio: 1,
            groups: ["green"],
            value: { title: "world" },
          },
        ],
      },
    };

    ok(action.validateArguments(recipe.arguments), "should validate arguments");

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();

    Assert.deepEqual(reportRecipe.args, [[recipe, Uptake.RECIPE_SUCCESS]]);
    Assert.deepEqual(
      onRecipeStub.args,
      [[recipe.arguments]],
      "should call onRecipe with recipe args"
    );
  }
);
