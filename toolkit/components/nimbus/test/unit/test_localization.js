/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  ExperimentAPI,
  _ExperimentFeature: ExperimentFeature,
} = ChromeUtils.importESModule("resource://nimbus/ExperimentAPI.sys.mjs");
const { ExperimentFakes, ExperimentTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { TelemetryEvents } = ChromeUtils.import(
  "resource://normandy/lib/TelemetryEvents.jsm"
);

const LEGACY_TELEMETRY_FILTER = {
  category: "normandy",
  method: "unenroll",
  object: "nimbus_experiment",
};

const LOCALIZATIONS = {
  "en-US": {
    foo: "localized foo text",
    qux: "localized qux text",
    grault: "localized grault text",
    waldo: "localized waldo text",
  },
};

const DEEPLY_NESTED_VALUE = {
  foo: {
    $l10n: {
      id: "foo",
      comment: "foo comment",
      text: "original foo text",
    },
  },
  bar: {
    qux: {
      $l10n: {
        id: "qux",
        comment: "qux comment",
        text: "original qux text",
      },
    },
    quux: {
      grault: {
        $l10n: {
          id: "grault",
          comment: "grault comment",
          text: "orginal grault text",
        },
      },
      garply: "original garply text",
    },
    corge: "original corge text",
  },
  baz: "original baz text",
  waldo: [
    {
      $l10n: {
        id: "waldo",
        comment: "waldo comment",
        text: "original waldo text",
      },
    },
  ],
};

const LOCALIZED_DEEPLY_NESTED_VALUE = {
  foo: "localized foo text",
  bar: {
    qux: "localized qux text",
    quux: {
      grault: "localized grault text",
      garply: "original garply text",
    },
    corge: "original corge text",
  },
  baz: "original baz text",
  waldo: ["localized waldo text"],
};

const FEATURE_ID = "testfeature1";
const TEST_PREF_BRANCH = "testfeature1.";
const FEATURE = new ExperimentFeature(FEATURE_ID, {
  isEarlyStartup: false,
  variables: {
    foo: {
      type: "string",
      fallbackPref: `${TEST_PREF_BRANCH}foo`,
    },
    bar: {
      type: "json",
      fallbackPref: `${TEST_PREF_BRANCH}bar`,
    },
    baz: {
      type: "string",
      fallbackPref: `${TEST_PREF_BRANCH}baz`,
    },
    waldo: {
      type: "json",
      fallbackPref: `${TEST_PREF_BRANCH}waldo`,
    },
  },
});

/**
 * Remove the experiment store.
 */
async function cleanupStore(store) {
  // We need to call finalize first to ensure that any pending saves from
  // JSONFile.saveSoon overwrite files on disk.
  await store._store.finalize();
  await IOUtils.remove(store._store.path);
}

function resetTelemetry() {
  Services.fog.testResetFOG();
  Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    /* clear = */ true
  );
}

add_setup(function setup() {
  do_get_profile();

  Services.fog.initializeFOG();
  TelemetryEvents.init();

  registerCleanupFunction(ExperimentTestUtils.addTestFeatures(FEATURE));
  registerCleanupFunction(resetTelemetry);
});

add_task(function test_substituteLocalizations() {
  Assert.equal(
    ExperimentFeature.substituteLocalizations("string", LOCALIZATIONS["en-US"]),
    "string",
    "String values should not be subsituted"
  );

  Assert.equal(
    ExperimentFeature.substituteLocalizations(
      {
        $l10n: {
          id: "foo",
          comment: "foo comment",
          text: "original foo text",
        },
      },
      LOCALIZATIONS["en-US"]
    ),
    "localized foo text",
    "$l10n objects should be substituted"
  );

  Assert.deepEqual(
    ExperimentFeature.substituteLocalizations(
      DEEPLY_NESTED_VALUE,
      LOCALIZATIONS["en-US"]
    ),
    LOCALIZED_DEEPLY_NESTED_VALUE,
    "Supports nested substitutions"
  );

  Assert.throws(
    () =>
      ExperimentFeature.substituteLocalizations(
        {
          foo: {
            $l10n: {
              id: "BOGUS",
              comment: "A variable with a missing id",
              text: "Original text",
            },
          },
        },
        LOCALIZATIONS["en-US"]
      ),
    ex => ex.reason === "l10n-missing-entry"
  );
});

