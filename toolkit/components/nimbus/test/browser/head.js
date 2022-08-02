/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Globals
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  ExperimentTestUtils: "resource://testing-common/NimbusTestUtils.jsm",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
});

add_setup(function() {
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
    .callsFake(async enrollment => {
      await ExperimentTestUtils.validateEnrollment(enrollment);
      return origAddExperiment(enrollment);
    });

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});
