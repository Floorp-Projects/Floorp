"use strict";
// Globals

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
});
