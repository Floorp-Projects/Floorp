/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.import(
  "resource://gre/modules/components-utils/Sampling.jsm",
  this
);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/ClientEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);
ChromeUtils.import(
  "resource://normandy/actions/PreferenceExperimentAction.jsm",
  this
);
ChromeUtils.import(
  "resource://normandy/actions/SinglePreferenceExperimentAction.jsm",
  this
);

function argumentsFactory(args) {
  return {
    slug: "test",
    preferenceName: "fake.preference",
    preferenceType: "string",
    preferenceBranchType: "default",
    branches: [{ slug: "test", value: "foo", ratio: 1 }],
    isHighPopulation: false,
    ...args,
  };
}

function preferenceExperimentFactory(args) {
  return recipeFactory({
    name: "preference-experiment",
    arguments: argumentsFactory(args),
  });
}

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperimentAction.prototype, "_run"),
  PreferenceExperiments.withMockExperiments([]),
  async function enroll_user_if_never_been_in_experiment(runStub) {
    const action = new SinglePreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "test",
      preferenceName: "fake.preference",
      preferenceBranchType: "user",
      branches: [
        {
          slug: "branch1",
          value: "branch1",
          ratio: 1,
        },
        {
          slug: "branch2",
          value: "branch2",
          ratio: 1,
        },
      ],
    });
    sinon
      .stub(action, "chooseBranch")
      .callsFake(async function(slug, branches) {
        return branches[0];
      });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(runStub.args, [
      [
        {
          id: recipe.id,
          name: "preference-experiment",
          arguments: {
            slug: "test",
            userFacingName: null,
            userFacingDescription: null,
            isHighPopulation: false,
            branches: [
              {
                slug: "branch1",
                ratio: 1,
                preferences: {
                  "fake.preference": {
                    preferenceValue: "branch1",
                    preferenceType: "string",
                    preferenceBranchType: "user",
                  },
                },
              },
              {
                slug: "branch2",
                ratio: 1,
                preferences: {
                  "fake.preference": {
                    preferenceValue: "branch2",
                    preferenceType: "string",
                    preferenceBranchType: "user",
                  },
                },
              },
            ],
          },
        },
      ],
    ]);
  }
);
