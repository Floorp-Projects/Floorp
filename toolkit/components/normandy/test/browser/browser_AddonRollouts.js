"use strict";

ChromeUtils.import("resource://gre/modules/IndexedDB.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonRollouts.jsm", this);
ChromeUtils.import("resource://normandy/lib/TelemetryEvents.jsm", this);

decorate_task(AddonRollouts.withTestMock, async function testGetMissing() {
  ok(
    !(await AddonRollouts.get("does-not-exist")),
    "get should return null when the requested rollout does not exist"
  );
});

decorate_task(AddonRollouts.withTestMock, async function testAddUpdateAndGet() {
  const rollout = {
    slug: "test-rollout",
    state: AddonRollouts.STATE_ACTIVE,
    extension: {},
  };
  await AddonRollouts.add(rollout);
  let storedRollout = await AddonRollouts.get(rollout.slug);
  Assert.deepEqual(
    rollout,
    storedRollout,
    "get should retrieve a rollout from storage."
  );

  rollout.state = AddonRollouts.STATE_ROLLED_BACK;
  await AddonRollouts.update(rollout);
  storedRollout = await AddonRollouts.get(rollout.slug);
  Assert.deepEqual(
    rollout,
    storedRollout,
    "get should retrieve a rollout from storage."
  );
});

decorate_task(
  AddonRollouts.withTestMock,
  async function testCantUpdateNonexistent() {
    const rollout = {
      slug: "test-rollout",
      state: AddonRollouts.STATE_ACTIVE,
      extensions: {},
    };
    await Assert.rejects(
      AddonRollouts.update(rollout),
      /doesn't already exist/,
      "Update should fail if the rollout doesn't exist"
    );
    ok(
      !(await AddonRollouts.has("test-rollout")),
      "rollout should not have been added"
    );
  }
);

decorate_task(AddonRollouts.withTestMock, async function testGetAll() {
  const rollout1 = { slug: "test-rollout-1", extension: {} };
  const rollout2 = { slug: "test-rollout-2", extension: {} };
  await AddonRollouts.add(rollout1);
  await AddonRollouts.add(rollout2);

  const storedRollouts = await AddonRollouts.getAll();
  Assert.deepEqual(
    storedRollouts.sort((a, b) => a.id - b.id),
    [rollout1, rollout2],
    "getAll should return every stored rollout."
  );
});

decorate_task(AddonRollouts.withTestMock, async function testGetAllActive() {
  const rollout1 = {
    slug: "test-rollout-1",
    state: AddonRollouts.STATE_ACTIVE,
  };
  const rollout3 = {
    slug: "test-rollout-2",
    state: AddonRollouts.STATE_ROLLED_BACK,
  };
  await AddonRollouts.add(rollout1);
  await AddonRollouts.add(rollout3);

  const activeRollouts = await AddonRollouts.getAllActive();
  Assert.deepEqual(
    activeRollouts,
    [rollout1],
    "getAllActive should return only active rollouts"
  );
});

decorate_task(AddonRollouts.withTestMock, async function testHas() {
  const rollout = { slug: "test-rollout", extensions: {} };
  await AddonRollouts.add(rollout);
  ok(
    await AddonRollouts.has(rollout.slug),
    "has should return true for an existing rollout"
  );
  ok(
    !(await AddonRollouts.has("does not exist")),
    "has should return false for a missing rollout"
  );
});

// init should mark active rollouts in telemetry
decorate_task(
  AddonRollouts.withTestMock,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  async function testInitTelemetry(setExperimentActiveStub) {
    await AddonRollouts.add({
      slug: "test-rollout-active-1",
      state: AddonRollouts.STATE_ACTIVE,
    });
    await AddonRollouts.add({
      slug: "test-rollout-active-2",
      state: AddonRollouts.STATE_ACTIVE,
    });
    await AddonRollouts.add({
      slug: "test-rollout-rolled-back",
      state: AddonRollouts.STATE_ROLLED_BACK,
    });

    await AddonRollouts.init();

    Assert.deepEqual(
      setExperimentActiveStub.args,
      [
        ["test-rollout-active-1", "active", { type: "normandy-addonrollout" }],
        ["test-rollout-active-2", "active", { type: "normandy-addonrollout" }],
      ],
      "init should set activate a telemetry experiment for active addons"
    );
  }
);