add_task(async function test_getLocalizedValue() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();

  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.onStartup();
  await manager.store.ready();

  const experiment = ExperimentFakes.recipe("experiment", {
    branches: [
      {
        slug: "control",
        features: [
          {
            featureId: FEATURE_ID,
            value: DEEPLY_NESTED_VALUE,
          },
        ],
      },
    ],
    localizations: LOCALIZATIONS,
  });

  const {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(experiment);
  await enrollmentPromise;

  const enrollment = manager.store.getExperimentForFeature(FEATURE_ID);

  Assert.deepEqual(
    FEATURE._getLocalizedValue(enrollment),
    LOCALIZED_DEEPLY_NESTED_VALUE,
    "_getLocalizedValue() for all values"
  );

  Assert.deepEqual(
    FEATURE._getLocalizedValue(enrollment, "foo"),
    LOCALIZED_DEEPLY_NESTED_VALUE.foo,
    "_getLocalizedValue() with a top-level localized variable"
  );

  Assert.deepEqual(
    FEATURE._getLocalizedValue(enrollment, "bar"),
    LOCALIZED_DEEPLY_NESTED_VALUE.bar,
    "_getLocalizedValue() with a nested localization"
  );

  await doExperimentCleanup();
  await cleanupStore(manager.store);
  sandbox.reset();
});

add_task(async function test_getLocalizedValue_unenroll_missingEntry() {
  resetTelemetry();

  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();

  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.onStartup();
  await manager.store.ready();

  const experiment = ExperimentFakes.recipe("experiment", {
    branches: [
      {
        slug: "control",
        features: [
          {
            featureId: FEATURE_ID,
            value: {
              bar: {
                $l10n: {
                  id: "BOGUS",
                  comment: "Bogus localization",
                  text: "Original text",
                },
              },
            },
          },
        ],
      },
    ],
    localizations: LOCALIZATIONS,
  });

  await ExperimentFakes.enrollmentHelper(experiment).enrollmentPromise;

  const enrollment = manager.store.getExperimentForFeature(FEATURE_ID);

  Assert.deepEqual(
    FEATURE._getLocalizedValue(enrollment),
    undefined,
    "_getLocalizedValue() with a bogus localization"
  );

  Assert.equal(
    manager.store.getExperimentForFeature(FEATURE_ID),
    null,
    "Experiment should be unenrolled"
  );

  const gleanEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(gleanEvents.length, 1, "Should be one unenrollment event");
  Assert.equal(
    gleanEvents[0].extra.reason,
    "l10n-missing-entry",
    "Reason should match"
  );
  Assert.equal(
    gleanEvents[0].extra.experiment,
    "experiment",
    "Slug should match"
  );

  TelemetryTestUtils.assertEvents(
    [
      {
        value: "experiment",
        extra: { reason: "l10n-missing-entry" },
      },
    ],
    LEGACY_TELEMETRY_FILTER
  );

  await cleanupStore(manager.store);
  sandbox.reset();
});

add_task(async function test_getLocalizedValue_unenroll_missingEntry() {
  resetTelemetry();

  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();

  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.onStartup();
  await manager.store.ready();

  const experiment = ExperimentFakes.recipe("experiment", {
    branches: [
      {
        slug: "control",
        features: [
          {
            featureId: FEATURE_ID,
            value: {
              bar: {
                $l10n: {
                  id: "BOGUS",
                  comment: "Bogus localization",
                  text: "Original text",
                },
              },
            },
          },
        ],
      },
    ],
    localizations: {
      "en-CA": {},
    },
  });

  await ExperimentFakes.enrollmentHelper(experiment).enrollmentPromise;

  const enrollment = manager.store.getExperimentForFeature(FEATURE_ID);

  Assert.deepEqual(
    FEATURE._getLocalizedValue(enrollment),
    undefined,
    "_getLocalizedValue() with a bogus localization"
  );

  Assert.equal(
    manager.store.getExperimentForFeature(FEATURE_ID),
    null,
    "Experiment should be unenrolled"
  );

  const gleanEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(gleanEvents.length, 1, "Should be one unenrollment event");
  Assert.equal(
    gleanEvents[0].extra.reason,
    "l10n-missing-locale",
    "Reason should match"
  );
  Assert.equal(
    gleanEvents[0].extra.experiment,
    "experiment",
    "Slug should match"
  );

  TelemetryTestUtils.assertEvents(
    [
      {
        value: "experiment",
        extra: { reason: "l10n-missing-locale" },
      },
    ],
    LEGACY_TELEMETRY_FILTER
  );

  await cleanupStore(manager.store);
  sandbox.reset();
});

