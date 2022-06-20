"use strict";

const { TelemetryEvents } = ChromeUtils.import(
  "resource://normandy/lib/TelemetryEvents.jsm"
);
const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
const STUDIES_OPT_OUT_PREF = "app.shield.optoutstudies.enabled";
const UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";

const globalSandbox = sinon.createSandbox();
globalSandbox.spy(TelemetryEnvironment, "setExperimentInactive");
globalSandbox.spy(TelemetryEvents, "sendEvent");
registerCleanupFunction(() => {
  globalSandbox.restore();
});

/**
 * FOG requires a little setup in order to test it
 */
add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

/**
 * Normal unenrollment for experiments:
 * - set .active to false
 * - set experiment inactive in telemetry
 * - send unrollment event
 */
add_task(async function test_set_inactive() {
  const manager = ExperimentFakes.manager();

  await manager.onStartup();
  await manager.store.addEnrollment(ExperimentFakes.experiment("foo"));

  manager.unenroll("foo", "some-reason");

  Assert.equal(
    manager.store.get("foo").active,
    false,
    "should set .active to false"
  );
});

add_task(async function test_unenroll_opt_out() {
  globalSandbox.reset();
  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  // Check that there aren't any Glean unenrollment events yet
  var unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(
    undefined,
    unenrollmentEvents,
    "no Glean unenrollment events before unenrollment"
  );

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, false);

  Assert.equal(
    manager.store.get(experiment.slug).active,
    false,
    "should set .active to false"
  );
  Assert.ok(TelemetryEvents.sendEvent.calledOnce);
  Assert.deepEqual(
    TelemetryEvents.sendEvent.firstCall.args,
    [
      "unenroll",
      "nimbus_experiment",
      experiment.slug,
      {
        reason: "studies-opt-out",
        branch: experiment.branch.slug,
        enrollmentId: experiment.enrollmentId,
      },
    ],
    "should send an unenrollment ping with the slug, reason, branch slug, and enrollmentId"
  );

  // Check that the Glean unenrollment event was recorded.
  unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    experiment.slug,
    unenrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.unenrollment recorded with correct experiment slug"
  );
  Assert.equal(
    experiment.branch.slug,
    unenrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.unenrollment recorded with correct branch slug"
  );
  Assert.equal(
    "studies-opt-out",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct reason"
  );
  Assert.equal(
    experiment.enrollmentId,
    unenrollmentEvents[0].extra.enrollment_id,
    "Glean.nimbusEvents.unenrollment recorded with correct enrollment id"
  );

  // reset pref
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_unenroll_rollout_opt_out() {
  globalSandbox.reset();
  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);
  const manager = ExperimentFakes.manager();
  const rollout = ExperimentFakes.rollout("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(rollout);

  // Check that there aren't any Glean unenrollment events yet
  var unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(
    undefined,
    unenrollmentEvents,
    "no Glean unenrollment events before unenrollment"
  );

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, false);

  Assert.equal(
    manager.store.get(rollout.slug).active,
    false,
    "should set .active to false"
  );
  Assert.ok(TelemetryEvents.sendEvent.calledOnce);
  Assert.deepEqual(
    TelemetryEvents.sendEvent.firstCall.args,
    [
      "unenroll",
      "nimbus_experiment",
      rollout.slug,
      {
        reason: "studies-opt-out",
        branch: rollout.branch.slug,
        enrollmentId: rollout.enrollmentId,
      },
    ],
    "should send an unenrollment ping with the slug, reason, branch slug, and enrollmentId"
  );

  // Check that the Glean unenrollment event was recorded.
  unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    rollout.slug,
    unenrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.unenrollment recorded with correct rollout slug"
  );
  Assert.equal(
    rollout.branch.slug,
    unenrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.unenrollment recorded with correct branch slug"
  );
  Assert.equal(
    "studies-opt-out",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct reason"
  );
  Assert.equal(
    rollout.enrollmentId,
    unenrollmentEvents[0].extra.enrollment_id,
    "Glean.nimbusEvents.unenrollment recorded with correct enrollment id"
  );

  // reset pref
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
});

add_task(async function test_unenroll_uploadPref() {
  globalSandbox.reset();
  const manager = ExperimentFakes.manager();
  const recipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();
  await ExperimentFakes.enrollmentHelper(recipe, { manager }).enrollmentPromise;

  Assert.equal(
    manager.store.get(recipe.slug).active,
    true,
    "Should set .active to true"
  );

  Services.prefs.setBoolPref(UPLOAD_ENABLED_PREF, false);

  Assert.equal(
    manager.store.get(recipe.slug).active,
    false,
    "Should set .active to false"
  );
  Services.prefs.clearUserPref(UPLOAD_ENABLED_PREF);
});

add_task(async function test_setExperimentInactive_called() {
  globalSandbox.reset();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  // Because `manager.store.addEnrollment()` sidesteps telemetry recording
  // we will also call on the Glean experiment API directly to test that
  // `manager.unenroll()` does in fact call `Glean.setExperimentActive()`
  Services.fog.setExperimentActive(
    experiment.slug,
    experiment.branch.slug,
    null
  );

  // Test Glean experiment API interaction
  Assert.notEqual(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "experiment should be active before unenroll"
  );

  manager.unenroll("foo", "some-reason");

  Assert.ok(
    TelemetryEnvironment.setExperimentInactive.calledWith("foo"),
    "should call TelemetryEnvironment.setExperimentInactive with slug"
  );

  // Test Glean experiment API interaction
  Assert.equal(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "experiment should be inactive after unenroll"
  );
});

