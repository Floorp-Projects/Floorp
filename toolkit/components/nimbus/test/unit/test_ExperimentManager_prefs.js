/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { _ExperimentFeature: ExperimentFeature, NimbusFeatures } =
  ChromeUtils.importESModule("resource://nimbus/ExperimentAPI.sys.mjs");

const { PrefUtils } = ChromeUtils.importESModule(
  "resource://normandy/lib/PrefUtils.sys.mjs"
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { TelemetryEvents } = ChromeUtils.importESModule(
  "resource://normandy/lib/TelemetryEvents.sys.mjs"
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
    variables: {
      foo: {
        type: "string",
        description: "Test variable",
        setPref: {
          branch: "default",
          pref: "nimbus.test-only.foo",
        },
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
        setPref: {
          branch: "user",
          pref: "nimbus.test-only.bar",
        },
      },
    },
  }),
];

const DEFAULT_VALUE = "default-value";
const USER_VALUE = "user-value";
const EXPERIMENT_VALUE = "experiment-value";
const ROLLOUT_VALUE = "rollout-value";
const OVERWRITE_VALUE = "overwrite-value";

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

  switch (branch) {
    case USER:
      Assert.equal(
        Services.prefs.prefHasUserValue(pref),
        hasBranchValue,
        hasValueMsg(hasBranchValue)
      );
      break;

    case DEFAULT:
      Assert.equal(
        Services.prefs.prefHasDefaultValue(pref),
        hasBranchValue,
        hasValueMsg(hasBranchValue)
      );
      break;

    default:
      Assert.ok(false, "invalid pref branch");
  }

  if (hasBranchValue) {
    Assert.equal(
      PrefUtils.getPref(pref, { branch }),
      expected,
      `Expected pref "${pref} on the ${branch} branch to be ${JSON.stringify(
        expected
      )} ${msg}`
    );
  }

  if (hasVisibleValue) {
    Assert.equal(
      PrefUtils.getPref(pref, { branch: USER }) ??
        PrefUtils.getPref(pref, { branch: DEFAULT }),
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
 * Assert the manager has no active pref observers.
 */
function assertNoObservers(manager) {
  Assert.equal(
    manager._prefs.size,
    0,
    "There should be no active pref observers"
  );
  Assert.equal(
    manager._prefsBySlug.size,
    0,
    "There should be no active pref observers"
  );
}

/**
 * Remove all pref observers on the given ExperimentManager.
 */
function removePrefObservers(manager) {
  for (const [name, entry] of manager._prefs.entries()) {
    Services.prefs.removeObserver(name, entry.observer);
  }

  manager._prefs.clear();
  manager._prefsBySlug.clear();
}

add_setup(function setup() {
  do_get_profile();
  Services.fog.initializeFOG();

  const cleanupFeatures = ExperimentTestUtils.addTestFeatures(...PREF_FEATURES);
  registerCleanupFunction(cleanupFeatures);
});

add_task(async function test_enroll_setPref_rolloutsAndExperiments() {
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);

  await manager.onStartup();

  await assertEmptyStore(store);

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
      cleanupFns[enrollmentKind] =
        await ExperimentFakes.enrollWithFeatureConfig(configs[enrollmentKind], {
          manager,
          isRollout,
        });

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

    await assertEmptyStore(store);
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

  await assertEmptyStore(store, { cleanup: true });
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

      await assertEmptyStore(store);

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

      removePrefObservers(manager);
      assertNoObservers(manager);
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

    assertNoObservers(manager);
    await assertEmptyStore(store, { cleanup: true });

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

add_task(async function test_prefChange() {
  TelemetryEvents.init();

  const LEGACY_FILTER = {
    category: "normandy",
    method: "unenroll",
    object: "nimbus_experiment",
  };

  /**
   * Test that pref tampering causes unenrollment.
   *
   * This test sets up some enrollments and then modifies the given `pref` on a
   * branch specified by `setBranch` and checks that unenrollments happen as
   * appropriate.
   *
   * @param {object} options
   *
   * @param {string} options.pref
   *        The name of the pref.
   *
   * @param {string?} options.defaultBranchValue
   *        An optional value to set for the pref on the default branch
   *        before the first enrollment.
   *
   * @param {string?} options.userBranchValue
   *        An optional value to set for the pref on the user branch
   *        before the first enrollment.
   *
   * @param {object} options.configs
   *        The rollout and experiment feature configurations.
   *
   * @param {string} options.setBranch
   *        The branch that the test will set (either "user" or "default").
   *
   * @param {string[]} options.expectedEnrollments
   *        The list of enrollment kinds (e.g., "rollout" or "experiment") that
   *        should be active after setting the pref on the requested branch.
   *
   * @param {string} options.expectedDefault
   *        The expected value of the default branch after setting the pref on
   *        the requested branch.
   *
   *        A value of null indicates that the pref should not be set on the
   *        default branch.
   *
   * @param {string} options.expectedUser
   *        The expected value of the user branch after setting the pref on the
   *        requested branch.
   *
   *        A value of null indicates that the pref should not be set on the
   *        user branch.
   */
  async function doBaseTest({
    pref,
    defaultBranchValue = null,
    userBranchValue = null,
    configs,
    setBranch,
    expectedEnrollments = [],
    expectedDefault = null,
    expectedUser = null,
  }) {
    Services.fog.testResetFOG();
    Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      /* clear = */ true
    );

    const store = ExperimentFakes.store();
    const manager = ExperimentFakes.manager(store);

    const cleanup = {};
    const slugs = {};

    await manager.onStartup();

    await assertEmptyStore(store);

    setPrefs(pref, { defaultBranchValue, userBranchValue });

    info(`Enrolling in ${Array.from(Object.keys(configs)).join(", ")} ...`);
    for (const [enrollmentKind, config] of Object.entries(configs)) {
      const isRollout = enrollmentKind === ROLLOUT;
      cleanup[enrollmentKind] = await ExperimentFakes.enrollWithFeatureConfig(
        config,
        {
          manager,
          isRollout,
        }
      );

      const enrollments = isRollout
        ? store.getAllActiveRollouts()
        : store.getAllActiveExperiments();

      Assert.equal(
        enrollments.length,
        1,
        `Expected one ${enrollmentKind} enrollment`
      );
      slugs[enrollmentKind] = enrollments[0].slug;
    }

    info(
      `Overwriting ${pref} with "${OVERWRITE_VALUE}" on ${setBranch} branch`
    );

    PrefUtils.setPref(pref, OVERWRITE_VALUE, { branch: setBranch });

    if (expectedDefault === null) {
      Assert.ok(
        !Services.prefs.prefHasDefaultValue(pref),
        `Expected the default branch not to be set for ${pref}`
      );
    } else {
      Assert.equal(
        Services.prefs.getDefaultBranch(null).getStringPref(pref),
        expectedDefault,
        `Expected the value of ${pref} on the default branch to match the expected value`
      );
    }

    if (expectedUser === null) {
      Assert.ok(
        !Services.prefs.prefHasUserValue(pref),
        `Expected the user branch not to be set for ${pref}`
      );
    } else {
      Assert.equal(
        Services.prefs.getStringPref(pref),
        expectedUser,
        `Expected the value of ${pref} on the user branch to match the expected value`
      );
    }

    for (const enrollmentKind of expectedEnrollments) {
      const enrollment = store.get(slugs[enrollmentKind]);

      Assert.ok(
        enrollment !== null,
        `An enrollment of kind ${enrollmentKind} should exist`
      );
      Assert.ok(enrollment.active, "It should still be active");
    }

    for (const enrollmentKind of Object.keys(configs)) {
      if (!expectedEnrollments.includes(enrollmentKind)) {
        const slug = slugs[enrollmentKind];
        const enrollment = store.get(slug);

        Assert.ok(
          enrollment !== null,
          `An enrollment of kind ${enrollmentKind} should exist`
        );
        Assert.ok(!enrollment.active, "It should not be active");
        Assert.equal(
          enrollment.unenrollReason,
          "changed-pref",
          "The unenrollment reason should be changed-pref"
        );

        store._deleteForTests(slug);
      }
    }

    const gleanEvents = Glean.nimbusEvents.unenrollment.testGetValue();
    const expectedLegacyEvents = Object.keys(configs)
      .filter(enrollmentKind => !expectedEnrollments.includes(enrollmentKind))
      .map(enrollmentKind => ({
        value: slugs[enrollmentKind],
        extra: {
          reason: "changed-pref",
          changedPref: pref,
        },
      }));

    TelemetryTestUtils.assertEvents(expectedLegacyEvents, LEGACY_FILTER);

    if (expectedLegacyEvents.length) {
      const processedGleanEvents = gleanEvents.map(event => ({
        reason: event.extra.reason,
        experiment: event.extra.experiment,
        changed_pref: event.extra.changed_pref,
      }));
      const expectedGleanEvents = expectedLegacyEvents.map(event => ({
        experiment: event.value,
        reason: event.extra.reason,
        changed_pref: event.extra.changedPref,
      }));

      Assert.deepEqual(
        processedGleanEvents,
        expectedGleanEvents,
        "Glean should have the expected unenrollment events"
      );
    } else {
      Assert.equal(
        gleanEvents,
        undefined,
        "Glean should have no unenrollment events"
      );
    }

    for (const enrollmentKind of expectedEnrollments) {
      await cleanup[enrollmentKind]();
    }

    assertNoObservers(manager);
    await assertEmptyStore(store, { cleanup: true });

    Services.prefs.deleteBranch(pref);
  }

  {
    const branch = DEFAULT;
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];

    const doTest = args => doBaseTest({ pref, branch, ...args });

    // Enrolled in rollout, set default branch.
    await doTest({
      configs: pick(configs, ROLLOUT),
      setBranch: DEFAULT,
      expectedDefault: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: DEFAULT,
      expectedDefault: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: USER_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: USER_VALUE,
    });

    // Enrolled in rollout, set user branch.
    await doTest({
      configs: pick(configs, ROLLOUT),
      setBranch: USER,
      expectedDefault: ROLLOUT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: ROLLOUT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    // Enrolled in experiment, set default branch.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      setBranch: DEFAULT,
      expectedDefault: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: DEFAULT,
      expectedDefault: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: USER_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: USER_VALUE,
    });

    // Enrolled in experiment, set user branch.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      setBranch: USER,
      expectedDefault: EXPERIMENT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: EXPERIMENT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    // Enroll in both, set default branch.
    await doTest({
      configs,
      setBranch: DEFAULT,
      expectedDefault: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: DEFAULT,
      expectedDefault: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT, ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: USER_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT, ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: USER_VALUE,
    });

    // Enroll in both, set user branch.
    await doTest({
      configs,
      setBranch: USER,
      expectedDefault: EXPERIMENT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: EXPERIMENT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });
  }

  {
    const branch = USER;
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];

    const doTest = args => doBaseTest({ pref, branch, ...args });

    // Enrolled in rollout, set default branch.
    await doTest({
      configs: pick(configs, ROLLOUT),
      setBranch: DEFAULT,
      expectedEnrollments: [ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: ROLLOUT_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: ROLLOUT_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: ROLLOUT_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: ROLLOUT_VALUE,
    });

    // Enrolled in rollout, set user branch.
    await doTest({
      configs: pick(configs, ROLLOUT),
      setBranch: USER,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    // Enrolled in experiment, set default branch.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    // Enrolled in experiment, set user branch.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      setBranch: USER,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    // Enrolled in both, set default branch.
    await doTest({
      configs,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT, ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT, ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT, ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: DEFAULT,
      expectedEnrollments: [EXPERIMENT, ROLLOUT],
      expectedDefault: OVERWRITE_VALUE,
      expectedUser: EXPERIMENT_VALUE,
    });

    // Enrolled in both, set user branch.
    await doTest({
      configs,
      setBranch: USER,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedUser: OVERWRITE_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      setBranch: USER,
      expectedDefault: DEFAULT_VALUE,
      expectedUser: OVERWRITE_VALUE,
    });
  }
});

add_task(async function test_deleteBranch() {
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);

  await manager.onStartup();

  await assertEmptyStore(store);

  const cleanup = [];
  cleanup.push(
    await ExperimentFakes.enrollWithFeatureConfig(CONFIGS[USER][EXPERIMENT], {
      manager,
    }),
    await ExperimentFakes.enrollWithFeatureConfig(CONFIGS[USER][ROLLOUT], {
      manager,
      isRollout: true,
    }),
    await ExperimentFakes.enrollWithFeatureConfig(
      CONFIGS[DEFAULT][EXPERIMENT],
      { manager }
    ),
    await ExperimentFakes.enrollWithFeatureConfig(CONFIGS[DEFAULT][ROLLOUT], {
      manager,
      isRollout: true,
    })
  );

  Services.prefs.deleteBranch(PREFS[USER]);
  Services.prefs.deleteBranch(PREFS[DEFAULT]);

  // deleteBranch does not trigger pref observers!
  Assert.equal(
    store.getAll().length,
    4,
    "nsIPrefBranch::deleteBranch does not trigger unenrollment"
  );

  for (const cleanupFn of cleanup) {
    await cleanupFn();
  }

  assertNoObservers(manager);
  await assertEmptyStore(store, { cleanup: true });
});

add_task(async function test_clearUserPref() {
  /**
   * Test that nsIPrefBranch::clearUserPref() correctly interacts with pref
   * tampering logic.
   *
   * This test sets up some enrollments and then clears the pref specified and
   * checks that unenrollments happen as * appropriate.
   *
   * @param {object} options
   *
   * @param {string} options.pref
   *        The name of the pref.
   *
   * @param {string?} options.defaultBranchValue
   *        An optional value to set for the pref on the default branch
   *        before the first enrollment.
   *
   * @param {string?} options.userBranchValue
   *        An optional value to set for the pref on the user branch
   *        before the first enrollment.
   *
   * @param {object} options.configs
   *        The rollout and experiment feature configurations.
   *
   * @param {boolean} options.expectedEnrolled
   *        Whether or not the enrollments defined in `configs` should still be
   *        active after clearing the user branch.
   *
   * @param {string} options.expectedDefault
   *        The expected value of the default branch after clearing the user branch.
   *
   *        A value of null indicates that the pref should not be set on the default
   *        branch.
   */
  async function doBaseTest({
    pref,
    defaultBranchValue = null,
    userBranchValue = null,
    configs,
    expectedEnrolled,
    expectedDefault = null,
  }) {
    const store = ExperimentFakes.store();
    const manager = ExperimentFakes.manager(store);

    await manager.onStartup();

    await assertEmptyStore(store);

    const cleanup = [];
    const slugs = {};

    setPrefs(pref, { defaultBranchValue, userBranchValue });

    for (const [enrollmentKind, config] of Object.entries(configs)) {
      const isRollout = enrollmentKind === ROLLOUT;
      cleanup.push(
        await ExperimentFakes.enrollWithFeatureConfig(config, {
          manager,
          isRollout,
        })
      );

      const enrollments = isRollout
        ? store.getAllActiveRollouts()
        : store.getAllActiveExperiments();

      Assert.equal(
        enrollments.length,
        1,
        `Expected one ${enrollmentKind} enrollment`
      );
      slugs[enrollmentKind] = enrollments[0].slug;
    }

    Services.prefs.clearUserPref(pref);

    for (const enrollmentKind of Object.keys(configs)) {
      const slug = slugs[enrollmentKind];
      const enrollment = store.get(slug);
      Assert.ok(
        enrollment !== null,
        `An enrollment of kind ${enrollmentKind} should exist`
      );

      if (expectedEnrolled) {
        Assert.ok(enrollment.active, "It should be active");
      } else {
        Assert.ok(!enrollment.active, "It should not be active");
      }
    }

    if (expectedDefault === null) {
      Assert.ok(
        !Services.prefs.prefHasDefaultValue(pref),
        `Expected the default branch not to be set for ${pref}`
      );
    } else {
      Assert.equal(
        Services.prefs.getDefaultBranch(null).getStringPref(pref),
        expectedDefault,
        `Expected the value of ${pref} on the default branch to match the expected value`
      );
    }

    Assert.ok(
      !Services.prefs.prefHasUserValue(pref),
      `Expected the user branch not to be set for ${pref}`
    );

    if (expectedEnrolled) {
      for (const cleanupFn of Object.values(cleanup)) {
        await cleanupFn();
      }
    } else {
      for (const slug of Object.values(slugs)) {
        store._deleteForTests(slug);
      }
    }

    assertNoObservers(manager);
    await assertEmptyStore(store, { cleanup: true });

    Services.prefs.deleteBranch(pref);
  }

  {
    const branch = DEFAULT;
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];
    const doTest = args => doBaseTest({ pref, branch, ...args });

    // Enroll in rollout.
    await doTest({
      configs: pick(configs, ROLLOUT),
      expectedEnrolled: true,
      expectedDefault: ROLLOUT_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      expectedEnrolled: true,
      expectedDefault: ROLLOUT_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      userBranchValue: USER_VALUE,
      expectedEnrolled: false,
      expectedDefault: ROLLOUT_VALUE,
    });

    await doTest({
      configs: pick(configs, ROLLOUT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      expectedEnrolled: false,
      expectedDefault: DEFAULT_VALUE,
    });

    // Enroll in experiment.
    await doTest({
      configs: pick(configs, EXPERIMENT),
      expectedEnrolled: true,
      expectedDefault: EXPERIMENT_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      expectedEnrolled: true,
      expectedDefault: EXPERIMENT_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      userBranchValue: USER_VALUE,
      expectedEnrolled: false,
      expectedDefault: EXPERIMENT_VALUE,
    });

    await doTest({
      configs: pick(configs, EXPERIMENT),
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      expectedEnrolled: false,
      expectedDefault: DEFAULT_VALUE,
    });

    // Enroll in both.
    await doTest({
      configs,
      expectedEnrolled: true,
      expectedDefault: EXPERIMENT_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      expectedEnrolled: true,
      expectedDefault: EXPERIMENT_VALUE,
    });

    await doTest({
      configs,
      userBranchValue: USER_VALUE,
      expectedEnrolled: false,
      expectedDefault: EXPERIMENT_VALUE,
    });

    await doTest({
      configs,
      defaultBranchValue: DEFAULT_VALUE,
      userBranchValue: USER_VALUE,
      expectedEnrolled: false,
      expectedDefault: DEFAULT_VALUE,
    });
  }

  {
    const branch = USER;
    const pref = PREFS[branch];
    const configs = CONFIGS[branch];
    const doTest = args =>
      doBaseTest({ pref, branch, expectedEnrolled: false, ...args });

    // Because this pref is set on the user branch, clearing the user branch has
    // the same effect for every suite of configs.
    for (const selectedConfig of [
      pick(configs, ROLLOUT),
      pick(configs, EXPERIMENT),
      configs,
    ]) {
      await doTest({
        configs: selectedConfig,
      });

      await doTest({
        configs: selectedConfig,
        defaultBranchValue: DEFAULT_VALUE,
        expectedDefault: DEFAULT_VALUE,
      });

      await doTest({
        configs: selectedConfig,
        userBranchValue: USER_VALUE,
      });

      await doTest({
        configs: selectedConfig,
        defaultBranchValue: DEFAULT_VALUE,
        userBranchValue: USER_VALUE,
        expectedDefault: DEFAULT_VALUE,
      });
    }
  }
});

// Test that unenrollment doesn't happen if a pref changes but it wasn't set.
add_task(async function test_prefChanged_noPrefSet() {
  const featureId = "test-set-pref-2";
  const pref = "nimbus.test-only.baz";

  function featureFactory(prefBranch) {
    if (![USER, DEFAULT].includes(prefBranch)) {
      Assert.ok(false, `invalid branch ${prefBranch}`);
    }

    return new ExperimentFeature(featureId, {
      description: "Test feature that sets a pref",
      owner: "test@test.test",
      hasExposure: false,
      variables: {
        baz: {
          type: "string",
          description: "Test variable",
          setPref: {
            branch: prefBranch,
            pref,
          },
        },
        qux: {
          type: "string",
          description: "Test variable",
        },
      },
    });
  }

  const config = {
    featureId,
    value: {
      qux: "qux",
    },
  };

  for (const prefBranch of [USER, DEFAULT]) {
    const feature = featureFactory(prefBranch);
    const cleanupFeature = ExperimentTestUtils.addTestFeatures(feature);

    const store = ExperimentFakes.store();
    const manager = ExperimentFakes.manager(store);
    await manager.onStartup();

    for (const branch of [USER, DEFAULT]) {
      for (const defaultBranchValue of [null, DEFAULT_VALUE]) {
        for (const userBranchValue of [null, USER_VALUE]) {
          for (const isRollout of [true, false]) {
            setPrefs(pref, { defaultBranchValue, userBranchValue });

            const doEnrollmentCleanup =
              await ExperimentFakes.enrollWithFeatureConfig(config, {
                manager,
                isRollout,
              });

            PrefUtils.setPref(pref, OVERWRITE_VALUE, { branch });

            const enrollments = await store.getAll();
            Assert.equal(
              enrollments.length,
              1,
              "There should be one enrollment"
            );
            Assert.ok(enrollments[0].active, "The enrollment should be active");

            Assert.equal(
              PrefUtils.getPref(pref, { branch }),
              OVERWRITE_VALUE,
              `The value of ${pref} on the ${branch} branch should be the expected value`
            );

            if (branch === USER) {
              if (defaultBranchValue) {
                Assert.equal(
                  PrefUtils.getPref(pref, { branch: DEFAULT }),
                  defaultBranchValue,
                  "The default branch should have the expected value"
                );
              } else {
                Assert.ok(
                  !Services.prefs.prefHasDefaultValue(pref),
                  "The default branch should not have a value"
                );
              }
            } else if (userBranchValue) {
              Assert.equal(
                PrefUtils.getPref(pref, { branch: USER }),
                userBranchValue,
                "The user branch should have the expected value"
              );
            } else {
              Assert.ok(
                !Services.prefs.prefHasUserValue(pref),
                "The user branch should not have a value"
              );
            }

            assertNoObservers(manager);

            await doEnrollmentCleanup();
            await assertEmptyStore(store);

            Services.prefs.deleteBranch(pref);
          }
        }
      }
    }

    cleanupFeature();
    await assertEmptyStore(store, { cleanup: true });
  }
});

add_task(async function test_restorePrefs_manifestChanged() {
  TelemetryEvents.init();

  const LEGACY_FILTER = {
    category: "normandy",
    method: "unenroll",
    object: "nimbus_experiment",
  };

  const BOGUS_PREF = "nimbus.test-only.bogus";

  const REMOVE_FEATURE = "remove-feature";
  const REMOVE_PREF_VARIABLE = "remove-pref-variable";
  const REMOVE_OTHER_VARIABLE = "remove-other-variable";
  const REMOVE_SETPREF = "remove-setpref";
  const CHANGE_SETPREF = "change-setpref";

  const OPERATIONS = [
    REMOVE_FEATURE,
    REMOVE_PREF_VARIABLE,
    REMOVE_OTHER_VARIABLE,
    REMOVE_SETPREF,
    CHANGE_SETPREF,
  ];

  const REASONS = {
    [REMOVE_FEATURE]: "invalid-feature",
    [REMOVE_PREF_VARIABLE]: "pref-variable-missing",
    [REMOVE_SETPREF]: "pref-variable-no-longer",
    [CHANGE_SETPREF]: "pref-variable-changed",
  };

  const featureId = "test-set-pref-temp";
  const pref = "nimbus.test-only.baz";

  // Return a new object so we can modified the returned value.
  function featureFactory(prefBranch) {
    if (![USER, DEFAULT].includes(prefBranch)) {
      Assert.ok(false, `invalid branch ${prefBranch}`);
    }

    return new ExperimentFeature(featureId, {
      description: "Test feature that sets a pref on the default branch.",
      owner: "test@test.test",
      hasExposure: false,
      variables: {
        baz: {
          type: "string",
          description: "Test variable",
          setPref: {
            branch: prefBranch,
            pref,
          },
        },
        qux: {
          type: "string",
          description: "Test variable",
        },
      },
    });
  }

  /*
   * Test that enrollments end when the manifest is sufficiently changed and
   * that the appropriate telemetry is submitted.
   *
   * This test sets up some enrollments and saves them to disk. Then the
   * manifest will be modified according to `operation`.
   *
   * A browser restart will be simulated by creating a new ExperimentStore and
   * ExperimentManager to restore the saved enrollments.
   *
   * @param {object} options
   *
   * @param {string} options.branch
   *        The name of the pref branch ("user" or "default").
   *
   * @param {string?} options.defaultBranchValue
   *        An optional value to set for the pref on the default branch
   *        before the first enrollment.
   *
   * @param {string?} options.userBranchValue
   *        An optional value to set for the pref on the user branch
   *        before the first enrollment.
   *
   * @param {object} options.configs
   *        The rollout and experiment feature configurations.
   *
   * @param {string} options.operation
   *        The operation that will be performed on the manifest.
   *
   *        See `OPERATIONS` above.
   *
   * @param {string[]} options.expectedEnrollments
   *        The list of enrollment kinds (e.g., "rollout" or "experiment") that
   *        should be active after setting the pref on the requested branch.
   *
   * @param {string} options.expectedDefault
   *        The expected value of the default branch after restoring enrollments.
   *
   *        A value of null indicates that the pref should not be set on the
   *        default branch.
   *
   * @param {string} options.expectedUser
   *        The expected value of the user branch after restoring enrollments.
   *
   *        A value of null indicates that the pref should not be set on the
   *        user branch.
   */
  async function doBaseTest({
    branch,
    defaultBranchValue = null,
    userBranchValue = null,
    configs,
    operation,
    expectedEnrollments = [],
    expectedDefault = null,
    expectedUser = null,
  }) {
    Services.fog.testResetFOG();
    Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      /* clear = */ true
    );

    const feature = featureFactory(branch);
    const cleanupFeatures = ExperimentTestUtils.addTestFeatures(feature);

    setPrefs(pref, { defaultBranchValue, userBranchValue });

    const slugs = {};
    let userPref = null;

    // Enroll in some experiments and save the state to disk.
    {
      const store = ExperimentFakes.store();
      const manager = ExperimentFakes.manager(store);

      await manager.onStartup();

      await assertEmptyStore(store);

      for (const [enrollmentKind, config] of Object.entries(configs)) {
        const isRollout = enrollmentKind === ROLLOUT;
        await ExperimentFakes.enrollWithFeatureConfig(config, {
          manager,
          isRollout,
        });

        const enrollments = isRollout
          ? store.getAllActiveRollouts()
          : store.getAllActiveExperiments();

        Assert.equal(
          enrollments.length,
          1,
          `Expected one ${enrollmentKind} enrollment`
        );
        slugs[enrollmentKind] = enrollments[0].slug;
      }

      store._store.saveSoon();
      await store._store.finalize();

      // User branch prefs persist through restart, so we only want to delete
      // the prefs if we changed the default branch.
      if (branch === "user") {
        userPref = PrefUtils.getPref(pref, { branch });
      }

      Services.prefs.deleteBranch(pref);

      removePrefObservers(manager);
      assertNoObservers(manager);
    }

    // Restore the default branch value as it was before "restarting".
    setPrefs(pref, {
      defaultBranchValue,
      userBranchValue: userPref ?? userBranchValue,
    });

    // Mangle the manifest.
    switch (operation) {
      case REMOVE_FEATURE:
        cleanupFeatures();
        break;

      case REMOVE_PREF_VARIABLE:
        delete NimbusFeatures[featureId].manifest.variables.baz;
        break;

      case REMOVE_OTHER_VARIABLE:
        delete NimbusFeatures[featureId].manifest.variables.qux;
        break;

      case REMOVE_SETPREF:
        delete NimbusFeatures[featureId].manifest.variables.baz.setPref;
        break;

      case CHANGE_SETPREF:
        NimbusFeatures[featureId].manifest.variables.baz.setPref = BOGUS_PREF;
        break;

      default:
        Assert.ok(false, "invalid operation");
    }

    const store = ExperimentFakes.store();
    const manager = ExperimentFakes.manager(store);

    await manager.onStartup();

    for (const enrollmentKind of expectedEnrollments) {
      const enrollment = store.get(slugs[enrollmentKind]);

      Assert.ok(
        enrollment !== null,
        `An experiment of kind ${enrollmentKind} should exist`
      );
      Assert.ok(enrollment.active, "It should still be active");
    }

    if (expectedDefault === null) {
      Assert.ok(
        !Services.prefs.prefHasDefaultValue(pref),
        `Expected the default branch not to be set for ${pref} value: ${PrefUtils.getPref(
          pref,
          { branch: "default" }
        )}`
      );
    } else {
      Assert.equal(
        Services.prefs.getDefaultBranch(null).getStringPref(pref),
        expectedDefault,
        `Expected the value of ${pref} on the default branch to match the expected value`
      );
    }

    if (expectedUser === null) {
      Assert.ok(
        !Services.prefs.prefHasUserValue(pref),
        `Expected the user branch not to be set for ${pref} value: ${PrefUtils.getPref(
          pref,
          { branch: "user" }
        )}`
      );
    } else {
      Assert.equal(
        Services.prefs.getStringPref(pref),
        expectedUser,
        `Expected the value of ${pref} on the user branch to match the expected value`
      );
    }

    if (operation === CHANGE_SETPREF) {
      Assert.ok(
        !Services.prefs.prefHasDefaultValue(BOGUS_PREF),
        "The new pref should not have a value on the default branch"
      );
      Assert.ok(
        !Services.prefs.prefHasUserValue(BOGUS_PREF),
        "The new pref should not have a value on the user branch"
      );
    }

    for (const enrollmentKind of Object.keys(configs)) {
      if (!expectedEnrollments.includes(enrollmentKind)) {
        const slug = slugs[enrollmentKind];
        const enrollment = store.get(slug);

        Assert.ok(
          enrollment !== null,
          `An enrollment of kind ${enrollmentKind} should exist`
        );
        Assert.ok(!enrollment.active, "It should not be active");

        store._deleteForTests(slug);
      }
    }

    const gleanEvents = Glean.nimbusEvents.unenrollment.testGetValue();
    if (expectedEnrollments.length === 0) {
      const expectedEvents = [EXPERIMENT, ROLLOUT]
        .filter(enrollmentKind => Object.hasOwn(slugs, enrollmentKind))
        .map(enrollmentKind => ({
          reason: REASONS[operation],
          experiment: slugs[enrollmentKind],
        }));

      // Extract only the values we care about.
      const processedEvents = gleanEvents.map(event => ({
        reason: event.extra.reason,
        experiment: event.extra.experiment,
      }));

      Assert.deepEqual(
        processedEvents,
        expectedEvents,
        "Glean should have the expected unenrollment events"
      );

      const expectedLegacyEvents = expectedEvents.map(extra => ({
        value: extra.experiment,
        extra: pick(extra, "reason"),
      }));

      TelemetryTestUtils.assertEvents(expectedLegacyEvents, LEGACY_FILTER);
    } else {
      Assert.equal(
        gleanEvents,
        undefined,
        "Glean should have no unenrollment events"
      );

      TelemetryTestUtils.assertEvents([], LEGACY_FILTER);
    }

    for (const enrollmentKind of expectedEnrollments) {
      const slug = slugs[enrollmentKind];
      manager.unenroll(slug);
      store._deleteForTests(slug);
    }

    await assertEmptyStore(store, { cleanup: true });

    assertNoObservers(manager);
    Services.prefs.deleteBranch(pref);

    if (operation !== REMOVE_FEATURE) {
      // If we try to remove the feature twice, we will throw an exception.
      cleanupFeatures();
    }
  }

  // Test only qux set. These tests should not cause any unenrollments.
  {
    const quxConfigs = {
      [EXPERIMENT]: {
        featureId,
        value: {
          qux: EXPERIMENT_VALUE,
        },
      },
      [ROLLOUT]: {
        featureId,
        value: {
          qux: ROLLOUT_VALUE,
        },
      },
    };

    const doTest = ({
      branch,
      defaultBranchValue = null,
      userBranchValue = null,
      configs,
      operation,
    }) =>
      doBaseTest({
        branch,
        configs,
        defaultBranchValue,
        userBranchValue,
        operation,
        expectedEnrollments: Object.keys(configs),
        expectedDefault: defaultBranchValue,
        expectedUser: userBranchValue,
      });

    for (const branch of [USER, DEFAULT]) {
      for (const defaultBranchValue of [null, DEFAULT_VALUE]) {
        for (const userBranchValue of [null, USER_VALUE]) {
          for (const specifiedConfigs of [
            pick(quxConfigs, ROLLOUT),
            pick(quxConfigs, EXPERIMENT),
            quxConfigs,
          ]) {
            for (const operation of OPERATIONS) {
              await doTest({
                branch,
                defaultBranchValue,
                userBranchValue,
                configs: specifiedConfigs,
                operation,
              });
            }
          }
        }
      }
    }
  }

  // Test only baz set. All operations except REMOVE_OTHER_VARIABLE will trigger
  // unenrollment.
  {
    const bazConfigs = {
      [EXPERIMENT]: {
        featureId,
        value: {
          baz: EXPERIMENT_VALUE,
        },
      },
      [ROLLOUT]: {
        featureId,
        value: {
          baz: ROLLOUT_VALUE,
        },
      },
    };

    const doTest = ({
      branch,
      defaultBranchValue = null,
      userBranchValue = null,
      configs,
      operation,
    }) => {
      const expectedEnrollments =
        operation === REMOVE_OTHER_VARIABLE ? Object.keys(configs) : [];

      function expectedPref(forBranch, originalValue) {
        if (forBranch === branch) {
          if (expectedEnrollments.includes(EXPERIMENT)) {
            return EXPERIMENT_VALUE;
          } else if (expectedEnrollments.includes(ROLLOUT)) {
            return ROLLOUT_VALUE;
          }
        }
        return originalValue;
      }

      const expectedDefault = expectedPref(DEFAULT, defaultBranchValue);
      const expectedUser = expectedPref(USER, userBranchValue);

      return doBaseTest({
        branch,
        configs,
        defaultBranchValue,
        userBranchValue,
        operation,
        expectedEnrollments,
        expectedDefault,
        expectedUser,
      });
    };

    for (const branch of [USER, DEFAULT]) {
      for (const defaultBranchValue of [null, DEFAULT_VALUE]) {
        for (const userBranchValue of [null, USER_VALUE]) {
          for (const specifiedConfigs of [
            pick(bazConfigs, ROLLOUT),
            pick(bazConfigs, EXPERIMENT),
            bazConfigs,
          ]) {
            for (const operation of OPERATIONS) {
              await doTest({
                branch,
                defaultBranchValue,
                userBranchValue,
                configs: specifiedConfigs,
                operation,
              });
            }
          }
        }
      }
    }
  }

  Services.fog.testResetFOG();
  Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    /* clear = */ true
  );
});

