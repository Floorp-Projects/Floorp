/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { _ExperimentFeature: ExperimentFeature } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);

const { PrefUtils } = ChromeUtils.import(
  "resource://normandy/lib/PrefUtils.jsm"
);

/**
 * Pick a single entry from an object and return a new object containing only
 * that entry.
 *
 * @param {object} obj The object to copy the value from.
 * @param {string} key The key whose value is to be copied.
 *
 * @returns {object} An object with the property `key` set to `obj[key]`.
 */
function pick(obj, key) {
  return { [key]: obj[key] };
}

const PREF_FEATURES = [
  new ExperimentFeature("test-set-pref", {
    description: "Test feature that sets a pref on the default branch.",
    owner: "test@test.test",
    hasExposure: false,
    isEarlyStartup: false,
    variables: {
      foo: {
        type: "string",
        description: "Test variable",
        setPref: "nimbus.test-only.foo",
      },
    },
  }),
  new ExperimentFeature("test-set-user-pref", {
    description: "Test feature that sets a pref on the user branch.",
    owner: "test@test.test",
    hasExposure: false,
    isEarlyStartup: true,
    variables: {
      bar: {
        type: "string",
        description: "Test variable",
        setPref: "nimbus.test-only.bar",
      },
    },
  }),
];

const DEFAULT_VALUE = "default-value";
const USER_VALUE = "user-value";
const EXPERIMENT_VALUE = "experiment-value";
const ROLLOUT_VALUE = "rollout-value";

const USER = "user";
const DEFAULT = "default";
const ROLLOUT = "rollout";
const EXPERIMENT = "experiment";

const PREFS = {
  [DEFAULT]: "nimbus.test-only.foo",
  [USER]: "nimbus.test-only.bar",
};

const FEATURE_IDS = {
  [DEFAULT]: "test-set-pref",
  [USER]: "test-set-user-pref",
};

const CONFIGS = {
  [DEFAULT]: {
    [ROLLOUT]: {
      featureId: FEATURE_IDS[DEFAULT],
      value: {
        foo: ROLLOUT_VALUE,
      },
    },
    [EXPERIMENT]: {
      featureId: FEATURE_IDS[DEFAULT],
      value: {
        foo: EXPERIMENT_VALUE,
      },
    },
  },
  [USER]: {
    [ROLLOUT]: {
      featureId: FEATURE_IDS[USER],
      value: {
        bar: ROLLOUT_VALUE,
      },
    },
    [EXPERIMENT]: {
      featureId: FEATURE_IDS[USER],
      value: {
        bar: EXPERIMENT_VALUE,
      },
    },
  },
};

/**
 * Set the given pref values on their respective branches (if they are not
 * null).
 */
function setPrefs(pref, { defaultBranchValue = null, userBranchValue = null }) {
  if (defaultBranchValue !== null) {
    Services.prefs
      .getDefaultBranch(null)
      .setStringPref(pref, defaultBranchValue);
  }

  if (userBranchValue !== null) {
    Services.prefs.setStringPref(pref, userBranchValue);
  }
}

function assertExpectedPrefValues(pref, branch, expected, visible, msg) {
  info(`Assert pref ${pref} on branch ${branch} matches ${expected} ${msg}`);
  const hasBranchValue = expected !== null;
  const hasVisibleValue = visible !== null;

  function hasValueMsg(hasValue) {
    return `Expected pref "${pref}" on the ${branch} branch to${
      hasValue ? " " : " not "
    }have a value ${msg}`;
  }

  let prefBranch;
  switch (branch) {
    case USER:
      Assert.equal(
        Services.prefs.prefHasUserValue(pref),
        hasBranchValue,
        hasValueMsg(hasBranchValue)
      );
      prefBranch = Services.prefs;
      break;

    case DEFAULT:
      Assert.equal(
        Services.prefs.prefHasDefaultValue(pref),
        hasBranchValue,
        hasValueMsg(hasBranchValue)
      );
      prefBranch = Services.prefs.getDefaultBranch(null);
      break;

    default:
      Assert.ok(false, "invalid pref branch");
  }

  if (hasBranchValue) {
    Assert.equal(
      prefBranch.getStringPref(pref),
      expected,
      `Expected pref "${pref} on the ${branch} branch to be ${JSON.stringify(
        expected
      )} ${msg}`
    );
  }

  if (hasVisibleValue) {
    Assert.equal(
      Services.prefs.getStringPref(pref),
      visible,
      `Expected pref "${pref}" to be ${JSON.stringify(visible)} ${msg}`
    );
  } else {
    Assert.ok(
      !Services.prefs.prefHasUserValue(pref) &&
        !Services.prefs.prefHasDefaultValue(pref),
      `Expected pref "${pref} to not be set ${msg}`
    );
  }
}

