"use strict";

const { BaseAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BaseAction.sys.mjs"
);
const { Uptake } = ChromeUtils.importESModule(
  "resource://normandy/lib/Uptake.sys.mjs"
);
const { MessagingExperimentAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/MessagingExperimentAction.sys.mjs"
);

const { _ExperimentManager, ExperimentManager } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentManager.sys.mjs"
);

decorate_task(
  withStudiesEnabled(),
  withStub(Uptake, "reportRecipe"),
  async function arguments_are_validated({ reportRecipeStub }) {
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
        isEnrollmentPaused: false,
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

    Assert.deepEqual(reportRecipeStub.args, [[recipe, Uptake.RECIPE_SUCCESS]]);
    Assert.deepEqual(
      onRecipeStub.args,
      [[recipe.arguments, "normandy"]],
      "should call onRecipe with recipe args and 'normandy' source"
    );
  }
);