add_task(async function test_nested_prefs_enroll_both() {
  // See bugs 1850127 and 1850120.
  const feature = new ExperimentFeature("test-set-pref-nested", {
    description: "Nested prefs",
    owner: "test@test.test",
    hasExposure: false,
    variables: {
      enabled: {
        type: "boolean",
        description: "enable this feature",
        setPref: {
          branch: "default",
          pref: "nimbus.test-only.nested",
        },
      },
      setting: {
        type: "string",
        description: "a nested setting",
        setPref: {
          branch: "default",
          pref: "nimbus.test-only.nested.setting",
        },
      },
    },
  });

  const cleanupFeature = ExperimentTestUtils.addTestFeatures(feature);

  async function doTest(enrollmentOrder) {
    PrefUtils.setPref("nimbus.test-only.nested", false, { branch: DEFAULT });
    PrefUtils.setPref("nimbus.test-only.nested.setting", "default", {
      branch: DEFAULT,
    });

    const rollout = ExperimentFakes.recipe("nested-rollout", {
      isRollout: true,
      branches: [
        {
          ...ExperimentFakes.recipe.branches[0],
          features: [
            {
              featureId: feature.featureId,
              value: {
                enabled: true,
              },
            },
          ],
        },
      ],
    });

    const experiment = ExperimentFakes.recipe("nested-experiment", {
      branches: [
        {
          ...ExperimentFakes.recipe.branches[0],
          features: [
            {
              featureId: feature.featureId,
              value: {
                setting: "custom",
              },
            },
          ],
        },
      ],
    });

    const store = ExperimentFakes.store();
    const manager = ExperimentFakes.manager(store);

    await manager.onStartup();
    await assertEmptyStore(store);

    const recipes = {
      [ROLLOUT]: rollout,
      [EXPERIMENT]: experiment,
    };

    for (const kind of enrollmentOrder) {
      await manager.enroll(recipes[kind], "test");
    }

    {
      const enrollments = store
        .getAll()
        .filter(e => e.active)
        .map(e => e.slug);
      Assert.deepEqual(
        enrollments.sort(),
        [experiment.slug, rollout.slug].sort(),
        "Experiment and rollout should be enrolled"
      );
    }

    assertExpectedPrefValues(
      "nimbus.test-only.nested",
      DEFAULT,
      true,
      true,
      "after enrollment"
    );
    assertExpectedPrefValues(
      "nimbus.test-only.nested.setting",
      DEFAULT,
      "custom",
      "custom",
      "after enrollment"
    );

    manager.unenroll(experiment.slug);

    {
      const enrollments = store
        .getAll()
        .filter(e => e.active)
        .map(e => e.slug);
      Assert.deepEqual(
        enrollments.sort(),
        [rollout.slug].sort(),
        "Rollout should still be enrolled"
      );
    }

    assertExpectedPrefValues(
      "nimbus.test-only.nested",
      DEFAULT,
      true,
      true,
      "After experiment unenrollment"
    );

    assertExpectedPrefValues(
      "nimbus.test-only.nested.setting",
      DEFAULT,
      "default",
      "default",
      "After experiment unenrollment"
    );

    manager.unenroll(rollout.slug);

    await assertEmptyStore(store, { cleanup: true });
  }

  info(
    "Test we can enroll in both a rollout and experiment for a feature with nested pref setting"
  );
  await doTest([ROLLOUT, EXPERIMENT]);
  info(
    "Test we can unenroll from just an experiment for a feature with nested pref setting"
  );
  await doTest([EXPERIMENT, ROLLOUT]);

  cleanupFeature();
  PrefUtils.setPref("nimbus.test-only.nested", null, { branch: DEFAULT });
  PrefUtils.setPref("nimbus.test-only.nested.setting", null, {
    branch: DEFAULT,
  });
});
