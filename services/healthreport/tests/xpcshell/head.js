/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We need to initialize the profile or OS.File may not work. See bug 810543.
do_get_profile();

(function initMetricsTestingInfrastructure() {
  let ns = {};
  Components.utils.import("resource://testing-common/services-common/logging.js",
                          ns);

  ns.initTestLogging();
}).call(this);

(function createAppInfo() {
  let ns = {};
  Components.utils.import("resource://testing-common/services/healthreport/utils.jsm", ns);
  ns.updateAppInfo();
}).call(this);

// The hack, it burns. This could go away if extensions code exposed its
// test environment setup functions as a testing-only JSM. See similar
// code in Sync's head_helpers.js.
let gGlobalScope = this;
function loadAddonManager() {
  let ns = {};
  Components.utils.import("resource://gre/modules/Services.jsm", ns);
  let head = "../../../../toolkit/mozapps/extensions/test/xpcshell/head_addons.js";
  let file = do_get_file(head);
  let uri = ns.Services.io.newFileURI(file);
  ns.Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();
}

