"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);
const { TelemetryEvents } = ChromeUtils.import(
  "resource://normandy/lib/TelemetryEvents.jsm"
);
const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);

const globalSandbox = sinon.sandbox.create();
globalSandbox.spy(TelemetryEnvironment, "setExperimentInactive");
globalSandbox.spy(TelemetryEvents, "sendEvent");
registerCleanupFunction(() => {
  globalSandbox.restore();
});

/**
 * Normal unenrollment:
 * - set .active to false
 * - set experiment inactive in telemetry
 * - send unrollment event
 */
add_task(async function test_set_inactive() {
  const manager = ExperimentFakes.manager();
  manager.store.addExperiment(ExperimentFakes.experiment("foo"));

  manager.unenroll("foo", { reason: "some-reason" });

  Assert.equal(
    manager.store.get("foo").active,
    false,
    "should set .active to false"
  );
});

add_task(async function test_setExperimentInactive_called() {
  globalSandbox.reset();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");
  manager.store.addExperiment(experiment);

  manager.unenroll("foo", { reason: "some-reason" });

  Assert.ok(
    TelemetryEnvironment.setExperimentInactive.calledWith("foo"),
    "should call TelemetryEnvironment.setExperimentInactive with slug"
  );
});

add_task(async function test_send_unenroll_event() {
  globalSandbox.reset();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");
  manager.store.addExperiment(experiment);

  manager.unenroll("foo", { reason: "some-reason" });

  Assert.ok(TelemetryEvents.sendEvent.calledOnce);
  Assert.deepEqual(
    TelemetryEvents.sendEvent.firstCall.args,
    [
      "unenroll",
      "preference_study", // This needs to be updated eventually
      "foo", // slug
      {
        reason: "some-reason",
        branch: experiment.branch.slug,
        enrollmentId: experiment.enrollmentId,
      },
    ],
    "should send an unenrollment ping with the slug, reason, branch slug, and enrollmentId"
  );
});

add_task(async function test_undefined_reason() {
  globalSandbox.reset();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");
  manager.store.addExperiment(experiment);

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
});
