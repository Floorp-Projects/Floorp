"use strict";
// Globals

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { Ajv } = ChromeUtils.import("resource://testing-common/ajv-6.12.6.js");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
});