add_task(async function test_getVariables() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();

  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.onStartup();
  await manager.store.ready();

  const experiment = ExperimentFakes.recipe("experiment", {
    branches: [
      {
        slug: "control",
        features: [
          {
            featureId: FEATURE_ID,
            value: DEEPLY_NESTED_VALUE,
          },
        ],
      },
    ],
    localizations: LOCALIZATIONS,
  });

  const {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(experiment);
  await enrollmentPromise;

  Assert.deepEqual(
    FEATURE.getAllVariables(),
    LOCALIZED_DEEPLY_NESTED_VALUE,
    "getAllVariables() returns subsituted values"
  );

  Assert.equal(
    FEATURE.getVariable("foo"),
    LOCALIZED_DEEPLY_NESTED_VALUE.foo,
    "getVariable() returns a top-level substituted value"
  );

  Assert.deepEqual(
    FEATURE.getVariable("bar"),
    LOCALIZED_DEEPLY_NESTED_VALUE.bar,
    "getVariable() returns a nested substitution"
  );

  Assert.deepEqual(
    FEATURE.getVariable("baz"),
    DEEPLY_NESTED_VALUE.baz,
    "getVariable() returns non-localized variables unmodified"
  );

  Assert.deepEqual(
    FEATURE.getVariable("waldo"),
    LOCALIZED_DEEPLY_NESTED_VALUE.waldo,
    "getVariable() returns substitutions inside arrays"
  );

  await doExperimentCleanup();
  await cleanupStore(manager.store);
  sandbox.reset();
});