/**
 * Assert the store has no active experiments or rollouts.
 */
function assertEmptyStore(store) {
  Assert.deepEqual(
    store.getAllActive(),
    [],
    "There should be no experiments active."
  );

  Assert.deepEqual(
    store.getAllRollouts(),
    [],
    "There should be no rollouts active"
  );
}

/**
 * Remove the experiment store.
 */
async function cleanupStore(store) {
  // We need to call finalize first to ensure that any pending saves from
  // JSONFile.saveSoon overwrite files on disk.
  await store._store.finalize();
  await IOUtils.remove(store._store.path);
}

add_setup(function setup() {
  const cleanupFeatures = ExperimentTestUtils.addTestFeatures(...PREF_FEATURES);
  registerCleanupFunction(cleanupFeatures);
});

add_task(async function test_enroll_setPref_rolloutsAndExperiments() {
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);

  await manager.onStartup();

  assertEmptyStore(store);

  /**
   * Test that prefs are set correctly before and after enrollment and
   * unenrollment.
   *
   * @param {object} options
   * @param {string} options.pref
   *        The name of the pref.
   *
   * @param {string} options.branch
   *        The name of the pref branch ("user" or "default").
   *
   * @param {object} options.configs
   *        The rollout and experiment feature configurations.
   *
   * @param {string?} options.defaultBranchValue
   *        An optional value to set for the pref on the default branch
   *        before the first enrollment.
   *
   * @param {string?} options.userBranchValue
   *        An optional value to set for the pref on the user branch
   *        before the first enrollment.
   *
   * @param {string[]} options.enrollOrder
   *        The order to do the enrollments. Must only contain
   *        "experiment" and "rollout" as values.
   *
   * @param {string[]} options.unenrollOrder
   *        The order to undo the enrollments. Must only contain
   *        "experiment" and "rollout" as values.
   *
   * @param {(string|null)[]} options.expectedValues
   *        The expected values of the preft on the given branch at each point:
   *
   *        * before enrollment;
   *        * one entry each each after enrolling in `options.enrollOrder[i]`; and
   *        * one entry each each after unenrolling in `options.unenrollOrder[i]`.
   *
   *        A value of null indicates that the pref should not be set on that
   *        branch.
   *
   * @param {(string|null)[]?} options.visibleValues
   *        The expected values returned by
   *        `Services.prefs.getStringPref` (i.e., the user branch if set,
   *        falling back to the default branch if not), in the same
   *        order as |options.expectedValues|.
   *
   *        If undefined, then it will default `options.expectedValues`.
   */
  async function doBaseTest({
    pref,
    branch,
    configs,
    userBranchValue = undefined,
    defaultBranchValue = undefined,
    enrollOrder,
    unenrollOrder,
    expectedValues,
    visibleValues = undefined,
  }) {
    if (visibleValues === undefined) {
      visibleValues = expectedValues;
    }

    const cleanupFns = {};
    let i = 0;

    setPrefs(pref, { defaultBranchValue, userBranchValue });

    assertExpectedPrefValues(
      pref,
      branch,
      expectedValues[i],
      visibleValues[i],
      "before enrollment"
    );
    i++;

    for (const enrollmentKind of enrollOrder) {
      const isRollout = enrollmentKind === ROLLOUT;
      cleanupFns[
        enrollmentKind
      ] = await ExperimentFakes.enrollWithFeatureConfig(
        configs[enrollmentKind],
        { manager, isRollout }
      );

      assertExpectedPrefValues(
        pref,
        branch,
        expectedValues[i],
        visibleValues[i],
        `after ${enrollmentKind} enrollment`
      );
      i++;
    }

    for (const enrollmentKind of unenrollOrder) {
      await cleanupFns[enrollmentKind]();

      assertExpectedPrefValues(
        pref,
        branch,
        expectedValues[i],
        visibleValues[i],
        `after ${enrollmentKind} unenrollment`
      );
      i++;
    }

    assertEmptyStore(store);
    Services.prefs.deleteBranch(pref);
  }

  // Tests for a feature that would set a pref on the default branch, but the variable is omitted.
  {
    const branch = DEFAULT;
    const pref = PREFS[branch];

    const configs = {
      [ROLLOUT]: {
        featureId: FEATURE_IDS[DEFAULT],
        value: {},
      },
      [EXPERIMENT]: {
        featureId: FEATURE_IDS[DEFAULT],
        value: {},
      },
    };

    const doTest = args => doBaseTest({ pref, branch, ...args });

    // Enroll in a rollout then unenroll.
    await doTest({
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, null, null],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [DEFAULT_VALUE, DEFAULT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, null, null],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [DEFAULT_VALUE, DEFAULT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    // Enroll in an experiment then unenroll.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, null, null],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [DEFAULT_VALUE, DEFAULT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, null, null],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [DEFAULT_VALUE, DEFAULT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });
  }

  // Test for a feature that would set a pref on the user branch, but the variable is omitted.
  {
    const branch = USER;
    const pref = PREFS[branch];

    const configs = {
      [ROLLOUT]: {
        featureId: FEATURE_IDS[DEFAULT],
        value: {},
      },
      [EXPERIMENT]: {
        featureId: FEATURE_IDS[DEFAULT],
        value: {},
      },
    };

    const doTest = args => doBaseTest({ pref, branch, ...args });

    // Enroll in a rollout then unenroll.
    await doTest({
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, null, null],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, null, null],
      visibleValues: [DEFAULT_VALUE, DEFAULT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    // Enroll in an experiment then unenroll.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, null, null],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, null, null],
      visibleValues: [DEFAULT_VALUE, DEFAULT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });
  }

  // Tests for a feature that sets prefs on the default branch.
  {
    const branch = DEFAULT;
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];

    const doTest = args => doBaseTest({ pref, branch, ...args });

    // Enroll in rollout then unenroll.
    await doTest({
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, ROLLOUT_VALUE, ROLLOUT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [DEFAULT_VALUE, ROLLOUT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, ROLLOUT_VALUE, ROLLOUT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [DEFAULT_VALUE, ROLLOUT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    // Enroll in experiment then unenroll.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, EXPERIMENT_VALUE, EXPERIMENT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [DEFAULT_VALUE, EXPERIMENT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, EXPERIMENT_VALUE, EXPERIMENT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [DEFAULT_VALUE, EXPERIMENT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    // Enroll in rollout then experiment; unenroll in reverse order.
    await doTest({
      configs,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        ROLLOUT_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        DEFAULT_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        DEFAULT_VALUE,
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        ROLLOUT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        DEFAULT_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        DEFAULT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });

    // Enroll in rollout then experiment; unenroll in same order.
    await doTest({
      configs,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        DEFAULT_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        DEFAULT_VALUE,
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        DEFAULT_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        DEFAULT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });

    // Enroll in experiment then rollout; unenroll in reverse order.
    await doTest({
      configs,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        DEFAULT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        DEFAULT_VALUE,
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        DEFAULT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        DEFAULT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });

    // Enroll in experiment then rollout; unenroll in same order.
    await doTest({
      configs,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        ROLLOUT_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        DEFAULT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        DEFAULT_VALUE,
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        ROLLOUT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        DEFAULT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        DEFAULT_VALUE,
      ],
      visibleValues: [
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
        USER_VALUE,
      ],
    });
  }

  // Tests for a feature that sets prefs on the user branch.
  {
    const branch = USER;
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];

    const doTest = args => doBaseTest({ pref, branch, ...args });

    // Enroll in rollout then unenroll.
    await doTest({
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, ROLLOUT_VALUE, null],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [null, ROLLOUT_VALUE, null],
      visibleValues: [DEFAULT_VALUE, ROLLOUT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [USER_VALUE, ROLLOUT_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      enrollOrder: [ROLLOUT],
      unenrollOrder: [ROLLOUT],
      expectedValues: [USER_VALUE, ROLLOUT_VALUE, USER_VALUE],
    });

    // Enroll in experiment then unenroll.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, EXPERIMENT_VALUE, null],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [null, EXPERIMENT_VALUE, null],
      visibleValues: [DEFAULT_VALUE, EXPERIMENT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [USER_VALUE, EXPERIMENT_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      enrollOrder: [EXPERIMENT],
      unenrollOrder: [EXPERIMENT],
      expectedValues: [USER_VALUE, EXPERIMENT_VALUE, USER_VALUE],
    });

    // Enroll in rollout then experiment; unenroll in reverse order.
    await doTest({
      configs,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        null,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        null,
      ],
      visibleValues: [
        DEFAULT_VALUE, // User branch falls back to default branch.
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        DEFAULT_VALUE, // User branch falls back to default branch.
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        USER_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        USER_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        USER_VALUE,
      ],
    });

    // Enroll in rollout then experiment; unenroll in same order.
    await doTest({
      configs,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        null,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        null,
      ],
      visibleValues: [
        DEFAULT_VALUE, // User branch falls back to default branch.
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        DEFAULT_VALUE, // User branch falls back to default branch.
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        USER_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      enrollOrder: [ROLLOUT, EXPERIMENT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        USER_VALUE,
        ROLLOUT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        USER_VALUE,
      ],
    });

    // Enroll in experiment then rollout; unenroll in reverse order.
    await doTest({
      configs,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        null,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        null,
      ],
      visibleValues: [
        DEFAULT_VALUE, // User branch falls back to default branch.
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        DEFAULT_VALUE, // User branch falls back to default branch.
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        USER_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [
        USER_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        USER_VALUE,
      ],
    });

    // Enroll in experiment then rollout; unenroll in same order.
    await doTest({
      configs,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        null,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        null,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        null,
      ],
      visibleValues: [
        DEFAULT_VALUE, // User branch falls back to default branch.
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        DEFAULT_VALUE, // User branch falls back to default branch.
      ],
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        USER_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        USER_VALUE,
      ],
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      enrollOrder: [EXPERIMENT, ROLLOUT],
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [
        USER_VALUE,
        EXPERIMENT_VALUE,
        EXPERIMENT_VALUE,
        ROLLOUT_VALUE,
        USER_VALUE,
      ],
    });
  }

  await cleanupStore(store);
});

