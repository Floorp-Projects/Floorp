"use strict";

const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
const { PreferenceExperiments } = ChromeUtils.import(
  "resource://normandy/lib/PreferenceExperiments.jsm"
);
const { CleanupManager } = ChromeUtils.import(
  "resource://normandy/lib/CleanupManager.jsm"
);
const { NormandyUtils } = ChromeUtils.import(
  "resource://normandy/lib/NormandyUtils.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);

// Save ourselves some typing
const { withMockExperiments } = PreferenceExperiments;
const DefaultPreferences = new Preferences({ defaultBranch: true });
const startupPrefs = "app.normandy.startupExperimentPrefs";
const { preferenceStudyFactory } = NormandyTestUtils.factories;

const NOW = new Date();

const mockV1Data = {
  hypothetical_experiment: {
    name: "hypothetical_experiment",
    branch: "hypo_1",
    expired: false,
    lastSeen: NOW.toJSON(),
    preferenceName: "some.pref",
    preferenceValue: 2,
    preferenceType: "integer",
    previousPreferenceValue: 1,
    preferenceBranchType: "user",
    experimentType: "exp",
  },
  another_experiment: {
    name: "another_experiment",
    branch: "another_4",
    expired: true,
    lastSeen: NOW.toJSON(),
    preferenceName: "another.pref",
    preferenceValue: true,
    preferenceType: "boolean",
    previousPreferenceValue: false,
    preferenceBranchType: "default",
    experimentType: "exp",
  },
};

const mockV2Data = {
  experiments: {
    hypothetical_experiment: {
      name: "hypothetical_experiment",
      branch: "hypo_1",
      expired: false,
      lastSeen: NOW.toJSON(),
      preferenceName: "some.pref",
      preferenceValue: 2,
      preferenceType: "integer",
      previousPreferenceValue: 1,
      preferenceBranchType: "user",
      experimentType: "exp",
    },
    another_experiment: {
      name: "another_experiment",
      branch: "another_4",
      expired: true,
      lastSeen: NOW.toJSON(),
      preferenceName: "another.pref",
      preferenceValue: true,
      preferenceType: "boolean",
      previousPreferenceValue: false,
      preferenceBranchType: "default",
      experimentType: "exp",
    },
  },
};

const mockV3Data = {
  experiments: {
    hypothetical_experiment: {
      name: "hypothetical_experiment",
      branch: "hypo_1",
      expired: false,
      lastSeen: NOW.toJSON(),
      preferences: {
        "some.pref": {
          preferenceValue: 2,
          preferenceType: "integer",
          previousPreferenceValue: 1,
          preferenceBranchType: "user",
        },
      },
      experimentType: "exp",
    },
    another_experiment: {
      name: "another_experiment",
      branch: "another_4",
      expired: true,
      lastSeen: NOW.toJSON(),
      preferences: {
        "another.pref": {
          preferenceValue: true,
          preferenceType: "boolean",
          previousPreferenceValue: false,
          preferenceBranchType: "default",
        },
      },
      experimentType: "exp",
    },
  },
};

const mockV4Data = {
  experiments: {
    hypothetical_experiment: {
      name: "hypothetical_experiment",
      branch: "hypo_1",
      actionName: "SinglePreferenceExperimentAction",
      expired: false,
      lastSeen: NOW.toJSON(),
      preferences: {
        "some.pref": {
          preferenceValue: 2,
          preferenceType: "integer",
          previousPreferenceValue: 1,
          preferenceBranchType: "user",
        },
      },
      experimentType: "exp",
    },
    another_experiment: {
      name: "another_experiment",
      branch: "another_4",
      actionName: "SinglePreferenceExperimentAction",
      expired: true,
      lastSeen: NOW.toJSON(),
      preferences: {
        "another.pref": {
          preferenceValue: true,
          preferenceType: "boolean",
          previousPreferenceValue: false,
          preferenceBranchType: "default",
        },
      },
      experimentType: "exp",
    },
  },
};

const mockV5Data = {
  experiments: {
    hypothetical_experiment: {
      slug: "hypothetical_experiment",
      branch: "hypo_1",
      actionName: "SinglePreferenceExperimentAction",
      expired: false,
      lastSeen: NOW.toJSON(),
      preferences: {
        "some.pref": {
          preferenceValue: 2,
          preferenceType: "integer",
          previousPreferenceValue: 1,
          preferenceBranchType: "user",
        },
      },
      experimentType: "exp",
    },
    another_experiment: {
      slug: "another_experiment",
      branch: "another_4",
      actionName: "SinglePreferenceExperimentAction",
      expired: true,
      lastSeen: NOW.toJSON(),
      preferences: {
        "another.pref": {
          preferenceValue: true,
          preferenceType: "boolean",
          previousPreferenceValue: false,
          preferenceBranchType: "default",
        },
      },
      experimentType: "exp",
    },
  },
};

const migrationsInfo = [
  {
    migration: PreferenceExperiments.migrations.migration01MoveExperiments,
    dataBefore: mockV1Data,
    dataAfter: mockV2Data,
  },
  {
    migration: PreferenceExperiments.migrations.migration02MultiPreference,
    dataBefore: mockV2Data,
    dataAfter: mockV3Data,
  },
  {
    migration: PreferenceExperiments.migrations.migration03AddActionName,
    dataBefore: mockV3Data,
    dataAfter: mockV4Data,
  },
  {
    migration: PreferenceExperiments.migrations.migration04RenameNameToSlug,
    dataBefore: mockV4Data,
    dataAfter: mockV5Data,
  },
  // Migration 5 is not a simple data migration. This style of tests does not apply to it.
];

/**
 * Make a mock `JsonFile` object with a no-op `saveSoon` method and a deep copy
 * of the data passed.
 * @param {Object} data the data in the store
 */
function makeMockJsonFile(data = {}) {
  return {
    // Deep clone the data in case migrations mutate it.
    data: JSON.parse(JSON.stringify(data)),
    saveSoon: () => {},
  };
}

/** Test that each migration results in the expected data */
add_task(async function test_migrations() {
  for (const { migration, dataAfter, dataBefore } of migrationsInfo) {
    let mockJsonFile = makeMockJsonFile(dataBefore);
    await migration(mockJsonFile);
    Assert.deepEqual(
      mockJsonFile.data,
      dataAfter,
      `Migration ${migration.name} should result in the expected data`
    );
  }
});

add_task(async function migrations_are_idempotent() {
  for (const { migration, dataBefore } of migrationsInfo) {
    const mockJsonFileOnce = makeMockJsonFile(dataBefore);
    const mockJsonFileTwice = makeMockJsonFile(dataBefore);
    await migration(mockJsonFileOnce);
    await migration(mockJsonFileTwice);
    await migration(mockJsonFileTwice);
    Assert.deepEqual(
      mockJsonFileOnce.data,
      mockJsonFileTwice.data,
      "migrating data twice should be idempotent for " + migration.name
    );
  }
});

add_task(async function migration03KeepsActionName() {
  let mockData = JSON.parse(JSON.stringify(mockV3Data));
  mockData.experiments.another_experiment.actionName = "SomeOldAction";
  const mockJsonFile = makeMockJsonFile(mockData);
  // Output should be the same as mockV4Data, but preserving the action.
  const migratedData = JSON.parse(JSON.stringify(mockV4Data));
  migratedData.experiments.another_experiment.actionName = "SomeOldAction";

  await PreferenceExperiments.migrations.migration03AddActionName(mockJsonFile);
  Assert.deepEqual(mockJsonFile.data, migratedData);
});

// Test that migration 5 works as expected
decorate_task(
  PreferenceExperiments.withMockExperiments([
    NormandyTestUtils.factories.preferenceStudyFactory({
      actionName: "PreferenceExperimentAction",
      expired: false,
    }),
    NormandyTestUtils.factories.preferenceStudyFactory({
      actionName: "SinglePreferenceExperimentAction",
      expired: false,
    }),
  ]),
  async function migration05Works([expKeep, expExpire]) {
    // pre check
    const activeSlugsBefore = (await PreferenceExperiments.getAllActive()).map(
      e => e.slug
    );
    Assert.deepEqual(
      activeSlugsBefore,
      [expKeep.slug, expExpire.slug],
      "Both experiments should be present and active before the migration"
    );

    // run the migration
    await PreferenceExperiments.migrations.migration05RemoveOldAction();

    // verify behavior
    const activeSlugsAfter = (await PreferenceExperiments.getAllActive()).map(
      e => e.slug
    );
    Assert.deepEqual(
      activeSlugsAfter,
      [expKeep.slug],
      "The single pref experiment should be ended by the migration"
    );
    const allSlugsAfter = (await PreferenceExperiments.getAll()).map(
      e => e.slug
    );
    Assert.deepEqual(
      allSlugsAfter,
      [expKeep.slug, expExpire.slug],
      "Both experiments should still exist after the migration"
    );
  }
);

// clearAllExperimentStorage
decorate_task(
  withMockExperiments([preferenceStudyFactory({ slug: "test" })]),
  async function(experiments) {
    ok(await PreferenceExperiments.has("test"), "Mock experiment is detected.");
    await PreferenceExperiments.clearAllExperimentStorage();
    ok(
      !(await PreferenceExperiments.has("test")),
      "clearAllExperimentStorage removed all stored experiments"
    );
  }
);

// start should throw if an experiment with the given name already exists
decorate_task(
  withMockExperiments([preferenceStudyFactory({ slug: "test" })]),
  withSendEventSpy(),
  async function(experiments, sendEventSpy) {
    await Assert.rejects(
      PreferenceExperiments.start({
        slug: "test",
        actionName: "SomeAction",
        branch: "branch",
        preferences: {
          "fake.preference": {
            preferenceValue: "value",
            preferenceType: "string",
            preferenceBranchType: "default",
          },
        },
      }),
      /test.*already exists/,
      "start threw an error due to a conflicting experiment name"
    );

    sendEventSpy.assertEvents([
      ["enrollFailed", "preference_study", "test", { reason: "name-conflict" }],
    ]);
  }
);

// start should throw if an experiment for any of the given
// preferences are active
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      preferences: { "fake.preferenceinteger": {} },
    }),
  ]),
  withSendEventSpy(),
  async function(experiments, sendEventSpy) {
    await Assert.rejects(
      PreferenceExperiments.start({
        slug: "different",
        actionName: "SomeAction",
        branch: "branch",
        preferences: {
          "fake.preference": {
            preferenceValue: "value",
            preferenceType: "string",
            preferenceBranchType: "default",
          },
          "fake.preferenceinteger": {
            preferenceValue: 2,
            preferenceType: "integer",
            preferenceBranchType: "default",
          },
        },
      }),
      /another.*is currently active/i,
      "start threw an error due to an active experiment for the given preference"
    );

    sendEventSpy.assertEvents([
      [
        "enrollFailed",
        "preference_study",
        "different",
        { reason: "pref-conflict" },
      ],
    ]);
  }
);

