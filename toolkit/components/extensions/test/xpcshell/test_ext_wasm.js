/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

// Common code snippet of background script in this test.
function background() {
  globalThis.onsecuritypolicyviolation = event => {
    browser.test.assertEq("wasm-eval", event.blockedURI, "blockedURI");
    if (browser.runtime.getManifest().version === 2) {
      // In MV2, wasm eval violations are advisory only, as a transition tool.
      browser.test.assertEq(event.disposition, "report", "MV2 disposition");
    } else {
      browser.test.assertEq(event.disposition, "enforce", "MV3 disposition");
    }
    browser.test.sendMessage("violated_csp", event.originalPolicy);
  };
  try {
    let wasm = new WebAssembly.Module(
      new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
    );
    browser.test.assertEq(wasm.toString(), "[object WebAssembly.Module]");
    browser.test.sendMessage("result", "allowed");
  } catch (e) {
    browser.test.assertEq(
      "call to WebAssembly.Module() blocked by CSP",
      e.message,
      "Expected error when blocked"
    );
    browser.test.sendMessage("result", "blocked");
  }
}

add_task(async function test_wasm_v2() {
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version: 2,
    },
  });

  await extension.startup();
  equal(await extension.awaitMessage("result"), "allowed");
  await extension.unload();
});

add_task(async function test_wasm_v2_explicit() {
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version: 2,
      content_security_policy: `object-src; script-src 'self' 'wasm-unsafe-eval'`,
    },
  });

  await extension.startup();
  equal(await extension.awaitMessage("result"), "allowed");
  await extension.unload();
});

// MV3 counterpart is test_wasm_v3_blocked_by_custom_csp.
add_task(async function test_wasm_v2_blocked_in_report_only_mode() {
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version: 2,
      content_security_policy: `object-src; script-src 'self'`,
    },
  });

  await extension.startup();
  // "allowed" because wasm-unsafe-eval in MV2 is in report-only mode.
  equal(await extension.awaitMessage("result"), "allowed");
  equal(
    await extension.awaitMessage("violated_csp"),
    "object-src 'none'; script-src 'self'"
  );
  await extension.unload();
});

add_task(async function test_wasm_v3_blocked_by_default() {
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version: 3,
    },
  });

  await extension.startup();
  equal(await extension.awaitMessage("result"), "blocked");
  equal(
    await extension.awaitMessage("violated_csp"),
    "script-src 'self'",
    "WASM usage violates default CSP in MV3"
  );
  await extension.unload();
});

// MV2 counterpart is test_wasm_v2_blocked_in_report_only_mode.
add_task(async function test_wasm_v3_blocked_by_custom_csp() {
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version: 3,
      content_security_policy: {
        extension_pages: "object-src; script-src 'self'",
      },
    },
  });

  await extension.startup();
  equal(await extension.awaitMessage("result"), "blocked");
  equal(
    await extension.awaitMessage("violated_csp"),
    "object-src 'none'; script-src 'self'"
  );
  await extension.unload();
});

add_task(async function test_wasm_v3_allowed() {
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version: 3,
      content_security_policy: {
        extension_pages: `script-src 'self' 'wasm-unsafe-eval'; object-src 'self'`,
      },
    },
  });

  await extension.startup();
  equal(await extension.awaitMessage("result"), "allowed");
  await extension.unload();
});
