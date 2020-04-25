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
ChromeUtils.import("resource://normandy/actions/BaseAction.jsm", this);
ChromeUtils.import(
  "resource://normandy/actions/PreferenceExperimentAction.jsm",
  this
);
ChromeUtils.import("resource://testing-common/NormandyTestUtils.jsm", this);

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
  const defaultBranches = (args && args.branches) || [{ preferences: [] }];
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    // Errors thrown in actions are caught and silenced, so instead check for an
    // explicit success here.
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);

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

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();

    Assert.deepEqual(startStub.args, [
      [
        {
          slug: "test",
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
  PreferenceExperiments.withMockExperiments([{ slug: "test", expired: false }]),
  async function markSeen_if_experiment_active(markLastSeenStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      name: "test",
    });

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();

    Assert.deepEqual(markLastSeenStub.args, [["test"]]);
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "markLastSeen"),
  PreferenceExperiments.withMockExperiments([{ slug: "test", expired: true }]),
  async function dont_markSeen_if_experiment_expired(markLastSeenStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      name: "test",
    });

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
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

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();

    Assert.deepEqual(startStub.args, [], "start was not called");
  }
);

decorate_task(
  withStudiesEnabled,
  withStub(PreferenceExperiments, "stop"),
  PreferenceExperiments.withMockExperiments([
    { slug: "seen", expired: false, actionName: "PreferenceExperimentAction" },
    {
      slug: "unseen",
      expired: false,
      actionName: "PreferenceExperimentAction",
    },
  ]),
  async function stop_experiments_not_seen(stopStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      slug: "seen",
    });

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
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
      slug: "seen",
      expired: false,
      actionName: "SinglePreferenceExperimentAction",
    },
    {
      slug: "unseen",
      expired: false,
      actionName: "SinglePreferenceExperimentAction",
    },
  ]),
  async function dont_stop_experiments_for_other_action(stopStub) {
    const action = new PreferenceExperimentAction();
    const recipe = preferenceExperimentFactory({
      name: "seen",
    });

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
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
      slug: "conflict",
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
      name: "new",
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

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
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

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
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

    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
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
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();

    const activeExperiments = await PreferenceExperiments.getAllActive();
    ok(!!activeExperiments.length);
    Assert.deepEqual(activeExperiments, [
      {
        slug: "integration test experiment",
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
        enrollmentId: activeExperiments[0].enrollmentId,
      },
    ]);

    // Session 2: recipe is filtered out and so does not run.
    const action2 = new PreferenceExperimentAction();
    await action2.finalize();

    // Experiment should be unenrolled
    Assert.deepEqual(await PreferenceExperiments.getAllActive(), []);
  }
);

// Check that the appropriate set of suitabilities are considered temporary errors
decorate_task(
  withStudiesEnabled,
  async function test_temporary_errors_set_deadline() {
    let suitabilities = [
      {
        suitability: BaseAction.suitability.SIGNATURE_ERROR,
        isTemporaryError: true,
      },
      {
        suitability: BaseAction.suitability.CAPABILITES_MISMATCH,
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
    // failures, give this a generous bound of 2 hour on either side.
    let now = Date.now();
    let hour = 60 * 60 * 1000;
    let expectedDeadline = now + 7 * 24 * hour;
    let minDeadline = new Date(expectedDeadline - 2 * hour);
    let maxDeadline = new Date(expectedDeadline + 2 * hour);

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const { suitability, isTemporaryError } of suitabilities) {
      const decorator = PreferenceExperiments.withMockExperiments([
        { slug: `test-for-suitability-${suitability}` },
      ]);
      await decorator(async ([experiment]) => {
        let action = new PreferenceExperimentAction();
        let recipe = preferenceExperimentFactory({ slug: experiment.slug });
        await action.processRecipe(recipe, suitability);
        let modifiedExperiment = await PreferenceExperiments.get(
          experiment.slug
        );
        if (isTemporaryError) {
          is(
            typeof modifiedExperiment.temporaryErrorDeadline,
            "string",
            `A temporary failure deadline should be set as a string for suitability ${suitability}`
          );
          let deadline = new Date(modifiedExperiment.temporaryErrorDeadline);
          ok(
            deadline >= minDeadline && deadline <= maxDeadline,
            `The temporary failure deadline should be in the expected range for ` +
              `suitability ${suitability} (got ${deadline})`
          );
        } else {
          ok(
            !modifiedExperiment.temporaryErrorDeadline,
            `No temporary failure deadline should be set for suitability ${suitability}`
          );
        }
      })();
    }
  }
);

// Check that if there is an existing deadline, temporary errors don't overwrite it
decorate_task(
  withStudiesEnabled,
  PreferenceExperiments.withMockExperiments([]),
  async function test_temporary_errors_dont_overwrite_deadline() {
    let temporaryFailureSuitabilities = [
      BaseAction.suitability.SIGNATURE_ERROR,
      BaseAction.suitability.FILTER_ERROR,
    ];

    // A deadline two hours in the future won't be hit during the test.
    let now = Date.now();
    let hour = 2 * 60 * 60 * 1000;
    let unhitDeadline = new Date(now + hour).toJSON();

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of temporaryFailureSuitabilities) {
      const decorator = PreferenceExperiments.withMockExperiments([
        {
          slug: `test-for-suitability-${suitability}`,
          expired: false,
          temporaryErrorDeadline: unhitDeadline,
        },
      ]);
      await decorator(async ([experiment]) => {
        let action = new PreferenceExperimentAction();
        let recipe = preferenceExperimentFactory({ slug: experiment.slug });
        await action.processRecipe(recipe, suitability);
        let modifiedExperiment = await PreferenceExperiments.get(
          experiment.slug
        );
        is(
          modifiedExperiment.temporaryErrorDeadline,
          unhitDeadline,
          `The temporary failure deadline should not be cleared for suitability ${suitability}`
        );
      })();
    }
  }
);