add_task(async function test_send_unenroll_event() {
  globalSandbox.reset();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  // Check that there aren't any Glean unenrollment events yet
  var unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(
    undefined,
    unenrollmentEvents,
    "no Glean unenrollment events before unenrollment"
  );

  manager.unenroll("foo", "some-reason");

  Assert.ok(TelemetryEvents.sendEvent.calledOnce);
  Assert.deepEqual(
    TelemetryEvents.sendEvent.firstCall.args,
    [
      "unenroll",
      "nimbus_experiment",
      "foo", // slug
      {
        reason: "some-reason",
        branch: experiment.branch.slug,
        enrollmentId: experiment.enrollmentId,
      },
    ],
    "should send an unenrollment ping with the slug, reason, branch slug, and enrollmentId"
  );

  // Check that the Glean unenrollment event was recorded.
  unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    experiment.slug,
    unenrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.unenrollment recorded with correct experiment slug"
  );
  Assert.equal(
    experiment.branch.slug,
    unenrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.unenrollment recorded with correct branch slug"
  );
  Assert.equal(
    "some-reason",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct reason"
  );
  Assert.equal(
    experiment.enrollmentId,
    unenrollmentEvents[0].extra.enrollment_id,
    "Glean.nimbusEvents.unenrollment recorded with correct enrollment id"
  );
});

add_task(async function test_undefined_reason() {
  globalSandbox.reset();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  manager.unenroll("foo");

  const options = TelemetryEvents.sendEvent.firstCall?.args[3];
  Assert.ok(
    "reason" in options,
    "options object with .reason should be the fourth param"
  );
  Assert.equal(
    options.reason,
    "unknown",
    "should include unknown as the reason if none was supplied"
  );

  // Check that the Glean unenrollment event was recorded.
  let unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event reason matches the expected reason
  Assert.equal(
    "unknown",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct (unknown) reason"
  );
});

/**
 * Normal unenrollment for rollouts:
 * - remove stored enrollment and synced data (prefs)
 * - set rollout inactive in telemetry
 * - send unrollment event
 */

add_task(async function test_remove_rollouts() {
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);
  const rollout = ExperimentFakes.rollout("foo");

  sinon.stub(store, "get").returns(rollout);
  sinon.spy(store, "updateExperiment");

  await manager.onStartup();

  manager.unenroll("foo", "some-reason");

  Assert.ok(
    manager.store.updateExperiment.calledOnce,
    "Called to set the rollout as !active"
  );
  Assert.ok(
    manager.store.updateExperiment.calledWith(rollout.slug, { active: false }),
    "Called with expected parameters"
  );
});

add_task(async function test_remove_rollout_onFinalize() {
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);
  const rollout = ExperimentFakes.rollout("foo");

  sinon.stub(store, "getAllRollouts").returns([rollout]);
  sinon.stub(store, "get").returns(rollout);
  sinon.spy(manager, "unenroll");
  sinon.spy(manager, "sendFailureTelemetry");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();

  manager.onFinalize("NimbusTestUtils");

  // Check that there aren't any Glean unenroll_failed events
  var unenrollFailedEvents = Glean.nimbusEvents.unenrollFailed.testGetValue();
  Assert.equal(
    undefined,
    unenrollFailedEvents,
    "no Glean unenroll_failed events when removing rollout"
  );

  Assert.ok(manager.sendFailureTelemetry.notCalled, "Nothing should fail");
  Assert.ok(manager.unenroll.calledOnce, "Should unenroll recipe not seen");
  Assert.ok(manager.unenroll.calledWith(rollout.slug, "recipe-not-seen"));
});

add_task(async function test_rollout_telemetry_events() {
  globalSandbox.restore();
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);
  const rollout = ExperimentFakes.rollout("foo");
  globalSandbox.spy(TelemetryEnvironment, "setExperimentInactive");
  globalSandbox.spy(TelemetryEvents, "sendEvent");

  sinon.stub(store, "getAllRollouts").returns([rollout]);
  sinon.stub(store, "get").returns(rollout);
  sinon.spy(manager, "sendFailureTelemetry");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();

  // Check that there aren't any Glean unenrollment events yet
  var unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  Assert.equal(
    undefined,
    unenrollmentEvents,
    "no Glean unenrollment events before unenrollment"
  );

  manager.onFinalize("NimbusTestUtils");

  // Check that there aren't any Glean unenroll_failed events
  var unenrollFailedEvents = Glean.nimbusEvents.unenrollFailed.testGetValue();
  Assert.equal(
    undefined,
    unenrollFailedEvents,
    "no Glean unenroll_failed events when removing rollout"
  );

  Assert.ok(manager.sendFailureTelemetry.notCalled, "Nothing should fail");
  Assert.ok(
    TelemetryEnvironment.setExperimentInactive.calledOnce,
    "Should unenroll recipe not seen"
  );
  Assert.ok(
    TelemetryEnvironment.setExperimentInactive.calledWith(rollout.slug),
    "Should set rollout to inactive."
  );
  // Test Glean experiment API interaction
  Assert.equal(
    undefined,
    Services.fog.testGetExperimentData(rollout.slug),
    "Should set rollout to inactive"
  );

  Assert.ok(
    TelemetryEvents.sendEvent.calledWith(
      "unenroll",
      sinon.match.string,
      rollout.slug,
      sinon.match.object
    ),
    "Should send unenroll event for rollout."
  );

  // Check that the Glean unenrollment event was recorded.
  unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue();
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    rollout.slug,
    unenrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.unenrollment recorded with correct rollout slug"
  );
  Assert.equal(
    rollout.branch.slug,
    unenrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.unenrollment recorded with correct branch slug"
  );
  Assert.equal(
    "recipe-not-seen",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct reason"
  );
  Assert.equal(
    rollout.enrollmentId,
    unenrollmentEvents[0].extra.enrollment_id,
    "Glean.nimbusEvents.unenrollment recorded with correct enrollment id"
  );

  globalSandbox.restore();
});
