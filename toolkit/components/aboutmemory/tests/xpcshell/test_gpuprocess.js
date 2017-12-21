/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let { classes: Cc, interfaces: Ci } = Components;

function run_test() {
  let gfxInfo = Cc["@mozilla.org/gfx/info;1"].
                getService(Ci.nsIGfxInfo);
  let mgr = Cc["@mozilla.org/memory-reporter-manager;1"].
            getService(Ci.nsIMemoryReporterManager);

  let ok = gfxInfo.controlGPUProcessForXPCShell(true);
  Assert.equal(ok, true);

  let endTesting = function() {
    gfxInfo.controlGPUProcessForXPCShell(false);
    do_test_finished();
  };

  let foundGPUProcess = false;
  let onHandleReport = function(aProcess, aPath, aKind, aUnits, aAmount, aDescription) {
    if (/GPU \(pid \d+\)/.test(aProcess)) {
      foundGPUProcess = true;
    }
  };
  let onFinishReporting = function() {
    Assert.equal(foundGPUProcess, true);
    endTesting();
  };

  mgr.getReports(
    onHandleReport,
    null,
    onFinishReporting,
    null,
    false);
  do_test_pending();
}
