/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Glean's here on `window`, but eslint doesn't know that. bug 1715542.
/* global Glean:false */

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

  let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.testResetFOG();

  is(
    undefined,
    Glean.testOnlyIpc.aCounter.testGetValue(),
    "Ensure we begin without value."
  );

  FOG.testTriggerGPUMetrics();
  await FOG.testFlushAllChildren();

  is(
    45326, // See gfx/ipc/GPUParent.cpp's RecvTestTriggerMetrics().
    Glean.testOnlyIpc.aCounter.testGetValue(),
    "Ensure the GPU-process-set value shows up in the parent process."
  );
});
