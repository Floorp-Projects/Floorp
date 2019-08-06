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

function branchFactory(opts = {}) {
  const defaultPreferences = {
    "fake.preference": {},
  };
  const defaultPrefInfo = {
    preferenceType: "string",
    preferenceBranchType: "default",
    preferenceValue: "foo",
  };
  const preferences = {};
  for (const [prefName, prefInfo] of Object.entries(
    opts.preferences || defaultPreferences
  )) {
    preferences[prefName] = { ...defaultPrefInfo, ...prefInfo };
  }
  return {
    slug: "test",
    ratio: 1,
    ...opts,
    preferences,
  };
}

function argumentsFactory(args) {
  const defaultBranches = (args && args.branches) || [{}];
  const branches = defaultBranches.map(branchFactory);
  return {
    slug: "test",
    userFacingName: "Super Cool Test Experiment",
    userFacingDescription:
      "Test experiment from browser_actions_PreferenceExperimentAction.",
    isHighPopulation: false,
    ...args,
    branches,
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
  withStub(Uptake, "reportRecipe"),
  async function run_without_errors(reportRecipe) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory();
    await action.runRecipe(recipe);
    await action.finalize();
    // runRecipe catches exceptions thrown by _run(), so
    // explicitly check for reported success here.
    Assert.deepEqual(reportRecipe.args, [[recipe, Uptake.RECIPE_SUCCESS]]);
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(Uptake, "reportRecipe"),
  withStub(Uptake, "reportAction"),
  withPrefEnv({ set: [["app.shield.optoutstudies.enabled", false]] }),
  async function checks_disabled(reportRecipe, reportAction) {
    const action = new PreferenceExperimentAction();
    action.log = mockLogger();

    const recipe = preferenceExperimentFactory();
    await action.runRecipe(recipe);

    Assert.ok(action.log.debug.args.length === 1);
    Assert.deepEqual(action.log.debug.args[0], [
      "User has opted-out of opt-out experiments, disabling action.",
    ]);
    Assert.deepEqual(action.log.warn.args, [
      [
        "Skipping recipe preference-experiment because PreferenceExperimentAction " +
          "was disabled during preExecution.",
      ],
    ]);

    await action.finalize();
    Assert.ok(action.log.debug.args.length === 2);
    Assert.deepEqual(action.log.debug.args[1], [
      "Skipping post-execution hook for PreferenceExperimentAction because it is disabled.",
    ]);
    Assert.deepEqual(reportRecipe.args, [
      [recipe, Uptake.RECIPE_ACTION_DISABLED],
    ]);
    Assert.deepEqual(reportAction.args, [[action.name, Uptake.ACTION_SUCCESS]]);
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "start"),
  PreferenceExperiments.withMockExperiments([]),
  async function enroll_user_if_never_been_in_experiment(startStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "test",
      branches: [
        {
          slug: "branch1",
          preferences: {
            "fake.preference": {
              preferenceBranchType: "user",
              preferenceValue: "branch1",
            },
          },
          ratio: 1,
        },
        {
          slug: "branch2",
          preferences: {
            "fake.preference": {
              preferenceBranchType: "user",
              preferenceValue: "branch2",
            },
          },
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

    Assert.deepEqual(startStub.args, [
      [
        {
          name: "test",
          actionName: "PreferenceExperimentAction",
          branch: "branch1",
          preferences: {
            "fake.preference": {
              preferenceValue: "branch1",
              preferenceBranchType: "user",
              preferenceType: "string",
            },
          },
          experimentType: "exp",
          userFacingName: "Super Cool Test Experiment",
          userFacingDescription:
            "Test experiment from browser_actions_PreferenceExperimentAction.",
        },
      ],
    ]);
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "markLastSeen"),
  PreferenceExperiments.withMockExperiments([{ name: "test", expired: false }]),
  async function markSeen_if_experiment_active(markLastSeenStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "test",
    });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(markLastSeenStub.args, [["test"]]);
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "markLastSeen"),
  PreferenceExperiments.withMockExperiments([{ name: "test", expired: true }]),
  async function dont_markSeen_if_experiment_expired(markLastSeenStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "test",
    });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(markLastSeenStub.args, [], "markLastSeen was not called");
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "start"),
  async function do_nothing_if_enrollment_paused(startStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      isEnrollmentPaused: true,
    });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(startStub.args, [], "start was not called");
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "stop"),
  PreferenceExperiments.withMockExperiments([
    { name: "seen", expired: false, actionName: "PreferenceExperimentAction" },
    {
      name: "unseen",
      expired: false,
      actionName: "PreferenceExperimentAction",
    },
  ]),
  async function stop_experiments_not_seen(stopStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "seen",
    });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(stopStub.args, [
      ["unseen", { resetValue: true, reason: "recipe-not-seen" }],
    ]);
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "stop"),
  PreferenceExperiments.withMockExperiments([
    {
      name: "seen",
      expired: false,
      actionName: "SinglePreferenceExperimentAction",
    },
    {
      name: "unseen",
      expired: false,
      actionName: "SinglePreferenceExperimentAction",
    },
  ]),
  async function dont_stop_experiments_for_other_action(stopStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "seen",
    });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(
      stopStub.args,
      [],
      "stop not called for other action's experiments"
    );
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "start"),
  withStub(Uptake, "reportRecipe"),
  PreferenceExperiments.withMockExperiments([
    {
      name: "conflict",
      preferences: {
        "conflict.pref": {},
      },
      expired: false,
    },
  ]),
  async function do_nothing_if_preference_is_already_being_tested(
    startStub,
    reportRecipeStub
  ) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "new",
      branches: [
        {
          preferences: { "conflict.pref": {} },
        },
      ],
    });
    action.chooseBranch = sinon
      .stub()
      .callsFake(async function(slug, branches) {
        return branches[0];
      });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(reportRecipeStub.args, [
      [recipe, Uptake.RECIPE_EXECUTION_ERROR],
    ]);
    Assert.deepEqual(startStub.args, [], "start not called");
    // No way to get access to log message/Error thrown
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "start"),
  PreferenceExperiments.withMockExperiments([]),
  async function experimentType_with_isHighPopulation_false(startStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      isHighPopulation: false,
    });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(startStub.args[0][0].experimentType, "exp");
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "start"),
  PreferenceExperiments.withMockExperiments([]),
  async function experimentType_with_isHighPopulation_true(startStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      isHighPopulation: true,
    });

    await action.runRecipe(recipe);
    await action.finalize();

    Assert.deepEqual(startStub.args[0][0].experimentType, "exp-highpop");
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(Sampling, "ratioSample"),
  async function chooseBranch_uses_ratioSample(ratioSampleStub) {
    ratioSampleStub.returns(Promise.resolve(1));
    const action = new PreferenceExperimentAction();
    const branches = [
      {
        preferences: {
          "fake.preference": {
            preferenceValue: "branch0",
          },
        },
        ratio: 1,
      },
      {
        preferences: {
          "fake.preference": {
            preferenceValue: "branch1",
          },
        },
        ratio: 2,
      },
    ];
    const sandbox = sinon.createSandbox();
    let result;
    try {
      sandbox.stub(ClientEnvironment, "userId").get(() => "fake-id");
      result = await action.chooseBranch("exp-slug", branches);
    } finally {
      sandbox.restore();
    }

    Assert.deepEqual(ratioSampleStub.args, [
      ["fake-id-exp-slug-branch", [1, 2]],
    ]);
    Assert.deepEqual(result, branches[1]);
  }
);