// Check that if the deadline is past, temporary errors end the experiment.
decorate_task(
  withStudiesEnabled,
  async function test_temporary_errors_hit_deadline() {
    let temporaryFailureSuitabilities = [
      BaseAction.suitability.SIGNATURE_ERROR,
      BaseAction.suitability.FILTER_ERROR,
    ];

    // Set a deadline of two hours in the past, so that the experiment expires.
    let now = Date.now();
    let hour = 2 * 60 * 60 * 1000;
    let hitDeadline = new Date(now - hour).toJSON();

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of temporaryFailureSuitabilities) {
      const decorator = PreferenceExperiments.withMockExperiments([
        {
          slug: `test-for-suitability-${suitability}`,
          expired: false,
          temporaryErrorDeadline: hitDeadline,
          preferences: [],
          branch: "test-branch",
        },
      ]);
      await decorator(async ([experiment]) => {
        let action = new PreferenceExperimentAction();
        let recipe = preferenceExperimentFactory({ slug: experiment.slug });
        await action.processRecipe(recipe, suitability);
        let modifiedExperiment = await PreferenceExperiments.get(
          experiment.slug
        );
        ok(
          modifiedExperiment.expired,
          `The experiment should be expired for suitability ${suitability}`
        );
      })();
    }
  }
);

// Check that non-temporary-error suitabilities clear the temporary deadline
decorate_task(
  withStudiesEnabled,
  PreferenceExperiments.withMockExperiments([]),
  async function test_non_temporary_error_clears_temporary_error_deadline() {
    let suitabilitiesThatShouldClearDeadline = [
      BaseAction.suitability.CAPABILITES_MISMATCH,
      BaseAction.suitability.FILTER_MATCH,
      BaseAction.suitability.FILTER_MISMATCH,
      BaseAction.suitability.ARGUMENTS_INVALID,
    ];

    // Use a deadline in the past to demonstrate that even if the deadline has
    // passed, only a temporary error suitability ends the experiment.
    let now = Date.now();
    let hour = 60 * 60 * 1000;
    let hitDeadline = new Date(now - 2 * hour).toJSON();

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of suitabilitiesThatShouldClearDeadline) {
      const decorator = PreferenceExperiments.withMockExperiments([
        NormandyTestUtils.factories.preferenceStudyFactory({
          slug: `test-for-suitability-${suitability}`.toLocaleLowerCase(),
          expired: false,
          temporaryErrorDeadline: hitDeadline,
        }),
      ]);
      await decorator(async ([experiment]) => {
        let action = new PreferenceExperimentAction();
        let recipe = preferenceExperimentFactory({ slug: experiment.slug });
        await action.processRecipe(recipe, suitability);
        let modifiedExperiment = await PreferenceExperiments.get(
          experiment.slug
        );
        ok(
          !modifiedExperiment.temporaryErrorDeadline,
          `The temporary failure deadline should be cleared for suitabilitiy ${suitability}`
        );
      })();
    }
  }
);

// Check that invalid deadlines are reset
decorate_task(
  withStudiesEnabled,
  PreferenceExperiments.withMockExperiments([]),
  async function test_non_temporary_error_clears_temporary_error_deadline() {
    let temporaryFailureSuitabilities = [
      BaseAction.suitability.SIGNATURE_ERROR,
      BaseAction.suitability.FILTER_ERROR,
    ];

    // The action should set a deadline 1 week from now. To avoid intermittent
    // failures, give this a generous bound of 2 hours on either side.
    let invalidDeadline = "not a valid date";
    let now = Date.now();
    let hour = 60 * 60 * 1000;
    let expectedDeadline = now + 7 * 24 * hour;
    let minDeadline = new Date(expectedDeadline - 2 * hour);
    let maxDeadline = new Date(expectedDeadline + 2 * hour);

    // For each suitability, build a decorator that sets up a suitabilty
    // environment, and then call that decorator with a sub-test that asserts
    // the suitability is handled correctly.
    for (const suitability of temporaryFailureSuitabilities) {
      const decorator = PreferenceExperiments.withMockExperiments([
        NormandyTestUtils.factories.preferenceStudyFactory({
          slug: `test-for-suitability-${suitability}`.toLocaleLowerCase(),
          expired: false,
          temporaryErrorDeadline: invalidDeadline,
        }),
      ]);
      await decorator(async ([experiment]) => {
        let action = new PreferenceExperimentAction();
        let recipe = preferenceExperimentFactory({ slug: experiment.slug });
        await action.processRecipe(recipe, suitability);
        is(action.lastError, null, "No errors should be reported");
        let modifiedExperiment = await PreferenceExperiments.get(
          experiment.slug
        );
        ok(
          modifiedExperiment.temporaryErrorDeadline != invalidDeadline,
          `The temporary failure deadline should be reset for suitabilitiy ${suitability}`
        );
        let deadline = new Date(modifiedExperiment.temporaryErrorDeadline);
        ok(
          deadline >= minDeadline && deadline <= maxDeadline,
          `The temporary failure deadline should be reset to a valid deadline for ${suitability}`
        );
      })();
    }
  }
);
