/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

add_setup(
  /* on Android FOG is set up through head.js */
  { skip_if: () => !runningInParent || AppConstants.platform == "android" },
  function test_setup() {
    // Give FOG a temp profile to init within.
    do_get_profile();

    // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
    Services.fog.initializeFOG();
  }
);

// Keep in sync with ipc.rs.
// "Why no -1?" Because the limit's 100k. The -1 is because of atomic ops.
const FOG_IPC_PAYLOAD_ACCESS_LIMIT = 100000;

add_task({ skip_if: () => runningInParent }, async function run_child_stuff() {
  for (let i = 0; i < FOG_IPC_PAYLOAD_ACCESS_LIMIT + 1; i++) {
    Glean.testOnly.badCode.add(1);
  }
});

add_task(
  { skip_if: () => !runningInParent },
  async function test_fog_ipc_limit() {
    await run_test_in_child("test_FOGIPCLimit.js");

    // The child exceeded the number of accesses to trigger an IPC flush.
    // Could potentially intermittently fail if `run_test_in_child` doesn't
    // wait until the main thread runnable performs the flush.
    // In practice this seems to reliably succeed, but who knows what the future may hold.
    Assert.greater(
      Glean.testOnly.badCode.testGetValue(),
      FOG_IPC_PAYLOAD_ACCESS_LIMIT
    );
  }
);