// start should throw if an invalid preferenceBranchType is given
decorate_task(withMockExperiments(), withSendEventSpy(), async function(
  experiments,
  sendEventSpy
) {
  await Assert.rejects(
    PreferenceExperiments.start({
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        "fake.preference": {
          preferenceValue: "value",
          preferenceType: "string",
          preferenceBranchType: "invalid",
        },
      },
    }),
    /invalid value for preferenceBranchType: invalid/i,
    "start threw an error due to an invalid preference branch type"
  );

  sendEventSpy.assertEvents([
    ["enrollFailed", "preference_study", "test", { reason: "invalid-branch" }],
  ]);
});

// start should save experiment data, modify preferences, and register a
// watcher.
decorate_task(
  withMockExperiments(),
  withMockPreferences(),
  withStub(PreferenceExperiments, "startObserver"),
  withSendEventSpy(),
  async function testStart(
    experiments,
    mockPreferences,
    startObserverStub,
    sendEventSpy
  ) {
    mockPreferences.set("fake.preference", "oldvalue", "default");
    mockPreferences.set("fake.preference", "uservalue", "user");
    mockPreferences.set("fake.preferenceinteger", 1, "default");
    mockPreferences.set("fake.preferenceinteger", 101, "user");

    const experiment = {
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        "fake.preference": {
          preferenceValue: "newvalue",
          preferenceBranchType: "default",
          preferenceType: "string",
        },
        "fake.preferenceinteger": {
          preferenceValue: 2,
          preferenceBranchType: "default",
          preferenceType: "integer",
        },
      },
    };
    await PreferenceExperiments.start(experiment);
    ok(await PreferenceExperiments.get("test"), "start saved the experiment");
    ok(
      startObserverStub.calledWith("test", experiment.preferences),
      "start registered an observer"
    );

    const expectedExperiment = {
      slug: "test",
      branch: "branch",
      expired: false,
      preferences: {
        "fake.preference": {
          preferenceValue: "newvalue",
          preferenceType: "string",
          previousPreferenceValue: "oldvalue",
          preferenceBranchType: "default",
        },
        "fake.preferenceinteger": {
          preferenceValue: 2,
          preferenceType: "integer",
          previousPreferenceValue: 1,
          preferenceBranchType: "default",
        },
      },
    };
    const experimentSubset = {};
    const actualExperiment = await PreferenceExperiments.get("test");
    Object.keys(expectedExperiment).forEach(
      key => (experimentSubset[key] = actualExperiment[key])
    );
    Assert.deepEqual(
      experimentSubset,
      expectedExperiment,
      "start saved the experiment"
    );

    is(
      DefaultPreferences.get("fake.preference"),
      "newvalue",
      "start modified the default preference"
    );
    is(
      Preferences.get("fake.preference"),
      "uservalue",
      "start did not modify the user preference"
    );
    is(
      Preferences.get(`${startupPrefs}.fake.preference`),
      "newvalue",
      "start saved the experiment value to the startup prefs tree"
    );
    is(
      DefaultPreferences.get("fake.preferenceinteger"),
      2,
      "start modified the default preference"
    );
    is(
      Preferences.get("fake.preferenceinteger"),
      101,
      "start did not modify the user preference"
    );
    is(
      Preferences.get(`${startupPrefs}.fake.preferenceinteger`),
      2,
      "start saved the experiment value to the startup prefs tree"
    );
  }
);

