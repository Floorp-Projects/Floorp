"use strict";

ChromeUtils.import("resource://gre/modules/IndexedDB.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceRollouts.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);

decorate_task(
  PreferenceRollouts.withTestMock,
  async function testGetMissing() {
    is(
      await PreferenceRollouts.get("does-not-exist"),
      null,
      "get should return null when the requested rollout does not exist"
    );
  }
);

decorate_task(
  PreferenceRollouts.withTestMock,
  async function testAddUpdateAndGet() {
    const rollout = {slug: "test-rollout", state: PreferenceRollouts.STATE_ACTIVE, preferences: []};
    await PreferenceRollouts.add(rollout);
    let storedRollout = await PreferenceRollouts.get(rollout.slug);
    Assert.deepEqual(rollout, storedRollout, "get should retrieve a rollout from storage.");

    rollout.state = PreferenceRollouts.STATE_GRADUATED;
    await PreferenceRollouts.update(rollout);
    storedRollout = await PreferenceRollouts.get(rollout.slug);
    Assert.deepEqual(rollout, storedRollout, "get should retrieve a rollout from storage.");
  },
);

decorate_task(
  PreferenceRollouts.withTestMock,
  async function testCantUpdateNonexistent() {
    const rollout = {slug: "test-rollout", state: PreferenceRollouts.STATE_ACTIVE, preferences: []};
    await Assert.rejects(
      PreferenceRollouts.update(rollout),
      /doesn't already exist/,
      "Update should fail if the rollout doesn't exist",
    );
    ok(!await PreferenceRollouts.has("test-rollout"), "rollout should not have been added");
  },
);


decorate_task(
  PreferenceRollouts.withTestMock,
  async function testGetAll() {
    const rollout1 = {slug: "test-rollout-1", preference: []};
    const rollout2 = {slug: "test-rollout-2", preference: []};
    await PreferenceRollouts.add(rollout1);
    await PreferenceRollouts.add(rollout2);

    const storedRollouts = await PreferenceRollouts.getAll();
    Assert.deepEqual(
      storedRollouts.sort((a, b) => a.id - b.id),
      [rollout1, rollout2],
      "getAll should return every stored rollout.",
    );
  }
);

decorate_task(
  PreferenceRollouts.withTestMock,
  async function testGetAllActive() {
    const rollout1 = {slug: "test-rollout-1", state: PreferenceRollouts.STATE_ACTIVE};
    const rollout2 = {slug: "test-rollout-2", state: PreferenceRollouts.STATE_GRADUATED};
    const rollout3 = {slug: "test-rollout-3", state: PreferenceRollouts.STATE_ROLLED_BACK};
    await PreferenceRollouts.add(rollout1);
    await PreferenceRollouts.add(rollout2);
    await PreferenceRollouts.add(rollout3);

    const activeRollouts = await PreferenceRollouts.getAllActive();
    Assert.deepEqual(activeRollouts, [rollout1], "getAllActive should return only active rollouts");
  }
);

decorate_task(
  PreferenceRollouts.withTestMock,
  async function testHas() {
    const rollout = {slug: "test-rollout", preferences: []};
    await PreferenceRollouts.add(rollout);
    ok(await PreferenceRollouts.has(rollout.slug), "has should return true for an existing rollout");
    ok(!await PreferenceRollouts.has("does not exist"), "has should return false for a missing rollout");
  }
);

decorate_task(
  PreferenceRollouts.withTestMock,
  async function testCloseDatabase() {
    await PreferenceRollouts.closeDB();
    const openSpy = sinon.spy(IndexedDB, "open");
    sinon.assert.notCalled(openSpy);

    try {
      // Using rollouts at all should open the database, but only once.
      await PreferenceRollouts.has("foo");
      await PreferenceRollouts.get("foo");
      sinon.assert.calledOnce(openSpy);
      openSpy.reset();

      // close can be called multiple times
      await PreferenceRollouts.closeDB();
      await PreferenceRollouts.closeDB();
      // and don't cause the database to be opened (that would be weird)
      sinon.assert.notCalled(openSpy);

      // After being closed, new operations cause the database to be opened again, but only once
      await PreferenceRollouts.has("foo");
      await PreferenceRollouts.get("foo");
      sinon.assert.calledOnce(openSpy);

    } finally {
      openSpy.restore();
    }
  }
);

// recordOriginalValue should update storage to note the original values
decorate_task(
  PreferenceRollouts.withTestMock,
  async function testRecordOriginalValuesUpdatesPreviousValues() {
    await PreferenceRollouts.add({
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ACTIVE,
      preferences: [{preferenceName: "test.pref", value: 2, previousValue: null}],
    });

    await PreferenceRollouts.recordOriginalValues({"test.pref": 1});

    Assert.deepEqual(
      await PreferenceRollouts.getAll(),
      [{
        slug: "test-rollout",
        state: PreferenceRollouts.STATE_ACTIVE,
        preferences: [{preferenceName: "test.pref", value: 2, previousValue: 1}],
      }],
      "rollout in database should be updated",
    );
  },
);

// recordOriginalValue should graduate a study when it is no longer relevant.
decorate_task(
  PreferenceRollouts.withTestMock,
  withSendEventStub,
  async function testRecordOriginalValuesUpdatesPreviousValues(sendEventStub) {
    await PreferenceRollouts.add({
      slug: "test-rollout",
      state: PreferenceRollouts.STATE_ACTIVE,
      preferences: [
        {preferenceName: "test.pref1", value: 2, previousValue: null},
        {preferenceName: "test.pref2", value: 2, previousValue: null},
      ],
    });

    // one pref being the same isn't enough to graduate
    await PreferenceRollouts.recordOriginalValues({"test.pref1": 1, "test.pref2": 2});
    let rollout = await PreferenceRollouts.get("test-rollout");
    is(
      rollout.state,
      PreferenceRollouts.STATE_ACTIVE,
      "rollouts should remain active when only one pref matches the built-in default",
    );

    Assert.deepEqual(sendEventStub.args, [], "no events should be sent yet");

    // both prefs is enough
    await PreferenceRollouts.recordOriginalValues({"test.pref1": 2, "test.pref2": 2});
    rollout = await PreferenceRollouts.get("test-rollout");
    is(
      rollout.state,
      PreferenceRollouts.STATE_GRADUATED,
      "rollouts should graduate when all prefs matches the built-in defaults",
    );

    Assert.deepEqual(
      sendEventStub.args,
      [["graduate", "preference_rollout", "test-rollout", {}]],
      "a graduation event should be sent",
    );
  },
);

// init should mark active rollouts in telemetry
decorate_task(
  PreferenceRollouts.withTestMock,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  async function testInitTelemetry(setExperimentActiveStub) {
    await PreferenceRollouts.add({
      slug: "test-rollout-active-1",
      state: PreferenceRollouts.STATE_ACTIVE,
    });
    await PreferenceRollouts.add({
      slug: "test-rollout-active-2",
      state: PreferenceRollouts.STATE_ACTIVE,
    });
    await PreferenceRollouts.add({
      slug: "test-rollout-rolled-back",
      state: PreferenceRollouts.STATE_ROLLED_BACK,
    });
    await PreferenceRollouts.add({
      slug: "test-rollout-graduated",
      state: PreferenceRollouts.STATE_GRADUATED,
    });

    await PreferenceRollouts.init();

    Assert.deepEqual(
      setExperimentActiveStub.args,
      [
        ["test-rollout-active-1", "active", {type: "normandy-prefrollout"}],
        ["test-rollout-active-2", "active", {type: "normandy-prefrollout"}],
      ],
      "init should set activate a telemetry experiment for active preferences"
    );
  },
);
