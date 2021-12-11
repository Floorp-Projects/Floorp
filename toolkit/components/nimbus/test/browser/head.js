/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Globals
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  Ajv: "resource://testing-common/ajv-6.12.6.js",
  ExperimentTestUtils: "resource://testing-common/NimbusTestUtils.jsm",
  RemoteDefaultsLoader:
    "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
});

add_task(function setup() {
  let sandbox = sinon.createSandbox();

  /* We stub the functions that operate with enrollments and remote rollouts
   * so that any access to store something is implicitly validated against
   * the schema and no records have missing (or extra) properties while in tests
   */

  let origAddExperiment = ExperimentManager.store.addExperiment.bind(
    ExperimentManager.store
  );
  let origOnUpdatesReady = RemoteDefaultsLoader._onUpdatesReady.bind(
    RemoteDefaultsLoader
  );
  sandbox
    .stub(ExperimentManager.store, "addExperiment")
    .callsFake(async enrollment => {
      await ExperimentTestUtils.validateEnrollment(enrollment);
      return origAddExperiment(enrollment);
    });
  // Unlike `addExperiment` the method to store remote rollouts is syncronous
  // and our validation method would turn it async. If we had changed to `await`
  // for remote configs storage it would have changed the code logic so we are
  // going up one level to the function that receives the RS records and do
  // the validation there.
  sandbox
    .stub(RemoteDefaultsLoader, "_onUpdatesReady")
    .callsFake(async (remoteDefaults, reason) => {
      for (let remoteDefault of remoteDefaults) {
        for (let config of remoteDefault.configurations) {
          await ExperimentTestUtils.validateRollouts(config);
        }
      }
      return origOnUpdatesReady(remoteDefaults, reason);
    });

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});