// start should modify the user preference for the user branch type
decorate_task(
  withMockExperiments(),
  withMockPreferences(),
  withStub(PreferenceExperiments, "startObserver"),
  async function(experiments, mockPreferences, startObserver) {
    mockPreferences.set("fake.preference", "olddefaultvalue", "default");
    mockPreferences.set("fake.preference", "oldvalue", "user");

    const experiment = {
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        "fake.preference": {
          preferenceValue: "newvalue",
          preferenceType: "string",
          preferenceBranchType: "user",
        },
      },
    };
    await PreferenceExperiments.start(experiment);
    ok(
      startObserver.calledWith("test", experiment.preferences),
      "start registered an observer"
    );

    const expectedExperiment = {
      slug: "test",
      branch: "branch",
      expired: false,
      preferences: {
        "fake.preference": {
          preferenceValue: "newvalue",
          preferenceType: "string",
          previousPreferenceValue: "oldvalue",
          preferenceBranchType: "user",
        },
      },
    };

    const experimentSubset = {};
    const actualExperiment = await PreferenceExperiments.get("test");
    Object.keys(expectedExperiment).forEach(
      key => (experimentSubset[key] = actualExperiment[key])
    );
    Assert.deepEqual(
      experimentSubset,
      expectedExperiment,
      "start saved the experiment"
    );

    Assert.notEqual(
      DefaultPreferences.get("fake.preference"),
      "newvalue",
      "start did not modify the default preference"
    );
    is(
      Preferences.get("fake.preference"),
      "newvalue",
      "start modified the user preference"
    );
  }
);

// start should detect if a new preference value type matches the previous value type
decorate_task(withMockPreferences(), withSendEventSpy(), async function(
  mockPreferences,
  sendEventSpy
) {
  mockPreferences.set("fake.type_preference", "oldvalue");

  await Assert.rejects(
    PreferenceExperiments.start({
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        "fake.type_preference": {
          preferenceBranchType: "user",
          preferenceValue: 12345,
          preferenceType: "integer",
        },
      },
    }),
    /previous preference value is of type/i,
    "start threw error for incompatible preference type"
  );

  sendEventSpy.assertEvents([
    ["enrollFailed", "preference_study", "test", { reason: "invalid-type" }],
  ]);
});

// startObserver should throw if an observer for the experiment is already
// active.
decorate_task(withMockExperiments(), async function() {
  PreferenceExperiments.startObserver("test", {
    "fake.preference": {
      preferenceType: "string",
      preferenceValue: "newvalue",
    },
  });
  Assert.throws(
    () =>
      PreferenceExperiments.startObserver("test", {
        "another.fake": {
          preferenceType: "string",
          preferenceValue: "othervalue",
        },
      }),
    /observer.*is already active/i,
    "startObservers threw due to a conflicting active observer"
  );
  PreferenceExperiments.stopAllObservers();
});

// startObserver should register an observer that calls stop when *any* preference
// changes from its experimental value.
decorate_task(
  withMockExperiments(),
  withMockPreferences(),
  async function testObserversCanObserveChanges(
    mockExperiments,
    mockPreferences
  ) {
    const preferences = {
      "fake.preferencestring": {
        preferenceType: "string",
        previousPreferenceValue: "startvalue",
        preferenceValue: "experimentvalue",
      },
      // "newvalue",
      "fake.preferenceboolean": {
        preferenceType: "boolean",
        previousPreferenceValue: false,
        preferenceValue: true,
      }, // false
      "fake.preferenceinteger": {
        preferenceType: "integer",
        previousPreferenceValue: 1,
        preferenceValue: 2,
      }, // 42
    };
    const newValues = {
      "fake.preferencestring": "newvalue",
      "fake.preferenceboolean": false,
      "fake.preferenceinteger": 42,
    };

    for (const [testPref, newValue] of Object.entries(newValues)) {
      const stopStub = sinon.stub(PreferenceExperiments, "stop");
      for (const [prefName, prefInfo] of Object.entries(preferences)) {
        mockPreferences.set(prefName, prefInfo.previousPreferenceValue);
      }

      // NOTE: startObserver does not modify the pref
      PreferenceExperiments.startObserver("test" + testPref, preferences);

      // Setting it to the experimental value should not trigger the call.
      for (const [prefName, prefInfo] of Object.entries(preferences)) {
        mockPreferences.set(prefName, prefInfo.preferenceValue);
        ok(
          !stopStub.called,
          "Changing to the experimental pref value did not trigger the observer"
        );
      }

      // Setting it to something different should trigger the call.
      mockPreferences.set(testPref, newValue);
      ok(
        stopStub.called,
        "Changing to a different value triggered the observer"
      );

      PreferenceExperiments.stopAllObservers();
      stopStub.restore();
    }
  }
);

