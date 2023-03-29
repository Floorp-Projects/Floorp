"use strict";
// Globals

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
  ExperimentTestUtils: "resource://testing-common/NimbusTestUtils.jsm",
});
