/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test attemps to verify that:
// - SharedWorkers cannot be created by web extensions when web extensions
//   are being run in the main process (because SharedWorkers are only
//   allowed to be spawned in the parent process if they have a system principal).
add_task(async function test_spawn_shared_worker() {
  const background = function() {
    // This test covers the builds where the extensions are still
    // running in the main process (it just checks that we don't
    // allow it).
    browser.test.assertThrows(
      () => {
        try {
          new SharedWorker("worker.js");
        } catch (e) {
          // assertThrows is currently failing to match the error message
          // automatically, let's cheat a little bit for now.
          throw new Error(`${e}`);
        }
      },
      /NS_ERROR_ABORT/,
      "Got the expected failure in non-remote mode"
    );

    browser.test.sendMessage("test-shared-worker:done");
  };
  const extension = ExtensionTestUtils.loadExtension({
    background,
    files: {
      "worker.js": function() {
        self.onconnect = evt => {
          const port = evt.ports[0];
          port.onmessage = () => port.postMessage("worker-reply");
        };
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("test-shared-worker:done");
  await extension.unload();
});
