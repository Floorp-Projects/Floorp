/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_shared_array_buffer_worker() {
  const extension_description = {
    isPrivileged: null,
    async background() {
      browser.test.onMessage.addListener(async isPrivileged => {
        const worker = new Worker("worker.js");
        worker.isPrivileged = isPrivileged;
        worker.onmessage = function (e) {
          const msg = `${
            this.isPrivileged
              ? "privileged addon can"
              : "non-privileged addon can't"
          } instantiate a SharedArrayBuffer
          in a worker`;
          if (e.data === this.isPrivileged) {
            browser.test.succeed(msg);
          } else {
            browser.test.fail(msg);
          }
          browser.test.sendMessage("test-sab-worker:done");
        };
      });
    },
    files: {
      "worker.js": function () {
        try {
          new SharedArrayBuffer(1);
          this.postMessage(true);
        } catch (e) {
          this.postMessage(false);
        }
      },
    },
  };

  // This test attempts to verify that a worker inside a privileged addon
  // is allowed to instantiate a SharedArrayBuffer
  extension_description.isPrivileged = true;
  let extension = ExtensionTestUtils.loadExtension(extension_description);
  await extension.startup();
  extension.sendMessage(extension_description.isPrivileged);
  await extension.awaitMessage("test-sab-worker:done");
  await extension.unload();

  // This test attempts to verify that a worker inside a non privileged addon
  // is not allowed to instantiate a SharedArrayBuffer
  extension_description.isPrivileged = false;
  extension = ExtensionTestUtils.loadExtension(extension_description);
  await extension.startup();
  extension.sendMessage(extension_description.isPrivileged);
  await extension.awaitMessage("test-sab-worker:done");
  await extension.unload();
});

add_task(async function test_shared_array_buffer_content() {
  let extension_description = {
    isPrivileged: null,
    async background() {
      browser.test.onMessage.addListener(async isPrivileged => {
        let succeed = null;
        try {
          new SharedArrayBuffer(1);
          succeed = true;
        } catch (e) {
          succeed = false;
        } finally {
          const msg = `${
            isPrivileged ? "privileged addon can" : "non-privileged addon can't"
          } instantiate a SharedArrayBuffer
          in the main thread`;
          if (succeed === isPrivileged) {
            browser.test.succeed(msg);
          } else {
            browser.test.fail(msg);
          }
          browser.test.sendMessage("test-sab-content:done");
        }
      });
    },
  };

  // This test attempts to verify that a non privileged addon
  // is allowed to instantiate a sharedarraybuffer
  extension_description.isPrivileged = true;
  let extension = ExtensionTestUtils.loadExtension(extension_description);
  await extension.startup();
  extension.sendMessage(extension_description.isPrivileged);
  await extension.awaitMessage("test-sab-content:done");
  await extension.unload();

  // This test attempts to verify that a non privileged addon
  // is not allowed to instantiate a sharedarraybuffer
  extension_description.isPrivileged = false;
  extension = ExtensionTestUtils.loadExtension(extension_description);
  await extension.startup();
  extension.sendMessage(extension_description.isPrivileged);
  await extension.awaitMessage("test-sab-content:done");
  await extension.unload();
});