decorate_task(
  withStudiesEnabled,
  withMockPreferences,
  PreferenceExperiments.withMockExperiments([]),
  async function integration_test_enroll_and_unenroll(prefs) {
    prefs.set("fake.preference", "oldvalue", "user");
    const recipe = preferenceExperimentFactory({
      slug: "integration test experiment",
      branches: [
        {
          slug: "branch1",
          preferences: {
            "fake.preference": {
              preferenceBranchType: "user",
              preferenceValue: "branch1",
            },
          },
          ratio: 1,
        },
        {
          slug: "branch2",
          preferences: {
            "fake.preference": {
              preferenceBranchType: "user",
              preferenceValue: "branch2",
            },
          },
          ratio: 1,
        },
      ],
      userFacingName: "userFacingName",
      userFacingDescription: "userFacingDescription",
    });

    // Session 1: we see the above recipe and enroll in the experiment.
    const action = new PreferenceExperimentAction();
    sinon
      .stub(action, "chooseBranch")
      .callsFake(async function(slug, branches) {
        return branches[0];
      });
    await action.runRecipe(recipe);
    await action.finalize();

    const activeExperiments = await PreferenceExperiments.getAllActive();
    ok(activeExperiments.length > 0);
    Assert.deepEqual(activeExperiments, [
      {
        name: "integration test experiment",
        actionName: "PreferenceExperimentAction",
        branch: "branch1",
        preferences: {
          "fake.preference": {
            preferenceBranchType: "user",
            preferenceValue: "branch1",
            preferenceType: "string",
            previousPreferenceValue: "oldvalue",
          },
        },
        expired: false,
        lastSeen: activeExperiments[0].lastSeen, // can't predict date
        experimentType: "exp",
        userFacingName: "userFacingName",
        userFacingDescription: "userFacingDescription",
      },
    ]);

    // Session 2: recipe is filtered out and so does not run.
    const action2 = new PreferenceExperimentAction();
    await action2.finalize();

    // Experiment should be unenrolled
    Assert.deepEqual(await PreferenceExperiments.getAllActive(), []);
  }
);