decorate_task(withMockExperiments(), async function testHasObserver() {
  PreferenceExperiments.startObserver("test", {
    "fake.preference": {
      preferenceType: "string",
      preferenceValue: "experimentValue",
    },
  });

  ok(
    await PreferenceExperiments.hasObserver("test"),
    "hasObserver should detect active observers"
  );
  ok(
    !(await PreferenceExperiments.hasObserver("missing")),
    "hasObserver shouldn't detect inactive observers"
  );

  PreferenceExperiments.stopAllObservers();
});

// stopObserver should throw if there is no observer active for it to stop.
decorate_task(withMockExperiments(), async function() {
  Assert.throws(
    () => PreferenceExperiments.stopObserver("neveractive"),
    /no observer.*found/i,
    "stopObserver threw because there was not matching active observer"
  );
});

// stopObserver should cancel an active observers.
decorate_task(
  withMockExperiments(),
  withMockPreferences(),
  withStub(PreferenceExperiments, "stop", { returnValue: Promise.resolve() }),
  async function(mockExperiments, mockPreferences, stopStub) {
    const preferenceInfo = {
      "fake.preferencestring": {
        preferenceType: "string",
        preferenceValue: "experimentvalue",
      },
      "fake.preferenceinteger": {
        preferenceType: "integer",
        preferenceValue: 2,
      },
    };
    mockPreferences.set("fake.preference", "startvalue");

    PreferenceExperiments.startObserver("test", preferenceInfo);
    PreferenceExperiments.stopObserver("test");

    // Setting the preference now that the observer is stopped should not call
    // stop.
    mockPreferences.set("fake.preferencestring", "newvalue");
    ok(
      !stopStub.called,
      "stopObserver successfully removed the observer for string"
    );

    mockPreferences.set("fake.preferenceinteger", 42);
    ok(
      !stopStub.called,
      "stopObserver successfully removed the observer for integer"
    );

    // Now that the observer is stopped, start should be able to start a new one
    // without throwing.
    try {
      PreferenceExperiments.startObserver("test", preferenceInfo);
    } catch (err) {
      ok(
        false,
        "startObserver did not throw an error for an observer that was already stopped"
      );
    }

    PreferenceExperiments.stopAllObservers();
  }
);

// stopAllObservers
decorate_task(
  withMockExperiments(),
  withMockPreferences(),
  withStub(PreferenceExperiments, "stop", { returnValue: Promise.resolve() }),
  async function(mockExperiments, mockPreferences, stopStub) {
    mockPreferences.set("fake.preference", "startvalue");
    mockPreferences.set("other.fake.preference", "startvalue");

    PreferenceExperiments.startObserver("test", {
      "fake.preference": {
        preferenceType: "string",
        preferenceValue: "experimentvalue",
      },
    });
    PreferenceExperiments.startObserver("test2", {
      "other.fake.preference": {
        preferenceType: "string",
        preferenceValue: "experimentvalue",
      },
    });
    PreferenceExperiments.stopAllObservers();

    // Setting the preference now that the observers are stopped should not call
    // stop.
    mockPreferences.set("fake.preference", "newvalue");
    mockPreferences.set("other.fake.preference", "newvalue");
    ok(!stopStub.called, "stopAllObservers successfully removed all observers");

    // Now that the observers are stopped, start should be able to start new
    // observers without throwing.
    try {
      PreferenceExperiments.startObserver("test", {
        "fake.preference": {
          preferenceType: "string",
          preferenceValue: "experimentvalue",
        },
      });
      PreferenceExperiments.startObserver("test2", {
        "other.fake.preference": {
          preferenceType: "string",
          preferenceValue: "experimentvalue",
        },
      });
    } catch (err) {
      ok(
        false,
        "startObserver did not throw an error for an observer that was already stopped"
      );
    }

    PreferenceExperiments.stopAllObservers();
  }
);

// markLastSeen should throw if it can't find a matching experiment
decorate_task(withMockExperiments(), async function() {
  await Assert.rejects(
    PreferenceExperiments.markLastSeen("neveractive"),
    /could not find/i,
    "markLastSeen threw because there was not a matching experiment"
  );
});

// markLastSeen should update the lastSeen date
const oldDate = new Date(1988, 10, 1).toJSON();
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({ slug: "test", lastSeen: oldDate }),
  ]),
  async function([experiment]) {
    await PreferenceExperiments.markLastSeen("test");
    Assert.notEqual(
      experiment.lastSeen,
      oldDate,
      "markLastSeen updated the experiment lastSeen date"
    );
  }
);

// stop should throw if an experiment with the given name doesn't exist
decorate_task(withMockExperiments(), withSendEventSpy(), async function(
  experiments,
  sendEventSpy
) {
  await Assert.rejects(
    PreferenceExperiments.stop("test"),
    /could not find/i,
    "stop threw an error because there are no experiments with the given name"
  );

  sendEventSpy.assertEvents([
    [
      "unenrollFailed",
      "preference_study",
      "test",
      { reason: "does-not-exist" },
    ],
  ]);
});

