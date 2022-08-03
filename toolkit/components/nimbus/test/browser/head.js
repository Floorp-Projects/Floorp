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
