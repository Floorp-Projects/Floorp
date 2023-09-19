/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  if (
    !(await ChromeUtils.requestProcInfo()).children.some(
      p => p.type == "socket"
    )
  ) {
    ok(true, "No Socket process? No test.");
    return;
  }
  ok(true, "Socket process found: Let's test.");

  Services.fog.testResetFOG();

  is(
    null,
    Glean.testOnlyIpc.aCounter.testGetValue(),
    "Ensure we begin without value."
  );

  await Services.fog.testTriggerMetrics(Ci.nsIXULRuntime.PROCESS_TYPE_SOCKET);
  await Services.fog.testFlushAllChildren();

  is(
    Glean.testOnlyIpc.aCounter.testGetValue(),
    Ci.nsIXULRuntime.PROCESS_TYPE_SOCKET,
    "Ensure the Socket-process-set value shows up in the parent process."
  );
});