add_task(async function test_getVariables_fallback() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();

  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.onStartup();
  await manager.store.ready();

  Services.prefs.setStringPref(
    FEATURE.manifest.variables.foo.fallbackPref,
    "fallback-foo-pref-value"
  );
  Services.prefs.setStringPref(
    FEATURE.manifest.variables.baz.fallbackPref,
    "fallback-baz-pref-value"
  );

  const recipes = {
    experiment: ExperimentFakes.recipe("experiment", {
      branches: [
        {
          slug: "control",
          features: [
            {
              featureId: FEATURE_ID,
              value: {
                foo: DEEPLY_NESTED_VALUE.foo,
              },
            },
          ],
        },
      ],
      localizations: {
        "en-US": {
          foo: LOCALIZATIONS["en-US"].foo,
        },
      },
    }),

    rollout: ExperimentFakes.recipe("rollout", {
      isRollout: true,
      branches: [
        {
          slug: "control",
          features: [
            {
              featureId: FEATURE_ID,
              value: {
                bar: DEEPLY_NESTED_VALUE.bar,
              },
            },
          ],
        },
      ],
      localizations: {
        "en-US": {
          qux: LOCALIZATIONS["en-US"].qux,
          grault: LOCALIZATIONS["en-US"].grault,
        },
      },
    }),
  };

  const cleanup = {};

  Assert.deepEqual(
    FEATURE.getAllVariables({ defaultValues: { waldo: ["default-value"] } }),
    {
      foo: "fallback-foo-pref-value",
      bar: null,
      baz: "fallback-baz-pref-value",
      waldo: ["default-value"],
    },
    "getAllVariables() returns only values from prefs and defaults"
  );

  Assert.equal(
    FEATURE.getVariable("foo"),
    "fallback-foo-pref-value",
    "variable foo returned from prefs"
  );
  Assert.equal(
    FEATURE.getVariable("bar"),
    undefined,
    "variable bar returned from rollout"
  );
  Assert.equal(
    FEATURE.getVariable("baz"),
    "fallback-baz-pref-value",
    "variable baz returned from prefs"
  );

  // Enroll in the rollout.
  {
    const {
      enrollmentPromise,
      doExperimentCleanup,
    } = ExperimentFakes.enrollmentHelper(recipes.rollout);
    await enrollmentPromise;

    cleanup.rollout = doExperimentCleanup;
  }

  Assert.deepEqual(
    FEATURE.getAllVariables({ defaultValues: { waldo: ["default-value"] } }),
    {
      foo: "fallback-foo-pref-value",
      bar: LOCALIZED_DEEPLY_NESTED_VALUE.bar,
      baz: "fallback-baz-pref-value",
      waldo: ["default-value"],
    },
    "getAllVariables() returns subsituted values from the rollout"
  );

  Assert.equal(
    FEATURE.getVariable("foo"),
    "fallback-foo-pref-value",
    "variable foo returned from prefs"
  );
  Assert.deepEqual(
    FEATURE.getVariable("bar"),
    LOCALIZED_DEEPLY_NESTED_VALUE.bar,
    "variable bar returned from rollout"
  );
  Assert.equal(
    FEATURE.getVariable("baz"),
    "fallback-baz-pref-value",
    "variable baz returned from prefs"
  );

  // Enroll in the experiment.
  {
    const {
      enrollmentPromise,
      doExperimentCleanup,
    } = ExperimentFakes.enrollmentHelper(recipes.experiment);
    await enrollmentPromise;

    cleanup.experiment = doExperimentCleanup;
  }

  Assert.deepEqual(
    FEATURE.getAllVariables({ defaultValues: { waldo: ["default-value"] } }),
    {
      foo: LOCALIZED_DEEPLY_NESTED_VALUE.foo,
      bar: null,
      baz: "fallback-baz-pref-value",
      waldo: ["default-value"],
    },
    "getAllVariables() returns subsituted values from the experiment"
  );

  Assert.equal(
    FEATURE.getVariable("foo"),
    LOCALIZED_DEEPLY_NESTED_VALUE.foo,
    "variable foo returned from experiment"
  );
  Assert.deepEqual(
    FEATURE.getVariable("bar"),
    LOCALIZED_DEEPLY_NESTED_VALUE.bar,
    "variable bar returned from rollout"
  );
  Assert.equal(
    FEATURE.getVariable("baz"),
    "fallback-baz-pref-value",
    "variable baz returned from prefs"
  );

  // Unenroll from the rollout so we are only enrolled in an experiment.
  await cleanup.rollout();

  Assert.deepEqual(
    FEATURE.getAllVariables({ defaultValues: { waldo: ["default-value"] } }),
    {
      foo: LOCALIZED_DEEPLY_NESTED_VALUE.foo,
      bar: null,
      baz: "fallback-baz-pref-value",
      waldo: ["default-value"],
    },
    "getAllVariables() returns substituted values from the experiment"
  );

  Assert.equal(
    FEATURE.getVariable("foo"),
    LOCALIZED_DEEPLY_NESTED_VALUE.foo,
    "variable foo returned from experiment"
  );
  Assert.equal(
    FEATURE.getVariable("bar"),
    undefined,
    "variable bar is not set"
  );
  Assert.equal(
    FEATURE.getVariable("baz"),
    "fallback-baz-pref-value",
    "variable baz returned from prefs"
  );

  // Unenroll from experiment. We are enrolled in nothing.
  await cleanup.experiment();

  Assert.deepEqual(
    FEATURE.getAllVariables({ defaultValues: { waldo: ["default-value"] } }),
    {
      foo: "fallback-foo-pref-value",
      bar: null,
      baz: "fallback-baz-pref-value",
      waldo: ["default-value"],
    },
    "getAllVariables() returns only values from prefs and defaults"
  );

  Assert.equal(
    FEATURE.getVariable("foo"),
    "fallback-foo-pref-value",
    "variable foo returned from prefs"
  );
  Assert.equal(
    FEATURE.getVariable("bar"),
    undefined,
    "variable bar returned from rollout"
  );
  Assert.equal(
    FEATURE.getVariable("baz"),
    "fallback-baz-pref-value",
    "variable baz returned from prefs"
  );

  Services.prefs.clearUserPref(FEATURE.manifest.variables.foo.fallbackPref);
  Services.prefs.clearUserPref(FEATURE.manifest.variables.baz.fallbackPref);

  await cleanupStore(manager.store);
  sandbox.reset();
});

