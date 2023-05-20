/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Globals
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
  ExperimentTestUtils: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
});

add_setup(function () {
  let sandbox = sinon.createSandbox();

  /* We stub the functions that operate with enrollments and remote rollouts
   * so that any access to store something is implicitly validated against
   * the schema and no records have missing (or extra) properties while in tests
   */

  let origAddExperiment = ExperimentManager.store.addEnrollment.bind(
    ExperimentManager.store
  );
  sandbox
    .stub(ExperimentManager.store, "addEnrollment")
    .callsFake(enrollment => {
      ExperimentTestUtils.validateEnrollment(enrollment);
      return origAddExperiment(enrollment);
    });

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});