add_task(async function test_restorePrefs_experimentAndRollout() {
  /**
   * Test that prefs are set correctly after restoring from a saved store file
   * and unrnollment.
   *
   * This test sets up some enrollments and saves them to disk.
   *
   * A browser restart will be simulated by creating a new ExperimentStore and
   * ExperimentManager to restore the saved enrollments.
   *
   * @param {object} options
   * @param {string} options.pref
   *        The name of the pref.
   *
   * @param {string} options.branch
   *        The name of the pref branch ("user" or "default").
   *
   * @param {object} options.configs
   *        The rollout and experiment feature configurations.
   *
   * @param {string?} options.defaultBranchValue
   *        An optional value to set for the pref on the default branch
   *        before the first enrollment.
   *
   * @param {string?} options.userBranchValue
   *        An optional value to set for the pref on the user branch
   *        before the first enrollment.
   *
   * @param {string[]} options.unenrollOrder
   *        An optional value to set for the pref on the default branch
   *        before the first enrollment.
   *
   * @param {(string|null)[]} options.expectedValues
   *        The expected values of the preft on the given branch at each point:
   *
   *        * before enrollment;
   *        * one entry each each after enrolling in `options.enrollOrder[i]`; and
   *        * one entry each each after unenrolling in `options.unenrollOrder[i]`.
   *
   *        A value of null indicates that the pref should not be set on that
   *        branch.
   *
   * @param {(string|null)[]?} options.visibleValues
   *        The expected values returned by
   *        Services.prefs.getStringPref (i.e., the user branch if set,
   *        falling back to the default branch if not), in the same
   *        order as `options.expectedValues`.
   *
   *        If undefined, then it will default to `options.expectedValues`.
   */
  async function doBaseTest({
    featureId,
    pref,
    branch,
    configs,
    defaultBranchValue = null,
    userBranchValue = null,
    unenrollOrder,
    expectedValues,
    visibleValues = undefined,
  }) {
    if (![USER, DEFAULT].includes(branch)) {
      Assert.ok(false, `invalid branch ${branch}`);
    }

    if (visibleValues === undefined) {
      visibleValues = expectedValues;
    }

    // Set the initial conditions.
    setPrefs(pref, { defaultBranchValue, userBranchValue });

    // Enroll in some experiments and save the state to disk.
    {
      const store = ExperimentFakes.store();
      const manager = ExperimentFakes.manager(store);

      await manager.onStartup();

      assertEmptyStore(store);

      for (const [enrollmentKind, config] of Object.entries(configs)) {
        await ExperimentFakes.enrollWithFeatureConfig(config, {
          manager,
          isRollout: enrollmentKind === ROLLOUT,
        });
      }

      store._store.saveSoon();
      await store._store.finalize();

      // User branch prefs persist through restart, so we only want to delete
      // the prefs if we changed the default branch.
      if (branch === "default") {
        Services.prefs.deleteBranch(pref);
      }
    }

    // Restore the default branch value as it was before "restarting".
    setPrefs(pref, { defaultBranchValue });
    // If this is not a user branch pref, restore the user branch value. User
    // branch values persist through restart, so we don't want to overwrite a
    // value we just set.
    if (branch === "default") {
      setPrefs(pref, { userBranchValue });
    }

    const sandbox = sinon.createSandbox();

    const store = ExperimentFakes.store();
    const manager = ExperimentFakes.manager(store);

    const setPrefSpy = sandbox.spy(PrefUtils, "setPref");

    await manager.onStartup();

    if (branch === DEFAULT) {
      Assert.ok(setPrefSpy.calledOnce, "Should have called setPref once total");
      Assert.ok(
        setPrefSpy.calledOnceWith(pref, expectedValues[0], { branch }),
        `Should have only called setPref with correct args (called with: ${JSON.stringify(
          setPrefSpy.getCall(0).args
        )}) expected ${JSON.stringify([pref, expectedValues[0], { branch }])})`
      );
    } else if (branch === USER) {
      Assert.ok(
        setPrefSpy.notCalled,
        "Should have not called setPref for a user branch pref"
      );
    }

    assertExpectedPrefValues(
      pref,
      branch,
      expectedValues[0],
      visibleValues[0],
      "after manager startup"
    );

    const slugs = {
      [ROLLOUT]: store.getRolloutForFeature(featureId)?.slug,
      [EXPERIMENT]: store.getExperimentForFeature(featureId)?.slug,
    };

    let i = 1;
    for (const enrollmentKind of unenrollOrder) {
      manager.unenroll(slugs[enrollmentKind]);

      assertExpectedPrefValues(
        pref,
        branch,
        expectedValues[i],
        visibleValues[i],
        `after ${enrollmentKind} unenrollment`
      );

      i++;
    }

    for (const enrollmentKind of unenrollOrder) {
      // The unenrollment happened normally, not through a cleanup function.
      store._deleteForTests(slugs[enrollmentKind]);
    }

    assertEmptyStore(store);
    await cleanupStore(store);

    Services.prefs.deleteBranch(pref);
    sandbox.restore();
  }

  {
    const branch = DEFAULT;
    const featureId = FEATURE_IDS[branch];
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];

    const doTest = args => doBaseTest({ featureId, pref, branch, ...args });

    // Tests with no prefs set beforehand.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE],
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, ROLLOUT_VALUE],
    });

    await doTest({
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, ROLLOUT_VALUE],
    });

    await doTest({
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, EXPERIMENT_VALUE],
    });

    // Tests where the default branch is set beforehand.

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, DEFAULT_VALUE],
    });

    // Tests where the user branch is set beforehand.

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, ROLLOUT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, ROLLOUT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, EXPERIMENT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    // Tests with both branches set beforehand
    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, DEFAULT_VALUE],
      visibleValues: [USER_VALUE, USER_VALUE, USER_VALUE],
    });
  }

  {
    const branch = USER;
    const featureId = FEATURE_IDS[branch];
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];

    const doTest = args =>
      doBaseTest({ featureId, pref, branch, configs, ...args });

    // Tests with no prefs set beforehand.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, null],
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, null],
    });

    await doTest({
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, null],
    });

    await doTest({
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, null],
    });

    // Tests with the default branch set beforehand.
    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, null],
      visibleValues: [EXPERIMENT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, null],
      visibleValues: [ROLLOUT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, null],
      visibleValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, DEFAULT_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, null],
      visibleValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, DEFAULT_VALUE],
    });

    // Tests with the user branch set beforehand.
    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, USER_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, USER_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, USER_VALUE],
    });

    await doTest({
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, USER_VALUE],
    });

    // Tests with both branches set beforehand
    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, EXPERIMENT),
      unenrollOrder: [EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs: pick(configs, ROLLOUT),
      unenrollOrder: [ROLLOUT],
      expectedValues: [ROLLOUT_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [EXPERIMENT, ROLLOUT],
      expectedValues: [EXPERIMENT_VALUE, ROLLOUT_VALUE, USER_VALUE],
    });

    await doTest({
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      configs,
      unenrollOrder: [ROLLOUT, EXPERIMENT],
      expectedValues: [EXPERIMENT_VALUE, EXPERIMENT_VALUE, USER_VALUE],
    });
  }
});