// stop should throw if the experiment is already expired
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({ slug: "test", expired: true }),
  ]),
  withSendEventSpy(),
  async function(experiments, sendEventSpy) {
    await Assert.rejects(
      PreferenceExperiments.stop("test"),
      /already expired/,
      "stop threw an error because the experiment was already expired"
    );

    sendEventSpy.assertEvents([
      [
        "unenrollFailed",
        "preference_study",
        "test",
        { reason: "already-unenrolled" },
      ],
    ]);
  }
);

// stop should mark the experiment as expired, stop its observer, and revert the
// preference value.
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      expired: false,
      branch: "fakebranch",
      preferences: {
        "fake.preference": {
          preferenceValue: "experimentvalue",
          preferenceType: "string",
          previousPreferenceValue: "oldvalue",
          preferenceBranchType: "default",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withSpy(PreferenceExperiments, "stopObserver"),
  withSendEventSpy(),
  async function testStop(
    experiments,
    mockPreferences,
    stopObserverSpy,
    sendEventSpy
  ) {
    // this assertion is mostly useful for --verify test runs, to make
    // sure that tests clean up correctly.
    ok(!Preferences.get("fake.preference"), "preference should start unset");

    mockPreferences.set(
      `${startupPrefs}.fake.preference`,
      "experimentvalue",
      "user"
    );
    mockPreferences.set("fake.preference", "experimentvalue", "default");
    PreferenceExperiments.startObserver("test", {
      "fake.preference": {
        preferenceType: "string",
        preferenceValue: "experimentvalue",
      },
    });

    await PreferenceExperiments.stop("test", { reason: "test-reason" });
    ok(stopObserverSpy.calledWith("test"), "stop removed an observer");
    const experiment = await PreferenceExperiments.get("test");
    is(experiment.expired, true, "stop marked the experiment as expired");
    is(
      DefaultPreferences.get("fake.preference"),
      "oldvalue",
      "stop reverted the preference to its previous value"
    );
    ok(
      !Services.prefs.prefHasUserValue(`${startupPrefs}.fake.preference`),
      "stop cleared the startup preference for fake.preference."
    );

    sendEventSpy.assertEvents([
      [
        "unenroll",
        "preference_study",
        "test",
        {
          didResetValue: "true",
          reason: "test-reason",
          branch: "fakebranch",
        },
      ],
    ]);

    PreferenceExperiments.stopAllObservers();
  }
);

// stop should also support user pref experiments
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      expired: false,
      preferences: {
        "fake.preference": {
          preferenceValue: "experimentvalue",
          preferenceType: "string",
          previousPreferenceValue: "oldvalue",
          preferenceBranchType: "user",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withStub(PreferenceExperiments, "stopObserver"),
  withStub(PreferenceExperiments, "hasObserver"),
  async function testStopUserPrefs(
    experiments,
    mockPreferences,
    stopObserver,
    hasObserver
  ) {
    hasObserver.returns(true);

    mockPreferences.set("fake.preference", "experimentvalue", "user");
    PreferenceExperiments.startObserver("test", {
      "fake.preference": {
        preferenceType: "string",
        preferenceValue: "experimentvalue",
      },
    });

    await PreferenceExperiments.stop("test");
    ok(stopObserver.calledWith("test"), "stop removed an observer");
    const experiment = await PreferenceExperiments.get("test");
    is(experiment.expired, true, "stop marked the experiment as expired");
    is(
      Preferences.get("fake.preference"),
      "oldvalue",
      "stop reverted the preference to its previous value"
    );
    stopObserver.restore();
    PreferenceExperiments.stopAllObservers();
  }
);

// stop should remove a preference that had no value prior to an experiment for user prefs
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      expired: false,
      preferences: {
        "fake.preference": {
          preferenceValue: "experimentvalue",
          preferenceType: "string",
          previousPreferenceValue: null,
          preferenceBranchType: "user",
        },
      },
    }),
  ]),
  withMockPreferences(),
  async function(experiments, mockPreferences) {
    mockPreferences.set("fake.preference", "experimentvalue", "user");

    await PreferenceExperiments.stop("test");
    ok(
      !Preferences.isSet("fake.preference"),
      "stop removed the preference that had no value prior to the experiment"
    );
  }
);

// stop should not modify a preference if resetValue is false
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      expired: false,
      branch: "fakebranch",
      preferences: {
        "fake.preference": {
          preferenceValue: "experimentvalue",
          preferenceType: "string",
          previousPreferenceValue: "oldvalue",
          preferenceBranchType: "default",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withStub(PreferenceExperiments, "stopObserver"),
  withSendEventSpy(),
  async function testStopReset(
    experiments,
    mockPreferences,
    stopObserverStub,
    sendEventSpy
  ) {
    mockPreferences.set("fake.preference", "customvalue", "default");

    await PreferenceExperiments.stop("test", {
      reason: "test-reason",
      resetValue: false,
    });
    is(
      DefaultPreferences.get("fake.preference"),
      "customvalue",
      "stop did not modify the preference"
    );
    sendEventSpy.assertEvents([
      [
        "unenroll",
        "preference_study",
        "test",
        {
          didResetValue: "false",
          reason: "test-reason",
          branch: "fakebranch",
        },
      ],
    ]);
  }
);

// stop should include the system that stopped it
decorate_task(
  withMockExperiments([preferenceStudyFactory({ expired: true })]),
  withSendEventSpy,
  async function testStopUserPrefs([experiment], sendEventSpy) {
    await Assert.rejects(
      PreferenceExperiments.stop(experiment.slug, {
        caller: "testCaller",
        reason: "original-reason",
      }),
      /.*already expired.*/,
      "Stopped an expired experiment should throw an exception"
    );

    const expectedExtra = {
      reason: "already-unenrolled",
      enrollmentId: experiment.enrollmentId,
      originalReason: "original-reason",
    };
    if (AppConstants.NIGHTLY_BUILD) {
      expectedExtra.caller = "testCaller";
    }

    sendEventSpy.assertEvents([
      ["unenrollFailed", "preference_study", experiment.slug, expectedExtra],
    ]);
  }
);

// get should throw if no experiment exists with the given name
decorate_task(withMockExperiments(), async function() {
  await Assert.rejects(
    PreferenceExperiments.get("neverexisted"),
    /could not find/i,
    "get rejects if no experiment with the given name is found"
  );
});

// get
decorate_task(
  withMockExperiments([preferenceStudyFactory({ slug: "test" })]),
  async function(experiments) {
    const experiment = await PreferenceExperiments.get("test");
    is(experiment.slug, "test", "get fetches the correct experiment");

    // Modifying the fetched experiment must not edit the data source.
    experiment.slug = "othername";
    const refetched = await PreferenceExperiments.get("test");
    is(refetched.slug, "test", "get returns a copy of the experiment");
  }
);

// get all
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({ slug: "experiment1", disabled: false }),
    preferenceStudyFactory({ slug: "experiment2", disabled: true }),
  ]),
  async function testGetAll([experiment1, experiment2]) {
    const fetchedExperiments = await PreferenceExperiments.getAll();
    is(
      fetchedExperiments.length,
      2,
      "getAll returns a list of all stored experiments"
    );
    Assert.deepEqual(
      fetchedExperiments.find(e => e.slug === "experiment1"),
      experiment1,
      "getAll returns a list with the correct experiments"
    );
    const fetchedExperiment2 = fetchedExperiments.find(
      e => e.slug === "experiment2"
    );
    Assert.deepEqual(
      fetchedExperiment2,
      experiment2,
      "getAll returns a list with the correct experiments, including disabled ones"
    );

    fetchedExperiment2.slug = "otherslug";
    is(
      experiment2.slug,
      "experiment2",
      "getAll returns copies of the experiments"
    );
  }
);

