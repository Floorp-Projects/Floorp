/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testExtensionWithBackground({
  with_scripts = false,
  with_service_worker = false,
  with_page = false,
  expected_background_type,
  expected_manifest_warnings = [],
}) {
  let background = {};
  if (with_scripts) {
    background.scripts = ["scripts.js"];
  }
  if (with_service_worker) {
    background.service_worker = "sw.js";
  }
  if (with_page) {
    background.page = "page.html";
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { background },
    files: {
      "scripts.js": () => {
        browser.test.sendMessage("from_bg", "scripts");
      },
      "sw.js": () => {
        browser.test.sendMessage("from_bg", "service_worker");
      },
      "page.html": `<!DOCTYPE html><script src="page.js"></script>`,
      "page.js": () => {
        browser.test.sendMessage("from_bg", "page");
      },
    },
  });
  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);
  Assert.deepEqual(
    extension.extension.warnings,
    expected_manifest_warnings,
    "Expected manifest warnings"
  );
  info("Waiting for background to start");
  Assert.equal(
    await extension.awaitMessage("from_bg"),
    expected_background_type,
    "Expected background type"
  );
  await extension.unload();
}

add_task(async function test_page_and_scripts() {
  await testExtensionWithBackground({
    with_page: true,
    with_scripts: true,
    // Should be expected_background_type: "scripts", not "page".
    // https://github.com/w3c/webextensions/issues/282#issuecomment-1443332913
    // ... but changing that may potentially affect backcompat of existing
    // Firefox add-ons.
    expected_background_type: "page",
    expected_manifest_warnings: [
      "Reading manifest: Warning processing background.scripts: An unexpected property was found in the WebExtension manifest.",
    ],
  });
});

add_task(
  { skip_if: () => WebExtensionPolicy.backgroundServiceWorkerEnabled },
  async function test_scripts_and_service_worker_when_sw_disabled() {
    await testExtensionWithBackground({
      with_scripts: true,
      with_service_worker: true,
      expected_background_type: "scripts",
      expected_manifest_warnings: [
        "Reading manifest: Warning processing background.service_worker: An unexpected property was found in the WebExtension manifest.",
      ],
    });
  }
);
