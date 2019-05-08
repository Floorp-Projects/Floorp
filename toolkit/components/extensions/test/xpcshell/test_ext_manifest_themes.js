/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function test_theme_property(property) {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    "theme": {
      [property]: {
      },
    },
  }, "manifest.ThemeManifest");

  if (property === "unrecognized_key") {
    const expectedWarning = `Error processing theme.${property}`;
    ok(normalized.errors[0].includes(expectedWarning),
       `The manifest warning ${JSON.stringify(normalized.errors[0])} must contain ${JSON.stringify(expectedWarning)}`);
  } else {
    equal(normalized.errors.length, 0, "Should have a warning");
  }
  equal(normalized.error, undefined, "Should not have an error");
}

add_task(async function test_manifest_themes() {
  await test_theme_property("images");
  await test_theme_property("colors");
  await test_theme_property("unrecognized_key");
});
