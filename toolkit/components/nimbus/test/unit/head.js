"use strict";
// Globals

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
ChromeUtils.defineESModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ExperimentTestUtils: "resource://testing-common/NimbusTestUtils.sys.mjs",
});
