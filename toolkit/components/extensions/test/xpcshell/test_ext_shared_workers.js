/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test attemps to verify that:
// - SharedWorkers can be created and successfully spawned by web extensions
//   when web-extensions run in their own child process.
add_task(async function test_spawn_shared_worker() {
  const background = async function() {
    const worker = new SharedWorker("worker.js");
    await new Promise(resolve => {
      worker.port.onmessage = resolve;
      worker.port.postMessage("bgpage->worker");
    });
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