add_task(async function test_getVariables_fallback_unenroll() {
  resetTelemetry();

  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();

  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);
  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  await manager.onStartup();
  await manager.store.ready();

  Services.prefs.setStringPref(
    FEATURE.manifest.variables.foo.fallbackPref,
    "fallback-foo-pref-value"
  );
  Services.prefs.setStringPref(
    FEATURE.manifest.variables.bar.fallbackPref,
    `"fallback-bar-pref-value"`
  );
  Services.prefs.setStringPref(
    FEATURE.manifest.variables.baz.fallbackPref,
    "fallback-baz-pref-value"
  );
  Services.prefs.setStringPref(
    FEATURE.manifest.variables.waldo.fallbackPref,
    JSON.stringify(["fallback-waldo-pref-value"])
  );

  const recipes = [
    ExperimentFakes.recipe("experiment", {
      branches: [
        {
          slug: "control",
          features: [
            {
              featureId: FEATURE_ID,
              value: {
                foo: DEEPLY_NESTED_VALUE.foo,
              },
            },
          ],
        },
      ],
      localizations: {},
    }),

    ExperimentFakes.recipe("rollout", {
      isRollout: true,
      branches: [
        {
          slug: "control",
          features: [
            {
              featureId: FEATURE_ID,
              value: {
                bar: DEEPLY_NESTED_VALUE.bar,
              },
            },
          ],
        },
      ],
      localizations: {
        "en-US": {},
      },
    }),
  ];

  for (const recipe of recipes) {
    await ExperimentFakes.enrollmentHelper(recipe).enrollmentPromise;
  }

  Assert.deepEqual(FEATURE.getAllVariables(), {
    foo: "fallback-foo-pref-value",
    bar: "fallback-bar-pref-value",
    baz: "fallback-baz-pref-value",
    waldo: ["fallback-waldo-pref-value"],
  });

  Assert.equal(
    manager.store.getExperimentForFeature(FEATURE_ID),
    null,
    "Experiment should be unenrolled"
  );

  Assert.equal(
    manager.store.getRolloutForFeature(FEATURE_ID),
    null,
    "Rollout should be unenrolled"
  );

  const gleanEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(gleanEvents.length, 2, "Should be two unenrollment events");
  Assert.equal(
    gleanEvents[0].extra.reason,
    "l10n-missing-locale",
    "Reason should match"
  );
  Assert.equal(
    gleanEvents[0].extra.experiment,
    "experiment",
    "Slug should match"
  );
  Assert.equal(
    gleanEvents[1].extra.reason,
    "l10n-missing-entry",
    "Reason should match"
  );
  Assert.equal(gleanEvents[1].extra.experiment, "rollout", "Slug should match");

  TelemetryTestUtils.assertEvents(
    [
      {
        value: "experiment",
        extra: { reason: "l10n-missing-locale" },
      },
      {
        value: "rollout",
        extra: { reason: "l10n-missing-entry" },
      },
    ],
    LEGACY_TELEMETRY_FILTER
  );

  Services.prefs.clearUserPref(FEATURE.manifest.variables.foo.fallbackPref);
  Services.prefs.clearUserPref(FEATURE.manifest.variables.bar.fallbackPref);
  Services.prefs.clearUserPref(FEATURE.manifest.variables.baz.fallbackPref);
  Services.prefs.clearUserPref(FEATURE.manifest.variables.waldo.fallbackPref);

  await cleanupStore(manager.store);
  sandbox.reset();
});