// get all active
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "active",
      expired: false,
    }),
    preferenceStudyFactory({
      slug: "inactive",
      expired: true,
    }),
  ]),
  withMockPreferences(),
  async function testGetAllActive([activeExperiment, inactiveExperiment]) {
    let allActiveExperiments = await PreferenceExperiments.getAllActive();
    Assert.deepEqual(
      allActiveExperiments,
      [activeExperiment],
      "getAllActive only returns active experiments"
    );

    allActiveExperiments[0].slug = "newfakename";
    allActiveExperiments = await PreferenceExperiments.getAllActive();
    Assert.notEqual(
      allActiveExperiments,
      "newfakename",
      "getAllActive returns copies of stored experiments"
    );
  }
);

// has
decorate_task(
  withMockExperiments([preferenceStudyFactory({ slug: "test" })]),
  async function(experiments) {
    ok(
      await PreferenceExperiments.has("test"),
      "has returned true for a stored experiment"
    );
    ok(
      !(await PreferenceExperiments.has("missing")),
      "has returned false for a missing experiment"
    );
  }
);

// init should register telemetry experiments
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      branch: "branch",
      preferences: {
        "fake.pref": {
          preferenceValue: "experiment value",
          preferenceBranchType: "default",
          preferenceType: "string",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(PreferenceExperiments, "startObserver"),
  async function testInit(
    experiments,
    mockPreferences,
    setActiveStub,
    startObserverStub
  ) {
    mockPreferences.set("fake.pref", "experiment value");
    await PreferenceExperiments.init();
    ok(
      setActiveStub.calledWith("test", "branch", {
        type: "normandy-exp",
        enrollmentId: experiments[0].enrollmentId,
      }),
      "Experiment is registered by init"
    );
  }
);

// init should use the provided experiment type
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      branch: "branch",
      preferences: {
        "fake.pref": {
          preferenceValue: "experiment value",
          preferenceType: "string",
        },
      },
      experimentType: "pref-test",
    }),
  ]),
  withMockPreferences(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(PreferenceExperiments, "startObserver"),
  async function testInit(
    experiments,
    mockPreferences,
    setActiveStub,
    startObserverStub
  ) {
    mockPreferences.set("fake.pref", "experiment value");
    await PreferenceExperiments.init();
    ok(
      setActiveStub.calledWith("test", "branch", {
        type: "normandy-pref-test",
        enrollmentId: sinon.match(NormandyTestUtils.isUuid),
      }),
      "init should use the provided experiment type"
    );
  }
);

// starting and stopping experiments should register in telemetry
decorate_task(
  withMockExperiments(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventSpy(),
  async function testStartAndStopTelemetry(
    experiments,
    setActiveStub,
    setInactiveStub,
    sendEventSpy
  ) {
    let { enrollmentId } = await PreferenceExperiments.start({
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        "fake.preference": {
          preferenceValue: "value",
          preferenceType: "string",
          preferenceBranchType: "default",
        },
      },
    });

    ok(
      NormandyTestUtils.isUuid(enrollmentId),
      "Experiment should have a UUID enrollmentId"
    );

    Assert.deepEqual(
      setActiveStub.getCall(0).args,
      ["test", "branch", { type: "normandy-exp", enrollmentId }],
      "Experiment is registered by start()"
    );
    await PreferenceExperiments.stop("test", { reason: "test-reason" });
    Assert.deepEqual(
      setInactiveStub.args,
      [["test"]],
      "Experiment is unregistered by stop()"
    );

    sendEventSpy.assertEvents([
      [
        "enroll",
        "preference_study",
        "test",
        {
          experimentType: "exp",
          branch: "branch",
          enrollmentId,
        },
      ],
      [
        "unenroll",
        "preference_study",
        "test",
        {
          reason: "test-reason",
          didResetValue: "true",
          branch: "branch",
          enrollmentId,
        },
      ],
    ]);
  }
);

// starting experiments should use the provided experiment type
decorate_task(
  withMockExperiments(),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  withSendEventSpy(),
  async function testInitTelemetryExperimentType(
    experiments,
    setActiveStub,
    setInactiveStub,
    sendEventSpy
  ) {
    const { enrollmentId } = await PreferenceExperiments.start({
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        "fake.preference": {
          preferenceValue: "value",
          preferenceType: "string",
          preferenceBranchType: "default",
        },
      },
      experimentType: "pref-test",
    });

    Assert.deepEqual(
      setActiveStub.getCall(0).args,
      ["test", "branch", { type: "normandy-pref-test", enrollmentId }],
      "start() should register the experiment with the provided type"
    );

    sendEventSpy.assertEvents([
      [
        "enroll",
        "preference_study",
        "test",
        {
          experimentType: "pref-test",
          branch: "branch",
          enrollmentId,
        },
      ],
    ]);

    // start sets the passed preference in a way that is hard to mock.
    // Reset the preference so it doesn't interfere with other tests.
    Services.prefs.getDefaultBranch("fake.preference").deleteBranch("");
  }
);

