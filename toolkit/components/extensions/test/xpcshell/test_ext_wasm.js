/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_wasm_v2() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let wasm = new WebAssembly.Module(
        new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
      );
      browser.test.assertEq(wasm.toString(), "[object WebAssembly.Module]");
      browser.test.notifyPass("finished");
    },
    manifest: {
      manifest_version: 2,
    },
  });

  await extension.startup();
  await extension.awaitFinish("finished");
  await extension.unload();
});

add_task(async function test_wasm_v2_explicit() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let wasm = new WebAssembly.Module(
        new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
      );
      browser.test.assertEq(wasm.toString(), "[object WebAssembly.Module]");
      browser.test.notifyPass("finished");
    },
    manifest: {
      manifest_version: 2,
      content_security_policy: "object-src; script-src 'self'",
    },
  });

  await extension.startup();
  await extension.awaitFinish("finished");
  await extension.unload();
});

add_task(async function test_wasm_v2_blocked() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.assertThrows(
        () => {
          new WebAssembly.Module(
            new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
          );
        },
        error => error instanceof WebAssembly.CompileError,
        "WASM should be blocked"
      );
      browser.test.notifyPass("finished");
    },
    manifest: {
      manifest_version: 2,
      content_security_policy: "object-src; script-src 'self'",
    },
  });

  await extension.startup();
  await extension.awaitFinish("finished");
  await extension.unload();
});

add_task(async function test_wasm_v3() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.assertThrows(
        () => {
          new WebAssembly.Module(
            new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
          );
        },
        error => error instanceof WebAssembly.CompileError,
        "WASM should be blocked"
      );
      browser.test.notifyPass("finished");
    },
    manifest: {
      manifest_version: 3,
    },
  });

  await extension.startup();
  await extension.awaitFinish("finished");
  await extension.unload();
  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});

add_task(async function test_wasm_v3_allowed() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let wasm = new WebAssembly.Module(
        new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
      );
      browser.test.assertEq(wasm.toString(), "[object WebAssembly.Module]");
      browser.test.notifyPass("finished");
    },
    manifest: {
      manifest_version: 3,
      content_security_policy: {
        extension_pages: `script-src 'self' 'wasm-unsafe-eval'; object-src 'self'`,
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("finished");
  await extension.unload();
  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});
