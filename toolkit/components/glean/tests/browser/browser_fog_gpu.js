/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  if (
    !(await ChromeUtils.requestProcInfo()).children.some(p => p.type == "gpu")
  ) {
    ok(
      true,
      'No GPU process? No test. Try again with --setpref "layers.gpu-process.force-enabled=true".'
    );
    return;
  }
  ok(true, "GPU Process found: Let's test.");

  Services.fog.testResetFOG();

  is(
    null,
    Glean.testOnlyIpc.aCounter.testGetValue(),
    "Ensure we begin without value."
  );

  await Services.fog.testTriggerMetrics(Ci.nsIXULRuntime.PROCESS_TYPE_GPU);
  await Services.fog.testFlushAllChildren();

  is(
    Glean.testOnlyIpc.aCounter.testGetValue(),
    Ci.nsIXULRuntime.PROCESS_TYPE_GPU,
    "Ensure the GPU-process-set value shows up in the parent process."
  );
});