// Experiments shouldn't be recorded by init() in telemetry if they are expired
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "expired",
      branch: "branch",
      expired: true,
    }),
  ]),
  withStub(TelemetryEnvironment, "setExperimentActive"),
  async function testInitTelemetryExpired(experiments, setActiveStub) {
    await PreferenceExperiments.init();
    ok(!setActiveStub.called, "Expired experiment is not registered by init");
  }
);

// Experiments should end if the preference has been changed when init() is called
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      preferences: {
        "fake.preference": {
          preferenceValue: "experiment value",
          preferenceType: "string",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withStub(PreferenceExperiments, "stop"),
  async function testInitChanges(experiments, mockPreferences, stopStub) {
    stopStub.returnValue = Promise.resolve();
    mockPreferences.set("fake.preference", "experiment value", "default");
    mockPreferences.set("fake.preference", "changed value", "user");
    await PreferenceExperiments.init();

    is(
      Preferences.get("fake.preference"),
      "changed value",
      "Preference value was not changed"
    );

    Assert.deepEqual(
      stopStub.getCall(0).args,
      [
        "test",
        {
          resetValue: false,
          reason: "user-preference-changed-sideload",
          changedPref: "fake.preference",
        },
      ],
      "Experiment is stopped correctly because value changed"
    );
  }
);

// init should register an observer for experiments
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      preferences: {
        "fake.preference": {
          preferenceValue: "experiment value",
          preferenceType: "string",
          previousPreferenceValue: "oldfakevalue",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stop"),
  withStub(CleanupManager, "addCleanupHandler"),
  async function testInitRegistersObserver(
    experiments,
    mockPreferences,
    startObserver,
    stopStub
  ) {
    stopStub.throws("Stop should not be called");
    mockPreferences.set("fake.preference", "experiment value", "default");
    is(
      Preferences.get("fake.preference"),
      "experiment value",
      "pref shouldn't have a user value"
    );
    await PreferenceExperiments.init();

    ok(startObserver.calledOnce, "init should register an observer");
    Assert.deepEqual(
      startObserver.getCall(0).args,
      [
        "test",
        {
          "fake.preference": {
            preferenceType: "string",
            preferenceValue: "experiment value",
            previousPreferenceValue: "oldfakevalue",
            preferenceBranchType: "default",
          },
        },
      ],
      "init should register an observer with the right args"
    );
  }
);

// saveStartupPrefs
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "char",
      preferences: {
        "fake.char": {
          preferenceValue: "string",
          preferenceType: "string",
        },
      },
    }),
    preferenceStudyFactory({
      slug: "int",
      preferences: {
        "fake.int": {
          preferenceValue: 2,
          preferenceType: "int",
        },
      },
    }),
    preferenceStudyFactory({
      slug: "bool",
      preferences: {
        "fake.bool": {
          preferenceValue: true,
          preferenceType: "boolean",
        },
      },
    }),
  ]),
  async function testSaveStartupPrefs(experiments) {
    Services.prefs.deleteBranch(startupPrefs);
    Services.prefs.setBoolPref(`${startupPrefs}.fake.old`, true);
    await PreferenceExperiments.saveStartupPrefs();

    ok(
      Services.prefs.getBoolPref(`${startupPrefs}.fake.bool`),
      "The startup value for fake.bool was saved."
    );
    is(
      Services.prefs.getCharPref(`${startupPrefs}.fake.char`),
      "string",
      "The startup value for fake.char was saved."
    );
    is(
      Services.prefs.getIntPref(`${startupPrefs}.fake.int`),
      2,
      "The startup value for fake.int was saved."
    );
    ok(
      !Services.prefs.prefHasUserValue(`${startupPrefs}.fake.old`),
      "saveStartupPrefs deleted old startup pref values."
    );
  }
);

// saveStartupPrefs errors for invalid pref type
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      preferences: {
        "fake.invalidValue": {
          preferenceValue: new Date(),
        },
      },
    }),
  ]),
  async function testSaveStartupPrefsError(experiments) {
    await Assert.rejects(
      PreferenceExperiments.saveStartupPrefs(),
      /invalid preference type/i,
      "saveStartupPrefs throws if an experiment has an invalid preference value type"
    );
  }
);

// saveStartupPrefs should not store values for user-branch recipes
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "defaultBranchRecipe",
      preferences: {
        "fake.default": {
          preferenceValue: "experiment value",
          preferenceType: "string",
          preferenceBranchType: "default",
        },
      },
    }),
    preferenceStudyFactory({
      slug: "userBranchRecipe",
      preferences: {
        "fake.user": {
          preferenceValue: "experiment value",
          preferenceType: "string",
          preferenceBranchType: "user",
        },
      },
    }),
  ]),
  async function testSaveStartupPrefsUserBranch(experiments) {
    Assert.deepEqual(
      Services.prefs.getChildList(startupPrefs),
      [],
      "As a prerequisite no startup prefs are set"
    );

    await PreferenceExperiments.saveStartupPrefs();

    Assert.deepEqual(
      Services.prefs.getChildList(startupPrefs),
      [`${startupPrefs}.fake.default`],
      "only the expected prefs are set"
    );
    is(
      Services.prefs.getCharPref(
        `${startupPrefs}.fake.default`,
        "fallback value"
      ),
      "experiment value",
      "The startup value for fake.default was set"
    );
    is(
      Services.prefs.getPrefType(`${startupPrefs}.fake.user`),
      Services.prefs.PREF_INVALID,
      "The startup value for fake.user was not set"
    );

    Services.prefs.deleteBranch(startupPrefs);
  }
);

