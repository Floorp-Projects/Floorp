/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  const utilityProcessTest = Cc[
    "@mozilla.org/utility-process-test;1"
  ].createInstance(Ci.nsIUtilityProcessTest);
  await utilityProcessTest
    .startProcess()
    .then(async () => {
      Services.fog.testResetFOG();

      is(
        null,
        Glean.testOnlyIpc.aCounter.testGetValue(),
        "Ensure we begin without value."
      );

      await Services.fog.testTriggerMetrics(
        Ci.nsIXULRuntime.PROCESS_TYPE_UTILITY
      );
      await Services.fog.testFlushAllChildren();

      is(
        Glean.testOnlyIpc.aCounter.testGetValue(),
        Ci.nsIXULRuntime.PROCESS_TYPE_UTILITY,
        "Ensure the utility-process-set value shows up in the parent process."
      );
    })
    .catch(async () => {
      ok(false, "Cannot start Utility process?");
    });

  await utilityProcessTest.stopProcess();
});