// test that default branch prefs restore to the right value if the default pref changes
decorate_task(
  withMockExperiments(),
  withMockPreferences(),
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stopObserver"),
  async function testDefaultBranchStop(mockExperiments, mockPreferences) {
    const prefName = "fake.preference";
    mockPreferences.set(prefName, "old version's value", "default");

    // start an experiment
    await PreferenceExperiments.start({
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        [prefName]: {
          preferenceValue: "experiment value",
          preferenceBranchType: "default",
          preferenceType: "string",
        },
      },
    });

    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Starting an experiment should change the pref"
    );

    // Now pretend that firefox has updated and restarted to a version
    // where the built-default value of fake.preference is something
    // else. Bootstrap has run and changed the pref to the
    // experimental value, and produced the call to
    // recordOriginalValues below.
    PreferenceExperiments.recordOriginalValues({
      [prefName]: "new version's value",
    });
    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Recording original values shouldn't affect the preference."
    );

    // Now stop the experiment. It should revert to the new version's default, not the old.
    await PreferenceExperiments.stop("test");
    is(
      Services.prefs.getCharPref(prefName),
      "new version's value",
      "Preference should revert to new default"
    );
  }
);

// test that default branch prefs restore to the right value if the preference is removed
decorate_task(
  withMockExperiments(),
  withMockPreferences(),
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stopObserver"),
  async function testDefaultBranchStop(mockExperiments, mockPreferences) {
    const prefName = "fake.preference";
    mockPreferences.set(prefName, "old version's value", "default");

    // start an experiment
    await PreferenceExperiments.start({
      slug: "test",
      actionName: "SomeAction",
      branch: "branch",
      preferences: {
        [prefName]: {
          preferenceValue: "experiment value",
          preferenceBranchType: "default",
          preferenceType: "string",
        },
      },
    });

    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Starting an experiment should change the pref"
    );

    // Now pretend that firefox has updated and restarted to a version
    // where fake.preference has been removed in the default pref set.
    // Bootstrap has run and changed the pref to the experimental
    // value, and produced the call to recordOriginalValues below.
    PreferenceExperiments.recordOriginalValues({ [prefName]: null });
    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Recording original values shouldn't affect the preference."
    );

    // Now stop the experiment. It should remove the preference
    await PreferenceExperiments.stop("test");
    is(
      Services.prefs.getCharPref(prefName, "DEFAULT"),
      "DEFAULT",
      "Preference should be absent"
    );
  }
).skip(/* bug 1502410 and bug 1505941 */);

// stop should pass "unknown" to telemetry event for `reason` if none is specified
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      preferences: {
        "fake.preference": {
          preferenceValue: "experiment value",
          preferenceType: "string",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withStub(PreferenceExperiments, "stopObserver"),
  withSendEventSpy(),
  async function testStopUnknownReason(
    experiments,
    mockPreferences,
    stopObserverStub,
    sendEventSpy
  ) {
    mockPreferences.set("fake.preference", "default value", "default");
    await PreferenceExperiments.stop("test");
    is(
      sendEventSpy.getCall(0).args[3].reason,
      "unknown",
      "PreferenceExperiments.stop() should use unknown as the default reason"
    );
  }
);

// stop should pass along the value for resetValue to Telemetry Events as didResetValue
decorate_task(
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test1",
      preferences: {
        "fake.preference1": {
          preferenceValue: "experiment value",
          preferenceType: "string",
        },
      },
    }),
    preferenceStudyFactory({
      slug: "test2",
      preferences: {
        "fake.preference2": {
          preferenceValue: "experiment value",
          preferenceType: "string",
        },
      },
    }),
  ]),
  withMockPreferences(),
  withStub(PreferenceExperiments, "stopObserver"),
  withSendEventSpy(),
  async function testStopResetValue(
    experiments,
    mockPreferences,
    stopObserverStub,
    sendEventSpy
  ) {
    mockPreferences.set("fake.preference1", "default value", "default");
    await PreferenceExperiments.stop("test1", { resetValue: true });
    is(sendEventSpy.callCount, 1);
    is(
      sendEventSpy.getCall(0).args[3].didResetValue,
      "true",
      "PreferenceExperiments.stop() should pass true values of resetValue as didResetValue"
    );

    mockPreferences.set("fake.preference2", "default value", "default");
    await PreferenceExperiments.stop("test2", { resetValue: false });
    is(sendEventSpy.callCount, 2);
    is(
      sendEventSpy.getCall(1).args[3].didResetValue,
      "false",
      "PreferenceExperiments.stop() should pass false values of resetValue as didResetValue"
    );
  }
);

// Should send the correct event telemetry when a study ends because
// the user changed preferences during a browser run.
decorate_task(
  withMockPreferences(),
  withSendEventSpy(),
  withMockExperiments([
    preferenceStudyFactory({
      slug: "test",
      expired: false,
      branch: "fakebranch",
      preferences: {
        "fake.preference": {
          preferenceValue: "experimentvalue",
          preferenceType: "string",
          previousPreferenceValue: "oldvalue",
          preferenceBranchType: "default",
        },
      },
    }),
  ]),
  async function testPrefChangeEventTelemetry(
    mockPreferences,
    sendEventSpy,
    mockExperiments
  ) {
    ok(!Preferences.get("fake.preference"), "preference should start unset");
    mockPreferences.set("fake.preference", "oldvalue", "default");
    PreferenceExperiments.startObserver("test", {
      "fake.preference": {
        preferenceType: "string",
        preferenceValue: "experimentvalue",
      },
    });

    // setting the preference on the user branch should trigger the observer to stop the experiment
    mockPreferences.set("fake.preference", "uservalue", "user");

    // Wait until the change is processed
    await TestUtils.topicObserved(
      "normandy:preference-experiment:stopped",
      (subject, message) => {
        return message == "test";
      }
    );

    sendEventSpy.assertEvents([
      [
        "unenroll",
        "preference_study",
        "test",
        {
          didResetValue: "false",
          reason: "user-preference-changed",
          branch: "fakebranch",
          enrollmentId: mockExperiments[0].enrollmentId,
          changedPref: "fake.preference",
        },
      ],
    ]);
  }
);
